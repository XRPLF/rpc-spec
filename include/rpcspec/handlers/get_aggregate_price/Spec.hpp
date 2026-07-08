/** @file */
#pragma once
// Shared constexpr spec for the 'get_aggregate_price' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

static constexpr auto kORACLES_MAX = 200;

static constexpr auto kORACLES_VALIDATOR = CustomModifier{[](auto& f) -> MaybeError {
    if (!f.isArray() || f.arraySize() == 0 || f.arraySize() > kORACLES_MAX)
        return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

    for (std::size_t i = 0; i < f.arraySize(); ++i)
    {
        auto elem = f.element(i);
        if (!elem.isObject())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        auto docIdFa = elem.child("oracle_document_id");
        auto accountFa = elem.child("account");

        if (!docIdFa.present() || !accountFa.present())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        if (auto err = Type<uint32_t, std::string>::verify(docIdFa); !err)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcOracleMalformed}};

        // convert string oracle_document_id to integer in-place;
        // propagate the error directly (mirrors the old behaviour: returns RpcInvalidParams
        // when the string is not a valid integer, e.g. "a")
        if (auto err = ToNumberModifier::modify(docIdFa); !err)
            return err;

        if (auto err = AccountBase58Validator::verify(accountFa); !err)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }

    return {};
}};

struct OraclesConverter
{
    static constexpr std::string_view kName = "oracles";
    using ValueType = std::vector<Oracle>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        ValueType result;
        result.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const elem = f.element(i);
            auto const docId = elem.child("oracle_document_id");
            auto const account = elem.child("account");
            // Both are guaranteed valid by kORACLES_VALIDATOR; extract directly.
            auto id = detail::accountFromStringStrict(std::string{account.asString()});
            if (!id)
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            result.push_back(
                Oracle{
                    .documentId = docId.asUint32(),
                    .account = *id,
                });
        }
        return result;
    }
};

struct Uint8Converter
{
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

struct CurrencyConverter
{
    static constexpr std::string_view kName = "currency";
    using ValueType = xrpl::Currency;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        // The `currency` validator already confirmed the field is a valid currency
        // code string; decode it into the strong type.
        xrpl::Currency currency;
        if (!f.isString() || !xrpl::toCurrency(currency, std::string{f.asString()}))
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return currency;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto oraclesConv = OraclesConverter{};
inline constexpr auto uint8Conv = Uint8Converter{};
inline constexpr auto currencyConv = CurrencyConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "base_asset",
        &Input::baseAsset,
        required,
        withCustomError(currency, RippledError::RpcInvalidParams),
        currencyConv),
    field(
        "quote_asset",
        &Input::quoteAsset,
        required,
        withCustomError(currency, RippledError::RpcInvalidParams),
        currencyConv),
    field("oracles", &Input::oracles, required, kORACLES_VALIDATOR, oraclesConv),
    field("time_threshold", &Input::timeThreshold, type<uint32_t>, asUint32),
    field("trim", &Input::trim, type<uint32_t>, between(uint32_t{1}, uint32_t{25}), uint8Conv));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::get_aggregate_price
