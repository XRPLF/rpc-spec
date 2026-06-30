/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>

#include <xrpl/basics/base_uint.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rpc::spec {

/**
 * @brief A ledger shortcut, mirroring rippled's LedgerShortcut.
 *
 * Clio only serves @c Validated locally and forwards @c Current / @c Closed to
 * rippled; rippled resolves all three. The spec preserves whichever the request
 * asked for so each server can act on it.
 */
enum class LedgerShortcut { Validated, Current, Closed };

/**
 * @brief The default ledger when a request names neither ledger_hash nor
 * ledger_index.
 *
 * Resolved at compile time from the server macro: rippled defaults to the
 * current ledger, Clio to the latest validated one.
 */
#if defined(RPCSPEC_IS_CLIO)
inline constexpr LedgerShortcut kDefaultLedgerShortcut = LedgerShortcut::Validated;
#elif defined(RPCSPEC_IS_RIPPLED)
inline constexpr LedgerShortcut kDefaultLedgerShortcut = LedgerShortcut::Current;
#else
#error "rpcspec: define RPCSPEC_IS_CLIO=1 or RPCSPEC_IS_RIPPLED=1 (the server backend macro)"
#endif

/**
 * @brief The ledger a request selects, as a single strong value.
 *
 * Exactly one of three states, unified from the legacy ledger_hash +
 * ledger_index pair:
 *   - a shortcut (validated / current / closed),
 *   - a concrete ledger hash,
 *   - a concrete ledger sequence.
 *
 * Default-constructs to @ref kDefaultLedgerShortcut, so a request that specifies
 * no ledger yields the server's default with no extra handler logic. An omitted
 * ledger and an explicit shortcut equal to the default are deliberately
 * indistinguishable: both servers treat them identically.
 */
struct LedgerSpecifier {
    std::variant<LedgerShortcut, xrpl::uint256, uint32_t> value{kDefaultLedgerShortcut};

    [[nodiscard]] bool
    isShortcut() const noexcept
    {
        return std::holds_alternative<LedgerShortcut>(value);
    }

    [[nodiscard]] bool
    isHash() const noexcept
    {
        return std::holds_alternative<xrpl::uint256>(value);
    }

    [[nodiscard]] bool
    isSequence() const noexcept
    {
        return std::holds_alternative<uint32_t>(value);
    }

    friend bool
    operator==(LedgerSpecifier const&, LedgerSpecifier const&) = default;
};

namespace detail {

template <SomeFieldView FA>
[[nodiscard]] inline std::expected<LedgerSpecifier, rpc::Status>
ledgerSpecifierFromIndex(FA const& f)
{
    auto const invalid = [&] {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field 'ledger_index', not string or number."
        }};
    };

    if (f.isUint32())
        return LedgerSpecifier{uint32_t{f.asUint32()}};
    if (f.isInt64())  // numeric but outside uint32 range
        return invalid();
    if (!f.isString())
        return invalid();

    auto const sv = f.asString();
    if (sv.empty())
        return LedgerSpecifier{kDefaultLedgerShortcut};
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
ledgerSpecifierFromHash(FA const& f)
{
    auto const invalid = [&] {
        return std::unexpected{
            rpc::Status{rpc::RippledError::RpcInvalidParams, "Invalid field 'ledger_hash', not hex string."}
        };
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
 * @brief A spec field that resolves the ledger_hash / ledger_index pair into a
 * single LedgerSpecifier Input member.
 *
 * Unlike an ordinary bound field (one JSON key, one converter) this reads both
 * root keys, enforces that at most one is given, and produces the unified value.
 * It duck-types as a bound field (exposes @c kIsBound, @c key, @c parseInto,
 * @c check and @c dump) so TypedSpec dispatches and counts it like any other.
 * The bound key is "ledger_index"; a spec using this must not also bind that key.
 */
template <typename InputT, typename Member>
struct LedgerSelectorField {
    static constexpr bool kIsBound = true;

    std::string_view key{"ledger_index"};
    Member InputT::* member;

    consteval explicit LedgerSelectorField(Member InputT::* m) : member{m}
    {
    }

    static_assert(
        std::is_assignable_v<Member&, LedgerSpecifier>,
        "rpcspec: ledgerSelector must bind a LedgerSpecifier Input member"
    );

    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, InputT& out) const
    {
        auto const hashFa = root.child("ledger_hash");
        auto const indexFa = root.child("ledger_index");

        if (hashFa.present() && indexFa.present())
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, "Cannot specify both 'ledger_hash' and 'ledger_index'."
            }};

        if (hashFa.present()) {
            auto res = detail::ledgerSpecifierFromHash(hashFa);
            if (!res.has_value())
                return std::unexpected{std::move(res).error()};
            out.*member = std::move(res).value();
            return {};
        }

        if (indexFa.present()) {
            auto res = detail::ledgerSpecifierFromIndex(indexFa);
            if (!res.has_value())
                return std::unexpected{std::move(res).error()};
            out.*member = std::move(res).value();
            return {};
        }

        return {};
    }

    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const&) const
    {
        return {};
    }

    void
    dump(SpecDumpWriter& w) const
    {
        w.bulletGroup("ledger_hash", [] {});
        w.bulletGroup("ledger_index", [] {});
    }
};

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
    return LedgerSelectorField<InputT, Member>{member};
}

}  // namespace rpc::spec
