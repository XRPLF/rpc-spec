from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class XrplRpcSpecConan(ConanFile):
    name = "xrpl-rpc-spec"
    version = "0.1.0"
    license = "ISC"
    author = "the XRP Ledger developers"
    url = "https://github.com/XRPLF/rpc-spec"
    description = "Consteval RPC spec DSL for XRPL — shared by Clio and rippled"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "header-library"

    # Build-time consumers (Clio, rippled) provide their own xrpl/ripple headers;
    # the only direct dependency of the headers is Boost::json. Keep this aligned
    # with rippled's boost version to avoid a clash when consumed there.
    requires = [
        "boost/1.91.0",
    ]

    # Standalone test build only. Consumers never enable this.
    options = {
        "tests": [True, False],
    }
    default_options = {
        "tests": False,
        # boost 1.91's cobalt_io_ssl component fails package_info() unless cobalt
        # is disabled (it expects an OpenSSL-backed build we don't pull in).
        # We only need Boost::json, so drop cobalt. Mirrors rippled.
        "boost/*:without_cobalt": True,
    }

    def requirements(self):
        if self.options.tests:
            # Tests run against the rippled (xrpl::) backend, but mock the small
            # libxrpl protocol surface they touch (see tests/stubs), so the only
            # real test dependency is gtest. Boost::json comes from the main
            # `requires` above.
            self.test_requires("gtest/1.17.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["rpcspec_tests"] = bool(self.options.tests)
        # The rippled backend is the one exercised by the standalone tests.
        if self.options.tests:
            tc.preprocessor_definitions["RPCSPEC_IS_RIPPLED"] = "1"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.set_property("cmake_target_name", "rpcspec::rpcspec")
        self.cpp_info.requires = ["boost::json"]
