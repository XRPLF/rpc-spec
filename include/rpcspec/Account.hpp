/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/ServerConditional.hpp>

#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace rpc::spec {

/**
 * @brief What xrpld does with an account field whose value is not a string.
 */
enum class NonStringAccount : std::uint8_t {
    Fails,     ///< The parse fails, with the error the base58 decoding gives.
    Deferred,  ///< The value is carried as @ref DeferredError::Malformed, like a bad string.
};

/**
 * @brief Converts an account field into a @ref DeferredAccountId.
 *
 * On xrpld the account is decoded as base58 only, and a string that does not decode is carried as
 * @ref DeferredError::Malformed for the handler to report; a non-string is handled as
 * @ref nonString says. On Clio @ref clioConv parses the field, and its failure fails the parse.
 *
 * @tparam ClioConv The converter Clio parses the field with.
 */
template <typename ClioConv>
struct DeferredAccountIdConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief The value this converter produces (`DeferredAccountId`).
     */
    using ValueType = DeferredAccountId;

    /**
     * @brief The converter Clio parses the field with.
     */
    ClioConv clioConv;

    /**
     * @brief What xrpld does with a value that is not a string.
     */
    NonStringAccount nonString;

    /**
     * @brief Validate the field and produce its value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The account, a deferred decoding failure, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if constexpr (kIsXrpldBuild)
        {
            auto id = AccountIdConverter{}.parse(fieldView);
            if (id.has_value())
                return ValueType{*id};
            if (fieldView.isString() or nonString == NonStringAccount::Deferred)
                return ValueType{std::unexpected{DeferredError::Malformed}};
            return std::unexpected{std::move(id).error()};
        }
        else
        {
            auto id = clioConv.parse(fieldView);
            if (not id.has_value())
                return std::unexpected{std::move(id).error()};
            return ValueType{*id};
        }
    }
};

/**
 * @brief An account whose decoding failure the xrpld handler reports; Clio parses it with @p conv.
 *
 * @param conv The converter Clio parses the field with.
 * @param nonString What xrpld does with a value that is not a string.
 * @return The deferring converter.
 */
template <typename ClioConv>
[[nodiscard]] consteval auto
deferredAccount(ClioConv conv, NonStringAccount nonString)
{
    return DeferredAccountIdConverter<ClioConv>{conv, nonString};
}

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: an account whose decoding failure the xrpld handler reports.
 */
inline constexpr auto deferredAccountId = deferredAccount(accountId, NonStringAccount::Fails);

/**
 * @brief Converter instance: an account whose every failure, a non-string included, the xrpld
 * handler reports; on Clio, @ref accountIdActMalformed.
 */
inline constexpr auto deferredAccountIdActMalformed =
    deferredAccount(accountIdActMalformed, NonStringAccount::Deferred);
// NOLINTEND(readability-identifier-naming)

}  // namespace rpc::spec
