# Square and cube benchmark core

- [case.hpp](case.hpp) loads `SQUARE1` data, times materialized output, and
  compares IDs or point multisets against the exhaustive exact oracle.
- [main.cpp](main.cpp) selects the backend and dimension. It rejects disk
  inputs and boxes whose side lengths are unequal.
- [indexicon_backends.hpp](indexicon_backends.hpp) calls each Indexicon native
  box API with a reusable output buffer of original input IDs.
- [other_backends.hpp](other_backends.hpp) calls the remaining library APIs.
  Only the PGM and nanoflann paths need post-filtering to match the integer box.

Add a backend with a separate target in the [root CMake file](../../CMakeLists.txt)
and register its names and supported dimensions in [run_theater.py](../../run_theater.py).
Keep native result materialization inside timing and oracle work outside it.
Use the [boundary and duplicate checks](../../tests/check_square.py) to verify
that each query reports the entire result.
