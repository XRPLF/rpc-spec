/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_index/Types.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::ledger_index {

/**
 * @brief Converts the date field into its strongly-typed value.
 */
struct DateConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("utcDate").
     */
    static constexpr std::string_view kName = "utcDate";

    /**
     * @brief The value this converter produces (`std::chrono::system_clock::time_point`).
     */
    using ValueType = std::chrono::system_clock::time_point;

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
        // timeFormat(kDateFormat) already validated the format, so this parses.
        return *rpc::spec::detail::systemTpFromUtcStr(
            std::string{fieldView.asString()}, kDateFormat);
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: date.
 */
inline constexpr auto dateConv = DateConverter{};

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec =
    spec<Input>(field("date", &Input::date, timeFormat(kDateFormat), dateConv));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

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

}  // namespace rpc::spec::handlers::ledger_index
