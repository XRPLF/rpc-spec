/** @file */
#pragma once

#include <admissionspec/Frame.hpp>
#include <admissionspec/Types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace admission::spec {

/**
 * @brief A cap on a repeated field, checked while the payload is walked, before it is parsed.
 */
struct RepeatedCap
{
    /**
     * @brief The field's number in the message's schema.
     */
    uint32_t fieldNumber{};

    /**
     * @brief The most entries the field may hold.
     */
    uint64_t maxEntries{};

    /**
     * @brief The fewest bytes one entry may take; smaller entries are refused, so a count under
     * the cap cannot be padded out with empty entries.
     */
    uint64_t minEntryBytes{};

    constexpr bool
    operator==(RepeatedCap const&) const = default;
};

/**
 * @brief The most repeated-field caps one spec may declare.
 */
inline constexpr size_t kMaxRepeatedCaps = 8;

/**
 * @brief A spec's repeated-field caps.
 */
struct RepeatedCaps
{
    std::array<RepeatedCap, kMaxRepeatedCaps> caps{};
    size_t count{};

    /**
     * @return The cap on @p fieldNumber, or nothing if the field is not capped.
     */
    [[nodiscard]] constexpr std::optional<RepeatedCap>
    find(uint32_t fieldNumber) const noexcept
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (caps[i].fieldNumber == fieldNumber)
            {
                return caps[i];
            }
        }
        return std::nullopt;
    }

    constexpr bool
    operator==(RepeatedCaps const&) const = default;
};

/**
 * @brief Build @ref RepeatedCaps from a braced list, e.g.
 * `repeatedCaps({{.fieldNumber = 3, .maxEntries = 12288, .minEntryBytes = 1}})`.
 */
template <size_t N>
    requires(N <= kMaxRepeatedCaps)
[[nodiscard]] consteval RepeatedCaps
repeatedCaps(RepeatedCap const (&caps)[N])
{
    auto result = RepeatedCaps{};
    for (size_t i = 0; i < N; ++i)
    {
        for (size_t j = 0; j < i; ++j)
        {
            if (caps[j].fieldNumber == caps[i].fieldNumber)
            {
                throw "repeatedCaps: a field is capped twice";  // a compile error in consteval
            }
        }
        result.caps[i] = caps[i];
    }
    result.count = N;
    return result;
}

/**
 * @brief A stateful duty a message's handler owes, which the admission stages cannot enforce.
 *
 * A spec lists them so every message's duties are reviewed in one place, and so a test can check
 * that each has a test that fails without it.
 */
enum class Obligation : uint32_t {
    None = 0,
    /// A reply must match a request this host made.
    AnswersOutstandingRequest = 1U << 0,
    /// Duplicates are suppressed by a key on the raw bytes, and a drop for load or sync after the
    /// key is recorded must not mark it as seen.
    DedupBeforeWork = 1U << 1,
    /// Trusted and untrusted senders take different paths, and signatures are checked before any
    /// unbounded work.
    TrustLookup = 1U << 2,
    /// The message's minimum charge is taken before it is queued for work.
    ChargedBeforeQueued = 1U << 3,
    /// Passing the message on counts against a relayed balance.
    RelayedBalance = 1U << 4,
    /// Every charge for one message can only raise what it costs.
    FeeMonotonic = 1U << 5,
};

[[nodiscard]] constexpr Obligation
operator|(Obligation a, Obligation b) noexcept
{
    using U = std::underlying_type_t<Obligation>;
    return static_cast<Obligation>(static_cast<U>(a) | static_cast<U>(b));
}

/**
 * @return Whether @p set includes @p obligation.
 */
[[nodiscard]] constexpr bool
includes(Obligation set, Obligation obligation) noexcept
{
    using U = std::underlying_type_t<Obligation>;
    return (static_cast<U>(set) & static_cast<U>(obligation)) == static_cast<U>(obligation);
}

/**
 * @brief The fixed facts about a message type that the admission stages act on.
 *
 * Unlike tunables, these are not configuration: they are properties of the protocol.
 */
struct SpecAttributes
{
    /**
     * @brief The type number a frame header names for this type. Required for @ref SpecRegistry.
     */
    std::optional<uint32_t> typeId;

    /**
     * @brief Whether only a sender in the host's cluster may send it.
     */
    bool fromClusterOnly{};

    /**
     * @brief A feature the connection must have negotiated. Set it only when every supported
     * sender checks the same feature before sending, or honest senders are refused.
     */
    std::optional<FeatureId> requiresFeature;

    /**
     * @brief Whether a supported older release passes this type on as received. If so, a refusal
     * for its size or content costs only @ref PenaltyClass::Trivial, so an honest relay is not
     * penalized for what it did not write.
     */
    bool relayedByOlderVersions{};

    /**
     * @brief Caps on repeated fields, checked while the payload is walked.
     */
    RepeatedCaps repeated{};

    /**
     * @brief The handler's duties; see @ref Obligation.
     */
    Obligation obligations{Obligation::None};

    constexpr bool
    operator==(SpecAttributes const&) const = default;
};

/**
 * @brief Limits applied to every message while it is walked, unless its spec overrides them.
 */
struct BaseLimits
{
    /**
     * @brief The most fields a message, or a message nested in it, may hold that its schema does
     * not define.
     */
    Tunable<"max_unknown_fields", uint64_t> maxUnknownFields{
        16,
        "admission.base.max_unknown_fields"};

    /**
     * @brief The most bytes those unknown fields may take together.
     */
    Tunable<"max_unknown_field_bytes", uint64_t> maxUnknownFieldBytes{
        1024,
        "admission.base.max_unknown_field_bytes"};

    /**
     * @brief The deepest messages may nest.
     */
    Tunable<"max_depth", uint64_t> maxDepth{100, "admission.base.max_depth"};

    /**
     * @brief Whether a protobuf group (wire types 3 and 4) is refused.
     */
    Tunable<"reject_groups", bool> rejectGroups{true, "admission.base.reject_groups"};
};

}  // namespace admission::spec
