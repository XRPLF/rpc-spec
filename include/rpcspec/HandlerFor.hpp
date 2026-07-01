/** @file */
#pragma once
// Base class that gives a handler its spec-driven entry points from just an Input type.
//
// A handler `public`-inherits `HandlerFor<Input>` (plain implementation inheritance — no virtuals)
// and thereby gains:
//   - `using Input`  — the request Input type;
//   - `parseInput(jv, apiVersion)` — validate + deserialise into Input (version-selected);
//   - `spec(apiVersion)` — the RpcSpecView for warnings/schema-dump.
// Both resolve the handler's versioned spec via ADL (`specFor(Input const*)`, declared alongside
// the Input in the spec library), so version selection lives here, not in the consumer.
//
// The members are DECLARED here but DEFINED out-of-line in HandlerForDefs.hpp. A translation unit
// that only dispatches (the handler registry, the processor) sees declarations only and links
// against the single explicit instantiation each handler's own .cpp emits
// (`template struct rpc::spec::HandlerFor<Input>;`). This keeps the heavy consteval specs out of
// those shared TUs — the same isolation the hand-written out-of-line parseInput used to provide.

#include <boost/json/value.hpp>

#include <rpcspec/Errors.hpp>
#include <rpcspec/RpcSpecView.hpp>

#include <cstdint>
#include <expected>

namespace rpc::spec {

/**
 * @brief CRTP-free base providing spec-driven entry points for a handler with Input @p InputT.
 *
 * @tparam InputT The handler's request Input struct (its namespace supplies `specFor`).
 */
template <typename InputT>
struct HandlerFor
{
    /** @brief The request Input type, inherited by the handler. */
    using Input = InputT;

    /**
     * @brief Validate the request and parse it into a strong-typed Input in one pass.
     *
     * @param jv The raw request JSON (by value: spec modifiers normalise it in place).
     * @param apiVersion The API version to validate against.
     * @return The parsed Input, or a Status describing the validation failure.
     */
    [[nodiscard]] static std::expected<InputT, rpc::Status>
    parseInput(boost::json::value jv, uint32_t apiVersion);

    /**
     * @brief The handler's spec for @p apiVersion, as a type-erased view (for warnings + dump).
     *
     * @param apiVersion The API version to select the spec for.
     * @return A view over the selected version's spec.
     */
    [[nodiscard]] static RpcSpecView
    spec(uint32_t apiVersion);
};

}  // namespace rpc::spec
