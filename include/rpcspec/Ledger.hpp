/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rpc::spec {

/**
 * @brief A ledger shortcut, mirroring xrpld's LedgerShortcut.
 *
 * Clio only serves @c Validated locally and forwards @c Current / @c Closed to
 * xrpld; xrpld resolves all three. The spec preserves whichever the request
 * asked for so each server can act on it.
 */
enum class LedgerShortcut { Validated, Current, Closed };

/**
 * @brief The default ledger when a request names neither ledger_hash nor
 * ledger_index.
 *
 * Resolved at compile time from the server macro: xrpld defaults to the
 * current ledger, Clio to the latest validated one.
 */
inline constexpr LedgerShortcut kDefaultLedgerShortcut =
    kIsClioBuild ? LedgerShortcut::Validated : LedgerShortcut::Current;

/**
 * @brief The ledger a request selects, as a single strong value.
 *
 * Exactly one of four states, unified from the legacy ledger_hash +
 * ledger_index pair:
 *   - unspecified (`std::monostate`) — the request named no ledger;
 *   - a shortcut (validated / current / closed);
 *   - a concrete ledger hash;
 *   - a concrete ledger sequence.
 *
 * Default-constructs to the unspecified state. The server's default ledger is
 * NOT baked in here: a handler resolves it via @ref resolved() (or by checking
 * @ref isUnspecified()), which is what lets handlers that also accept a
 * ledger_index range (account_tx, nft_history) tell "no ledger given" apart from
 * an explicit shortcut.
 */
struct LedgerSpecifier
{
    /**
     * @brief The selected ledger: unspecified, a shortcut, a hash, or a sequence.
     */
    std::variant<std::monostate, LedgerShortcut, xrpl::uint256, uint32_t> value;

    /**
     * @brief True when the request named no ledger (neither hash nor index).
     *
     * @return true when no ledger was named; false otherwise.
     */
    [[nodiscard]] bool
    isUnspecified() const noexcept
    {
        return std::holds_alternative<std::monostate>(value);
    }

    /**
     * @brief True when the value is a shortcut (validated / current / closed).
     *
     * @return true when the value is a shortcut; false otherwise.
     */
    [[nodiscard]] bool
    isShortcut() const noexcept
    {
        return std::holds_alternative<LedgerShortcut>(value);
    }

    /**
     * @brief True when the value is a concrete ledger hash.
     *
     * @return true when the value is a ledger hash; false otherwise.
     */
    [[nodiscard]] bool
    isHash() const noexcept
    {
        return std::holds_alternative<xrpl::uint256>(value);
    }

    /**
     * @brief True when the value is a concrete ledger sequence.
     *
     * @return true when the value is a ledger sequence; false otherwise.
     */
    [[nodiscard]] bool
    isSequence() const noexcept
    {
        return std::holds_alternative<uint32_t>(value);
    }

    /**
     * @brief This specifier, with an unspecified value replaced by the
     * compile-time server default (@ref kDefaultLedgerShortcut).
     *
     * A concrete hash/sequence/shortcut is returned unchanged.
     *
     * @return A specifier whose value is never unspecified.
     */
    [[nodiscard]] LedgerSpecifier
    resolved() const
    {
        if (isUnspecified())
            return LedgerSpecifier{kDefaultLedgerShortcut};
        return *this;
    }

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    friend bool
    operator==(LedgerSpecifier const&, LedgerSpecifier const&) = default;
};

namespace detail {

/**
 * @brief Parse a `ledger_index` value into a LedgerSpecifier.
 *
 * @param fieldView The `ledger_index` field.
 * @return The selection, or a Status describing the failure.
 */
template <SomeFieldView View>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
ledgerSpecifierFromIndex(View const& fieldView)
{
    auto const invalid = [&] {
        return std::unexpected{
            rpc::Status{rpc::kMalformedField, rpc::malformedLedgerIndexMessage()}};
    };

    if (fieldView.isUint32())
        return LedgerSpecifier{uint32_t{fieldView.asUint32()}};
    if (fieldView.isInt64())  // numeric but outside uint32 range
        return invalid();
    if (not fieldView.isString())
        return invalid();

    auto const sv = fieldView.asString();
    if (sv == "validated")
        return LedgerSpecifier{LedgerShortcut::Validated};

    uint32_t seq = 0;
    auto const* const begin = sv.data();
    auto const* const end = sv.data() + sv.size();
    if (auto const [ptr, ec] = std::from_chars(begin, end, seq); ec == std::errc{} and ptr == end)
        return LedgerSpecifier{seq};
    return invalid();
}

/**
 * @brief Parse a `ledger_hash` value into a LedgerSpecifier.
 *
 * @param fieldView The `ledger_hash` field.
 * @return The selection, or a Status describing the failure.
 */
template <SomeFieldView View>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
ledgerSpecifierFromHash(View const& fieldView)
{
    if (not fieldView.isString())
    {
        return std::unexpected{
            rpc::Status{rpc::kMalformedField, rpc::notStringFieldMessage("ledger_hash")}};
    }
    xrpl::uint256 hash;
    if (not hash.parseHex(std::string{fieldView.asString()}.c_str()))
    {
        return std::unexpected{
            rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage("ledger_hash")}};
    }
    return LedgerSpecifier{hash};
}

/**
 * @brief Parse a ledger hash the way xrpld's `ledgerFromHash` does.
 *
 * @param fieldView A string field holding the hash.
 * @return The selection, or a Status naming @p fieldView's key.
 */
template <SomeFieldView View>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
xrpldLedgerFromHash(View const& fieldView)
{
    xrpl::uint256 hash;
    if (not hash.parseHex(std::string{fieldView.asString()}.c_str()))
    {
        return std::unexpected{rpc::Status{
            rpc::kMalformedField, rpc::expectedFieldMessage(fieldView.key(), "hex string")}};
    }
    return LedgerSpecifier{hash};
}

/**
 * @brief Parse a ledger index the way xrpld's `ledgerFromIndex` does.
 *
 * @param fieldView A string or integer field holding the index.
 * @return The selection, or a Status naming @p fieldView's key.
 */
template <SomeFieldView View>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
xrpldLedgerFromIndex(View const& fieldView)
{
    auto const invalid = [&] {
        return std::unexpected{rpc::Status{
            rpc::kMalformedField, rpc::expectedFieldMessage(fieldView.key(), "string or number")}};
    };

    if (not fieldView.isString())
    {
        if (fieldView.isUint32())
            return LedgerSpecifier{uint32_t{fieldView.asUint32()}};
        return invalid();
    }

    auto sv = fieldView.asString();
    if (sv == "current" or sv.empty())
        return LedgerSpecifier{LedgerShortcut::Current};
    if (sv == "validated")
        return LedgerSpecifier{LedgerShortcut::Validated};
    if (sv == "closed")
        return LedgerSpecifier{LedgerShortcut::Closed};

    // beast::lexicalCastChecked, which xrpld parses with, allows a leading '+'.
    if (sv.starts_with('+'))
        sv.remove_prefix(1);

    uint32_t seq = 0;
    auto const* const begin = sv.data();
    auto const* const end = sv.data() + sv.size();
    if (auto const [ptr, ec] = std::from_chars(begin, end, seq); ec == std::errc{} and ptr == end)
        return LedgerSpecifier{seq};
    return invalid();
}

/**
 * @brief Resolve the ledger a request selects the way xrpld's `ledgerFromRequest` does.
 *
 * @param root The request root to read from.
 * @return The selection, unspecified when none of the three is present, or a Status
 *         describing the failure.
 */
template <SomeObjectView Root>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
xrpldLedgerFromRequest(Root& root)
{
    auto const legacyView = root.child("ledger");
    auto const hashView = root.child("ledger_hash");
    auto const indexView = root.child("ledger_index");

    auto const count = static_cast<int>(legacyView.present()) +
        static_cast<int>(hashView.present()) + static_cast<int>(indexView.present());
    if (count > 1)
    {
        if (legacyView.present())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField,
                "Exactly one of 'ledger', 'ledger_hash', or 'ledger_index' can be specified."}};
        }
        return std::unexpected{rpc::Status{
            rpc::kMalformedField,
            "Exactly one of 'ledger_hash' or 'ledger_index' can be specified."}};
    }

    auto const isStringOrInteger = [](auto const& view) {
        return view.isString() or view.isInt64();
    };

    if (legacyView.present())
    {
        if (not isStringOrInteger(legacyView))
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField, rpc::expectedFieldMessage("ledger", "string or number")}};
        }
        if (legacyView.isString() and legacyView.asString().size() == 64)
            return xrpldLedgerFromHash(legacyView);
        return xrpldLedgerFromIndex(legacyView);
    }

    if (hashView.present())
    {
        if (not hashView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField, rpc::expectedFieldMessage("ledger_hash", "hex string")}};
        }
        return xrpldLedgerFromHash(hashView);
    }

    if (indexView.present())
    {
        if (not isStringOrInteger(indexView))
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField,
                rpc::expectedFieldMessage("ledger_index", "string or number")}};
        }
        return xrpldLedgerFromIndex(indexView);
    }

    return LedgerSpecifier{};
}

}  // namespace detail

