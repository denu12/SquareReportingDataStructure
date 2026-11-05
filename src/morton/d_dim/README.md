# D-dimensional Morton

- [morton_d_dim.cc](morton_d_dim.cc): generic Morton for 1–16 dimensions.
- [mortonplus_d_dim.hpp](mortonplus_d_dim.hpp): Morton (plus), with prefix lookup,
  cell pruning and bulk reporting.

Both are standalone C++20 includes using AVX2. Compile with `-O3 -mavx2`.
Their tests are `morton_d_dim_test.cc` and `mortonplus_d_dim_test.cc`.
CMake builds the indexes and tests from `src/runner/dimensional`.
The higher-dimensional campaign excludes the specialized 3D implementations.
See the parent [license](../LICENSE) and [credits](../../../docs/THIRD_PARTY.md).
