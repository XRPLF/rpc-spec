/** @file */
#pragma once

#include <cstdint>
#include <expected>

namespace rpc::spec {

/**
 * @brief Why a deferred field holds no value.
 */
enum class DeferredError : std::uint8_t {
    Missing,    ///< The request did not send the field, or sent a value that reads as absent.
    Malformed,  ///< The value does not have the shape the field takes.
    Invalid,    ///< The value has the right shape, but a part of it is not acceptable.
};

/**
 * @brief A field whose parsing failure is reported by the handler rather than the spec.
 *
 * xrpld reports some parameter errors only after it has resolved the ledger, and with the ledger
 * fields in the response, so on xrpld such a field is carried here instead of failing the parse.
 * Clio rejects it in the spec, so on Clio a present field always holds a value. A field the
 * request may omit defaults to `std::unexpected{DeferredError::Missing}`.
 *
 * @tparam T The field's value.
 */
template <typename T>
using Deferred = std::expected<T, DeferredError>;

}  // namespace rpc::spec
