"""Export recorded experiment artifacts, excluding inputs, builds and private material.

One gzip JSONL bundle per run retains source-relative paths and original hashes.
Content has local home paths, machine names and private IPv4 addresses redacted.
Source directories are read only; active runs are explicitly partial snapshots.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
import gzip
import hashlib
import json
import os
from pathlib import Path
import re

ALLOWED={'.json','.jsonl','.log','.txt','.csv'}
SKIP={'workspace','data','inputs','prepared','fixtures','bin','build','deps','toolchains',
      '.git','.cargo','.cache','.venv','__pycache__','target','target-native','node_modules'}
SECRET=re.compile(r'(?:gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{25,}|sk-[A-Za-z0-9_-]{32,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)')

def clean(text):
    text=re.sub(r'/home/[^/\s"\']+', '<HOME>', text)
    text=re.sub(r'C:(?:\\\\|\\|/)Users(?:\\\\|\\|/)[^/\\\s"\']+', '<HOME>', text, flags=re.I)
    text=re.sub(r'(?<!\d)(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})(?!\d)', '<PRIVATE-IP>',text)
    text=re.sub(r'("(?:hostname|fqdn|username|user_name)"\s*:\s*)"[^"\n]*"',r'\1"<REDACTED>"',text)
    if SECRET.search(text):raise ValueError('Possible credential in export content')
    return text

def files(base):
    for directory,dirs,names in os.walk(base,followlinks=False):
        dirs[:]=sorted(d for d in dirs if d not in SKIP and not d.startswith(('bazel-','build-')) and not (Path(directory)/d).is_symlink())
        for name in sorted(names):
            p=Path(directory)/name
            if p.suffix in ALLOWED and not p.is_symlink():yield p

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',action='append',required=True,help='NAME=directory; repeat for history')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=False)
    index={'schema':1,'snapshot_started_utc':datetime.now(timezone.utc).isoformat(),'bundles':[],'phases':[],'notes':['Historical and invalid outcomes are retained, not endorsed.','An active run is a partial snapshot, not a completed campaign.','State rows and job artifacts overlap; do not count both.','Input binaries, software builds, private documents and caches are excluded.','Local home paths, host/user names and private IP addresses are redacted.']}
    for source in args.source:
        name,location=source.split('=',1);base=Path(location)
        if not re.fullmatch(r'[a-z0-9-]+',name):raise ValueError(name)
        groups={}
        for p in files(base):
            rel=p.relative_to(base)
            # Queue workspaces contain duplicate code and data, excluded above.
            group=Path(*rel.parts[:2]) if len(rel.parts)>2 else Path('_metadata')
            groups.setdefault(group,[]).append(p)
        for group,paths in sorted(groups.items()):
            target=out/name/group.with_suffix(group.suffix+'.jsonl.gz');target.parent.mkdir(parents=True,exist_ok=True)
            count=rawbytes=0;unreadable=[]
            with target.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0,compresslevel=6) as stream:
                for p in paths:
                    rel=str(p.relative_to(base))
                    try:data=p.read_bytes();text=data.decode('utf-8-sig')
                    except (OSError,UnicodeError) as e:unreadable.append({'path':rel,'error':type(e).__name__});continue
                    content=clean(text)
                    record={'path':rel,'original_sha256':hashlib.sha256(data).hexdigest(),'original_bytes':len(data),'content':content}
                    stream.write((json.dumps(record,separators=(',',':'),ensure_ascii=False)+'\n').encode())
                    count+=1;rawbytes+=len(data)
                    if p.name=='state.json':
                        try:s=json.loads(text)
                        except ValueError:continue
                        rows=s.get('results',[]) if isinstance(s,dict) else []
                        if isinstance(rows,list) and rows:
                            index['phases'].append({'source':name,'path':rel,'status':s.get('status'),'rows':len(rows),'outcomes':dict(Counter(x.get('status','unknown') for x in rows)),'algorithms':sorted({x.get('algorithm','unknown') for x in rows})})
            index['bundles'].append({'path':str(target.relative_to(out)),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'files':count,'source_bytes':rawbytes,'compressed_bytes':target.stat().st_size,'unreadable':unreadable})
        print(name,'exported',len(groups),'bundles',flush=True)
    index['snapshot_finished_utc']=datetime.now(timezone.utc).isoformat()
    index['total_files']=sum(x['files'] for x in index['bundles']);index['total_phase_rows']=sum(x['rows'] for x in index['phases'])
    (out/'index.json').write_text(json.dumps(index,indent=2)+'\n')
    print('files',index['total_files'],'phase_rows',index['total_phase_rows'],'compressed_MB',round(sum(x['compressed_bytes'] for x in index['bundles'])/1e6,1),flush=True)

if __name__=='__main__':main()
