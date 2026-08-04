/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <charconv>
#include <cstdint>
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
#if defined(RPCSPEC_IS_CLIO)
inline constexpr LedgerShortcut kDefaultLedgerShortcut = LedgerShortcut::Validated;
#elif defined(RPCSPEC_IS_XRPLD)
inline constexpr LedgerShortcut kDefaultLedgerShortcut = LedgerShortcut::Current;
#else
#error "rpcspec: define RPCSPEC_IS_CLIO=1 or RPCSPEC_IS_XRPLD=1 (the server backend macro)"
#endif

/**
 * @brief Whether the deprecated `ledger` key actually selects a ledger.
 *
 * xrpld has always resolved it (ledgerFromRequest reads it for every handler that
 * looks a ledger up); Clio only ever warned and ignored the value. Both servers
 * warn — this governs resolution alone.
 */
#if defined(RPCSPEC_IS_CLIO)
inline constexpr bool kResolvesLegacyLedgerField = false;
#elif defined(RPCSPEC_IS_XRPLD)
inline constexpr bool kResolvesLegacyLedgerField = true;
#endif

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
    std::variant<std::monostate, LedgerShortcut, xrpl::uint256, uint32_t> value;

    /** @brief True when the request named no ledger (neither hash nor index). */
    [[nodiscard]] bool
    isUnspecified() const noexcept
    {
        return std::holds_alternative<std::monostate>(value);
    }

    /** @brief True when the value is a shortcut (validated / current / closed). */
    [[nodiscard]] bool
    isShortcut() const noexcept
    {
        return std::holds_alternative<LedgerShortcut>(value);
    }

    /** @brief True when the value is a concrete ledger hash. */
    [[nodiscard]] bool
    isHash() const noexcept
    {
        return std::holds_alternative<xrpl::uint256>(value);
    }

    /** @brief True when the value is a concrete ledger sequence. */
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

    friend bool
    operator==(LedgerSpecifier const&, LedgerSpecifier const&) = default;
};

namespace detail {

// The key an error message names. Normally the key actually read, but the legacy
// xrpld `ledger` key reuses the hash/index parsers and must report itself.
template <SomeFieldView FA>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
ledgerSpecifierFromIndex(FA const& f, std::string_view reportedKey = "ledger_index")
{
    auto const invalid = [&] {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{reportedKey} + "', not string or number."}};
    };

    if (f.isUint32())
        return LedgerSpecifier{uint32_t{f.asUint32()}};
    if (f.isInt64())  // numeric but outside uint32 range
        return invalid();
    if (!f.isString())
        return invalid();

    auto const sv = f.asString();
    if (sv.empty())
        return LedgerSpecifier{};  // unspecified; the handler applies its default
    if (sv == "validated")
        return LedgerSpecifier{LedgerShortcut::Validated};
    if (sv == "current")
        return LedgerSpecifier{LedgerShortcut::Current};
    if (sv == "closed")
        return LedgerSpecifier{LedgerShortcut::Closed};

    uint32_t seq = 0;
    auto const* const begin = sv.data();
    auto const* const end = sv.data() + sv.size();
    if (auto const [p, ec] = std::from_chars(begin, end, seq); ec == std::errc{} && p == end)
        return LedgerSpecifier{seq};
    return invalid();
}

template <SomeFieldView FA>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
ledgerSpecifierFromHash(FA const& f, std::string_view reportedKey = "ledger_hash")
{
    auto const invalid = [&] {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{reportedKey} + "', not hex string."}};
    };
    if (!f.isString())
        return invalid();
    xrpl::uint256 hash;
    if (!hash.parseHex(std::string{f.asString()}.c_str()))
        return invalid();
    return LedgerSpecifier{hash};
}

}  // namespace detail

/**
 * @brief A spec field that resolves the keys naming a ledger into a single
 * LedgerSpecifier Input member.
 *
 * Unlike an ordinary bound field (one JSON key, one converter) this reads several
 * root keys and produces the unified value. ledger_hash takes precedence over
 * ledger_index when both are present (mirroring the historical
 * getLedgerHeaderFromHashOrSeq contract — the two are NOT mutually exclusive),
 * and naming none leaves the member unspecified. It duck-types as a bound
 * field (exposes @c kIsBound, @c key, @c parseInto, @c check and @c dump) so
 * TypedSpec dispatches and counts it like any other. The bound key is
 * "ledger_index"; a spec using this must not also bind that key.
 *
 * With @p Legacy the deprecated `ledger` key joins them, as the lowest-priority
 * alternative. It predates the hash/index split and so carries either: a
 * 64-character string is a hash, anything else takes the ledger_index reading.
 * The key belongs to this field rather than a separate `field("ledger",
 * deprecated)` because it names the same thing and writes the same member — one
 * field owns everything about ledger selection, including the warning. It is
 * always warned about; whether it also resolves is per-server
 * (@ref kResolvesLegacyLedgerField).
 *
 * @tparam Legacy Whether this spec accepts the deprecated `ledger` key.
 */
