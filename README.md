# Square Reporting Data Structure

A square range reporting benchmark derived from
[DiskReportingDataStructure](https://github.com/denu12/DiskReportingDataStructure).
Given a static set of integer points and a closed, axis-aligned square, report
every point in the square. The same runner supports closed cubes in 3D.

An answer can be a coordinate tuple or the original input ID. Each input record
counts, including duplicates. Native box reporters receive the query bounds and
write their usual output; they do no distance filtering or timed conversion
between points and IDs. Correctness mode checks the entire returned ID set or
point multiset against an exhaustive scan outside the query timer.

## Build and run

Use Linux or WSL, Python 3, CMake, and a C++20 GCC or Clang compiler on an x86-64
CPU with BMI2. Boost, CGAL and libspatialindex development packages enable their
respective backends. On Debian/Ubuntu these are `libboost-dev`, `libcgal-dev`, and
`libspatialindex-dev`.

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure

mkdir -p data
python3 tools/make_case.py data/example.bin --n 10000 --queries 100 --target-m 100
python3 run_theater.py --list
python3 run_theater.py data/example.bin --algorithms morton morton_plus boost_rstar indexicon_rtree_point
```

Use `--dimension 3` in `make_case.py` for cubes. `run_theater.py --bench
--repetitions 3 --cpu 1` runs the selected backends serially on CPU 1 without
the exhaustive oracle. Run correctness checks first. Each JSON line records
`n` input records, `queries`, `m` total reported records, build and query times
in seconds, and the native output type. The actual average result size is
`m / queries`; `--target-m` only controls the generated side length.

## Competitors and query paths

The runner includes Morton and MortonPlus in 2D and 3D, Boost R-tree variants,
CGAL, THST, PH-tree, SpaceTreeLib, libspatialindex, Indexicon, PGM, nanoflann,
and an exhaustive scan control. `--list` shows dimensions and built backends.
Indexicon uses all nine applicable 2D variants and all six applicable 3D
variants. `-DSQUARE_SIMD=ON` additionally builds the inherited 2D AVX-512
Morton variant; run that executable only on a host that supports AVX-512.

- **Native box:** Morton and MortonPlus search their Morton intervals against
  the square or cube. The rectangle-capable competitors use their own box API.
  SpaceTreeLib's native output buffer is consumed directly.
- **Coarse box then exact box filter:** PGM retains the inherited coordinate
  mapping and iterator fixes. Points on a coarse boundary are checked against
  the original integer bounds so the full-width query stays exact.
- **Enclosing radius then box filter:** The pinned nanoflann header exposes a
  radius query. Its adapter queries a conservative enclosing radius, then
  checks the square or cube; the extra work is included in its timing.

The disk project's radius-only competitors and dynamic experiments are not
automatically carried over. Its source snapshot is retained for provenance;
the top-level CMake build and `run_theater.py` are the square benchmark. This
repository does not include the old disk measurements, prepared inputs, private
research files, or recovered source with unresolved redistribution permission.

## Input format and provenance

`SQUARE1` is little endian: eight bytes `SQUARE1\0`, a `uint32` dimension,
`uint64 n`, `uint64 q`, followed by `n × dimension` unsigned 32-bit coordinates.
Each query is a lower corner then an upper corner in the same format. Bounds
are inclusive and side lengths must match across axes. Zero-width and
full-width queries are valid. The runner rejects the old disk case format.

Based on DiskReportingDataStructure commit `aac46efa2c4b58a6439aaaa5c0767546529d8402`.
The Indexicon headers are pinned at `69da8a364f3437a1110d89b9ebeb99473dc25807`;
THST headers are pinned at `4ff6aa575f87dce4321754981314a923b0b4bbb6`.
Submodule pins and bundled license files identify the other source versions.
