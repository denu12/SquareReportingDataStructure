# 2D Morton

| File | Implementation | Compiler flags |
|---|---|---|
| [morton.cc](morton.cc) | Morton: 32-point ordinary C++ membership batches and iterator output | `-O3 -march=native` (BMI2) |
| [mortonSIMD.cc](mortonSIMD.cc) | Morton (simd): explicit vector filtering and packed output | `-O3 -mavx512f -mbmi2` |
| [mortonplus.hpp](mortonplus.hpp) | Morton (plus): prefix lookup, splitting, bulk reporting and SIMD | `-O3 -mavx512f -mbmi2` |

All retain exact closed-ball membership on uint32 coordinates, 65-bit distance
sums, duplicate points and uint128 squared radii. Plain Morton accepts an output
iterator; SIMD and Plus provide native output buffers. Clear a reused buffer
between independent queries. The source files are standalone C++20 includes.

Tests: `morton_test.cc`, `morton_simd_batch_test.cc`, `mortonplus_test.cc`.
Build campaign backends with `bash tools/build_selected_2d.sh` from the repository
root; validate with `python tools/check_selected_2d.py`.
See the parent [license](../LICENSE) and [credits](../../../docs/THIRD_PARTY.md).
