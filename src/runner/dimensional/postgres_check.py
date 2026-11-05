"""Original PostgreSQL Z-range refinement with exact integer-ball reporting.

A private server stays inside the benchmark job's CPU/memory cgroup. Index keys
use coarse cells to fit upstream's 63-bit limit; stored coordinates stay exact.
"""
import ctypes as C
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'src/third_party/psql-refine-z-range/src'


def runtime():
    return Path(os.environ.get('DRR_POSTGRES_HOME', ROOT / 'toolchains/postgresql-18.4'))


def dependency_files():
    home = runtime()
    return [Path(__file__), home/'bin/postgres', home/'bin/initdb',
            home/'bin/pg_ctl', home/'lib/libpq.so', home/'lib/plpgsql.so', *sorted(SOURCE.glob('*.sql'))]


class Connection:
    """Small libpq binding: one connection, synchronous queries and streamed COPY."""
    def __init__(self, home, socket):
        self.lib = C.CDLL(str(home/'lib/libpq.so'))
        signatures = {
            'PQconnectdb': (C.c_void_p, [C.c_char_p]),
            'PQstatus': (C.c_int, [C.c_void_p]),
            'PQerrorMessage': (C.c_char_p, [C.c_void_p]),
            'PQexec': (C.c_void_p, [C.c_void_p, C.c_char_p]),
            'PQresultStatus': (C.c_int, [C.c_void_p]),
            'PQresultErrorMessage': (C.c_char_p, [C.c_void_p]),
            'PQntuples': (C.c_int, [C.c_void_p]),
            'PQnfields': (C.c_int, [C.c_void_p]),
            'PQgetvalue': (C.c_char_p, [C.c_void_p, C.c_int, C.c_int]),
            'PQclear': (None, [C.c_void_p]),
            'PQfinish': (None, [C.c_void_p]),
            'PQputCopyData': (C.c_int, [C.c_void_p, C.c_char_p, C.c_int]),
            'PQputCopyEnd': (C.c_int, [C.c_void_p, C.c_char_p]),
            'PQgetResult': (C.c_void_p, [C.c_void_p]),
        }
        for name, (result, arguments) in signatures.items():
            function = getattr(self.lib, name)
            function.restype, function.argtypes = result, arguments
        self.conn = self.lib.PQconnectdb(f"host={socket} dbname=postgres connect_timeout=5".encode())
        if self.lib.PQstatus(self.conn):
            error = self.lib.PQerrorMessage(self.conn).decode()
            self.close()
            raise RuntimeError(error)

    def close(self):
        if self.conn:
            self.lib.PQfinish(self.conn)
            self.conn = None

    def execute(self, sql, expected=(1, 2)):
        result = self.lib.PQexec(self.conn, sql.encode())
        try:
            if not result or self.lib.PQresultStatus(result) not in expected:
                message = (self.lib.PQresultErrorMessage(result) if result else self.lib.PQerrorMessage(self.conn))
                raise RuntimeError(message.decode())
            return [[self.lib.PQgetvalue(result, i, j).decode()
                     for j in range(self.lib.PQnfields(result))]
                    for i in range(self.lib.PQntuples(result))]
        finally:
            if result: self.lib.PQclear(result)

    def copy_points(self, points):
        self.execute('COPY points (id, coords) FROM STDIN', expected=(4,))
        try:
            for start in range(0, len(points), 4096):
                data = ''.join(str(i)+'\t{'+','.join(map(str, points[i]))+'}\n'
                               for i in range(start, min(start+4096, len(points)))).encode()
                if self.lib.PQputCopyData(self.conn, data, len(data)) != 1:
                    raise RuntimeError(self.lib.PQerrorMessage(self.conn).decode())
            if self.lib.PQputCopyEnd(self.conn, None) != 1:
                raise RuntimeError('PostgreSQL COPY termination failed')
        except BaseException:
            self.lib.PQputCopyEnd(self.conn, b'client aborted COPY')
            raise
        while result := self.lib.PQgetResult(self.conn):
            try:
                if self.lib.PQresultStatus(result) != 1:
                    raise RuntimeError(self.lib.PQresultErrorMessage(result).decode())
            finally:
                self.lib.PQclear(result)


