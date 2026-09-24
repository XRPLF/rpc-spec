/** @file */
#pragma once
// A per-handler spec that owns API-version selection.
//
// Handlers declare ONE `kSpec = versioned<Input>(v1Spec, v2Spec, ...)` plus an
// ADL hook `specFor(Input const*)` returning it. Consumers (Clio, xrpld) then
// parse / check / dump a request by API version WITHOUT knowing which underlying
// spec backs which version — the selection lives here, not at the call site.
//
// The specs are held by pointer rather than type-erased behind a view, so this type
// does not depend on any backend: a handler's `kSpec` is the same type in every
// consumer, and the backend only appears where a request is actually read.

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/RpcSpecView.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <tuple>
#include <utility>

namespace rpc::spec {

/**
 * @brief A spec bundling one @ref TypedSpec per supported API version, selecting
 * the right one at runtime from an API version.
 *
 * Version mapping is positional: argument @c i to @ref versioned() backs API
 * version @c i+1, and any API version beyond the last argument uses the last
 * (the newest spec). A single-argument @c versioned() therefore serves every
 * version. Selection is the only thing this type adds; @ref parse / @ref check /
 * @ref dump delegate to the selected version.
 *
 * @tparam InputT The handler Input struct produced by @ref parse.
 * @tparam Specs One spec type per supported version, in ascending version order.
 */
template <typename InputT, typename... Specs>
struct VersionedSpec
{
    static_assert(sizeof...(Specs) >= 1, "rpcspec: versioned() needs at least one spec");

    /**
     * @brief The number of supported versions.
     */
    static constexpr std::size_t kCount = sizeof...(Specs);

    /**
     * @brief The underlying spec per version, in ascending version order.
     */
    std::tuple<Specs const*...> specs;

    /**
     * @brief Map an API version to a version index.
     *
     * A concrete version in [1, N] selects that spec (version k → index k-1).
     * Anything else — an unspecified/invalid 0, or a version newer than the last
     * entry — resolves to the newest spec (index N-1). This mirrors the legacy
     * per-handler selection `apiVersion == 1 ? V1 : V2`, where every value other
     * than 1 (including the default-constructed 0) used the latest spec.
     *
     * @param apiVersion The requested API version.
     * @return The index into @ref specs for that version.
     */
    [[nodiscard]] static constexpr std::size_t
    indexFor(uint32_t apiVersion) noexcept
    {
        if (apiVersion >= 1 and apiVersion <= kCount)
            return static_cast<std::size_t>(apiVersion) - 1;
        return kCount - 1;
    }

    /**
     * @brief Validate @p root and deserialise it into @c InputT using the spec for @p apiVersion.
     *
     * @tparam View The backend's object-view type.
     * @param root Mutable root object view (spec modifiers normalise it in place).
     * @param apiVersion The API version to select the spec for.
     * @return The parsed Input, or a Status describing the validation failure.
     */
    template <SomeObjectView View>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(View& root, uint32_t apiVersion) const
    {
        return selected(apiVersion, [&](auto const& spec) { return spec.parse(root); });
    }

    /**
     * @brief @ref parse overload accepting a raw document from a backend.
     *
     * @tparam V A mutable value type a backend has bound a view to via `ObjectViewFor`.
     * @param value The request document (by value: modifiers normalise it in place).
     * @param apiVersion The API version to select the spec for.
     * @return The parsed Input, or a Status describing the validation failure.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(V value, uint32_t apiVersion) const
    {
        ObjectViewForT<V> root{value};
        return parse(root, apiVersion);
    }

    /**
     * @brief Collect warnings for @p root using the spec for @p apiVersion.
     *
     * @tparam View The backend's object-view type.
     * @param root The request root.
     * @param apiVersion The API version to select the spec for.
     * @return All warnings produced by the selected spec's check items.
     */
    template <SomeObjectView View>
    [[nodiscard]] Warnings
    check(View const& root, uint32_t apiVersion) const
    {
        return selected(apiVersion, [&](auto const& spec) { return spec.check(root); });
    }

    /**
     * @brief @ref check overload accepting a raw document from a backend.
     *
     * @tparam V A value type a backend has bound a view to via `ObjectViewFor`.
     * @param value The request document.
     * @param apiVersion The API version to select the spec for.
     * @return All warnings produced by the selected spec's check items.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] Warnings
    check(V const& value, uint32_t apiVersion) const
    {
        ObjectViewForT<V> const root{value};
        return check(root, apiVersion);
    }

    /**
     * @brief Render the schema for @p apiVersion into @p writer.
     *
     * @param writer The writer receiving the schema output.
     * @param apiVersion The API version to select the spec for.
     */
    void
    dump(SpecDumpWriter& writer, uint32_t apiVersion) const
    {
        selected(apiVersion, [&](auto const& spec) { spec.dump(writer); });
    }

    /**
     * @brief A type-erased view of the spec for @p apiVersion, for callers needing a
     * uniform type across versions.
     *
     * @tparam View The backend's object-view type.
     * @param apiVersion The API version to select the spec for.
     * @return A view over the selected version's spec.
     */
    template <SomeObjectView View>
    [[nodiscard]] RpcSpecView<View>
    view(uint32_t apiVersion) const
    {
        return selected(apiVersion, [](auto const& spec) { return RpcSpecView<View>{spec}; });
    }

private:
    /**
     * @brief Invoke @p fn with the spec backing @p apiVersion.
     *
     * The index is only known at run time but `specs` is a tuple, so the walk turns it
     * back into a compile-time one. Returning directly from each arm — rather than
     * assigning into a result — keeps this usable for types that are not default
     * constructible. Every spec produces the same `InputT`, so all arms agree.
     *
     * @tparam I The version index being tested.
     * @tparam Fn The callable to apply.
     * @param apiVersion The API version to select the spec for.
     * @param fn The callable.
     * @return Whatever @p fn returns for the selected spec.
     */
    template <std::size_t I = 0, typename Fn>
    [[nodiscard]] decltype(auto)
    selected(uint32_t apiVersion, Fn&& fn) const
    {
        if constexpr (I + 1 < kCount)
        {
            if (I == indexFor(apiVersion))
                return fn(*std::get<I>(specs));
            return selected<I + 1>(apiVersion, std::forward<Fn>(fn));
        }
        else
        {
            return fn(*std::get<I>(specs));
        }
    }
};

/**
 * @brief Build a @ref rpc::spec::VersionedSpec from one @ref rpc::spec::TypedSpec per API version.
 *
 * Each argument must be a spec producing the same @c InputT. Argument order is
 * the version order (arg 0 → version 1, …); see @ref rpc::spec::VersionedSpec.
 *
 * @tparam InputT The shared handler Input struct.
 * @tparam Specs One spec type per supported version.
 * @param specs One spec per version, in ascending version order.
 * @return A @c VersionedSpec over the given specs.
 */
template <typename InputT, typename... Specs>
[[nodiscard]] consteval auto
versioned(Specs const&... specs)
{
    return VersionedSpec<InputT, Specs...>{.specs = {(&specs)...}};
}

}  // namespace rpc::spec
