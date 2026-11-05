# Shared benchmark support

This is harness code, not a Morton implementation.

- `common.hh`: point/rectangle types used by the competitor adapters.
- `circle.hh`: exact integer circle representation and membership.
- `esa_campaign.hh`: dataset loading, algorithm registry, build/query/update
  measurements and the independent correctness oracle.

The standalone algorithms are in [../morton/](../morton/README.md).
