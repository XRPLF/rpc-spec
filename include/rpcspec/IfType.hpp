/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/Types.hpp>

#include <string_view>
#include <tuple>

namespace rpc::spec {

/**
 * @brief Runs sub-processors only when the field's runtime type matches T.
 *
 * T may be any type accepted by FieldView::is<T>(), including the JsonObject
 * and JsonArray markers.
 *
 * Satisfies SomeModifier so it receives a mutable field view, enabling both
 * requirement and modifier sub-items. Checkers are excluded from sub-items;
 * hang them directly on the FieldSpec if conditional warning emission is
 * needed.
 */
template <typename T, SomeProcessor... SubItems>
struct IfType
{
    /**
     * @brief Identifier for this item in the schema dump ("ifType").
     */
    static constexpr std::string_view kName = "ifType";

    /**
     * @brief Schema-dump name of the JSON type this branch tests for.
     */
    static constexpr std::string_view kBranchType = typeNameOf<T>();

    /**
     * @brief Processors run only when the runtime type matches.
     */
    std::tuple<SubItems...> subItems;

    /**
     * @brief Construct a @ref IfType.
     *
     * @param items The processors to run when the runtime type matches.
     */
    consteval explicit IfType(SubItems... items) : subItems{items...}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("type", kBranchType);
    }

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify(View& fieldView) const
    {
        if (not fieldView.present() or not fieldView.template is<T>())
            return {};

        return runProcessors(subItems, fieldView);
    }
};

}  // namespace rpc::spec
