# Captured rectangle follow-up dependencies

These header trees were captured from the dependency directories referenced by
Archie's PAM, ParGeo and PKD square follow-up build.
They include the **pre-existing local dependency patches** from the disk
integration. They are not presented as pristine upstream releases.

| Directory | Upstream base revision | Source |
|---|---|---|
| PAM | `3574543ad84c34acb22edbeecb9d11a97a5eb87d` | https://github.com/cmuparlay/PAM |
| pargeo | `b87077ca253d28595e6887bb369501a64774712a` | https://github.com/wangyiqiu/pargeo |
| parlaylib | `51017699dcc421f80479cdb238d3092233ad0d26` | https://github.com/cmuparlay/parlaylib |
| pkd_tree | `62c8839aa6e87b8cbf652a341a472f1fcf789ec0` | https://github.com/ucrparlay/Pkd-tree |

[source-manifest.json](source-manifest.json) records all 197 supplied files and
their SHA-256 hashes. Copyright/license notices are retained, including PAM's
`copyright` file and PKD's bundled Parlay license. Inherited patches are also
visible under `src/utils/workspace` in this repository. File hashes identify the
captured trees more precisely than the upstream base revisions alone.

The original follow-up recorded binary hashes but not hashes of every dependency
header. This capture supplies reviewable source and a precise identity for new
builds; it cannot retrospectively prove that every header byte was identical at
the time an older binary was compiled.

The default CMake build uses these headers for the three 2D follow-up backends.
Explicit `SQUARE_PAM`, `SQUARE_PARGEO`, `SQUARE_PKD` and `SQUARE_PARLAY` cache
paths can override them; record such overrides with new results.

For compatibility with the captured Parlay API on GCC 13, the build prepares a
copy of ParGeo's `kdTree.h` and qualifies its legacy prefix scan as
`parlay::scan_inplace`. This selects Parlay's public legacy-monoid adapter rather
than the internal overload. Captured files remain unchanged. No partitioning,
search predicate or reporting traversal is altered by that qualification.

PAM and PKD use rectangle-reporting interfaces. ParGeo's square adapter is a
local rectangle traversal over the native tree, disclosed as
`query_path="local_box_traversal"` in new results. Old raw result labels retain
their historical values.
