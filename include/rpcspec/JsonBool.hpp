/** @file */
#pragma once
// A JSON-value-to-bool wrapper that accepts any JSON scalar as a truthy/falsy
// bool (V1 API compatibility). The coercion itself lives in the `jsonBool`
// converter, which reads through the field-view concept.

namespace rpc::spec {

/**
 * @brief A wrapper around bool that allows conversion from any JSON value.
 *
 * Used by handler Input structs that accept the V1 API's lenient bool fields
 * (e.g. account_info's `signer_lists`).
 */
struct JsonBool
{
    /**
     * @brief The coerced boolean value.
     */
    bool value = false;

    /** @cond */
    operator bool() const
    {
        return value;
    }
    /** @endcond */
};

}  // namespace rpc::spec
