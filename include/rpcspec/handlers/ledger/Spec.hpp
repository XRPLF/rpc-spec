/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/ledger/Types.hpp>

namespace rpc::spec::handlers::ledger {

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger, withLegacyLedgerField),
    field("transactions", &Input::transactions, type<bool>, jsonBool),
    field("expand", &Input::expand, type<bool>, jsonBool),
    field("binary", &Input::binary, type<bool>, jsonBool),
    field("owner_funds", &Input::ownerFunds, type<bool>, jsonBool),
    field("diff", &Input::diff, type<bool>, jsonBool),

    field("queue", &Input::queue)  //
        | type<bool>               //
        | ifServerClio(notSupportedIf(true)) | jsonBool,
    field("full", &Input::full)  //
        | type<bool>             //
        | ifServerClio(notSupportedIf(true), deprecated) | jsonBool,
    field("accounts", &Input::accounts)  //
        | type<bool>                     //
        | ifServerClio(notSupportedIf(true), deprecated) | jsonBool,

    field("type", deprecated));

inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("transactions", &Input::transactions, jsonBoolStrict),
    field("expand", &Input::expand, jsonBoolStrict),
    field("binary", &Input::binary, jsonBoolStrict),
    field("owner_funds", &Input::ownerFunds, jsonBoolStrict),
    field("diff", &Input::diff, jsonBoolStrict));

/** @brief Version-selecting spec for 'ledger' (V1, V2+). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::ledger
