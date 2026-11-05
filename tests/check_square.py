"""Compare each built native reporter with exhaustive exact reporting."""
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from run_theater import BACKENDS,executable
from tools.make_case import MAX,write_case

build=Path(sys.argv[1]);calls=0
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'case.bin'
    for d in (2,3):
        z=(0,)*d;h=(MAX,)*d;middle=(MAX//2,)*d
        rng=random.Random(20261105+d)
        # Degenerate/full-width boxes, inclusive endpoints, coincident records,
        # and a point in a square corner that a disk predicate would reject.
        points=[z,z,h,h,middle,middle]+[(1,)*d,(2,)*d,(3,)*d,(1,3)+((1,)*(d-2))]
        points += [tuple(rng.getrandbits(32) for _ in range(d)) for _ in range(521)]
        boxes=[(z,z),(h,h),(middle,middle),(z,h),(z,(1,)*d),((1,)*d,(3,)*d),((MAX-1,)*d,h)]
        for _ in range(40):
            width=rng.randrange(MAX+1);lo=tuple(rng.randrange(MAX+1-width) for _ in range(d))
            boxes.append((lo,tuple(x+width for x in lo)))
        for data in ([],points,points+[middle]*140):
            write_case(path,d,data,boxes)
            expected=sum(all(a<=p<=b for a,p,b in zip(lo,pt,hi)) for lo,hi in boxes for pt in data)
            for name,(_,dims) in BACKENDS.items():
                exe=executable(build,name)
                if d not in dims or not exe.is_file():continue
                p=subprocess.run([exe,path,name],capture_output=True,text=True,timeout=20)
                if p.returncode:raise RuntimeError(f'{name} {d}D (n={len(data)}): {p.returncode} {p.stderr}')
                row=json.loads(p.stdout)
                assert row['verified'] and row['m']==expected,(name,d,row,expected)
                calls+=1
    # Verify input compatibility guard instead of silently timing old disk files.
    path.write_bytes(b'ESA2D01\0'+bytes(24))
    p=subprocess.run([executable(build,'scan'),path,'scan'],capture_output=True,text=True,timeout=5)
    assert p.returncode and 'Not a SQUARE1 input' in p.stderr
print(f'{calls} exact square/cube benchmark checks passed')
