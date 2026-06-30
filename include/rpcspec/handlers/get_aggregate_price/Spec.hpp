/** @file */
#pragma once
// Shared constexpr spec for the 'get_aggregate_price' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

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

struct OraclesConverter {
    static constexpr std::string_view kName = "oracles";
    using ValueType = std::vector<Oracle>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        ValueType result;
        result.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i) {
            auto const elem = f.element(i);
            auto const docId = elem.child("oracle_document_id");
            auto const account = elem.child("account");
            // Both are guaranteed valid by kORACLES_VALIDATOR; extract directly.
            auto id = detail::accountFromStringStrict(std::string{account.asString()});
            if (!id)
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            result.push_back(Oracle{
                .documentId = docId.asUint32(),
                .account = *id,
            });
        }
        return result;
    }
};

struct Uint8Converter {
    static constexpr std::string_view kName = "uint8";
    using ValueType = uint8_t;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isUint32())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return static_cast<uint8_t>(f.asUint32());
    }
};

struct CurrencyStringConverter {
    static constexpr std::string_view kName = "currencyString";
    using ValueType = std::string;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return std::string{f.asString()};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto oraclesConv = OraclesConverter{};
inline constexpr auto uint8Conv = Uint8Converter{};
inline constexpr auto currencyString = CurrencyStringConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt),
    field(
        "base_asset",
        &Input::baseAsset,
        required,
        withCustomError(currency, RippledError::RpcInvalidParams),
        currencyString
    ),
    field(
        "quote_asset",
        &Input::quoteAsset,
        required,
        withCustomError(currency, RippledError::RpcInvalidParams),
        currencyString
    ),
    field("oracles", &Input::oracles, required, kORACLES_VALIDATOR, oraclesConv),
    field("time_threshold", &Input::timeThreshold, type<uint32_t>, asUint32),
    field("trim", &Input::trim, type<uint32_t>, between(uint32_t{1}, uint32_t{25}), uint8Conv)
);

} // namespace rpc::spec::handlers::get_aggregate_price
