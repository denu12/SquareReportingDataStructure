const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const crypto = require('node:crypto');
const base = path.resolve(process.argv[2]);
const index = JSON.parse(fs.readFileSync(path.join(base,'index.json'),'utf8'));
const sha = b => crypto.createHash('sha256').update(b).digest('hex');
let records=0;
const jsonl=[],statuses=[],failures=[],sensitive=[];
const secret=/(?:gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{25,}|sk-[A-Za-z0-9_-]{32,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)/;
const password=/["'](?:password|passwd|access_token|refresh_token|client_secret)["']\s*:\s*["'](?!<REDACTED>|["'])[^"']+["']/i;
for (const bundle of index.bundles) {
  const file=path.join(base,bundle.path);
  const raw=fs.readFileSync(file);
  if (sha(raw)!==bundle.sha256) failures.push({path:bundle.path,reason:'bundle hash mismatch'});
  const lines=zlib.gunzipSync(raw).toString('utf8').split('\n').filter(Boolean);
  if (lines.length!==bundle.files) failures.push({path:bundle.path,reason:'record count mismatch'});
  for (const line of lines) {
    const record=JSON.parse(line);records++;
    if (secret.test(record.content)||password.test(record.content)) sensitive.push(record.path);
    if (/(?:\/home\/[^/\s"']+|192\.168\.\d+\.\d+)/.test(record.content)) failures.push({path:record.path,reason:'unredacted local identity'});
    if (record.path.endsWith('.jsonl')) {
      const counts={},algorithms=new Set();let rows=0,invalid=0;
      for (const l of record.content.split('\n').filter(Boolean)) {
        let row;try {row=JSON.parse(l);}catch {invalid++;continue;}
        rows++;const phase=row.phase??'unspecified';const s=row.status??'unspecified';
        counts[phase+':'+s]=(counts[phase+':'+s]??0)+1;
        if (row.algorithm) algorithms.add(row.algorithm);
      }
      jsonl.push({source:bundle.path.split('/')[0],path:record.path,rows,invalid_lines:invalid,outcomes:counts,algorithms:[...algorithms].sort()});
    } else if (record.path.endsWith('status.json')) {
      try {const s=JSON.parse(record.content);statuses.push({source:bundle.path.split('/')[0],path:record.path,status:s.status,completed:s.completed,campaign:s.campaign,job:s.job,total_jobs:s.total_jobs});}catch{}
    }
  }
  if (bundle.unreadable.length) failures.push({path:bundle.path,reason:'unreadable source files',count:bundle.unreadable.length});
}
if (records!==index.total_files) failures.push({reason:'total count mismatch'});
const report={verified_bundles:index.bundles.length,verified_records:records,compressed_bytes:index.bundles.reduce((n,b)=>n+b.compressed_bytes,0),capture_started_utc:index.snapshot_started_utc,capture_finished_utc:index.snapshot_finished_utc,failures,sensitive_paths:sensitive,jsonl_artifacts:jsonl,queue_statuses:statuses};
fs.writeFileSync(path.join(base,'verification.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({bundles:report.verified_bundles,records,compressed_bytes:report.compressed_bytes,capture_started_utc:report.capture_started_utc,capture_finished_utc:report.capture_finished_utc,failures,sensitive_paths:sensitive,jsonl_artifacts:jsonl.map(r=>({path:r.path,rows:r.rows,invalid_lines:r.invalid_lines,outcomes:r.outcomes})),queue_statuses:statuses},null,2));
if (failures.length||sensitive.length) process.exitCode=1;
