# xrpl-rpc-spec

A header-only, `consteval` RPC specification DSL for the XRP Ledger, shared by
[Clio](https://github.com/XRPLF/clio) and [xrpld](https://github.com/XRPLF/rippled).

The DSL lets a handler declare its request parameters — types, requirements,
defaults, modifiers — as a compile-time spec. The framework validates incoming
request parameters against that spec before the handler runs, replacing
hand-written per-handler validation logic.

## Backends

The library is backend-agnostic at its core but resolves XRPL protocol types
(`AccountID`, `LedgerEntryType`, error codes, …) from the consuming project.
Exactly one backend macro must be defined by the consumer's build:

- `RPCSPEC_IS_RIPPLED=1` — resolves types from `xrpl::` (libxrpl).
- `RPCSPEC_IS_CLIO=1` — resolves types from Clio's `ripple::` namespace.

Defining both, or neither, is a compile error (see `ServerConditional.hpp`).

## Layout

```
include/rpcspec/
  RpcSpec.hpp        # the spec container + process()
  FieldSpec.hpp      # per-field declarations
  Concepts.hpp       # SomeFieldView / SomeObjectView backend concepts
  Errors.hpp         # Status / error-code mapping
  Validators.hpp     # Clio-specific JSON param validators
  detail/            # backend type resolution (XrplNs, XrplParse)
  handlers/          # per-handler spec definitions (e.g. ledger)
tests/               # standalone unit tests (rippled backend)
```

## Consuming it

The library is distributed as a Conan `header-library` package exporting the
CMake target `rpcspec::rpcspec`. Add it to your requirements and define the
backend macro in your toolchain. Its only direct dependency is `Boost::json`;
the XRPL protocol headers come from your project.

## Building the tests

Tests exercise the rippled backend and require libxrpl (from the `xrplf` Conan
remote, `https://conan.ripplex.io`):

```sh
conan install . -o tests=True --build=missing
cmake --preset conan-release -Drpcspec_tests=ON
cmake --build --preset conan-release
ctest --preset conan-release
```

## License

[ISC](LICENSE.md)
