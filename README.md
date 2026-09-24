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
`ifServerClio(...)` / `ifServerXrpld(...)` — each applies its wrapped
validators only in the matching build.

Consumers pick a backend through the package's `server` Conan option, which the
package turns into exactly one macro:

| `server` option | macro             | active wrapper       |
| --------------- | ----------------- | -------------------- |
| `xrpld`         | `RPCSPEC_IS_XRPLD=1` | `ifServerXrpld(...)` |
| `clio`          | `RPCSPEC_IS_CLIO=1`  | `ifServerClio(...)`  |

The option has no default, so leaving it unset fails the Conan graph rather than
producing a build with neither macro. Defining both, or neither, by hand is a
compile error (see `ServerConditional.hpp`).

## Layout

```
include/rpcspec/
  Concepts.hpp          # SomeFieldView / SomeObjectView / ObjectViewFor / item concepts
  Errors.hpp            # Status, error codes, shared error messages

  backends/
    BoostJson.hpp       # the boost::json backend + its wire-format helpers

  RpcSpec.hpp           # validate-only spec container + process()/check()
  FieldSpec.hpp         # per-field declarations for RpcSpec
  Typed.hpp             # TypedSpec: validate AND parse into a handler Input
  Converters.hpp        # typed converters used by TypedSpec fields
  Aliases.hpp           # the DSL surface (required, type<T>, clamp, oneOf, …)

  Validators.hpp        # built-in JSON param validators
  Section.hpp           # nested-object validation
  IfType.hpp            # run sub-items only for a given runtime JSON type
  WithCustomError.hpp   # override a wrapped item's error
  ServerConditional.hpp # ifServerClio / ifServerXrpld + kIsClioBuild flags

  Ledger.hpp            # ledger_hash/ledger_index -> LedgerSpecifier
  LedgerTypes.hpp       # ledger object type registry
  TxTypes.hpp           # transaction type-name registry
  JsonBool.hpp          # lenient V1-API bool

  VersionedSpec.hpp     # one spec per API version + selection
  HandlerFor.hpp        # the per-handler entry points (parseInput / spec)
  HandlerForDefs.hpp    # their out-of-line definitions (instantiation TUs only)
  RpcSpecView.hpp       # type-erased view over any spec

  SpecDump.hpp          # schema dumper
  SpecDumpWriter.hpp    # its YAML-ish output writer

  detail/               # backend type resolution + parsing (XrplParse)
  handlers/             # per-handler Types.hpp + Spec.hpp (e.g. ledger)

include/admissionspec/  # independent admission-control/rate-limiting DSL
  folly/                # vendored folly TokenBucket (not linted or formatted)

cmake/                  # generator for the per-handler instantiation TUs
tests/                  # standalone unit tests
  stubs/                # libxrpl mock — tests need only gtest + Boost::json
```

Tests build as two executables because the backend macros are mutually exclusive
within one binary: `rpcspec_tests` (xrpld backend) covers the bulk, and
`rpcspec_clio_tests` covers the Clio-only branches.

## Consuming it

The library is distributed as a Conan `header-library` package exporting the
CMake target `rpcspec::rpcspec`. Add it to your requirements and select a
backend with the `server` option:

```python
requires = ["xrpl-rpc-spec/<version>"]
default_options = {"xrpl-rpc-spec/*:server": "clio"}  # or "xrpld"
```

The backend macro rides on `rpcspec::rpcspec` as an interface compile
definition, so only the CMake targets that link the DSL see it — consumers do
not add a global define of their own. Its only direct dependency is
`Boost::json`; the XRPL protocol headers come from your project.

## Local development (editable package)

When hacking on the DSL while building a consumer (Clio or xrpld) against it,
register this repo as an **editable** Conan package. Consumers that require
`xrpl-rpc-spec/<version>` then resolve to your working tree instead of the Conan
cache, so header edits are picked up on the consumer's next build — no
`conan export`/`conan create` round-trip.

```sh
# From this repo's root — registers xrpl-rpc-spec/<version> → this working copy.
# (name + version come from the conanfile.)
conan editable add .

# Verify it's registered.
conan editable list        # -> xrpl-rpc-spec/<version>  Path: .../xrpl-rpc-spec

# Now build the consumer as usual; its `conan install` resolves the requirement
# to this folder. Edit headers here, rebuild the consumer, changes apply.

# When done, revert to the cached/remote package.
conan editable remove .    # or: conan editable remove -r xrpl-rpc-spec/<version>
```

The recipe's `layout()` exposes `include/` as the include dir in editable mode,
so consumers find the headers directly in the source tree (no packaging step).
After `conan editable remove`, consumers fall back to the cached package, so make
sure one is available (`conan create .`) or re-export as needed.

## Building the tests

The standalone tests run against the xrpld (`xrpl::`) backend, but the small
libxrpl protocol surface the DSL references is mocked in `tests/stubs` — so the
only test dependencies are `gtest` and `Boost::json` (no libxrpl, no Conan
remote beyond the defaults).

### With Conan (recommended)

Conan provides both dependencies and generates the CMake presets. The `tests`
option also wires up `RPCSPEC_IS_XRPLD=1` and `rpcspec_tests=ON` in the
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
