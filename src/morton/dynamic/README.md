# Dynamic Morton

The source project did not retain a dynamic Morton implementation. The square
and cube runner is static: it builds an index, then reports queries against
that fixed set of records. The inherited update adapters are source reference,
not an active square workload.

See the [active Morton indexes](../README.md).
