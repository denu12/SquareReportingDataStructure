# Source layout

The active benchmark is [square/](square/README.md). It loads `SQUARE1`
cases, runs native square and cube queries, and checks full result sets or
multisets outside the timer. The top-level [CMake build](../CMakeLists.txt)
and [runner](../run_theater.py) are the entry points.

| Directory | Role |
| --- | --- |
| [square](square/README.md) | Square/cube input, timing, verification and native library calls |
| [morton](morton/README.md) | Standalone Morton indexes and square/cube query methods |
| [third_party](third_party/README.md) | Vendored libraries, pinned submodules and inherited research code |
| [benchmark](benchmark/README.md), [runner](runner/README.md), `app`, `utils` | Retained disk project source for reference |

See the [root README](../README.md) for supported competitors and build commands.
The copied disk harness is not part of the active CMake build.
