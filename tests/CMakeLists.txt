# Standalone unit tests. They run against the xrpld (xrpl::) backend
# (RPCSPEC_IS_XRPLD), but the small libxrpl protocol surface the DSL references
# is mocked in tests/stubs, so there is no dependency on libxrpl — only gtest and
# Boost::json (the latter via rpcspec::rpcspec).
find_package(GTest REQUIRED)

add_executable(
    rpcspec_tests
    admissionspec/ConnectionLimiterTests.cpp
    admissionspec/ResolverTests.cpp
    admissionspec/TokenBucketTests.cpp
    admissionspec/TypesTests.cpp
    SpecAccountLinesTests.cpp
    SpecAccountObjectsTests.cpp
    SpecAccountOffersTests.cpp
    SpecAccountTxDelegateTests.cpp
    SpecAmmInfoTests.cpp
    SpecBookOffersTests.cpp
    SpecCoreTests.cpp
    SpecCoreTypesTests.cpp
    SpecDefaultTests.cpp
    SpecDepositAuthorizedTests.cpp
    SpecFieldViewTests.cpp
    SpecGatewayBalancesTests.cpp
    SpecGetAggregatePriceTests.cpp
    SpecJsonBoolTests.cpp
    SpecLedgerDataTests.cpp
    SpecLedgerEntryAliasTests.cpp
    SpecLedgerEntryTests.cpp
    SpecLedgerIndexTests.cpp
    SpecLedgerTests.cpp
    SpecMockBackendTests.cpp
    SpecModifierTests.cpp
    SpecMPTHoldersTests.cpp
    SpecMPTokenIssuanceHistoryTests.cpp
    SpecNftHistoryTests.cpp
    SpecNoRippleCheckTests.cpp
    SpecSubscribeTests.cpp
    SpecValidatorTests.cpp
    SpecVaultInfoTests.cpp
    StatusPrinter.cpp
)

target_link_libraries(
    rpcspec_tests
    PRIVATE
        rpcspec::rpcspec
        GTest::gtest_main
)

# tests/stubs is searched first, so #include <xrpl/...> resolves to the mock
# headers instead of a real libxrpl.
target_include_directories(
    rpcspec_tests
    BEFORE
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${CMAKE_CURRENT_SOURCE_DIR}
)

# Select the xrpld backend even for a plain `cmake -Drpcspec_tests=ON` build
# without the Conan toolchain.
target_compile_definitions(rpcspec_tests PRIVATE RPCSPEC_IS_XRPLD=1)

add_executable(
    rpcspec_clio_tests
    SpecClioErrorsTests.cpp
    SpecClioHandlerErrorsTests.cpp
    SpecLedgerEntryAliasTests.cpp
    SpecServerConditionalTests.cpp
    StatusPrinter.cpp
)
target_link_libraries(rpcspec_clio_tests PRIVATE rpcspec::rpcspec GTest::gtest_main)
target_include_directories(
    rpcspec_clio_tests
    BEFORE
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rpcspec_clio_tests PRIVATE RPCSPEC_IS_CLIO=1)

include(GoogleTest)
gtest_discover_tests(rpcspec_tests)
gtest_discover_tests(rpcspec_clio_tests TEST_PREFIX "Clio.")