/**
 * @brief A spec field that resolves the ledger_hash / ledger_index pair into a
 * single LedgerSpecifier Input member.
 *
 * Unlike an ordinary bound field (one JSON key, one converter) this reads several
 * root keys and produces the unified value, and naming none of them leaves the
 * member unspecified. The two servers disagree on the rules:
 *   - Clio: ledger_hash takes precedence over ledger_index when both are present
 *     (mirroring the historical getLedgerHeaderFromHashOrSeq contract — the two are
 *     NOT mutually exclusive).
 *   - xrpld: at most one of ledger_hash, ledger_index and the legacy `ledger` may be
 *     present; see detail::xrpldLedgerFromRequest. It duck-types as a bound
 * field (exposes @c kIsBound, @c key, @c parseInto, @c check and @c dump) so
 * TypedSpec dispatches and counts it like any other. The bound key is
 * "ledger_index"; a spec using this must not also bind that key.
 */
template <typename InputT, typename Member>
struct LedgerSelectorField
{
    /**
     * @brief Marks this type as a bound field, so TypedSpec dispatches to parseInto().
     */
    static constexpr bool kIsBound = true;

    /**
     * @brief The bound key; always "ledger_index".
     */
    std::string_view key{"ledger_index"};

    /**
     * @brief Pointer to the LedgerSpecifier member that receives the selection.
     */
    Member InputT::* member;

