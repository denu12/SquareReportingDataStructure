# Square/cube results captured on 27 November 2026

This snapshot contains **44 result/metadata files in 29 bundles**
(2.29 MB compressed), with **65,950 recorded job rows** and
**48,260 successful final repetitions**. Row counts include correctness,
screening, repeats and unsuccessful outcomes; they are not independent cases.

The two square queues remain complete. In addition to their recorded results,
this snapshot includes **5,123 square-only follow-up rows** from shared disk
controllers, replacing older observations for **1,275 algorithm/workload pairs**:

- Updated 2D PGM and ParGeo measurements from the parity queue.
- Updated 3D PGM, PSI SPaC-Z and PSI SPaC-H measurements from the PH/PSI queue.

The shared-controller records contain only `geometry=square` rows. Their
`capture-filter.json` gives original-log and filtered-content hashes. Historical
`pgm_multidimensional` is the square `pgm` identifier; raw IDs are preserved.
The matching disk-source profiles are preserved in the companion repository's
frozen source collection. Retain these versions when comparing measurements.

Superseded rows were removed from this snapshot and the earlier 22 November
snapshot using exact campaign/algorithm/case identities. Identical case labels
in 2D and 3D do not authorize cross-campaign deletion. Disk observations are
excluded. Existing failures, timeouts and correctness flags remain explicit.

[index.json](index.json) and [verification.json](verification.json) provide bundle
hashes and successful record/JSONL/redaction checks. [capture.json](capture.json)
records the source capture; the shared-controller filter records its own later
capture interval. `cleanup-replacements.json.gz` records the replacement keys.
Use this snapshot together with its version labels; do not add overlapping
captures together as independent repetitions.
