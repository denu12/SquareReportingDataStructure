# Benchmark drivers

[dimensional/](dimensional/README.md) contains the 3D and generic-dimensional
benchmark drivers and competitor adapters. The 2D executable entry point is
[../app/esa_runner.cc](../app/esa_runner.cc). Root `tools/` launches and limits
these programs through `run_theater.py`.

Retired protobuf experiment controllers and their plotting pipeline were removed.
