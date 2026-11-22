# Repository audit — 22 November 2026

Audited the published square baseline `72975eb`, its November 22 results and an
existing uncommitted rectangle integration in the original local working copy.
That integration and active Archie jobs were not modified by this audit.

## Source-integration follow-up

The source and scheduling gaps below have been addressed after the audit:

- PAM, ParGeo and PKD are integrated into the public runner and default CMake
  build. Their captured dependency headers, base revisions, file hashes and
  license notices are published under [rectangle](../src/third_party/rectangle/README.md).
- ParGeo's local rectangle traversal is now labelled `local_box_traversal` in
  new output. Historical raw labels are retained.
- A build-only legacy Parlay API qualification leaves captured headers and
  index search implementations unchanged.
- Timeout kills include descendant processes and suppress remaining repeats.
  Malformed JSON/backend errors are recorded without aborting other algorithms.
  Incorrect outputs remain explicitly timed when requested.

The baseline findings below are retained as audit history, not current missing-source claims.

## Source-integration validation

- The Release CMake build now compiles PAM, ParGeo and PKD, plus PH-tree, PGM
  and PSI using the pinned submodules supplied by the companion disk worktree.
- 171 exact square/cube checks passed across the built non-AVX-512 backends.
- Three real-process runner tests passed: timeout descendant cleanup and repeat
  suppression; malformed-JSON isolation; and continued timing of incorrect output.
- All 197 captured rectangle dependency files have recorded hashes and retained
  notices. The Pargeo build compatibility copy leaves them unchanged.
- AVX-512 was not enabled in this local build. Historical measurement bundles
  and active Archie experiments were not changed.

## Findings at the audited baseline

### High: published PAM/ParGeo/PKD results lack their published adapter source

The snapshot contains `square-rectangle-followup-2d-20261113` measurements for
PAM, ParGeo and PKD. The published [runner](../run_theater.py) and
[CMake build](../CMakeLists.txt) do not register these backends, and the published
tree has no `src/square/rectangle_backends.hpp`.

An existing local integration contains those additions but is uncommitted.
Publishing results did not publish that source. Preserve and validate it, with
the exact upstream revisions and build configuration used for the follow-up,
before claiming that this checkout reproduces that campaign.

The inspected ParGeo follow-up uses a local recursive box traversal over native
tree nodes. Its result field `query_path="native_box"` does not establish that
it calls an upstream rectangle-reporting API. That distinction needs an explicit
adapter disclosure when the follow-up source is published; historical result
records should remain unchanged.

### High: timeouts do not suppress remaining repetitions

The [runner](../run_theater.py) catches a timeout inside its repetition loop
and then starts the next repetition. A local synthetic reporter that always
times out produced **three timeout attempts** with `--repetitions 3`.
This differs from the newer skip-after-timeout behavior requested for Archie.

The runner also leaves malformed successful-process JSON uncaught, so one such
backend can abort execution before the remaining algorithms. Failure isolation
and repeat suppression need a harness change and targeted validation, without
altering the indexes.

## Documentation corrections

- The root README now identifies the follow-up source gap instead of implying
  that every published result is reproducible with the listed backends.
- Four links in the retained dimensional disk notes now point to the pinned
  parent revision. The notes explicitly identify themselves as historical disk
  documentation, rather than documentation for the active square runner.

## Validation of the original audited baseline

- Configured and compiled the checked-in Release CMake build under local Ubuntu
  WSL with GCC 13.3, using the available Boost, CGAL and libspatialindex packages.
- CTest passed **126 exact square/cube checks**, including duplicates, inclusive
  endpoints, empty inputs and rejection of the old disk input format.
- PH-tree, PGM and SpaceTreeLib were not built because the corresponding
  submodules were not initialized in this audit worktree. Optional AVX-512 was
  disabled. The unpublished PAM/ParGeo/PKD integration was not built or certified.
- The snapshot verification record reports 18 verified bundles, 44 artifacts
  and 65,950 job rows, with no unreadable files or credential-pattern findings.
- Small correctness fixtures and a successful build do not certify every
  historical measurement, performance ranking or upstream API claim.
