import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout
from conan.tools.files import copy


class XrplRpcSpecConan(ConanFile):
    name = "xrpl-rpc-spec"
    version = "0.1.0"
    license = "ISC"
    author = "the XRP Ledger developers"
    url = "https://github.com/XRPLF/rpc-spec"
    description = "Consteval RPC spec DSL for XRPL — shared by Clio and xrpld"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "header-library"
    no_copy_source = True

    # The headers travel with the recipe so consumers get a real package (not a
    # source build). CMakeLists/tests are exported too for local `conan create`.
    exports_sources = (
        "include/*",
        "CMakeLists.txt",
        "tests/*",
        "LICENSE.md",
        "README.md",
    )

    # Build-time consumers (Clio, xrpld) provide their own xrpl/ripple headers;
    # the only direct dependency of the headers is Boost::json. Keep this aligned
    # with xrpld's boost version to avoid a clash when consumed there.
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
        # We only need Boost::json, so drop cobalt. Mirrors xrpld.
        "boost/*:without_cobalt": True,
    }

    def requirements(self):
        if self.options.tests:
            # Tests run against the xrpld (xrpl::) backend, but mock the small
            # libxrpl protocol surface they touch (see tests/stubs), so the only
            # real test dependency is gtest. Boost::json comes from the main
            # `requires` above.
            self.test_requires("gtest/1.17.0")

    def layout(self):
        cmake_layout(self)
        # In editable mode there is no packaged folder; point consumers at the
        # headers in the working tree so `conan editable add .` just works.
        self.cpp.source.includedirs = ["include"]

    # Header-only: the binary is identical across settings, so don't rebuild
    # per compiler/arch/build_type.
    def package_id(self):
        self.info.clear()

    # Only used for local standalone test builds (cmake -Drpcspec_tests=ON);
    # consumers never run this.
    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["rpcspec_tests"] = bool(self.options.tests)
        tc.generate()

    # Header-only: no compilation. Just copy the headers into the package.
    def package(self):
        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "LICENSE.md",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.set_property("cmake_target_name", "rpcspec::rpcspec")
        self.cpp_info.requires = ["boost::json"]
