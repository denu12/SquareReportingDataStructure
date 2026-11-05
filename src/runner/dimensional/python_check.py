"""Dimension-general interfaces to the existing Python ESA backends."""
import json
import math
import struct
import sys
import time
import numpy as np

with open(sys.argv[1], 'rb') as f:
    d,n,q=struct.unpack('<IQQ',f.read(20))
    p=np.frombuffer(f.read(n*d*4),dtype='<u4').reshape(n,d).copy()
    queries=[]
    for _ in range(q):
        c=np.frombuffer(f.read(d*4),dtype='<u4').copy()
        lo,hi=struct.unpack('<QQ',f.read(16));queries.append((c,lo+(hi<<64)))
kind=sys.argv[2]
bench='--bench' in sys.argv[3:]
# Import/runtime setup is excluded consistently with the native bridge.
if kind=='esa_snn':
    import snnpy
elif kind!='brute_force':
    from sklearn.neighbors import KDTree,BallTree
build_start=time.perf_counter()
coords=p.astype(np.float64)/2**32
tree=None
if n:
    if kind=='esa_snn':
        import snnpy
        tree=snnpy.build_snn_model(coords,0)
    elif kind!='brute_force':
        from sklearn.neighbors import KDTree,BallTree
        tree=(KDTree if kind=='esa_sklearn_kd' else BallTree)(coords)
build_seconds=time.perf_counter()-build_start
answers=0;query_seconds=0.0
batch_start=time.perf_counter()
for j,(c,r2) in enumerate(queries):
    # Python integers give an exact, independent distance oracle.
    expected=None if bench else [i for i,row in enumerate(p) if sum((int(x)-int(y))**2 for x,y in zip(row,c))<=r2]
    query_start=None if bench else time.perf_counter()
    if kind=='brute_force':
        actual=[i for i,row in enumerate(p) if sum((int(x)-int(y))**2 for x,y in zip(row,c))<=r2]
    elif tree is None: actual=[]
    else:
        center=c.astype(np.float64)/2**32;radius=math.sqrt(r2)/2**32
        actual=tree.query_radius(center if kind=='esa_snn' else center.reshape(1,-1),radius)
        if kind!='esa_snn':actual=actual[0]
    ids=np.asarray(actual,dtype=np.intp)
    if np.any(ids<0) or np.any(ids>=n):raise ValueError("Invalid reported point ID")
    # Retain the native indices; no timed filtering or coordinate gathering.
    if not bench: query_seconds+=time.perf_counter()-query_start
    if not bench: actual=sorted(map(int,ids))
    if not bench and actual!=expected:
        print(json.dumps(dict(status='incorrect',query=j,actual_count=len(actual),expected_count=len(expected))))
        sys.exit(2)
    answers+=len(ids)
if bench: query_seconds=time.perf_counter()-batch_start
print(json.dumps(dict(status='success',queries=q,answers=answers,build_seconds=build_seconds,query_seconds=query_seconds,output_contract='point_ids_only',membership_contract='original_f64_api')))
