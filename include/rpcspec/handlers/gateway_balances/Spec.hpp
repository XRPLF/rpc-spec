/** @file */
#pragma once
// Two versioned specs are exposed:
//   kInputSpecV1 — uses RpcInvalidHotwallet for type mismatches on 'hotwallet'
//   kInputSpecV2 — uses RpcInvalidParams for type mismatches on 'hotwallet'

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/tokens.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>

#include <cstddef>
#include <expected>
#include <set>
#include <string>

namespace rpc::spec::handlers::gateway_balances {

/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kHotWalletV1 = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isString() and not fieldView.isArray())
    {
        return std::unexpected{rpc::Status{
            rpc::XrpldError::RpcInvalidHotwallet,
            std::string{fieldView.key()} + "NotStringOrArray"}};
    }
    auto const getAccountID = [](auto const& elem) -> bool {
        if (not elem.isString())
            return false;
        auto const str = std::string{elem.asString()};
        auto const pk =
            detail::parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, str);
        if (pk.has_value())
            return true;
        return detail::parseBase58Wrapper<xrpl::AccountID>(str).has_value();
    };
    if (fieldView.isArray())
    {
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            if (not getAccountID(fieldView.element(i)))
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcInvalidHotwallet,
                    std::string{fieldView.key()} + "Malformed"}};
            }
        }
    }
    else
    {
        if (not getAccountID(fieldView))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidHotwallet, std::string{fieldView.key()} + "Malformed"}};
        }
    }
    return {};
}};

/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kHotWalletV2 = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isString() and not fieldView.isArray())
    {
        return std::unexpected{rpc::Status{
            rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "NotStringOrArray"}};
    }
    auto const getAccountID = [](auto const& elem) -> bool {
        if (not elem.isString())
            return false;
        auto const str = std::string{elem.asString()};
        auto const pk =
            detail::parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, str);
        if (pk.has_value())
            return true;
        return detail::parseBase58Wrapper<xrpl::AccountID>(str).has_value();
    };
    if (fieldView.isArray())
    {
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            if (not getAccountID(fieldView.element(i)))
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "Malformed"}};
            }
        }
    }
    else
    {
        if (not getAccountID(fieldView))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "Malformed"}};
        }
    }
    return {};
}};

/**
 * @brief Converts the hot wallet field into its strongly-typed value.
 */
struct HotWalletConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("hotWallet").
     */
    static constexpr std::string_view kName = "hotWallet";

    /**
     * @brief The value this converter produces (`std::set<xrpl::AccountID>`).
     */
    using ValueType = std::set<xrpl::AccountID>;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        ValueType result;
        auto const parseOne = [&](View const& elem) -> bool {
            if (not elem.isString())
                return false;
            auto id = detail::accountFromStringStrict(std::string{elem.asString()});
            if (not id.has_value())
                return false;
            result.insert(*id);
            return true;
        };

        if (fieldView.isString())
        {
            if (not parseOne(fieldView))
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
        }
        else
        {
            for (auto i = 0uz; i < fieldView.arraySize(); ++i)
            {
                if (not parseOne(fieldView.element(i)))
                    return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
            }
        }
        return result;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: hot wallet.
 */
inline constexpr auto hotWalletConv = HotWalletConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("hotwallet", &Input::hotWallets, kHotWalletV1, hotWalletConv));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 =
    extend(kInputSpecV1, field("hotwallet", &Input::hotWallets, kHotWalletV2, hotWalletConv));

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

}  // namespace rpc::spec::handlers::gateway_balances
