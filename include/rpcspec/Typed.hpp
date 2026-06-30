/** @file */
#pragma once
// Typed spec: validates a JSON request AND produces a strong-typed handler Input
// struct in a single pass.
//
// Why this exists
// ---------------
// The classic path is: RpcSpec::process() validates the JSON, then the handler
// separately deserialises it (tag_invoke / value_to) into an Input struct, and
// process() trusts that the two agree. When they diverge the handler can read a
// field the validator never guaranteed and crash (ASSERT / unchecked optional
// dereference).
//
// Here a field binds its JSON key to a pointer-to-member of the Input struct and
// to a typed converter. parse() walks the fields, runs each converter (which both
// validates and produces the value), and assigns the result through the member
// pointer. Consequences:
//   * single pass — no separate deserialisation, no double validation;
//   * presence is the member's type — a std::optional<T> member is an optional
//     field; a non-optional member is populated-or-defaulted. A handler can no
//     longer be handed an "optional that the spec promised was present";
//   * two compile-time guards (below) keep the spec and the Input in lockstep.

#include <rpcspec/Concepts.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/FieldView.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/SpecDump.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>

#include <boost/pfr/core.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rpc::spec {

namespace detail {

template <typename T>
struct IsOptional : std::false_type {};
template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

}  // namespace detail

template <typename T>
inline constexpr bool kIS_OPTIONAL = detail::IsOptional<std::remove_cvref_t<T>>::value;

/**
 * @brief A converter validates a field and produces its strong-typed value.
 *
 * Exposes ValueType and parse(fieldView) -> std::expected<ValueType, Status>.
 * Witnessed against FieldViewArchetype so converters stay backend-agnostic.
 */
template <typename C>
concept SomeConverter = requires(C const c, detail::FieldViewArchetype const& f) {
    typename C::ValueType;
    { c.parse(f) } -> std::same_as<std::expected<typename C::ValueType, rpc::Status>>;
};

/**
 * @brief A spec field bound to an Input member, with a converter producing it.
 *
 * @tparam InputT The handler Input struct.
 * @tparam Member The bound member's type (its optional-ness defines field presence).
 * @tparam Conv   The typed converter producing the member value.
 * @tparam Items  Extra requirements/modifiers/checks (e.g. `required`, `clamp`, `deprecated`).
 *
 * On parse, Items run first in declaration order — requirements validate and
 * modifiers (clamp, toLower, …) mutate the JSON in place — and only then does the
 * converter validate the (possibly modified) value and transform it into the
 * strong member type. So a field can clamp a number and then convert it, exactly
 * as the legacy validate+modify pipeline allowed.
 */
template <typename InputT, typename Member, SomeConverter Conv, SomeFieldItem... Items>
struct BoundField {
    static constexpr bool kIsBound = true;

    std::string_view key;
    Member InputT::* member;
    Conv conv;
    std::tuple<Items...> items;

    consteval BoundField(std::string_view k, Member InputT::* m, Conv c, Items... it)
        : key{k}, member{m}, conv{c}, items{it...}
    {
    }

    // Guard 1: the converter's output must be assignable to the bound member.
    // A spec/Input type mismatch is therefore a compile error, not a runtime bug.
    static_assert(
        std::is_assignable_v<Member&, typename Conv::ValueType>,
        "rpcspec: converter output type is not assignable to the bound Input member"
    );

    /**
     * @brief Run this field's items against @p root and assign the converted value into @p out.
     *
     * Items execute in declaration order (requirements then modifiers), then the
     * converter transforms the (possibly modified) value into the member type and
     * assigns it through the pointer-to-member. An absent optional field is a no-op.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Mutable root object view (modifiers may write back into it).
     * @param out   The `InputT` instance being populated.
     * @return An error if any item or the converter fails; empty on success.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, InputT& out) const
    {
        auto fa = root.child(key);  // mutable view so modifiers can write in place

        // Requirements + modifiers in declaration order (e.g. `required`, `clamp`,
        // `toLower`): validate and mutate the field, stopping at the first error.
        MaybeError pre{};
        std::apply(
            [&](auto const&... it) {
                (void)((pre = callIfProcessor(it, fa), pre.has_value()) && ...);
            },
            items
        );
        if (!pre.has_value())
            return pre;

        // Absent + (optional member | has default) → leave the default value.
        if (!fa.present())
            return {};

        // Validate and transform the (possibly modified) value into the strong type.
        auto res = conv.parse(fa);
        if (!res.has_value())
            return std::unexpected{std::move(res).error()};
        out.*member = std::move(res).value();
        return {};
    }

    /**
     * @brief Collect warnings from check items for this field.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Const root object view.
     * @return All warnings emitted by check items for this field.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        auto const fa = root.child(key);
        Warnings out;
        std::apply([&](auto const&... it) { (callIfChecker(it, fa, out), ...); }, items);
        return out;
    }

    /**
     * @brief Render this field's schema entry into the spec dump writer.
     *
     * @param w  The `SpecDumpWriter` receiving the schema output.
     */
    void
    dump(SpecDumpWriter& w) const
    {
        w.bulletGroup(key, [&] {
            std::apply([&](auto const&... it) { (dumpItem(w, it), ...); }, items);
            dumpItem(w, conv);  // the converter renders via its kName
        });
    }
};

