# Standalone unit tests. They run against the rippled (xrpl::) backend
# (RPCSPEC_IS_RIPPLED), but the small libxrpl protocol surface the DSL references
# is mocked in tests/stubs, so there is no dependency on libxrpl — only gtest and
# Boost::json (the latter via rpcspec::rpcspec).
find_package(GTest REQUIRED)

add_executable(rpcspec_tests RpcSpec.cpp)

target_link_libraries(
    rpcspec_tests
    PRIVATE
        rpcspec::rpcspec
        GTest::gtest_main
)

# tests/stubs is searched first, so #include <xrpl/...> resolves to the mock
# headers instead of a real libxrpl.
target_include_directories(rpcspec_tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

# Select the rippled backend even for a plain `cmake -Drpcspec_tests=ON` build
# without the Conan toolchain.
target_compile_definitions(rpcspec_tests PRIVATE RPCSPEC_IS_RIPPLED=1)

include(GoogleTest)
gtest_discover_tests(rpcspec_tests)
