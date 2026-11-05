# Square workload tools

[make_case.py](make_case.py) writes a deterministic `SQUARE1` workload. Select
2D squares or 3D cubes, point count, query count, seed and point distribution.
`--target-m` sets the approximate output size under uniform density; the
reported `m` in results is the actual total across all queries.

```sh
mkdir -p data
python3 tools/make_case.py data/squares.bin --n 10000 --queries 100 --target-m 100
python3 run_theater.py data/squares.bin --algorithms morton boost_rstar
```

[prepare_pgm.py](prepare_pgm.py) is invoked by CMake to prepare the inherited
PGM iterator fixes in the build directory. It leaves the submodule untouched.
See the [root README](../README.md) for build, correctness and timing commands.
