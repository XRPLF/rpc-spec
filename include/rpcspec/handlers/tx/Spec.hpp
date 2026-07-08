/** @file */
#pragma once
// Shared constexpr spec for the 'tx' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/tx/Types.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::tx {

struct ToUpperModifier
{
    static constexpr std::string_view kName = "toUpper";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    modify(FA& f)
    {
        if (!f.present() || !f.isString())
            return {};
        auto const sv = f.asString();
        std::string upper{sv};
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        f.set(std::string_view{upper});
        return {};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto toUpper = ToUpperModifier{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    field("transaction", &Input::transaction, asUint256),
    field("ctid", &Input::ctid, toUpper, asString),
    field("binary", &Input::binary, jsonBool),
    field("min_ledger", &Input::minLedger, asUint32),
    field("max_ledger", &Input::maxLedger, asUint32));

inline constexpr auto kInputSpecV2 =
    extend(kInputSpecV1, field("binary", &Input::binary, jsonBoolStrict));

inline constexpr auto& kSpecV1 = kInputSpecV1;
inline constexpr auto& kSpecV2 = kInputSpecV2;

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::tx
