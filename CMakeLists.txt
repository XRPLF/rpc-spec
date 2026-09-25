cmake_minimum_required(VERSION 3.20)
project(xrpl-rpc-spec VERSION 0.1.19 LANGUAGES CXX)

add_library(rpcspec INTERFACE)
add_library(rpcspec::rpcspec ALIAS rpcspec)

target_include_directories(
    rpcspec
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(rpcspec INTERFACE cxx_std_23)

# Only Boost::json is a direct dep; xrpl headers are provided by each consuming project.
find_package(Boost REQUIRED COMPONENTS json)

target_link_libraries(rpcspec INTERFACE Boost::json)

# Boost's headers ask MSVC to auto-link version-mangled library names that Conan's packages
# don't use. Link the real targets only, as xrpld does.
if(TARGET Boost::disable_autolinking)
    target_link_libraries(rpcspec INTERFACE Boost::disable_autolinking)
endif()

# The spec proper must not name a JSON library: consumers supply the view types it reads
# requests through (see include/rpcspec/backends for the one shipped implementation, and
# the ObjectViewFor customization point in Concepts.hpp). Checked here so the invariant
# survives review rather than depending on it.
file(GLOB_RECURSE rpcspec_core_headers "${CMAKE_CURRENT_SOURCE_DIR}/include/rpcspec/*.hpp")
list(FILTER rpcspec_core_headers EXCLUDE REGEX "/backends/")
foreach(header IN LISTS rpcspec_core_headers)
    file(STRINGS "${header}" rpcspec_json_hits REGEX "boost/json|boost::json")
    if(rpcspec_json_hits)
        message(
            FATAL_ERROR
            "rpcspec: ${header} names boost::json. JSON backends belong in "
            "include/rpcspec/backends/; the spec itself reads requests through the "
            "SomeObjectView/SomeFieldView concepts."
        )
    endif()
endforeach()

# Generates the per-handler explicit-instantiation TUs; see the module for the rationale.
# Conan consumers get this automatically via cmake_build_modules, so it is included here
# only for in-tree (add_subdirectory / sibling-checkout) consumers.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/RpcSpecInstantiations.cmake)

# --- Install / export (used by the Conan package) -----------------------------
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    FILES cmake/RpcSpecInstantiations.cmake cmake/Instantiate.cpp.in
    DESTINATION cmake
)
install(TARGETS rpcspec EXPORT rpcspecTargets)
install(
    EXPORT rpcspecTargets
    NAMESPACE rpcspec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpcspec
    FILE rpcspec-config.cmake
)

# --- Tests (standalone only) --------------------------------------------------
option(rpcspec_tests "Build the rpcspec unit tests" OFF)
if(rpcspec_tests)
    enable_testing()
    add_subdirectory(tests)
endif()
