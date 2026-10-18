/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/ServerConditional.hpp>

#include <expected>
#include <string_view>
#include <utility>

namespace rpc::spec {

/**
 * @brief Converts an account field into a @ref DeferredAccountId.
 */
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
        auto id = AccountIdConverter{}.parse(fieldView);
        if (id.has_value())
            return ValueType{*id};
        if constexpr (kIsXrpldBuild)
        {
            if (fieldView.isString())
                return ValueType{std::unexpected{AccountError::Malformed}};
        }
        return std::unexpected{std::move(id).error()};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: an account whose decoding failure the xrpld handler reports.
 */
inline constexpr auto deferredAccountId = DeferredAccountIdConverter{};

}  // namespace rpc::spec
