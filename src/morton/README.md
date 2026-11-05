# Morton implementations

The retained standalone C++20 implementations and their tests are grouped by dimension.

| Folder | Retained implementations |
|---|---|
| [2d](2d/README.md) | Morton, Morton (simd), Morton (plus) |
| [3d](3d/README.md) | Morton, Morton (simd), Morton (plus) |
| [d_dim](d_dim/README.md) | Morton and Morton (plus), for 1–16 dimensions |
| [dynamic](dynamic/README.md) | No active dynamic Morton implementation |

Each implementation is a self-contained include file, including files with historical
`.cc` extensions. Test files are stored beside their implementation. The parent
BUILD preserves the public Bazel target `//morton:morton` and existing test names.

Plain 2D/3D Morton uses ordinary C++ membership loops that the compiler can vectorize.
The explicit SIMD and Plus variants use AVX-512; d-dimensional variants use AVX2.
Use GCC/Clang with C++20 and unsigned __int128 support. See each folder for flags.

Plus derives from the ESA authors' MortonPlus, with the Rust implementation by
Dennis Kobert; see [credits](../../docs/THIRD_PARTY.md). Keep [LICENSE](LICENSE)
with redistributed source. The 2D Plus source hash lock is unchanged by this move.

Deprecated scalar/ablation and superseded 3D source copies have been removed.
Historical measurements and lineage records remain; do not pool historical Morton
measurements with the promoted compiler-vectorized implementation.
