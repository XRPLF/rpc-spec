/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_index' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
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

}  // namespace rpc::spec::handlers::ledger_index
