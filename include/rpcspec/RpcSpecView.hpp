/** @file */
#pragma once

#include <rpcspec/FieldView.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/SpecDump.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>

#include <concepts>

namespace rpc::spec {

/**
 * @brief Non-owning, type-erased view over any @ref RpcSpec instantiation.
 *
 * Stores a raw pointer to the underlying spec together with stateless
 * function pointers; no heap allocation takes place and the constructor is
 * genuinely @c noexcept.
 *
 * **Lifetime contract**: this class does *not* own the spec it refers to.
 * The caller is responsible for ensuring that the @ref RpcSpec object
 * outlives every @c RpcSpecView constructed from it.  The recommended
 * pattern is to declare the spec as @c static @c constexpr (or any other
 * object with static storage duration) and construct an @c RpcSpecView on
 * demand:
 *
 * @code
 * static constexpr auto kMySpec = RpcSpec{ ... };
 * RpcSpecView view{kMySpec};  // safe: kMySpec lives forever
 * @endcode
 *
 * Enables uniform return type for versioned specs.
 */
class RpcSpecView
{
    void const* self_;
    MaybeError (*processImpl_)(void const*, ObjectView&);
    Warnings (*checkImpl_)(void const*, ObjectView const&);
    void (*dumpImpl_)(void const*, SpecDumpWriter&);

public:
    /**
     * @brief Construct a view over an `RpcSpec`.
     *
     * @tparam Fields The field types of the spec.
     * @param spec    The `RpcSpec` to wrap; must outlive this view.
     */
    template <typename... Fields>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr RpcSpecView(RpcSpec<Fields...> const& spec) noexcept
        : self_{&spec}
        , processImpl_{[](void const* s, ObjectView& r) {
            return static_cast<RpcSpec<Fields...> const*>(s)->process(r);
        }}
        , checkImpl_{[](void const* s, ObjectView const& r) {
            return static_cast<RpcSpec<Fields...> const*>(s)->check(r);
        }}
        , dumpImpl_{[](void const* s, SpecDumpWriter& w) {
            dumpRpcSpec(w, *static_cast<RpcSpec<Fields...> const*>(s));
        }}
    {
    }

    /**
     * @brief Construct a view over a `TypedSpec`.
     *
     * `process()` on the resulting view is a no-op: a `TypedSpec` validates
     * entirely inside its own `parse()` (invoked by the handler's `parseInput`).
     * Only `check()` and `dump()` delegate to the underlying spec.
     *
     * @tparam InputT  The handler Input struct of the spec.
     * @tparam Fields  The field types of the spec.
     * @param spec     The `TypedSpec` to wrap; must outlive this view.
     */
    template <typename InputT, typename... Fields>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr RpcSpecView(TypedSpec<InputT, Fields...> const& spec) noexcept
        : self_{&spec}
        , processImpl_{[](void const*, ObjectView&) -> MaybeError { return {}; }}
        , checkImpl_{[](void const* s, ObjectView const& r) {
            return static_cast<TypedSpec<InputT, Fields...> const*>(s)->check(r);
        }}
        , dumpImpl_{[](void const* s, SpecDumpWriter& w) {
            static_cast<TypedSpec<InputT, Fields...> const*>(s)->dump(w);
        }}
    {
    }

    /**
     * @brief Validate @p root by delegating to the underlying spec's `process()`.
     *
     * @param root  Mutable root object view.
     * @return An error on the first failing field; empty on success (or always
     *         empty when wrapping a `TypedSpec`).
     */
    [[nodiscard]] MaybeError
    process(ObjectView& root) const
    {
        return processImpl_(self_, root);
    }

    /**
     * @brief Collect warnings by delegating to the underlying spec's `check()`.
     *
     * @param root  Const root object view.
     * @return All warnings produced by check items.
     */
    [[nodiscard]] Warnings
    check(ObjectView const& root) const
    {
        return checkImpl_(self_, root);
    }

    /**
     * @brief Render the spec schema into @p w.
     *
     * @param w  The `SpecDumpWriter` receiving the schema output.
     */
    void
    dump(SpecDumpWriter& w) const
    {
        dumpImpl_(self_, w);
    }

    /**
     * @brief `process()` overload accepting any value constructible into an `ObjectView`.
     *
     * @tparam V A mutable value type convertible to `ObjectView`.
     * @param v  Mutable value to validate.
     * @return An error on the first failing field; empty on success.
     */
    template <typename V>
        requires(!std::same_as<V, ObjectView>) && std::constructible_from<ObjectView, V&>
    [[nodiscard]] MaybeError
    process(V& v) const
    {
        ObjectView root{v};
        return processImpl_(self_, root);
    }

    /**
     * @brief `check()` overload accepting any value constructible into a const `ObjectView`.
     *
     * @tparam V A value type convertible to `ObjectView const`.
     * @param v  Const value to check.
     * @return All warnings produced by check items.
     */
    template <typename V>
        requires(!std::same_as<V, ObjectView>) && std::constructible_from<ObjectView, V const&>
    [[nodiscard]] Warnings
    check(V const& v) const
    {
        ObjectView const root{v};
        return checkImpl_(self_, root);
    }
};

/** @brief Backward-compatible alias for `RpcSpecView`. */
using RpcSpecConstRef = RpcSpecView;

}  // namespace rpc::spec
