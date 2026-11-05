# 2D Morton square queries

| File | Square query API | Output |
| --- | --- | --- |
| [morton.cc](morton.cc) | `Morton::query(Box(lo, hi), out)` | Iterator of points |
| [mortonplus.hpp](mortonplus.hpp) | `MortonPlus::query_box(lo, hi, out)` | Reusable point buffer |
| [mortonSIMD.cc](mortonSIMD.cc) | `MortonSIMD::query(Box(lo, hi), out)` | Iterator or reusable point buffer |

The lower and upper corners are inclusive, including a zero-width square at
one coordinate. Duplicate point occurrences are preserved. These methods append
to the destination; clear a reusable buffer between independent queries.

Build from the [root CMake project](../../../README.md) with C++20 and BMI2.
The MortonPlus square path supports scalar rank comparisons on CPUs without
AVX-512. The explicit MortonSIMD executable is optional (`-DSQUARE_SIMD=ON`)
and requires AVX-512. The old circle overloads and their tests are retained as
source reference. [Morton license](../LICENSE).
