/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
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
 *
 * @tparam View The backend's object-view type. The spec library names no JSON type, so
 *         the type erased over the *spec* is still explicit about the *backend*.
 */
template <SomeObjectView View>
class RpcSpecView
{
    void const* self_;
    MaybeError (*processImpl_)(void const*, View&);
    Warnings (*checkImpl_)(void const*, View const&);
    void (*dumpImpl_)(void const*, SpecDumpWriter&);

public:
    /**
     * @brief Construct a view over an `RpcSpec`.
     *
     * @tparam Fields The field types of the spec.
     * @param spec The `RpcSpec` to wrap; must outlive this view.
     */
    template <typename... Fields>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr RpcSpecView(RpcSpec<Fields...> const& spec) noexcept
        : self_{&spec}
        , processImpl_{[](void const* self, View& root) {
            return static_cast<RpcSpec<Fields...> const*>(self)->process(root);
        }}
        , checkImpl_{[](void const* self, View const& root) {
            return static_cast<RpcSpec<Fields...> const*>(self)->check(root);
        }}
        , dumpImpl_{[](void const* self, SpecDumpWriter& writer) {
            dumpRpcSpec(writer, *static_cast<RpcSpec<Fields...> const*>(self));
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
     * @tparam InputT The handler Input struct of the spec.
     * @tparam Fields The field types of the spec.
     * @param spec The `TypedSpec` to wrap; must outlive this view.
     */
    template <typename InputT, typename... Fields>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr RpcSpecView(TypedSpec<InputT, Fields...> const& spec) noexcept
        : self_{&spec}
        , processImpl_{[](void const*, View&) -> MaybeError { return {}; }}
        , checkImpl_{[](void const* self, View const& root) {
            return static_cast<TypedSpec<InputT, Fields...> const*>(self)->check(root);
        }}
        , dumpImpl_{[](void const* self, SpecDumpWriter& writer) {
            static_cast<TypedSpec<InputT, Fields...> const*>(self)->dump(writer);
        }}
    {
    }

    /**
     * @brief Validate @p root by delegating to the underlying spec's `process()`.
     *
     * @param root Mutable root object view.
     * @return An error on the first failing field; empty on success (or always
     *         empty when wrapping a `TypedSpec`).
     */
    [[nodiscard]] MaybeError
    process(View& root) const
    {
        return processImpl_(self_, root);
    }

    /**
     * @brief Collect warnings by delegating to the underlying spec's `check()`.
     *
     * @param root Const root object view.
     * @return All warnings produced by check items.
     */
    [[nodiscard]] Warnings
    check(View const& root) const
    {
        return checkImpl_(self_, root);
    }

    /**
     * @brief Render the spec schema into @p w.
     *
     * @param writer The `SpecDumpWriter` receiving the schema output.
     */
    void
    dump(SpecDumpWriter& writer) const
    {
        dumpImpl_(self_, writer);
    }

    /**
     * @brief `process()` overload accepting a raw document from a backend.
     *
     * @tparam V A mutable value type a backend has bound a view to via `ObjectViewFor`.
     * @param value Mutable value to validate.
     * @return An error on the first failing field; empty on success.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] MaybeError
    process(V& value) const
    {
        View root{value};
        return processImpl_(self_, root);
    }

    /**
     * @brief `check()` overload accepting a raw document from a backend.
     *
     * @tparam V A value type a backend has bound a view to via `ObjectViewFor`.
     * @param value Const value to check.
     * @return All warnings produced by check items.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] Warnings
    check(V const& value) const
    {
        View const root{value};
        return checkImpl_(self_, root);
    }
};

}  // namespace rpc::spec
