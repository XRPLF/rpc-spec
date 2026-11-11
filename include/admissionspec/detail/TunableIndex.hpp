/** @file */
#pragma once

#include <admissionspec/FixedString.hpp>

#include <cstddef>
#include <string_view>

namespace admission::spec::detail {

template <FixedString Name, typename... Tunables>
[[nodiscard]] consteval size_t
tunableIndex() noexcept
{
    size_t idx = sizeof...(Tunables);
    size_t i{};
    ((Tunables::kName == Name.view() ? static_cast<void>(idx = i) : static_cast<void>(0), ++i),
     ...);
    return idx;
}

}  // namespace admission::spec::detail
