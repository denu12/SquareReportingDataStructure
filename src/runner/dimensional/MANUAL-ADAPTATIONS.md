# Manually adapted reporting competitors

The campaign IDs deliberately expose local changes:

* `chan_sss` and `chan_sss_ddim_MANUALLY_ADAPTED`: the campaign route now uses
  `chan_reporting.h`, derived directly from Chan's original SSS source. It keeps
  qsort, the original random-shift generator and seed formula, midpoint-first
  examination, distance-to-cell pruning, and center-first recursion. A fixed
  exact radius replaces nearest-neighbour selection; inclusive comparisons
  preserve boundary duplicates. Arithmetic is widened for uint32 coordinates.
  The older `ChanManuallyAdapted` class is not used by this campaign route.
* `stann_fr_ddim_MANUALLY_ADAPTED`: Michael Connor and Piyush Kumar's STANN,
  using the existing locally modified `init_reporting`/`box_report` traversal.
  The dimensional wrapper instantiates d-dimensional points, carries original
  indices through the sort, and applies exact ball membership to box candidates.
  The existing traversal is reused without further changes. Sorting is serial;
  no OpenMP is enabled.

Neither entry is an unmodified upstream radius-reporting implementation. Both
use full uint32 coordinates and unsigned 128-bit squared distances. The
campaign compares all output indices against an independent brute-force scan;
incorrect outputs, crashes and timeouts are recorded, not repaired.

See the original sources and detailed inherited notices at
`src/third_party/chan_sss/NOTICE.txt` and `src/third_party/stann/NOTICE.txt`.
The upstream files and copyright notices are preserved. No new license is
asserted over either upstream implementation.
