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

    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        auto const fa = root.child(key);
        Warnings out;
        std::apply([&](auto const&... it) { (callIfChecker(it, fa, out), ...); }, items);
        return out;
    }

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

    // Pipe a modifier/check: append it to the (still converter-less) field.
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

    // Pipe a converter: complete the field into a BoundField.
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

// field() overload that binds a key to an Input member. Distinguished from the
// validate-only field() by the pointer-to-member argument. The trailing arguments
// are the field's items in source = execution order: modifiers/checks first
// (left-to-right), then the final converter that produces the strong member value.
//
//   field("limit", &Input::limit, clamp(10, 400), asUint32)
//          name     member        modifier         converter (last)
template <typename InputT, typename Member, typename... Rest>
consteval auto
field(std::string_view key, Member InputT::* member, Rest... rest)
{
    static_assert(sizeof...(Rest) >= 1, "rpcspec: a bound field needs a final converter");
    return detail::makeBoundField<InputT, Member>(
        key, member, std::tuple{rest...}, std::make_index_sequence<sizeof...(Rest) - 1>{}
    );
}

// Pipe-style entry point: bind a key to an Input member, then add items/converter
// with operator|. Returns a PartialBoundField that completes once a converter is piped.
//
//   field("limit", &Input::limit) | clamp(10, 400) | asUint32
template <typename InputT, typename Member>
consteval auto
field(std::string_view key, Member InputT::* member)
{
    return PartialBoundField<InputT, Member>{key, member};
}

// Counts the number of DISTINCT keys among the bound fields. A later field that
// re-binds an existing key (an override from extend(), e.g. V2 retightening a V1
// field) is counted once, so the count reflects how many Input members are
// covered — not how many bindings were written.
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

    // parse() takes a MUTABLE root: modifiers (clamp, toLower, …) write the
    // normalised value back into the JSON before conversion reads it.
    template <SomeObjectView Root>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(Root& root) const
    {
        return parseImpl(root, std::index_sequence_for<Fields...>{});
    }

    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        return checkImpl(root, std::index_sequence_for<Fields...>{});
    }

    // Render the schema (used by the spec dumper). Mirrors RpcSpec's dump, with
    // last-wins key dedup, dispatching bound fields to BoundField::dump and
    // validate-only fields to the shared dumpFieldSpec.
    void
    dump(SpecDumpWriter& w) const
    {
        dumpImpl(w, std::index_sequence_for<Fields...>{});
    }

    // boost::json::value (or any value constructible into an ObjectView) overloads.
    // parse() needs a mutable value (modifiers); check() does not.
    template <typename V>
        requires(!SomeObjectView<V>) && std::constructible_from<ObjectView, V&>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(V& v) const
    {
        ObjectView root{v};
        return parse(root);
    }

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
