/** @file */
#pragma once

#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/SpecDumpWriter.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace rpc::spec {

/**
 * @brief Detects spec items that own a `subFields` member (e.g. nested object validators).
 * @tparam T The type to check.
 */
template <typename T>
concept HasSubFields = requires(T const &t) { t.subFields; };

/**
 * @brief Detects spec items that own a `subItems` member (e.g. Section, OneOf).
 * @tparam T The type to check.
 */
template <typename T>
concept HasSubItems = requires(T const &t) { t.subItems; };

/**
 * @brief Detects spec items that expose a `wrapped()` accessor (e.g. WithMessage).
 * @tparam T The type to check.
 */
template <typename T>
concept HasWrapped = requires(T const &t) {
  { t.wrapped() };
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
    requires(T const &t, Writer &w) { t.describeParams(w); };

/**
 * @brief Write a single spec item to the dump writer.
 *
 * Dispatches on the item's capabilities (HasSubFields, HasSubItems, HasWrapped,
 * HasKName, HasDescribeParams) and emits an appropriate YAML-ish bullet entry.
 *
 * @tparam Item The spec item type.
 * @param w The writer to emit output to.
 * @param item The item to dump.
 */
template <typename Item> void dumpItem(SpecDumpWriter &w, Item const &item);

/**
 * @brief Write a FieldSpec (one named field and all its attached items) to the dump writer.
 *
 * @tparam Items The item types attached to the field spec.
 * @param w The writer to emit output to.
 * @param f The field spec to dump.
 */
template <typename... Items>
void dumpFieldSpec(SpecDumpWriter &w, FieldSpec<Items...> const &f);

/**
 * @brief Write an entire RpcSpec (all its fields) to the dump writer.
 *
 * Fields that share a key with a later override are suppressed in favour of
 * the overriding entry (matching the runtime override-plan logic).
 *
 * @tparam Fields The field types in the spec.
 * @param w The writer to emit output to.
 * @param spec The spec to dump.
 */
template <typename... Fields>
void dumpRpcSpec(SpecDumpWriter &w, RpcSpec<Fields...> const &spec);

template <typename Item> void dumpItem(SpecDumpWriter &w, Item const &item) {
  if constexpr (HasSubFields<Item>) {
    w.bulletGroup(Item::kName, [&] {
      std::apply([&](auto const &...sf) { (dumpFieldSpec(w, sf), ...); },
                 item.subFields);
    });
  } else if constexpr (HasSubItems<Item>) {
    w.bulletGroup(Item::kName, [&] {
      if constexpr (HasDescribeParams<Item, SpecDumpWriter>)
        item.describeParams(w);
      std::apply([&](auto const &...it) { (dumpItem(w, it), ...); },
                 item.subItems);
    });
  } else if constexpr (HasWrapped<Item>) {
    w.bulletGroup(Item::kName, [&] {
      auto const msg = item.message();
      if (!msg.empty())
        w.param("message", msg);
      dumpItem(w, item.wrapped());
    });
  } else if constexpr (HasKName<Item>) {
    if constexpr (HasDescribeParams<Item, SpecDumpWriter>) {
      w.bulletGroup(Item::kName, [&] { item.describeParams(w); });
    } else {
      w.bullet(Item::kName, [] {});
    }
  } else {
    w.bullet("custom", [] {});
  }
}

template <typename... Items>
void dumpFieldSpec(SpecDumpWriter &w, FieldSpec<Items...> const &f) {
  w.bulletGroup(f.key, [&] {
    std::apply([&](auto const &...it) { (dumpItem(w, it), ...); }, f.items);
  });
}

namespace impl {

template <typename... Fields, std::size_t... Is>
void dumpRpcSpec(SpecDumpWriter &w, RpcSpec<Fields...> const &spec,
                 std::index_sequence<Is...>) {
  if constexpr (sizeof...(Is) == 0) {
    return;
  } else {
    using FieldsTuple = typename RpcSpec<Fields...>::FieldsTuple;
    constexpr auto kN = sizeof...(Is);
    std::array<std::string_view, kN> const keys{
        std::get<Is>(spec.fields).key...};
    auto const plan = buildOverridePlan(keys);

    using DumpFn = void (*)(SpecDumpWriter &, FieldsTuple const &);
    static constexpr std::array<DumpFn, kN> kDISPATCH{
        +[](SpecDumpWriter &wr, FieldsTuple const &t) {
          dumpFieldSpec(wr, std::get<Is>(t));
        }...};

    for (std::size_t i = 0; i < kN; ++i) {
      if (!plan.shouldRun[i])
        continue;
      kDISPATCH[plan.effectiveIdx[i]](w, spec.fields);
    }
  }
}

} // namespace impl

template <typename... Fields>
void dumpRpcSpec(SpecDumpWriter &w, RpcSpec<Fields...> const &spec) {
  impl::dumpRpcSpec(w, spec, std::index_sequence_for<Fields...>{});
}

} // namespace rpc::spec
