# Inherited disk benchmark support

This directory preserves the disk project's shared point types, integer circle
predicate, case loader and correctness harness. `common.hh`, `circle.hh` and
`esa_campaign.hh` describe the original experiment and are not used to time
the square project.

The square benchmark has its own [case and timing code](../square/README.md).
It accepts closed integer box bounds rather than center/radius records.
