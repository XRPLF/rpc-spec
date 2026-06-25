# xrpl-rpc-spec

A header-only, `consteval` RPC specification DSL for the XRP Ledger, shared by
[Clio](https://github.com/XRPLF/clio) and [xrpld](https://github.com/XRPLF/rippled).

The DSL lets a handler declare its request parameters — types, requirements,
defaults, modifiers — as a compile-time spec. The framework validates incoming
request parameters against that spec before the handler runs, replacing
hand-written per-handler validation logic.

## Backends

Both backends resolve XRPL protocol types (`AccountID`, `LedgerEntryType`,
error codes, …) from `xrpl::` (libxrpl). The backend macro instead selects which
server the spec is compiled for, controlling the server-conditional validators
`ifServerClio(...)` / `ifServerRippled(...)` — each applies its wrapped
validators only in the matching build. Exactly one macro must be defined by the
consumer's build:

- `RPCSPEC_IS_RIPPLED=1` — rippled build; `ifServerRippled(...)` is active.
- `RPCSPEC_IS_CLIO=1` — Clio build; `ifServerClio(...)` is active.

Defining both, or neither, is a compile error (see `ServerConditional.hpp`).

## Layout

```
include/rpcspec/
  RpcSpec.hpp          # the spec container + process()
  FieldSpec.hpp        # per-field declarations
  Concepts.hpp         # SomeFieldView / SomeObjectView backend concepts
  Errors.hpp           # Status / error-code mapping
  Validators.hpp       # built-in JSON param validators
  ServerConditional.hpp # ifServerClio / ifServerRippled wrappers
  detail/              # backend type resolution + parsing (XrplParse)
  handlers/            # per-handler spec definitions (e.g. ledger)
tests/                 # standalone unit tests (rippled backend)
  stubs/               # libxrpl mock — tests need only gtest + Boost::json
```

## Consuming it

The library is distributed as a Conan `header-library` package exporting the
CMake target `rpcspec::rpcspec`. Add it to your requirements and define the
backend macro in your toolchain. Its only direct dependency is `Boost::json`;
the XRPL protocol headers come from your project.

## Building the tests

The standalone tests run against the rippled (`xrpl::`) backend, but the small
libxrpl protocol surface the DSL references is mocked in `tests/stubs` — so the
only test dependencies are `gtest` and `Boost::json` (no libxrpl, no Conan
remote beyond the defaults).

### With Conan (recommended)

Conan provides both dependencies and generates the CMake presets. The `tests`
option also wires up `RPCSPEC_IS_RIPPLED=1` and `rpcspec_tests=ON` in the
generated toolchain, so no extra `-D` flags are needed:

```sh
# 1. Install deps and generate the toolchain + presets. The generated preset is
#    named after the build type, so pin it explicitly to get `conan-release`
#    (a plain `conan install` follows your profile's default — often Debug,
#    which yields `conan-debug` instead).
conan install . -o tests=True -s build_type=Release --build=missing

# 2. Configure, build, and run. (Use `conan-debug` for a Debug install;
#    run `cmake --list-presets` if unsure which presets exist.)
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
```

### Plain CMake

If `gtest` and `Boost::json` are already discoverable by `find_package`
(e.g. installed system-wide), skip Conan and drive CMake directly. Here
`-Drpcspec_tests=ON` is required, and the backend macro is set automatically for
the test target:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Drpcspec_tests=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

[ISC](LICENSE.md)
