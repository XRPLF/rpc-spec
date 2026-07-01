/** @file */
#pragma once
// A per-handler spec that owns API-version selection.
//
// Handlers declare ONE `kSpec = versioned<Input>(v1Spec, v2Spec, ...)` plus an
// ADL hook `specFor(Input const*)` returning it. Consumers (Clio, rippled) then
// parse / check / dump a request by API version WITHOUT knowing which underlying
// spec backs which version — the selection lives here, not at the call site.

#include <boost/json/value.hpp>

#include <rpcspec/Errors.hpp>
#include <rpcspec/RpcSpecView.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

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
 * @tparam N      The number of supported versions.
 */
template <typename InputT, std::size_t N>
struct VersionedSpec
{
    static_assert(N >= 1, "rpcspec: versioned() needs at least one spec");

    using ParseFn = std::expected<InputT, rpc::Status> (*)(void const*, boost::json::value&);

    /** @brief A type-erased view per version (for check/dump). */
    std::array<RpcSpecView, N> views;
    /** @brief The parse thunk per version (RpcSpecView carries no parse). */
    std::array<ParseFn, N> parseFns;
    /** @brief The underlying spec object per version, passed back to @c parseFns. */
    std::array<void const*, N> selves;

    /**
     * @brief Map an API version to a version index.
     *
     * A concrete version in [1, N] selects that spec (version k → index k-1).
     * Anything else — an unspecified/invalid 0, or a version newer than the last
     * entry — resolves to the newest spec (index N-1). This mirrors the legacy
     * per-handler selection `apiVersion == 1 ? V1 : V2`, where every value other
     * than 1 (including the default-constructed 0) used the latest spec.
     */
    [[nodiscard]] static constexpr std::size_t
    indexFor(uint32_t apiVersion) noexcept
    {
        if (apiVersion >= 1 && apiVersion <= N)
            return static_cast<std::size_t>(apiVersion) - 1;
        return N - 1;
    }

    /**
     * @brief Validate @p jv and deserialise it into @c InputT using the spec for @p apiVersion.
     *
     * @param jv         The request JSON (taken by value: spec modifiers normalise it in place).
     * @param apiVersion The API version to select the spec for.
     * @return The parsed Input, or a Status describing the validation failure.
     */
    [[nodiscard]] std::expected<InputT, rpc::Status>
    parse(boost::json::value jv, uint32_t apiVersion) const
    {
        auto const i = indexFor(apiVersion);
        return parseFns[i](selves[i], jv);
    }

    /**
     * @brief Collect warnings for @p jv using the spec for @p apiVersion.
     */
    [[nodiscard]] Warnings
    check(boost::json::value const& jv, uint32_t apiVersion) const
    {
        return views[indexFor(apiVersion)].check(jv);
    }

    /**
     * @brief Render the schema for @p apiVersion into @p w.
     */
    void
    dump(SpecDumpWriter& w, uint32_t apiVersion) const
    {
        views[indexFor(apiVersion)].dump(w);
    }

    /**
     * @brief The type-erased view for @p apiVersion (for callers that only need check/dump).
     */
    [[nodiscard]] RpcSpecView
    view(uint32_t apiVersion) const
    {
        return views[indexFor(apiVersion)];
    }
};

/**
 * @brief Build a @ref VersionedSpec from one @ref TypedSpec per API version.
 *
 * Each argument must be a spec producing the same @c InputT. Argument order is
 * the version order (arg 0 → version 1, …); see @ref VersionedSpec.
 *
 * @tparam InputT The shared handler Input struct.
 * @param specs   One spec per version, in ascending version order.
 * @return A @c VersionedSpec over the given specs.
 */
template <typename InputT, typename... Specs>
[[nodiscard]] consteval auto
versioned(Specs const&... specs)
{
    return VersionedSpec<InputT, sizeof...(Specs)>{
        .views = {RpcSpecView{specs}...},
        .parseFns = {(
            +[](void const* s, boost::json::value& jv) -> std::expected<InputT, rpc::Status> {
                return static_cast<Specs const*>(s)->parse(jv);
            })...},
        .selves = {static_cast<void const*>(&specs)...}};
}

}  // namespace rpc::spec