@contextmanager
def database():
    home = runtime()
    if not all(p.is_file() for p in dependency_files()):
        raise RuntimeError('PostgreSQL runtime or original SQL missing; see docs/POSTGRESQL.md')
    env = dict(os.environ, LD_LIBRARY_PATH=str(home/'lib')+':'+os.environ.get('LD_LIBRARY_PATH',''))
    with tempfile.TemporaryDirectory(prefix='drr-pg-') as directory:
        folder = Path(directory)
        cluster = folder/'db'
        with (folder/'setup.log').open('w') as log:
            subprocess.run([str(home/'bin/initdb'), '-D', str(cluster), '-A', 'trust',
                            '--no-locale', '--encoding=UTF8'], check=True, stdout=log, stderr=log, env=env)
            # Private Unix socket only. No shared system database is touched.
            options = f"-k {folder} -h '' -c max_parallel_workers_per_gather=0 -c max_parallel_maintenance_workers=0 -c max_parallel_workers=0 -c jit=off -c autovacuum=off -c shared_buffers=256MB -c work_mem=64MB -c maintenance_work_mem=256MB"
            subprocess.run([str(home/'bin/pg_ctl'), '-D', str(cluster), '-l', str(folder/'server.log'),
                            '-o', options, '-w', 'start'], check=True, stdout=log, stderr=log, env=env)
            connection = None
            try:
                connection = Connection(home, folder)
                for source in sorted(SOURCE.glob('*.sql')):
                    connection.execute(source.read_text())
                yield connection
            finally:
                if connection: connection.close()
                subprocess.run([str(home/'bin/pg_ctl'), '-D', str(cluster), '-m', 'immediate',
                                '-w', 'stop'], stdout=log, stderr=log, timeout=30, env=env)


def read_case(filename):
    import numpy as np
    with open(filename, 'rb') as stream:
        d, n, q = struct.unpack('<IQQ', stream.read(20))
        if not 2 <= d <= 63: return d, None, None
        points = np.fromfile(stream, dtype='<u4', count=n*d).reshape(n, d)
        queries = []
        for _ in range(q):
            center = struct.unpack('<'+'I'*d, stream.read(d*4))
            low, high = struct.unpack('<QQ', stream.read(16))
            queries.append((center, low+(high<<64)))
        if stream.read(1): raise ValueError('Trailing input bytes')
    return d, points, queries


def run(filename, bench):
    d, points, queries = read_case(filename)
    if points is None:
        return dict(status='unsupported_input', reason='Original SQL supports at most 63 Morton bits; at least one bit per dimension is required')
    bits = min(31, 63//d)
    shift = 32-bits
    with database() as db:
        version = db.execute('SHOW server_version')[0][0]
        start = time.perf_counter()
        # Original integer-code overload, with a monotone cell mapping. Raw uint32
        # coordinates live in int8[] because PostgreSQL int4 is signed.
        encoded = ','.join(f'(coords[{a}] >> {shift})::int4' for a in range(1,d+1))
        db.execute(f'CREATE TEMP TABLE points (id bigint, coords bigint[], z bigint GENERATED ALWAYS AS (GenZOrderCode(ARRAY[{encoded}], {bits})) STORED)')
        db.copy_points(points)
        db.execute('CREATE INDEX points_z ON points (z)')
        db.execute('ANALYZE points')
        build_seconds = time.perf_counter()-start
        query_seconds = 0.0
        answers = 0
        for index, (center, radius2) in enumerate(queries):
            start = time.perf_counter()
            radius = math.isqrt(radius2)
            if radius*radius < radius2: radius += 1
            lower = [max(0,x-radius) for x in center]
            upper = [min(2**32-1,x+radius) for x in center]
            lo = ','.join(str(x>>shift) for x in lower)
            hi = ','.join(str(x>>shift) for x in upper)
            distance = '+'.join(f'(p.coords[{a}]::numeric-{c})*(p.coords[{a}]::numeric-{c})' for a,c in enumerate(center,1))
            sql = f'''SELECT p.id FROM LoosenZOrderRange(
                GenZOrderCode(ARRAY[{lo}]::int4[],{bits}),
                GenZOrderCode(ARRAY[{hi}]::int4[],{bits}),{bits},{d}) bounds
                CROSS JOIN LATERAL RefineZOrderRange(bounds.loosened_start, bounds.loosened_end, {bits}, {d}) r
                JOIN points p ON p.z BETWEEN r.s AND r.e
                WHERE ({distance}) <= {radius2}::numeric'''
            ids = [int(row[0]) for row in db.execute(sql)]
            query_seconds += time.perf_counter()-start
            answers += len(ids)
            if not bench:
                expected = [i for i,p in enumerate(points)
                            if sum((int(x)-y)**2 for x,y in zip(p,center)) <= radius2]
                if sorted(ids) != expected:
                    return dict(status='incorrect', query=index, actual_count=len(ids), expected_count=len(expected))
        return dict(status='success', dimension=d, queries=len(queries), answers=answers,
                    build_seconds=build_seconds, query_seconds=query_seconds,
                    output_contract='point_ids_only', output_representation='materialized_postgresql_ids',
                    membership_contract='original_sql_refinement_with_exact_integer_ball_filter',
                    postgres_version=version, bits_per_coordinate=bits, cell_width=1<<shift,
                    timing_scope='server execution, local socket transfer and client ID materialization; private server startup and SQL installation excluded')


if __name__ == '__main__':
    result = run(sys.argv[1], '--bench' in sys.argv[3:])
    print(json.dumps(result))
    raise SystemExit(2 if result['status']=='incorrect' else 0)
