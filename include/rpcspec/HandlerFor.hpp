/** @file */
#pragma once
// Base class that gives a handler its spec-driven entry points from an Input type and the
// consuming project's JSON value type.
//
// A handler `public`-inherits `HandlerFor<Input, Value>` (plain implementation inheritance — no
// virtuals) and thereby gains:
//   - `using Input`  — the request Input type;
//   - `parseInput(jv, apiVersion)` — validate + deserialise into Input (version-selected);
//   - `spec(apiVersion)` — the RpcSpecView for warnings/schema-dump.
// Both resolve the handler's versioned spec via ADL (`specFor(Input const*)`, declared alongside
// the Input in the spec library), so version selection lives here, not in the consumer.
//
// The spec library names no JSON type, so `Value` is supplied by the consumer — which each one
// does once, as an alias:
//
//     template <typename InputT>
//     using HandlerFor = rpc::spec::HandlerFor<InputT, MyJsonValue>;
//
// Handlers and call sites then never mention a backend: `Handler::spec(apiVersion)` and
// `Handler::parseInput(jv, apiVersion)` are ordinary static members.
//
// The members are DECLARED here but DEFINED out-of-line in HandlerForDefs.hpp. A translation unit
// that only dispatches (the handler registry, the processor) sees declarations only and links
// against the single explicit instantiation the generated per-handler TU emits. This keeps the
// heavy consteval specs out of those shared TUs.

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/RpcSpecView.hpp>

#include <cstdint>
#include <expected>

namespace rpc::spec {

/**
 * @brief CRTP-free base providing spec-driven entry points for a handler.
 *
 * @tparam InputT The handler's request Input struct (its namespace supplies `specFor`).
 * @tparam ValueT The consumer's JSON value type; a backend must bind a view to it via
 *         @ref ObjectViewFor.
 */
template <typename InputT, typename ValueT>
    requires HasObjectView<ValueT>
struct HandlerFor
{
    /**
     * @brief The request Input type, inherited by the handler.
     */
    using Input = InputT;

    /**
     * @brief The consumer's JSON value type.
     */
    using Value = ValueT;

    /**
     * @brief The object view the spec reads @ref Value through.
     */
    using View = ObjectViewForT<ValueT>;

    /**
     * @brief Validate the request and parse it into a strong-typed Input in one pass.
     *
     * @param jv The raw request JSON (by value: spec modifiers normalise it in place).
     * @param apiVersion The API version to validate against.
     * @return The parsed Input, or a Status describing the validation failure.
     */
    [[nodiscard]] static std::expected<InputT, rpc::Status>
    parseInput(ValueT jv, uint32_t apiVersion);

    /**
     * @brief The handler's spec for @p apiVersion, as a type-erased view (for warnings + dump).
     *
     * @param apiVersion The API version to select the spec for.
     * @return A view over the selected version's spec.
     */
    [[nodiscard]] static RpcSpecView<View>
    spec(uint32_t apiVersion);
};

}  // namespace rpc::spec
