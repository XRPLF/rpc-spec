# Standalone unit tests. Exercise the rippled (xrpl::) backend, so RPCSPEC_IS_RIPPLED
# is defined by the Conan toolchain (see conanfile.py) and libxrpl provides the
# xrpl:: protocol types the DSL references.
find_package(GTest REQUIRED)
find_package(xrpl REQUIRED)

add_executable(rpcspec_tests RpcSpec.cpp)

target_link_libraries(
    rpcspec_tests
    PRIVATE
        rpcspec::rpcspec
        xrpl::libxrpl
        GTest::gtest_main
)

# Defined here as well so a plain `cmake -Drpcspec_tests=ON` build (without the
# Conan toolchain) still selects the rippled backend.
target_compile_definitions(rpcspec_tests PRIVATE RPCSPEC_IS_RIPPLED=1)

include(GoogleTest)
gtest_discover_tests(rpcspec_tests)
