# Third-party code

The active square build uses Indexicon and THST from the vendored headers here,
the nanoflann header retained from the ESA integration, and the pinned PH-tree,
PGM-index and SpaceTreeLib submodules. Boost, CGAL and libspatialindex can be
provided by system development packages. Each library retains its own license.

See the [square benchmark README](../../README.md) for the query path used for
each family. The other directories are preserved source from the disk project;
they are not automatically active square competitors.

The [rectangle follow-up snapshot](rectangle/README.md) supplies the captured
PAM, ParGeo, ParlayLib and PKD dependency headers, their upstream base revisions,
file hashes, inherited patches and retained license notices.
