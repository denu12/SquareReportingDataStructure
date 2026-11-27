# Recorded measurements

- [27 November 2026](2026-11-27/README.md): refreshed complete square/cube data, including corrected 2D PGM/ParGeo and 3D PGM/PSI measurements from shared controllers. Superseded observations have been removed by exact campaign identity.

- [22 November 2026](2026-11-22/README.md): the full 2D/3D square/cube campaign and the additional 2D rectangle-reporting campaign captured from Archie.

Snapshots preserve successful timings, failures, timeouts, correctness admissions and queue status. Input datasets and generated executables are excluded. Local home paths, host/user fields and private IP addresses are redacted.

Each dated directory contains `index.json`, `verification.json` and gzip-compressed JSONL bundles. Each bundle line contains a source-relative `path`, `original_sha256`, `original_bytes` and UTF-8 `content`. Original hashes describe bytes before redaction; the index hashes describe the published bundles. Paths inside each source are preserved.

```python
import gzip, json
with gzip.open('measurements/2026-11-22/current/square-full-2d-3d-20261106/Static3D.jsonl.gz', 'rt') as stream:
    for line in stream:
        artifact = json.loads(line)
        if artifact['path'].endswith('/jobs.jsonl'):
            jobs = [json.loads(row) for row in artifact['content'].splitlines()]
            print(len(jobs))
```

Queue metadata and individual jobs can describe the same measurements. Historical runs and later adapter runs can also overlap. Keep run labels, reporting contracts, source identities and correctness fields when selecting data; do not sum overlapping records or treat process success as proof of correctness. A timeout is an observed outcome, not a completed numeric timing.

To capture and verify a snapshot:

```sh
python tools/export_measurements.py --source current=results --output measurements/YYYY-MM-DD
node tools/verify_measurements.cjs measurements/YYYY-MM-DD
```

Export is read only and captures files over an interval. Active campaigns are partial snapshots; later measurements require another dated export.
