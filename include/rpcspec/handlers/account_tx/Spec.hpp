/** @file */
#pragma once
// Shared constexpr spec for the 'account_tx' RPC command.
// Single source of truth — both Clio and xrpld include this file.
//
// V1: account, ledger_hash, ledger_index, ledger_index_min/max, ctid, limit, marker,
//     tx_type, mpt_issuance_id
// V2: V1 + binary + forward

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/TxTypes.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_tx {

// Validates tx_type against the runtime-generated set of known transaction type names
// (lowercase). Because the set comes from xrpl::TxFormats at runtime, we use a
// CustomValidator lambda rather than the consteval spec::oneOf factory.
// Returns the same error shape as the old validation::OneOf: "Invalid field '<key>'."
inline constexpr auto kTxTypeValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isString())
    {
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
    auto const& validTypes = txTypesInLowercase();
    auto const sv = f.asString();
    if (!validTypes.contains(std::string{sv}))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field '" + std::string{f.key()} + "'."}};
    }
    return {};
}};

struct Int32BoundConverter
{
    static constexpr std::string_view kName = "int32";
    using ValueType = std::optional<int32_t>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const v = static_cast<int32_t>(f.asInt64());
        if (v == -1)
            return std::optional<int32_t>{std::nullopt};
        return std::optional<int32_t>{v};
    }
};

struct MarkerConverter
{
    static constexpr std::string_view kName = "marker";
    using ValueType = Marker;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        return Marker{.ledger = f.child("ledger").asUint32(), .seq = f.child("seq").asUint32()};
    }
};

inline constexpr auto kDelegateValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isObject())
    {
        return std::unexpected{
            rpc::Status{rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotObject"}};
    }

    auto const filterFa = f.child("delegate_filter");
    if (!filterFa.present())
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Field 'delegate_filter' is required but missing."}};
    }

    if (!filterFa.isString() ||
        (filterFa.asString() != "actor" && filterFa.asString() != "authorizer"))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Field 'delegate_filter' value must be 'actor' or 'authorizer'."}};
    }

    auto const counterPartyFa = f.child("counter_party");
    if (counterPartyFa.present())
    {
        if (auto const err = AccountFormat::verify(counterPartyFa); !err)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed,
                "Field 'counter_party' value must be a valid account."}};
        }
    }

    return {};
}};

/**
 * @brief Builds the DelegateFilter once kDelegateValidator has accepted the object.
 */
struct DelegateConverter
{
    static constexpr std::string_view kName = "delegate";
    using ValueType = DelegateFilter;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        DelegateFilter out{};
        out.delegateType = f.child("delegate_filter").asString() == "actor"
            ? DelegateFilter::Role::Actor
            : DelegateFilter::Role::Authorizer;

        auto const counterPartyFa = f.child("counter_party");
        if (counterPartyFa.present())
            out.counterParty = std::string{counterPartyFa.asString()};

        return out;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto delegateConv = DelegateConverter{};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto int32Bound = Int32BoundConverter{};
inline constexpr auto markerConv = MarkerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("ledger_index_min", &Input::ledgerIndexMin, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ledger_index_max", &Input::ledgerIndexMax, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ctid", type<std::string>),  // validated but not stored (no Input member)
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32),
    field(
        "marker",
        &Input::marker,
        withCustomError(type<JsonObject>, rpc::RippledError::RpcInvalidParams, "invalidMarker"),
        ifType<JsonObject>(section(
            field("ledger", required, type<uint32_t>),
            field("seq", required, type<uint32_t>))),
        markerConv),
    // binary/forward exist on Input for both versions; V1 coerces leniently (they are
    // not part of the V1 schema), V2 retightens them to a strict bool below.
    field("binary", &Input::binary, jsonBool),
    field("forward", &Input::forward, jsonBool),
    field("tx_type", &Input::transactionTypeInLowercase, toLower, kTxTypeValidator, asString),
    field("mpt_issuance_id", &Input::mptIssuanceId, asUint192),
    field("delegate", &Input::delegateFilter, kDelegateValidator, delegateConv));

inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_tx