/**
 * @brief A bound field still being built via the pipe (|) syntax: it has a key,
 * a member, and zero or more items, but no converter yet.
 *
 * `| item` appends another modifier/check; `| converter` finishes the field,
 * producing a complete BoundField. Because the only way to complete a field is to
 * pipe a converter, the converter-last invariant holds by construction:
 *
 *   field("limit", &Input::limit) | clamp(10, 400) | asUint32
 */
template <typename InputT, typename Member, SomeFieldItem... Items>
struct PartialBoundField {
    std::string_view key;
    Member InputT::* member;
    std::tuple<Items...> items;

    consteval PartialBoundField(std::string_view k, Member InputT::* m, Items... it)
        : key{k}, member{m}, items{it...}
    {
    }

    /**
     * @brief Append a modifier or check item, returning a new `PartialBoundField`.
     *
     * @tparam Item A field-item type satisfying `SomeFieldItem`.
     * @param item  The item to append.
     * @return A new `PartialBoundField` with @p item appended; still awaiting a converter.
     */
    template <SomeFieldItem Item>
    [[nodiscard]] consteval auto
    operator|(Item item) const
    {
        return std::apply(
            [&](auto const&... existing) {
                return PartialBoundField<InputT, Member, Items..., Item>{key, member, existing..., item};
            },
            items
        );
    }

    /**
     * @brief Complete the field by piping a converter, returning a `BoundField`.
     *
     * @tparam Conv A converter type satisfying `SomeConverter`.
     * @param conv  The converter that validates and transforms the field value.
     * @return A fully constructed `BoundField` ready for use in a `TypedSpec`.
     */
    template <SomeConverter Conv>
    [[nodiscard]] consteval auto
    operator|(Conv conv) const
    {
        return std::apply(
            [&](auto const&... existing) {
                return BoundField<InputT, Member, Conv, Items...>{key, member, conv, existing...};
            },
            items
        );
    }
};

namespace detail {

template <typename F>
inline constexpr bool kIsBoundField = requires { F::kIsBound; };

// Splits the trailing pack of field() into [items..., converter] and builds the
// BoundField. ItemIs indexes the leading items; the last element is the converter.
template <typename InputT, typename Member, typename... Rest, std::size_t... ItemIs>
consteval auto
makeBoundField(std::string_view key, Member InputT::* member, std::tuple<Rest...> rest, std::index_sequence<ItemIs...>)
{
    using RestTuple = std::tuple<Rest...>;
    constexpr std::size_t kLast = sizeof...(Rest) - 1;
    static_assert(
        SomeConverter<std::tuple_element_t<kLast, RestTuple>>,
        "rpcspec: the final argument of a bound field must be a converter"
    );
    auto conv = std::get<kLast>(rest);
    return BoundField<InputT, Member, decltype(conv), std::tuple_element_t<ItemIs, RestTuple>...>{
        key, member, conv, std::get<ItemIs>(rest)...
    };
}

}  // namespace detail

/**
 * @brief Create a `BoundField` that binds a JSON key to an `InputT` member with inline items.
 *
 * Distinguished from the validate-only `field()` overload by the pointer-to-member argument.
 * Trailing arguments are items in execution order — modifiers/checks first (left-to-right),
 * followed by the final converter that produces the strongly-typed member value:
 *
 * @code
 * field("limit", &Input::limit, clamp(10, 400), asUint32)
 *        name     member        modifier         converter (last)
 * @endcode
 *
 * @tparam InputT  The handler Input struct.
 * @tparam Member  The type of the bound member.
 * @tparam Rest    Items in execution order; the last element must satisfy `SomeConverter`.
 * @param key      JSON field name.
 * @param member   Pointer-to-member that will receive the converted value.
 * @param rest     Items (modifiers/checks) followed by the converter.
 * @return A fully constructed `BoundField`.
 */
template <typename InputT, typename Member, typename... Rest>
consteval auto
field(std::string_view key, Member InputT::* member, Rest... rest)
{
    static_assert(sizeof...(Rest) >= 1, "rpcspec: a bound field needs a final converter");
    return detail::makeBoundField<InputT, Member>(
        key, member, std::tuple{rest...}, std::make_index_sequence<sizeof...(Rest) - 1>{}
    );
}

