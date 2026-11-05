# 3D Morton cube queries

| File | Cube query API | Output |
| --- | --- | --- |
| [morton3D.cc](morton3D.cc) | `Morton3D::query_box(lo, hi, out)` | Iterator of point triples |
| [mortonplus3d.hpp](mortonplus3d.hpp) | `MortonPlus::query_box(lo, hi, out)` | Reusable point buffer |

Both preserve all 96 Morton bits and use closed bounds on the original uint32
coordinates. Each input occurrence is reported. MortonPlus can refine octree
cells and bulk-copy cells contained in the cube. Clear reused output buffers
between queries.

The [root CMake build](../../../README.md) enables these backends with C++20
and BMI2. [mortonSIMD3D.hpp](mortonSIMD3D.hpp) retains the original AVX-512
ball reporter for reference; it is not a cube backend. [Morton license](../LICENSE).
