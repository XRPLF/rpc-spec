# Vendored folly TokenBucket

`TokenBucket.h` in this directory is vendored from
[folly](https://github.com/facebook/folly) (`folly/TokenBucket.h`) so the
admission rate limiter can use folly's token-bucket primitive without taking a
dependency on the rest of the Folly library.

- **Upstream:** folly `folly/TokenBucket.h`
- **License:** Apache License 2.0 — see [LICENSE](LICENSE). Folly ships no
  `NOTICE` file. The Apache LICENSE upstream also contains an MIT appendix that
  applies only to `folly/external/farmhash`, which is not vendored here, so that
  appendix has been omitted.
- **Modifications:** The token-bucket logic is unchanged. Only folly-internal
  utilities the header relied on were inlined so the file is self-contained:
  - `folly::constexpr_min` / `folly::constexpr_max` → `std::min` / `std::max`
  - `folly::Optional` / `folly::none` → `std::optional` / `std::nullopt`
  - `FOLLY_UNLIKELY` → a file-local macro
  - `hardware_destructive_interference_size` → a local `constexpr` constant

See the header comment in `TokenBucket.h` for details.