/**
 * @brief Create a `PartialBoundField` that binds a key to an `InputT` member for pipe-style composition.
 *
 * Attach modifiers/checks and a final converter with successive `operator|` calls:
 *
 * @code
 * field("limit", &Input::limit) | clamp(10, 400) | asUint32
 * @endcode
 *
 * @tparam InputT  The handler Input struct.
 * @tparam Member  The type of the bound member.
 * @param key      JSON field name.
 * @param member   Pointer-to-member that will receive the converted value.
 * @return A `PartialBoundField` awaiting a converter via `operator|`.
 */
template <typename InputT, typename Member>
consteval auto
field(std::string_view key, Member InputT::* member)
{
    return PartialBoundField<InputT, Member>{key, member};
}

/**
 * @brief Count the number of distinct keys among the bound fields.
 *
 * A later field that re-binds an existing key (an override from `extend()`,
 * e.g. a V2 spec retightening a V1 field) is counted once, so the count
 * reflects how many `InputT` members are covered, not the raw number of
 * bindings written.
 *
 * @tparam Fields The field types in the spec.
 * @param f       The fields to inspect.
 * @return The number of distinct bound keys.
 */
template <typename... Fields>
[[nodiscard]] consteval std::size_t
distinctBoundKeyCount(Fields const&... f)
{
    constexpr std::size_t kN = sizeof...(Fields);
    if constexpr (kN == 0) {
        return 0;
    } else {
        std::array<std::string_view, kN> const keys{f.key...};
        std::array<bool, kN> const bound{detail::kIsBoundField<Fields>...};
        std::size_t distinct = 0;
        for (std::size_t i = 0; i < kN; ++i) {
            if (!bound[i])
                continue;
            bool seen = false;
            for (std::size_t j = 0; j < i; ++j) {
                if (bound[j] && keys[j] == keys[i]) {
                    seen = true;
                    break;
                }
            }
            if (!seen)
                ++distinct;
        }
        return distinct;
    }
}

/**
 * @brief A spec that parses a request directly into a strong-typed Input struct.
 *
 * Fields are a mix of BoundField (carry a member + converter) and plain
 * (validate-only) FieldSpec entries such as deprecated markers. Fields sharing a
 * key follow last-wins override semantics (so a V2 spec built via extend() can
 * retighten a V1 field), exactly like RpcSpec.
 */
template <typename InputT, typename... Fields>
struct TypedSpec {
    std::tuple<Fields...> fields;

    // Guard 2: every member of the Input aggregate must be bound by exactly one
    // (distinct) field. Forgetting to bind a member would silently leave it
    // default-constructed; binding the same number of distinct members as the
    // aggregate has is therefore enforced at compile time via boost::pfr (no
    // macros/reflection). The check lives in the consteval constructor — which
    // every spec()/extend() result runs through — so a failure makes that
    // constexpr definition ill-formed. Counting distinct keys (not raw bindings)
    // is what lets extend() override a field without tripping the guard.
    consteval explicit TypedSpec(Fields... f) : fields{f...}
    {
        if (distinctBoundKeyCount(f...) != boost::pfr::tuple_size_v<InputT>)
            throw "rpcspec: every Input member must be bound by exactly one field "
                  "(an Input member is unbound, or the bound-member count disagrees with the Input)";
    }

