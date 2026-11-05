# Morton indexes

These C++20 implementations retain their original Morton order and point
storage. The square build uses their box queries, which report every point
occurrence inside inclusive integer bounds.

| Dimension | Active box queries |
| --- | --- |
| [2D](2d/README.md) | Morton, MortonPlus, optional AVX-512 MortonSIMD |
| [3D](3d/README.md) | Morton and MortonPlus |
| [Generic dimensions](d_dim/README.md) | Inherited disk implementations only |
| [Dynamic](dynamic/README.md) | No dynamic Morton in the square runner |

MortonPlus uses the prefix table and [box refinement](box_refinement.hpp) to
skip disjoint cells and bulk-copy contained cells. The implementation is
derived from the ESA authors' MortonPlus, whose Rust implementation was led
by Dennis Kobert. See the [license](LICENSE) and the [benchmark guide](../../README.md).