    /**
     * @brief Construct a @ref LedgerSelectorField.
     *
     * @param member Pointer to the LedgerSpecifier member to bind.
     */
    consteval explicit LedgerSelectorField(Member InputT::* member) : member{member}
    {
    }

    static_assert(
        std::is_assignable_v<Member&, LedgerSpecifier>,
        "rpcspec: ledgerSelector must bind a LedgerSpecifier Input member");

    /**
     * @brief Resolve the ledger pair and assign the result into @p out.
     *
     * @param root The request root to read from.
     * @param out The Input being populated.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, InputT& out) const
    {
        if constexpr (kIsXrpldBuild)
        {
            auto res = detail::xrpldLedgerFromRequest(root);
            if (not res.has_value())
                return std::unexpected{std::move(res).error()};
            out.*member = std::move(res).value();
        }
        else
        {
            auto const hashView = root.child("ledger_hash");
            auto const indexView = root.child("ledger_index");

            // ledger_hash is checked first so that, when both are malformed, its error is the one
            // reported - every handler declared ledger_hash ahead of ledger_index.
            if (hashView.present())
            {
                auto res = detail::ledgerSpecifierFromHash(hashView);
                if (not res.has_value())
                    return std::unexpected{std::move(res).error()};
                // ledger_index is still validated even though the hash takes precedence.
                if (indexView.present())
                {
                    if (auto idx = detail::ledgerSpecifierFromIndex(indexView); not idx.has_value())
                        return std::unexpected{std::move(idx).error()};
                }
                out.*member = std::move(res).value();
                return {};
            }

            if (indexView.present())
            {
                auto res = detail::ledgerSpecifierFromIndex(indexView);
                if (not res.has_value())
                    return std::unexpected{std::move(res).error()};
                out.*member = std::move(res).value();
            }
        }

        return {};
    }

    /**
     * @brief Inspect the field and optionally raise a non-blocking warning.
     *
     * @return The warning to report, or nullopt when none applies.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const&) const
    {
        return {};
    }

    /**
     * @brief Render this field's schema entry.
     *
     * @param writer The writer receiving the schema output.
     */
    void
    dump(SpecDumpWriter& writer) const
    {
        // Render the two underlying keys so the unified selector is still
        // discoverable in the schema dump, each with the value it accepts.
        writer.bulletGroup("ledger_hash", [&] { writer.bullet("uint256Hex", [] {}); });
        writer.bulletGroup("ledger_index", [&] {
            writer.bullet("uint32 or shortcut (validated/current/closed)", [] {});
        });
    }
};

/**
 * @brief Bind the ledger_hash / ledger_index pair to a LedgerSpecifier member.
 *
 * Reusable across handlers: a spec needing ledger selection writes
 * `ledgerSelector(&Input::ledger)` instead of declaring the two fields by hand.
 *
 * @tparam InputT The handler Input struct.
 * @tparam Member The bound member's type; must accept a LedgerSpecifier.
 * @param member The LedgerSpecifier member to bind.
 * @return A field that produces the unified ledger selection.
 */
template <typename InputT, typename Member>
[[nodiscard]] consteval auto
ledgerSelector(Member InputT::* member)
{
    return LedgerSelectorField<InputT, Member>{member};
}

}  // namespace rpc::spec
