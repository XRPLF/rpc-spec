/** @file */
#pragma once

#include <xrpl/protocol/TxFormats.h>

#include <cctype>
#include <string>
#include <unordered_set>

namespace rpc::spec::detail {

/**
 * @brief The set of known transaction type names, lowercased.
 *
 * Derived at runtime from libxrpl's @ref xrpl::TxFormats, so the list always
 * matches the linked rippled/libxrpl version. Used by the `account_tx` spec to
 * validate the `tx_type` field.
 *
 * @return A reference to the static set of lowercase transaction type names
 */
[[nodiscard]] inline std::unordered_set<std::string> const&
txTypesInLowercase()
{
    static std::unordered_set<std::string> const kTYPES = []() {
        std::unordered_set<std::string> keys;
        for (auto const& item : xrpl::TxFormats::getInstance())
        {
            std::string name{item.getName()};
            for (auto& c : name)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            keys.insert(std::move(name));
        }
        return keys;
    }();
    return kTYPES;
}

}  // namespace rpc::spec::detail
