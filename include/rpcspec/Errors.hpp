/** @file */
#pragma once

#include <xrpl/protocol/ErrorCodes.h>

#include <boost/json/object.hpp>

#include <algorithm>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace rpc {

/** @brief Custom clio RPC Errors. */
enum class ClioError {
    // normal clio errors start with 5000
    RpcMalformedCurrency = 5000,
    RpcMalformedRequest = 5001,
    RpcMalformedOwner = 5002,
    RpcMalformedAddress = 5003,
    RpcUnknownOption = 5005,
    RpcFieldNotFoundTransaction = 5006,
    RpcMalformedOracleDocumentId = 5007,
    RpcMalformedAuthorizedCredentials = 5008,

    // special system errors start with 6000
    RpcInvalidApiVersion = 6000,
    RpcCommandIsMissing = 6001,
    RpcCommandNotString = 6002,
    RpcCommandIsEmpty = 6003,
    RpcParamsUnparsable = 6004,

    RpcForwardingConnectionError = 7000,
    RpcForwardingRequestError = 7001,
    RpcForwardingTimeout = 7002,
    RpcForwardingInvalidResponse = 7003,
};

/** @brief Clio uses compatible Rippled error codes for most RPC errors. */
using RippledError = xrpl::ErrorCodeI;

/**
 * @brief Clio operates on a combination of Rippled and custom Clio error codes.
 *
 * @see RippledError For xrpld error codes
 * @see ClioError For custom clio error codes
 */
using CombinedError = std::variant<RippledError, ClioError>;

/** @brief A status returned from any RPC handler. */
struct Status
{
    CombinedError code = xrpl::RpcSuccess;
    std::string error;
    std::string message;
    std::optional<boost::json::object> extraInfo;

    Status() = default;

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     */
    /* implicit */ Status(CombinedError code) : code(code) {};

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param extraInfo The extra info
     */
    Status(CombinedError code, boost::json::object&& extraInfo)
        : code(code), extraInfo(std::move(extraInfo)) {};

    /**
     * @brief Construct a new Status object with a custom message
     *
     * @note HACK. Some xrpld handlers explicitly specify errors. This means
     * that we have to be able to duplicate this functionality.
     *
     * @param message The message
     */
    explicit Status(std::string message) : code(xrpl::RpcUnknown), message(std::move(message))
    {
    }

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param message The message
     */
    Status(CombinedError code, std::string message) : code(code), message(std::move(message))
    {
    }

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param error The error
     * @param message The message
     */
    Status(CombinedError code, std::string error, std::string message)
        : code(code), error(std::move(error)), message(std::move(message))
    {
    }

    bool
    operator==(Status const& other) const = default;

    /**
     * @brief Check if the status is not OK
     *
     * @return true if the status is not OK; false otherwise
     */
    operator bool() const
    {
        if (auto err = std::get_if<RippledError>(&code))
            return *err != xrpl::RpcSuccess;

        return true;  // ClioError is always truthy
    }

    /**
     * @brief Returns true if the @ref rpc::Status contains the desired @ref
     * rpc::RippledError
     *
     * @param other The @ref rpc::RippledError to match
     * @return true if status matches given error; false otherwise
     */
    bool
    operator==(RippledError other) const
    {
        if (auto err = std::get_if<RippledError>(&code))
            return *err == other;

        return false;
    }

    /**
     * @brief Returns true if the Status contains the desired @ref ClioError
     *
     * @param other The ClioError to match
     * @return true if status matches given error; false otherwise
     */
    bool
    operator==(ClioError other) const
    {
        if (auto err = std::get_if<ClioError>(&code))
            return *err == other;

        return false;
    }

    /**
     * @brief Custom output stream for Status
     *
     * @param stream The output stream
     * @param status The Status
     * @return The same ostream we were given
     */
    friend std::ostream&
    operator<<(std::ostream& stream, Status const& status);
};

/** @brief Warning codes that can be returned by clio. */
// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
enum WarningCode {
    WarnUnknown = -1,
    WarnRpcClio = 2001,
    WarnRpcOutdated = 2002,
    WarnRpcRateLimit = 2003,
    WarnRpcDeprecated = 2004
};

/** @brief Holds information about a clio warning. */
struct WarningInfo
{
    constexpr WarningInfo() = default;

    /**
     * @brief Construct a new Warning Info object
     *
     * @param code The warning code
     * @param message The warning message
     */
    constexpr WarningInfo(WarningCode code, char const* message) : code(code), message(message)
    {
    }

    WarningCode code = WarningCode::WarnUnknown;
    std::string_view const message = "unknown warning";
};

/**
 * @brief Get the warning info object from a warning code.
 *
 * @param code The warning code
 * @return A reference to the static warning info
 */
[[nodiscard]] inline WarningInfo const&
getWarningInfo(WarningCode code)
{
    static constexpr WarningInfo kINFOS[]{
        {WarningCode::WarnUnknown, "Unknown warning"},
        {WarningCode::WarnRpcClio,
         "This is a clio server. clio only serves validated data. If you want to talk to xrpld, "
         "include 'ledger_index':'current' in your request"},
        {WarningCode::WarnRpcOutdated, "This server may be out of date"},
        {WarningCode::WarnRpcRateLimit, "You are about to be rate limited"},
        {WarningCode::WarnRpcDeprecated,
         "Some fields from your request are deprecated. Please check the documentation at "
         "https://xrpl.org/docs/references/http-websocket-apis/ and update your request."}};

    auto matchByCode = [code](auto const& info) { return info.code == code; };
    if (auto it = std::ranges::find_if(kINFOS, matchByCode); it != std::end(kINFOS))
        return *it;

    throw std::out_of_range("Invalid WarningCode");
}

/**
 * @brief Generate JSON from a @ref rpc::WarningCode.
 *
 * @param code The warning code
 * @return The JSON output
 */
[[nodiscard]] inline boost::json::object
makeWarning(WarningCode code)
{
    auto const& info = getWarningInfo(code);
    return boost::json::object{{"id", static_cast<int>(code)}, {"message", info.message}};
}

}  // namespace rpc
