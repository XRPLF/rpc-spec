/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <type_traits>

namespace rpc::spec {

/**
 * @brief Converts an account field into a @ref DeferredAccountId.
 */
struct DeferredAccountIdConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief The value this converter produces (`DeferredAccountId`).
     */
    using ValueType = DeferredAccountId;

    /**
     * @brief Validate the field and produce its value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The account, a deferred decoding failure, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        auto id = AccountIdConverter{}.parse(fieldView);
        if (id.has_value())
            return ValueType{*id};
        if constexpr (kIsXrpldBuild)
        {
            if (fieldView.isString())
                return ValueType{std::unexpected{AccountError::Malformed}};
        }
        return std::unexpected{std::move(id).error()};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: an account whose decoding failure the xrpld handler reports.
 */
inline constexpr auto deferredAccountId = DeferredAccountIdConverter{};

/**
 * @brief Root-level bound field for `account`, falling back to the legacy `ident` on xrpld.
 *
 * On xrpld `ident` is read only when `account` is absent, and is otherwise ignored entirely, so
 * a malformed `ident` beside a valid `account` is not an error. A request with neither is
 * reported as a missing `account`. On Clio this is `required` + @ref deferredAccountId on
 * `account`, and `ident` is ignored.
 *
 * Duck-types as a bound field, like @ref LedgerSelectorField; the bound key is "account".
 */
template <typename InputT, typename Member>
struct AccountOrIdentField
{
    /**
     * @brief Marks this type as a bound field, so TypedSpec dispatches to parseInto().
     */
    static constexpr bool kIsBound = true;

    /**
     * @brief The bound key; always "account".
     */
    std::string_view key{"account"};

    /**
     * @brief Pointer to the DeferredAccountId member that receives the account.
     */
    Member InputT::* member;

    /**
     * @brief Construct an @ref AccountOrIdentField.
     *
     * @param member Pointer to the DeferredAccountId member to bind.
     */
    consteval explicit AccountOrIdentField(Member InputT::* member) : member{member}
    {
    }

    static_assert(
        std::is_assignable_v<Member&, DeferredAccountId>,
        "rpcspec: accountOrIdent must bind a DeferredAccountId Input member");

    /**
     * @brief Read `account` (or `ident`) and assign the result into @p out.
     *
     * @param root The request root to read from.
     * @param out The Input being populated.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, InputT& out) const
    {
        std::string_view const chosen =
            kIsXrpldBuild and not root.child(key).present() ? "ident" : key;
        auto view = root.child(chosen);
        if constexpr (kIsXrpldBuild)
        {
            if (not view.present())
            {
                return std::unexpected{
                    rpc::Status{rpc::XrpldError::RpcInvalidParams, rpc::missingFieldMessage(key)}};
            }
        }
        else
        {
            if (auto const res = Required::verify(view); not res.has_value())
                return res;
        }

        auto res = deferredAccountId.parse(view);
        if (not res.has_value())
            return std::unexpected{std::move(res).error()};
        out.*member = std::move(res).value();
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
        writer.bulletGroup("account", [&] {
            writer.bullet(Required::kName, [] {});
            writer.bullet(DeferredAccountIdConverter::kName, [] {});
        });
        if constexpr (kIsXrpldBuild)
        {
            writer.bulletGroup(
                "ident", [&] { writer.bullet(DeferredAccountIdConverter::kName, [] {}); });
        }
    }
};

/**
 * @brief Bind `account`, with xrpld's `ident` fallback, to a DeferredAccountId member.
 *
 * @param member Pointer to the DeferredAccountId member to bind.
 * @return The bound field.
 */
template <typename InputT, typename Member>
[[nodiscard]] consteval auto
accountOrIdent(Member InputT::* member)
{
    return AccountOrIdentField<InputT, Member>{member};
}

}  // namespace rpc::spec
