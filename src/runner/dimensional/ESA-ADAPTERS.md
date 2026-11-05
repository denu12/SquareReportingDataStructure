## Current SPRK scope

Our old dimensional exact SPRK wrapper has been removed. The authors' 2D
IntegerPointSprk now has a local exact 3D extension, `esa_sprk_RUST_EXACT_3D`.
`MortonPlus3D` extends the C++ MortonPlus port to 3D. Both materialize original
integer coordinates and support only dimension 3. The original IDs-only SPRK
driver and other dimensional competitors remain available. See
[version and validation notes](../../../docs/MORTONPLUS-VERSIONS.md).

# Dimensional correctness interfaces

These adapters support the current dimensional correctness and timing harness; they do not reproduce the source paper's measurement protocol. The current rosters are recorded in the [d-dimensional theater definitions](../../../theaters/d-dim/README.md).
Complete original-index lists (including duplicates) are compared with an
exact integer scan. A first mismatch excludes an entry; competitor search
implementations are not repaired after failures.

* Floating backends receive integer coordinates divided by 2^32 and
  sqrt(integer squared radius)/2^32, rounded to their native float type.
  No tolerance or result correction is applied. Strict/open radius APIs,
  rounded boundary distances, duplicate handling and cancellation can fail
  the closed, exact integer contract. Such failures do not by themselves
  establish errors relative to a library's own documented contract.
* SPRK (original) uses the historical
  f64 width-8 tree, floating-point radius search and ID output, omitting the
  integer filter and coordinate output; admission checks its declared floating
  contract and records integer disagreements separately. Kiddo, Nabo, Neighbourhood, VP-tree,
  grid and Orthtree reuse the existing dimensional Rust modules, including
  previously disclosed local modifications.
* Appendix G Rust SNN imports rembed `src/snn.rs` unchanged, retaining f32 and
  its internal tolerance. Minimal point/graph interfaces supply coordinates
  and IDs; its construction and search are unchanged.
* Python SNN and scikit-learn's trees use their native radius APIs.
* ANN uses the Euclidean build and exact annkFRSearch (epsilon zero), first
  counting and then retrieving answers. No true output count is supplied.
* nanoflann uses its native squared-radius API and runtime dimension.
* CGAL uses Cartesian_d<double>, Kd_tree and Fuzzy_sphere with epsilon zero.
  This differs from the paper's Epick_d wrapper. Returned duplicate points
  are mapped to distinct original IDs without correcting membership.
* Boost linear/quadratic/R-star use bulk construction, capacity 16, bounding
  box queries and exact integer filtering to implement ball reporting.
* Grid retains normalized width 0.001 instead of the paper's mean-radius
  hint. This can increase allocation costs; failures are recorded.
* Brute force uses Python integers for correctness, not competitive timing.

Processes are pinned to one CPU; OpenMP, BLAS and Rayon thread limits are one.
Performance runs separate construction and coordinate-reporting query time; retain the run identity when interpreting them. Sources and credits remain in `docs/THIRD_PARTY.md`
and the existing vendored notices. Chan and STANN have separate manual-change
records in `MANUAL-ADAPTATIONS.md`.

Reporting contract (2026-10-07): all timed dimensional drivers explicitly
materialize the original uint32 coordinates, including the manual
Chan/STANN entries. ID lookup and coordinate writes are inside query timing;
correctness compares coordinate multisets. The Rust SPRK bridge writes directly
to a reusable ID buffer. Existing native floating predicates are not repaired
or post-filtered to conceal incorrect competitor answers. Historical ID-only
timings are not comparable with this reporting revision.

The original IDs-only dimensional diagnostic is an explicit exception to coordinate reporting. See [the output contract](../../../docs/REPORTING.md).

Publication labels and modification categories are specified in the [adapter catalog](../../../docs/ADAPTERS.md). Use **STANN (adapted)** and **Chan (adapted)** consistently across dimensions; machine IDs remain unchanged.

Adapter revision (2026-10-09): nanoflann (adapted) builds once and reuses
unsorted query buffers, writing coordinates without an intermediate ID vector.
Boost (adapted) checks exact sphere membership on tree-resident coordinates and
reports them directly, without bounding-box hit or ID vectors. These changes
remove local adapter work; they do not repair upstream search implementations.
