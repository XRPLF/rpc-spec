cmake_minimum_required(VERSION 3.20)
project(xrpl-rpc-spec VERSION 0.1.0 LANGUAGES CXX)

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

# --- Install / export (used by the Conan package) -----------------------------
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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
