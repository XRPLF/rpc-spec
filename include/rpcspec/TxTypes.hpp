/** @file */
#pragma once
// Public registry of XRPL transaction type names.
//
// Used by the `account_tx` spec to validate `tx_type`, and by consumers that need
// the set directly, which is why this is public API and not detail/.

#include <xrpl/protocol/TxFormats.h>

#include <cctype>
#include <string>
#include <unordered_set>

namespace rpc::spec {

/**
 * @brief The set of known transaction type names, lowercased.
 *
 * Derived at runtime from libxrpl's @ref xrpl::TxFormats, so the list always
 * matches the linked xrpld/libxrpl version.
 *
 * @return A reference to the static set of lowercase transaction type names
 */
[[nodiscard]] inline std::unordered_set<std::string> const&
txTypesInLowercase()
{
    static std::unordered_set<std::string> const kTypes = [] {
        std::unordered_set<std::string> keys;
        for (auto const& item : xrpl::TxFormats::getInstance())
        {
            std::string name{item.getName()};
            for (auto& chr : name)
                chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
            keys.insert(std::move(name));
        }
        return keys;
    }();
    return kTypes;
}

}  // namespace rpc::spec
