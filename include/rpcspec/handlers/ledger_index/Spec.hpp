/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_index' RPC command.
// Single source of truth — both Clio and xrpld include this file.

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

struct DateConverter
{
    static constexpr std::string_view kName = "utcDate";
    using ValueType = std::chrono::system_clock::time_point;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        // timeFormat(kDateFormat) already validated the format, so this parses.
        return *rpc::spec::detail::systemTpFromUtcStr(std::string{f.asString()}, kDateFormat);
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto dateConv = DateConverter{};

inline constexpr auto kInputSpec =
    spec<Input>(field("date", &Input::date, timeFormat(kDateFormat), dateConv));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::ledger_index
