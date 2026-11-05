# Source code

Our standalone Morton implementations and tests live in [morton/](morton/README.md),
organized into `2d`, `3d`, `d_dim`, and `dynamic`. Each retained implementation
can be included directly in a C++20 client without the experiment harness.

| Location | Purpose |
|---|---|
| [morton/2d/](morton/2d/README.md) | Morton, SIMD and Plus for 2D. |
| [morton/3d/](morton/3d/README.md) | Morton, SIMD and Plus for 3D. |
| [morton/d_dim/](morton/d_dim/README.md) | Morton and Plus for 1–16 dimensions. |
| [morton/dynamic/](morton/dynamic/README.md) | Reserved; no active dynamic Morton. |
| [app/](app/) | Native entry points and algorithm registration. |
| [app/algorithms/static/](app/algorithms/static/) | Static adapters, including separate wrappers for our 2D indexes. |
| [app/algorithms/dynamic/reporting/](app/algorithms/dynamic/reporting/) | Dynamic competitor adapters. |
| [benchmark/](benchmark/) | Shared point/query types, integer geometry, timing and correctness oracle. |
| [runner/](runner/) | Current dimensional benchmark drivers and adapters. |
| [third_party/](third_party/README.md) | Vendored libraries and imported workload-generation code. |
| [utils/](utils/) | Shared logging and random-generation utilities. |

Bazel builds the 2D harness. CMake, direct C++ compilation and Cargo build the
dimensional drivers. Use the root build scripts documented in
[RUNNING.md](../docs/RUNNING.md). The public experiment command is
`python run_theater.py THEATER`; the standalone indexes do not run experiments.

MortonPlus is now included as `morton/2d/mortonplus.hpp`, credited to the ESA
authors; `runner/morton_2d_campaign.cpp` runs the retained 2D Morton variants.
Their exact Rust SPRK campaign driver is
`third_party/esa2026/sprk/examples/campaign.rs`.
