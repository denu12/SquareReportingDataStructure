"""Run the built square/cube competitors serially, with optional CPU pinning."""
import argparse
import json
import os
import signal
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parent


def run_backend(cmd, timeout):
    process=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
                             start_new_session=True)
    try:
        out,err=process.communicate(timeout=timeout)
    except (subprocess.TimeoutExpired,KeyboardInterrupt):
        if os.name=='posix':
            try:os.killpg(process.pid,signal.SIGKILL)
            except ProcessLookupError:pass
        else:process.kill()
        process.communicate()
        raise
    if process.returncode:
        return dict(status='incorrect' if process.returncode==2 else 'error',detail=err.strip())
    try:
        row=json.loads(out)
        if not isinstance(row,dict) or not isinstance(row.get('status'),str):
            raise ValueError('Expected a JSON object with a status')
        return row
    except ValueError:
        return dict(status='error',detail='Backend returned malformed JSON',stdout_tail=out[-1000:])

# Executables group library variants that share an upstream interface.
BACKENDS = {
    'scan': ('scan', (2,3)),
    'morton': ('morton', (2,)),
    'morton_3d': ('morton', (3,)),
    'morton_plus': ('morton', (2,)),
    'morton_plus_3d': ('morton', (3,)),
    'morton_simd': ('morton_simd', (2,)),
    'boost_linear': ('boost', (2,3)),
    'boost_quadratic': ('boost', (2,3)),
    'boost_rstar': ('boost', (2,3)),
    'cgal_range': ('cgal', (2,)),
    'cgal_kd': ('cgal', (2,3)),
    'phtree': ('phtree', (2,3)),
    'pgm': ('pgm', (2,3)),
    'psi_kd': ('psi', (2,3)),
    'psi_orth': ('psi', (2,3)),
    'psi_spac_z': ('psi', (2,3)),
    'psi_spac_h': ('psi', (2,3)),
    'lsi_linear': ('spatialindex', (2,3)),
    'lsi_quadratic': ('spatialindex', (2,3)),
    'lsi_rstar': ('spatialindex', (2,3)),
    'lsi_str': ('spatialindex', (2,3)),
    'thst_quadtree': ('thst', (2,)),
    'thst_rtree': ('thst', (2,)),
    'indexicon_rtree_point': ('indexicon_rpoint', (2,3)),
    'indexicon_rtree_mbr': ('indexicon_rmbr', (2,3)),
    'indexicon_kdtree_adaptive': ('indexicon_kd', (2,3)),
    'indexicon_kdtree_round_robin': ('indexicon_kd', (2,3)),
    'indexicon_kdtree_longest_axis': ('indexicon_kd', (2,3)),
    'indexicon_quadtree_pr': ('indexicon_quad', (2,)),
    'indexicon_quadtree_pseudo_median': ('indexicon_quad', (2,)),
    'indexicon_quadtree_longest_axis': ('indexicon_quad', (2,)),
    'indexicon_mxcif_quadtree': ('indexicon_mxcif', (2,)),
    'indexicon_octree': ('indexicon_oct', (3,)),
    'nanoflann': ('nanoflann', (2,3)),
    'pam': ('pam', (2,)),
    'pargeo': ('pargeo', (2,)),
    'pkd': ('pkd', (2,)),
}


def executable(build, name):
    return Path(build).resolve()/('square_'+BACKENDS[name][0])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('case',nargs='?',type=Path)
    p.add_argument('--build',type=Path,default=ROOT/'build')
    p.add_argument('--list',action='store_true')
    p.add_argument('--algorithms',nargs='+',choices=BACKENDS)
    p.add_argument('--bench',action='store_true',help='Skip the exhaustive oracle on large cases')
    p.add_argument('--repetitions',type=int,default=1)
    p.add_argument('--timeout',type=float,default=180)
    p.add_argument('--cpu',type=int,help='Pin this controller and all children to one CPU on Linux')
    a=p.parse_args()
    if a.list:
        for name,(_,dims) in BACKENDS.items():
            print(name, '/'.join(map(str,dims))+'D', 'built' if executable(a.build,name).is_file() else 'unavailable')
        return 0
    if a.case is None:p.error('Supply a SQUARE1 case or --list')
    if a.repetitions<1 or a.timeout<=0:p.error('Repetitions and timeout must be positive')
    with a.case.open('rb') as f:
        magic,d=struct.unpack('<8sI',f.read(12))
    if magic!=b'SQUARE1\0' or d not in (2,3):p.error('Expected a 2D or 3D SQUARE1 case')
    if a.cpu is not None:
        os.sched_setaffinity(0,{a.cpu})
    names=a.algorithms or [n for n,(_,dims) in BACKENDS.items() if d in dims and executable(a.build,n).is_file()]
    if not names:p.error('No matching built backends')
    failed=False
    for key in ('OMP_NUM_THREADS','OPENBLAS_NUM_THREADS','MKL_NUM_THREADS','RAYON_NUM_THREADS','PARLAY_NUM_THREADS'):
        os.environ[key]='1'
    for name in names:
        if d not in BACKENDS[name][1] or not executable(a.build,name).is_file():
            print(json.dumps(dict(algorithm=name,status='unavailable')),flush=True)
            failed=True;continue
        skip=None
        for rep in range(a.repetitions):
            cmd=[str(executable(a.build,name)),str(a.case),name]+(['--bench'] if a.bench else [])
            try:
                row=dict(status=skip) if skip else run_backend(cmd,a.timeout)
            except subprocess.TimeoutExpired:
                row=dict(status='timeout',algorithm=name)
            except OSError as error:
                row=dict(status='error',detail=str(error))
            if row['status']=='timeout':skip='skipped_after_timeout'
            elif row['status']=='error':skip='skipped_after_error'
            failed |= row['status']!='success'
            print(json.dumps(dict(row,algorithm=name,repetition=rep)),flush=True)
    return int(failed)


if __name__=='__main__':
    raise SystemExit(main())
