/** @file */
#pragma once

#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/SpecDumpWriter.hpp>

#include <tuple>
#include <utility>

namespace rpc::spec {

/**
 * @brief Detects spec items that own a `subFields` member (e.g. nested object validators).
 * @tparam T The type to check.
 */
template <typename T>
concept HasSubFields = requires(T const& item) { item.subFields; };

/**
 * @brief Detects spec items that own a `subItems` member (e.g. Section, OneOf).
 * @tparam T The type to check.
 */
template <typename T>
concept HasSubItems = requires(T const& item) { item.subItems; };

/**
 * @brief Detects spec items that expose a `wrapped()` accessor (e.g. WithMessage).
 * @tparam T The type to check.
 */
template <typename T>
concept HasWrapped = requires(T const& item) {
    { item.wrapped() };
};

/**
 * @brief Detects types that carry a static `kName` identifier string.
 * @tparam T The type to check.
 */
template <typename T>
concept HasKName = requires {
    { T::kName };
};

/**
 * @brief Detects spec items that can describe their own parameters to a writer.
 * @tparam T The item type to check.
 * @tparam Writer The writer type passed to `describeParams`.
 */
template <typename T, typename Writer>
concept HasDescribeParams =
    requires(T const& item, Writer& writer) { item.describeParams(writer); };

/**
 * @brief Write a single spec item to the dump writer.
 *
 * Dispatches on the item's capabilities (HasSubFields, HasSubItems, HasWrapped,
 * HasKName, HasDescribeParams) and emits an appropriate YAML-ish bullet entry.
 *
 * @tparam Item The spec item type.
 * @param writer The writer to emit output to.
 * @param item The item to dump.
 */
template <typename Item>
void
dumpItem(SpecDumpWriter& writer, Item const& item);

/**
 * @brief Write a FieldSpec (one named field and all its attached items) to the dump writer.
 *
 * @tparam Items The item types attached to the field spec.
 * @param writer The writer to emit output to.
 * @param fieldSpec The field spec to dump.
 */
template <typename... Items>
void
dumpFieldSpec(SpecDumpWriter& writer, FieldSpec<Items...> const& fieldSpec);

/**
 * @brief Write an entire RpcSpec (all its fields) to the dump writer.
 *
 * Fields that share a key with a later override are suppressed in favour of
 * the overriding entry (matching the runtime override-plan logic).
 *
 * @tparam Fields The field types in the spec.
 * @param writer The writer to emit output to.
 * @param spec The spec to dump.
 */
template <typename... Fields>
void
dumpRpcSpec(SpecDumpWriter& writer, RpcSpec<Fields...> const& spec);

template <typename Item>
void
dumpItem(SpecDumpWriter& writer, Item const& item)
{
    if constexpr (HasSubFields<Item>)
    {
        writer.bulletGroup(Item::kName, [&] {
            std::apply(
                [&](auto const&... sf) { (dumpFieldSpec(writer, sf), ...); }, item.subFields);
        });
    }
    else if constexpr (HasSubItems<Item>)
    {
        writer.bulletGroup(Item::kName, [&] {
            if constexpr (HasDescribeParams<Item, SpecDumpWriter>)
                item.describeParams(writer);
            std::apply([&](auto const&... it) { (dumpItem(writer, it), ...); }, item.subItems);
        });
    }
    else if constexpr (HasWrapped<Item>)
    {
        writer.bulletGroup(Item::kName, [&] {
            auto const msg = item.message();
            if (not msg.empty())
                writer.param("message", msg);
            dumpItem(writer, item.wrapped());
        });
    }
    else if constexpr (HasKName<Item>)
    {
        if constexpr (HasDescribeParams<Item, SpecDumpWriter>)
        {
            writer.bulletGroup(Item::kName, [&] { item.describeParams(writer); });
        }
        else
        {
            writer.bullet(Item::kName, [] {});
        }
    }
    else
    {
        writer.bullet("custom", [] {});
    }
}

template <typename... Items>
void
dumpFieldSpec(SpecDumpWriter& writer, FieldSpec<Items...> const& fieldSpec)
{
    writer.bulletGroup(fieldSpec.key, [&] {
        std::apply([&](auto const&... it) { (dumpItem(writer, it), ...); }, fieldSpec.items);
    });
}

namespace impl {

/**
 * @brief Write the spec's fields to @p w, honouring last-wins key overrides.
 *
 * @param writer The writer receiving the schema output.
 * @param spec The spec to render.
 * @param seq Index sequence over the spec's fields.
 */
template <typename... Fields, std::size_t... Is>
void
dumpRpcSpec(SpecDumpWriter& writer, RpcSpec<Fields...> const& spec, std::index_sequence<Is...> seq)
{
    forEachEffectiveField(
        spec.fields,
        [&](auto const& fields, auto idx) { dumpFieldSpec(writer, std::get<idx()>(fields)); },
        seq);
}

}  // namespace impl

template <typename... Fields>
void
dumpRpcSpec(SpecDumpWriter& writer, RpcSpec<Fields...> const& spec)
{
    impl::dumpRpcSpec(writer, spec, std::index_sequence_for<Fields...>{});
}

}  // namespace rpc::spec
