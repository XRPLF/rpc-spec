# Standalone unit tests. They run against the rippled (xrpl::) backend
# (RPCSPEC_IS_XRPLD), but the small libxrpl protocol surface the DSL references
# is mocked in tests/stubs, so there is no dependency on libxrpl — only gtest and
# Boost::json (the latter via rpcspec::rpcspec).
find_package(GTest REQUIRED)

add_executable(
    rpcspec_tests
    admissionspec/AdmissionSpecTests.cpp
    admissionspec/ConnectionLimiterTests.cpp
    admissionspec/ResolverTests.cpp
    admissionspec/TokenBucketTests.cpp
    admissionspec/TypesTests.cpp
    RpcSpec.cpp
    StatusPrinter.cpp
    SpecCoreTests.cpp
    SpecFieldViewTests.cpp
    SpecLedgerEntryTests.cpp
    SpecLedgerTests.cpp
    SpecMockBackendTests.cpp
    SpecModifierTests.cpp
    SpecSubscribeTests.cpp
    SpecValidatorTests.cpp
)

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
target_compile_definitions(rpcspec_tests PRIVATE RPCSPEC_IS_XRPLD=1)

add_executable(rpcspec_clio_tests SpecServerConditionalTests.cpp StatusPrinter.cpp)
target_link_libraries(rpcspec_clio_tests PRIVATE rpcspec::rpcspec GTest::gtest_main)
target_include_directories(rpcspec_clio_tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
target_compile_definitions(rpcspec_clio_tests PRIVATE RPCSPEC_IS_CLIO=1)

include(GoogleTest)
gtest_discover_tests(rpcspec_tests)
gtest_discover_tests(rpcspec_clio_tests)
