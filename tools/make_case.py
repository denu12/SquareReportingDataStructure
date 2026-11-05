"""Write deterministic square or cube workloads in the SQUARE1 format."""
import argparse
from pathlib import Path
import random
import struct

MAX = (1 << 32) - 1


def write_case(path, d, points, boxes):
    point = struct.Struct('<' + 'I'*d)
    with Path(path).open('wb') as out:
        out.write(struct.pack('<8sIQQ', b'SQUARE1\0', d, len(points), len(boxes)))
        for p in points:
            out.write(point.pack(*p))
        for lo, hi in boxes:
            out.write(point.pack(*lo))
            out.write(point.pack(*hi))


def generate(d, n, q, m, seed, distribution='uniform', matched=False):
    rng = random.Random(seed)

    def point():
        if distribution == 'normal':
            return tuple(min(MAX, max(0, int(rng.gauss(MAX/2, MAX/8)))) for _ in range(d))
        if distribution == 'skew':
            return tuple(int(MAX*rng.random()**3) for _ in range(d))
        return tuple(rng.getrandbits(32) for _ in range(d))

    points = [point() for _ in range(n)]
    # Uniform-volume target only; measured m is reported by each backend.
    width = min(MAX, int((m / max(n, 1))**(1/d) * (MAX+1)))
    boxes = []
    for _ in range(q):
        center = rng.choice(points) if matched and points else tuple(rng.getrandbits(32) for _ in range(d))
        lo = tuple(min(MAX-width, max(0, x-width//2)) for x in center)
        boxes.append((lo, tuple(x+width for x in lo)))
    return points, boxes


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output',type=Path)
    p.add_argument('--dimension',type=int,choices=[2,3],default=2)
    p.add_argument('--n',type=int,default=10000)
    p.add_argument('--queries',type=int,default=100)
    p.add_argument('--target-m',type=float,default=100)
    p.add_argument('--seed',type=int,default=1)
    p.add_argument('--distribution',choices=['uniform','normal','skew'],default='uniform')
    p.add_argument('--matched',action='store_true')
    a = p.parse_args()
    if min(a.n,a.queries,a.target_m)<0:
        p.error('n, queries and target m must be nonnegative')
    write_case(a.output,a.dimension,*generate(a.dimension,a.n,a.queries,a.target_m,a.seed,a.distribution,a.matched))
