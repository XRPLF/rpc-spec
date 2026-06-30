/** @file */
#pragma once
// A JSON-value-to-bool wrapper that accepts any JSON scalar as a truthy/falsy
// bool (V1 API compatibility).  Boost.Json tag_invoke overload lives here so
// ADL can find it wherever the type is used.

#include <boost/json/conversion.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>

#include <stdexcept>

namespace rpc::spec {

/**
 * @brief A wrapper around bool that allows conversion from any JSON value.
 *
 * Used by handler Input structs that accept the V1 API's lenient bool fields
 * (e.g. account_info's `signer_lists`).
 */
struct JsonBool
{
    bool value = false;

    /** @cond */
    operator bool() const
    {
        return value;
    }
    /** @endcond */
};

/**
 * @brief Convert a JSON value to a JsonBool
 *
 * @param jsonValue The JSON value to convert
 * @return The converted JsonBool
 */
inline JsonBool
// NOLINTNEXTLINE(readability-identifier-naming)
tag_invoke(boost::json::value_to_tag<JsonBool> const&, boost::json::value const& jsonValue)
{
    switch (jsonValue.kind())
    {
        case boost::json::kind::null:
            return JsonBool{false};
        case boost::json::kind::bool_:
            return JsonBool{jsonValue.as_bool()};
        case boost::json::kind::uint64:
            [[fallthrough]];
        case boost::json::kind::int64:
            return JsonBool{jsonValue.as_int64() != 0};
        case boost::json::kind::double_:
            return JsonBool{jsonValue.as_double() != 0.0};
        case boost::json::kind::string:
            // Also should be `jsonValue.as_string() != "false"` but rippled doesn't do
            // that. Anyway for v2 api we have bool validation
            return JsonBool{!jsonValue.as_string().empty() && jsonValue.as_string()[0] != 0};
        case boost::json::kind::array:
            return JsonBool{!jsonValue.as_array().empty()};
        case boost::json::kind::object:
            return JsonBool{!jsonValue.as_object().empty()};
    }
    throw std::runtime_error("Invalid json value");
}

}  // namespace rpc::spec
