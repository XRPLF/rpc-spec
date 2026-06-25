/** @file */
#pragma once
// Shared constexpr spec for the 'get_aggregate_price' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <cstddef>
#include <cstdint>

namespace rpc::spec::handlers::get_aggregate_price {

static constexpr auto kORACLES_MAX = 200;

// Validates and normalises the "oracles" array field.
// Each element must be an object containing both "account" (base58) and
// "oracle_document_id" (uint32 or string).  String document IDs are
// converted to integers in-place via ToNumber.
static constexpr auto kORACLES_VALIDATOR = CustomModifier{[](auto& f) -> MaybeError {
    if (!f.isArray() || f.arraySize() == 0 || f.arraySize() > kORACLES_MAX)
        return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

    for (std::size_t i = 0; i < f.arraySize(); ++i) {
        auto elem = f.element(i);
        if (!elem.isObject())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        auto docIdFa = elem.child("oracle_document_id");
        auto accountFa = elem.child("account");

        if (!docIdFa.present() || !accountFa.present())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        // oracle_document_id must be uint32 or convertible string
        if (auto err = Type<uint32_t, std::string>::verify(docIdFa); !err)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        // convert string oracle_document_id to integer in-place;
        // propagate the error directly (mirrors the old behaviour: returns RpcInvalidParams
        // when the string is not a valid integer, e.g. "a")
        if (auto err = ToNumberModifier::modify(docIdFa); !err)
            return err;

        // account must be a valid base58 account ID
        if (auto err = AccountBase58Validator::verify(accountFa); !err)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }

    return {};
}};

inline constexpr auto kSpec = RpcSpec{
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    // validate quoteAsset and base_asset in accordance to the currency code found in XRPL
    // doc:
    // https://xrpl.org/docs/references/protocol/data-types/currency-formats#currency-codes
    // usually Clio returns RpcMalformedCurrency , return InvalidParam here just to mimic
    // rippled
    field("base_asset", required, withCustomError(currency, RippledError::RpcInvalidParams)),
    field(
        "quote_asset", required, withCustomError(currency, RippledError::RpcInvalidParams)
    ),
    field("oracles", required, kORACLES_VALIDATOR),
    // note: Unlike `rippled`, Clio only supports UInt as input, no string, no `null`, etc.
    field("time_threshold", type<uint32_t>),
    field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
};

} // namespace rpc::spec::handlers::get_aggregate_price
