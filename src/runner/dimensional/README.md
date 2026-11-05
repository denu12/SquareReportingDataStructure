# D-dimensional competitor adapters

This folder contains dimensional benchmark drivers and competitor adapters. The standalone Morton algorithms are in `src/morton/`.

The dimensional roster has 23 entries; unsupported dimensions are recorded explicitly. All retained competitors and the scan control are enabled by default. The publication labels are **Chan (adapted)** and **STANN (adapted)**. Stable `MANUALLY_ADAPTED` machine IDs select these entries. See [manual adaptations](MANUAL-ADAPTATIONS.md), [adapter details](ESA-ADAPTERS.md), and the [ESA campaign guide](../../../docs/ESA-WORKLOADS.md).

Build on Arch with `python tools/build_ddim.py`. The CMake target `ddim_check` handles the Chan (adapted) and STANN (adapted) competitors; the build script also builds the foreign and Rust adapters. Third-party credits and licensing notices remain in the repository.

## Morton drivers

The index implementations are in [../../morton/](../../morton/README.md).
`morton_campaign.cpp` and `morton_3d_campaign.cpp` only adapt those indexes to the
binary input format, timers and independent oracle. `check_campaign.cpp` hosts
Chan (adapted) and STANN (adapted); `foreign_check.cpp` hosts C++ library adapters;
`rust/` and `python_check.py` host the other language integrations.

Use `python run_theater.py d-dim --run FRESH_LABEL --algorithms morton_d_dim --execute`
to select generic Morton for the full dimensional theater. Dimensions above 16
are recorded as unsupported. Specialized Morton3D belongs to `static-3d`.

The 3D diagnostic `esa_sprk_CHEATING_IDS_ONLY` is displayed as **SPRK (IDs only)**. Its separate Rust executable performs native f32 candidate search and exact integer membership inside timing, then materializes IDs only. Coordinates are materialized afterwards for verification. It is separate from coordinate-reporting SPRK.

The diagnostic label refers to a local difference in reporting contract, not misconduct by upstream authors. See [reporting details](../../../docs/REPORTING.md).

## Native output policy

Retained non-SPRK runners use native reporting output. Morton returns points.
Boost writes its native point objects directly into a reusable vector, using
`rtree::query` with the exact sphere predicate; conversion for the oracle occurs
after timing. SPRK configurations are unchanged; see the current
[reporting contract](../../../docs/REPORTING.md) and root README output table.
Validate Boost with `python tools/check_native_boost.py`.
