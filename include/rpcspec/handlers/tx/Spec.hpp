/** @file */
#pragma once

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

/**
 * @brief Modifier that uppercases a string field in place.
 */
struct ToUpperModifier
{
    /**
     * @brief Identifier for this item in the schema dump ("toUpper").
     */
    static constexpr std::string_view kName = "toUpper";

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    modify(View& fieldView)
    {
        if (not fieldView.present() or not fieldView.isString())
            return {};
        auto const sv = fieldView.asString();
        std::string upper{sv};
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char chr) {
            return static_cast<char>(std::toupper(chr));
        });
        fieldView.set(std::string_view{upper});
        return {};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Modifier instance: uppercase a string field in place.
 */
inline constexpr auto toUpper = ToUpperModifier{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    field("transaction", &Input::transaction, asUint256),
    field("ctid", &Input::ctid, toUpper, asString),
    field("binary", &Input::binary, jsonBool),
    field("min_ledger", &Input::minLedger, asUint32),
    field("max_ledger", &Input::maxLedger, asUint32));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 =
    extend(kInputSpecV1, field("binary", &Input::binary, jsonBoolStrict));

/**
 * @brief Spec v1.
 */
inline constexpr auto& kSpecV1 = kInputSpecV1;

/**
 * @brief Spec v2.
 */
inline constexpr auto& kSpecV2 = kInputSpecV2;

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

}  // namespace rpc::spec::handlers::tx
