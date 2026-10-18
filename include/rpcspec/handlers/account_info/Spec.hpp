/** @file */
#pragma once

#include <rpcspec/Account.hpp>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_info/Types.hpp>

namespace rpc::spec::handlers::account_info {

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "ident",
        &Input::account,
        ifServerXrpld(
            ifAbsent("account"),
            withCustomError(
                required,
                rpc::XrpldError::RpcInvalidParams,
                "Missing field 'account'.")),
        ifServerClio(deprecated),
        deferredAccountId),
    field("account", &Input::account, deferredAccountId),
    field("signer_lists", &Input::signerLists, jsonBool),
    field("queue", &Input::queue, jsonBool),
    field("ledger", ifServerClio(deprecated)),
    field("strict", ifServerClio(deprecated)));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 *
 * Clio requires a bool `signer_lists`. xrpld checks that in its handler, after the account is
 * found, so its v2 spec is v1's.
 */
inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("signer_lists", &Input::signerLists, JsonBoolConverterT<kIsClioBuild>{}));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/**
 * @brief ADL hook: resolve the versioned spec from the Input type.
 *
 * @return A reference to this handler's `kSpec`, for `HandlerFor` to select a version from.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_info
