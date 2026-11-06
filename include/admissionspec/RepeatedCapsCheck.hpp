/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/VisitEvent.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace admission::spec {

/**
 * @brief A check that enforces a spec's @ref RepeatedCaps from the walk's events, then hands each
 * event to the spec's own check.
 *
 * It works the same for every walker:
 * - a protobuf cap counts the events of its field number at depth 0, a packed field's elements one
 *   by one;
 * - a JSON cap counts the elements directly inside an array with its key, from the array's
 *   `ArrayBegin` to its `ArrayEnd`.
 *
 * A cap's @c minEntryBytes applies to entries whose event carries a size: string and bytes
 * entries, and protobuf sub-messages (`ObjectBegin`); not to a JSON object, whose size is not known
 * when it begins. Made fresh for each message.
 *
 * @tparam Check The spec's check, invoked as `AdmissionDecision(VisitEvent const&)`.
 */
template <typename Check>
class RepeatedCapsCheck
{
    RepeatedCaps const* caps_;
    Check check_;
    std::array<uint64_t, kMaxRepeatedCaps> entries_{};
    // The depth of the JSON array a cap is counting, while inside it.
    std::array<std::optional<uint32_t>, kMaxRepeatedCaps> arrayDepth_{};

public:
    /**
     * @param caps The caps to enforce. They must outlive the check.
     * @param check The spec's check.
     */
    RepeatedCapsCheck(RepeatedCaps const& caps, Check check)
        : caps_{&caps}, check_{std::move(check)}
    {
    }

    AdmissionDecision
    operator()(VisitEvent const& event)
    {
        for (size_t i{}; i < caps_->count; ++i)
        {
            if (auto const decision = count(i, event); decision.dropped())
            {
                return decision;
            }
        }
        return check_(event);
    }

private:
    [[nodiscard]] static bool
    startsEntry(EventKind kind) noexcept
    {
        switch (kind)
        {
            case EventKind::Int64:
            case EventKind::Uint64:
            case EventKind::Double:
            case EventKind::Bool:
            case EventKind::Null:
            case EventKind::String:
            case EventKind::Bytes:
            case EventKind::ObjectBegin:
            case EventKind::ArrayBegin:
                return true;
            default:
                return false;
        }
    }

    // Whether the event carries its entry's size. A JSON object's size is not known when it
    // begins, so only a protobuf sub-message's `ObjectBegin` carries one.
    [[nodiscard]] static bool
    hasEntrySize(EventKind kind, bool fromJson) noexcept
    {
        return kind == EventKind::String or kind == EventKind::Bytes or
            (kind == EventKind::ObjectBegin and not fromJson);
    }

    // Count `event` against cap `i`, if it is one of its entries.
    [[nodiscard]] AdmissionDecision
    count(size_t i, VisitEvent const& event)
    {
        auto const& cap = caps_->caps[i];
        bool const fromJson = event.fieldNumber == std::numeric_limits<uint64_t>::max();
        bool entry = false;

        if (cap.fieldNumber != 0 and not fromJson)
        {
            entry = event.depth == 0 and event.fieldNumber == cap.fieldNumber and
                startsEntry(event.kind);
        }
        else if (not cap.key.empty() and fromJson)
        {
            if (arrayDepth_[i] and event.depth == *arrayDepth_[i] + 1)
            {
                entry = startsEntry(event.kind);
            }
            else if (event.kind == EventKind::ArrayBegin and event.key == cap.key)
            {
                arrayDepth_[i] = event.depth;
                entries_[i] = 0;
            }
            else if (
                event.kind == EventKind::ArrayEnd and arrayDepth_[i] and
                event.depth == *arrayDepth_[i])
            {
                arrayDepth_[i].reset();
            }
        }

        if (not entry)
        {
            return AdmissionDecision::admit();
        }
        if (++entries_[i] > cap.maxEntries)
        {
            return AdmissionDecision::drop(
                "too many entries in a repeated field", PenaltyClass::Invalid);
        }
        if (hasEntrySize(event.kind, fromJson) and event.size < cap.minEntryBytes)
        {
            return AdmissionDecision::drop(
                "repeated entry smaller than its type allows", PenaltyClass::Invalid);
        }
        return AdmissionDecision::admit();
    }
};

}  // namespace admission::spec