template <typename InputT, typename Member, bool Legacy>
struct LedgerSelectorField
{
    static constexpr bool kIsBound = true;

    std::string_view key{"ledger_index"};
    Member InputT::* member;

    consteval explicit LedgerSelectorField(Member InputT::* m) : member{m}
    {
    }

    static_assert(
        std::is_assignable_v<Member&, LedgerSpecifier>,
        "rpcspec: ledgerSelector must bind a LedgerSpecifier Input member");

    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, InputT& out) const
    {
        auto const hashFa = root.child("ledger_hash");
        auto const indexFa = root.child("ledger_index");

        // ledger_hash takes precedence over ledger_index (the two are not
        // mutually exclusive — accepting both preserves the existing contract).
        if (hashFa.present())
        {
            auto res = detail::ledgerSpecifierFromHash(hashFa);
            if (!res.has_value())
                return std::unexpected{std::move(res).error()};
            out.*member = std::move(res).value();
            return {};
        }

        if (indexFa.present())
        {
            auto res = detail::ledgerSpecifierFromIndex(indexFa);
            if (!res.has_value())
                return std::unexpected{std::move(res).error()};
            out.*member = std::move(res).value();
            return {};
        }

        if constexpr (Legacy && kResolvesLegacyLedgerField)
        {
            // Errors name `ledger`, not the key whose parser produced them.
            if (auto const fa = root.child("ledger"); fa.present())
            {
                auto res = fa.isString() && fa.asString().size() == 64
                    ? detail::ledgerSpecifierFromHash(fa, "ledger")
                    : detail::ledgerSpecifierFromIndex(fa, "ledger");
                if (!res.has_value())
                    return std::unexpected{std::move(res).error()};
                out.*member = std::move(res).value();
                return {};
            }
        }

        return {};
    }

    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check([[maybe_unused]] Root const& root) const
    {
        Warnings out;
        if constexpr (Legacy)
        {
            // Both servers warn, even where only one resolves the value.
            if (auto w = Deprecated::check(root.child("ledger")); w)
                out.push_back(std::move(*w));
        }
        return out;
    }

    void
    dump(SpecDumpWriter& w) const
    {
        // Render the underlying keys so the unified selector is still
        // discoverable in the schema dump, each with the value it accepts.
        w.bulletGroup("ledger_hash", [&] { w.bullet("uint256Hex", [] {}); });
        w.bulletGroup("ledger_index", [&] {
            w.bullet("uint32 or shortcut (validated/current/closed)", [] {});
        });
        if constexpr (Legacy)
        {
            w.bulletGroup("ledger", [&] {
                w.bullet("uint256Hex, uint32, or shortcut", [] {});
                w.bullet("deprecated", [] {});
            });
        }
    }
};

/** @brief Opt a @ref ledgerSelector into also accepting the deprecated `ledger` key. */
struct WithLegacyLedgerField
{
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto withLegacyLedgerField = WithLegacyLedgerField{};

/**
 * @brief Bind the ledger_hash / ledger_index pair to a LedgerSpecifier member.
 *
 * Reusable across handlers: a spec needing ledger selection writes
 * `ledgerSelector(&Input::ledger)` instead of declaring the two fields by hand.
 */
template <typename InputT, typename Member>
[[nodiscard]] consteval auto
ledgerSelector(Member InputT::* member)
{
    return LedgerSelectorField<InputT, Member, false>{member};
}

/**
 * @brief As @ref ledgerSelector, but the spec also accepts the deprecated `ledger`
 * key: `ledgerSelector(&Input::ledger, withLegacyLedgerField)`.
 *
 * Opt-in rather than automatic so the schema stays honest — a spec consumes
 * `ledger` only where it says it does, and the dump lists it there. Do NOT pair it
 * with a separate `field("ledger", deprecated)`: the selector emits that warning
 * itself, and two fields binding one member trips TypedSpec's bind-count guard.
 */
template <typename InputT, typename Member>
[[nodiscard]] consteval auto
ledgerSelector(Member InputT::* member, WithLegacyLedgerField)
{
    return LedgerSelectorField<InputT, Member, true>{member};
}

}  // namespace rpc::spec
