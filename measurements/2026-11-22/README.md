# Square and cube measurements captured on 22 November 2026

Updated on 27 November after campaign-scoped cleanup: **44 retained files
in 18 bundles (2.13 MB compressed)**. Recorded job rows: 60,827; successful final repetitions: 44,435. The historical capture description below records the original capture.
Superseded observations were removed only for exact campaign/algorithm/case
matches. Unsuperseded 3D data was restored from the original archive. Updated
hashes and counts are in index.json and verification.json. Newer replacements
are in the [27 November snapshot](../2026-11-27/README.md).

The original capture contained **44 recorded files in 18 bundles**, totaling **2.3 MB compressed**. It originally preserved **65,950 job rows**, including correctness checks, screening, final repetitions and unsuccessful outcomes, with **48,260 successful final-repetition rows**. These are not independent algorithm/case pairs.

Capture interval: **18:52:56–18:53:03 UTC**. Both captured queues report `complete`:

- `square-full-2d-3d-20261106`: all ten 2D/3D campaigns.
- `square-rectangle-followup-2d-20261113`: all seven additional 2D campaigns.

Completed queues retain unsuccessful jobs. The snapshot includes 955 final timeouts, 15 final out-of-memory outcomes, and unsuccessful screening/correctness checks. These observations are retained as recorded; completion does not mean every algorithm passed every case.

See [index.json](index.json) for bundle hashes and source file counts, [verification.json](verification.json) for verified counts and per-file job summaries, and the [format description](../README.md) for reading the bundles. All bundles passed SHA-256, record-count and credential-pattern checks; no source files were unreadable.

Prepared binary inputs, software builds and dependency trees are excluded. The results retain historical backend identifiers, even when later source changes or campaign rosters differ. These are square/cube results; disk measurements belong in the companion repository.
