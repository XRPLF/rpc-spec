/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::transaction_entry {

/**
 * @brief Converts `tx_hash` into a hash, or into `TxHashError::Malformed` if it is not one.
 *
 * Never fails: a malformed hash is a value, not a parse error, because xrpld reports it only after
 * resolving the ledger. Clio rejects it earlier, with `uint256Hex`.
 */
struct TxHashConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("uint256Hex").
     */
    static constexpr std::string_view kName = "uint256Hex";

    /**
     * @brief The value this converter produces (`std::expected<xrpl::uint256, TxHashError>`).
     */
    using ValueType = std::expected<xrpl::uint256, TxHashError>;

    /**
     * @brief Produce the field's hash, or `TxHashError::Malformed`.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The hash, or `TxHashError::Malformed`; never a Status.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        xrpl::uint256 hash;
        if (fieldView.isString() and hash.parseHex(std::string{fieldView.asString()}.c_str()))
            return ValueType{hash};
        return ValueType{std::unexpected{TxHashError::Malformed}};
    }
};

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "tx_hash",
        &Input::txHash,
        ifServerClio(withCustomError(required, rpc::kFieldNotFoundTransaction), uint256Hex),
        TxHashConverter{}));

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

}  // namespace rpc::spec::handlers::transaction_entry