    /**
     * @brief Validate @p root and deserialise it directly into a strong-typed `InputT`.
     *
     * Modifiers run first (writing normalised values back into @p root), then
     * converters transform each field into the corresponding `InputT` member.
     * The root must be mutable so modifiers can write in place.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Mutable root object view.
     * @return The populated `InputT` on success, or an error on the first failing field.
     */
    template <SomeObjectView Root>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(Root& root) const
    {
        return parseImpl(root, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief Collect all warnings emitted by check items across all fields.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Const root object view.
     * @return All warnings produced by check items.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        return checkImpl(root, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief Render the schema for this spec into @p w.
     *
     * Uses last-wins key deduplication, delegating bound fields to
     * `BoundField::dump` and validate-only fields to `dumpFieldSpec`.
     *
     * @param w  The `SpecDumpWriter` receiving the schema output.
     */
    void
    dump(SpecDumpWriter& w) const
    {
        dumpImpl(w, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief `parse()` overload accepting any value constructible into an `ObjectView`.
     *
     * @tparam V A mutable value type convertible to `ObjectView` (e.g. `boost::json::value`).
     * @param v  Mutable value to parse.
     * @return The populated `InputT` on success, or an error on the first failing field.
     */
    template <typename V>
        requires(!SomeObjectView<V>) && std::constructible_from<ObjectView, V&>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(V& v) const
    {
        ObjectView root{v};
        return parse(root);
    }

    /**
     * @brief `check()` overload accepting any value constructible into a const `ObjectView`.
     *
     * @tparam V A value type convertible to `ObjectView const`.
     * @param v  Const value to check.
     * @return All warnings produced by check items.
     */
    template <typename V>
        requires(!SomeObjectView<V>) && std::constructible_from<ObjectView, V const&>
    [[nodiscard]] Warnings
    check(V const& v) const
    {
        ObjectView const root{v};
        return check(root);
    }

private:
    template <SomeObjectView Root, std::size_t... Is>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parseImpl(Root& root, std::index_sequence<Is...>) const
    {
        InputT out{};
        if constexpr (sizeof...(Is) > 0) {
            constexpr auto kN = sizeof...(Is);
            std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
            auto const plan = impl::buildOverridePlan(keys);

            using Fn = MaybeError (*)(std::tuple<Fields...> const&, Root&, InputT&);
            static constexpr std::array<Fn, kN> kDispatch{
                +[](std::tuple<Fields...> const& t, Root& r, InputT& o) -> MaybeError {
                    return parseOne(std::get<Is>(t), r, o);
                }...
            };

            for (std::size_t i = 0; i < kN; ++i) {
                if (!plan.shouldRun[i])
                    continue;
                if (auto res = kDispatch[plan.effectiveIdx[i]](fields, root, out); !res.has_value())
                    return std::unexpected{std::move(res).error()};
            }
        }
        return out;
    }

    template <SomeObjectView Root, std::size_t... Is>
    [[nodiscard]] Warnings
    checkImpl(Root const& root, std::index_sequence<Is...>) const
    {
        Warnings out;
        if constexpr (sizeof...(Is) > 0) {
            constexpr auto kN = sizeof...(Is);
            std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
            auto const plan = impl::buildOverridePlan(keys);

            using Fn = Warnings (*)(std::tuple<Fields...> const&, Root const&);
            static constexpr std::array<Fn, kN> kDispatch{
                +[](std::tuple<Fields...> const& t, Root const& r) -> Warnings {
                    return std::get<Is>(t).check(r);
                }...
            };

            for (std::size_t i = 0; i < kN; ++i) {
                if (!plan.shouldRun[i])
                    continue;
                auto w = kDispatch[plan.effectiveIdx[i]](fields, root);
                out.insert(out.end(), w.begin(), w.end());
            }
        }
        return out;
    }

    template <std::size_t... Is>
    void
    dumpImpl(SpecDumpWriter& w, std::index_sequence<Is...>) const
    {
        if constexpr (sizeof...(Is) > 0) {
            constexpr auto kN = sizeof...(Is);
            std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
            auto const plan = impl::buildOverridePlan(keys);

            using Fn = void (*)(SpecDumpWriter&, std::tuple<Fields...> const&);
            static constexpr std::array<Fn, kN> kDispatch{
                +[](SpecDumpWriter& wr, std::tuple<Fields...> const& t) { dumpOne(wr, std::get<Is>(t)); }...
            };

            for (std::size_t i = 0; i < kN; ++i) {
                if (plan.shouldRun[i])
                    kDispatch[plan.effectiveIdx[i]](w, fields);
            }
        }
    }

    template <typename F>
    static void
    dumpOne(SpecDumpWriter& w, F const& f)
    {
        if constexpr (detail::kIsBoundField<F>) {
            f.dump(w);
        } else {
            dumpFieldSpec(w, f);  // validate-only field (shared FieldSpec dumper)
        }
    }

    template <typename F, SomeObjectView Root>
    [[nodiscard]] static MaybeError
    parseOne(F const& f, Root& root, InputT& out)
    {
        if constexpr (detail::kIsBoundField<F>) {
            return f.parseInto(root, out);
        } else {
            // Validate-only field with no Input member (e.g. a deprecated marker or a
            // field like account_tx's `ctid` that is validated but not stored). Run its
            // validators/modifiers; it populates no member. Warnings come via check().
            return f.process(root);
        }
    }
};

/**
 * @brief Build a TypedSpec for the given Input struct.
 */
template <typename InputT, typename... Fields>
consteval auto
spec(Fields... fields)
{
    return TypedSpec<InputT, Fields...>{fields...};
}

/**
 * @brief Derive a spec from an existing one by appending fields.
 *
 * Mirrors RpcSpec's extend(): the same Input is shared, base fields are kept, and
 * appended fields with an existing key override the base (last-wins) — so a V2
 * spec extends V1, mapping any extra members and retightening shared ones.
 */
template <typename InputT, typename... Existing, typename... Extra>
[[nodiscard]] consteval auto
extend(TypedSpec<InputT, Existing...> const& base, Extra... extra)
{
    return std::apply(
        [&](auto const&... existing) { return TypedSpec<InputT, Existing..., Extra...>{existing..., extra...}; },
        base.fields
    );
}

}  // namespace rpc::spec
