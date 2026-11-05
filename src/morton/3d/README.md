# 3D Morton

| File | Implementation |
|---|---|
| [morton3D.cc](morton3D.cc) | Morton: readable compiler-vectorized membership batches |
| [mortonSIMD3D.hpp](mortonSIMD3D.hpp) | Morton (simd): AVX-512 rank search, eight-point tests and packed xyz output |
| [mortonplus3d.hpp](mortonplus3d.hpp) | Morton (plus): prefix lookup, octree splitting, bulk reporting and AVX-512 |

Standalone C++20 includes. Use `-O3 -march=native`; plain encoding requires BMI2,
and explicit SIMD/Plus require AVX-512F (Plus also uses BMI2). All retain full
96-bit Morton keys, exact 66-bit distance sums, uint128 squared radii and duplicates.
Plain `query_squared` takes an output iterator; SIMD `query_squared` and Plus
`query` take native Results buffers. Clear reused buffers between queries.

Tests: `morton_3d_variants_test.cc` and `mortonplus3d_test.cc`.
Build with `bash tools/build_selected_3d.sh` and validate with
`python tools/check_selected_3d.py`, from the repository root. CMake targets live
in `src/runner/dimensional`. See [upgrade evidence](../../../docs/MORTON-3D-UPGRADE.md),
the parent [license](../LICENSE) and [credits](../../../docs/THIRD_PARTY.md).
