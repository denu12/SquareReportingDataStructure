"""Exercise timeout suppression and failure isolation with real child processes."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name=='posix','The benchmark runner targets Linux')
class RunnerPolicy(unittest.TestCase):
    def exercise(self, bad):
        with tempfile.TemporaryDirectory() as folder:
            base=Path(folder);case=base/'case.bin'
            case.write_bytes(struct.pack('<8sIQQ',b'SQUARE1\0',2,0,0))
            def executable(name,body):
                f=base/('square_'+name)
                f.write_text('#!'+sys.executable+'\nfrom pathlib import Path\n'+body+'\n')
                f.chmod(0o755)
            executable('morton',"p=Path(__file__).with_name('attempts'); p.write_text(p.read_text()+'x' if p.exists() else 'x')\n"+bad)
            executable('scan',"print('{\"status\":\"success\"}')")
            p=subprocess.run([sys.executable,str(ROOT/'run_theater.py'),str(case),'--build',str(base),
                              '--algorithms','morton','scan','--bench','--repetitions','3','--timeout','0.25'],
                             capture_output=True,text=True,timeout=10)
            self.assertEqual(p.returncode,1,p.stderr)
            rows=[json.loads(line) for line in p.stdout.splitlines()]
            self.assertEqual([r['status'] for r in rows if r['algorithm']=='scan'],['success']*3)
            attempts=(base/'attempts').read_text()
            if (base/'child.pid').exists():
                pid=int((base/'child.pid').read_text());stat=Path('/proc')/str(pid)/'stat'
                self.assertTrue(not stat.exists() or stat.read_text().split()[2]=='Z','Timeout left a child running')
            return rows,attempts

    def test_timeout_kills_descendants_and_skips_repeats(self):
        rows,attempts=self.exercise("import subprocess,sys,time\np=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'])\nPath(__file__).with_name('child.pid').write_text(str(p.pid))\ntime.sleep(30)")
        self.assertEqual(attempts,'x')
        self.assertEqual([r['status'] for r in rows[:3]],['timeout','skipped_after_timeout','skipped_after_timeout'])

    def test_malformed_json_does_not_abort_other_algorithms(self):
        rows,attempts=self.exercise("print('not-json')")
        self.assertEqual(attempts,'x')
        self.assertEqual([r['status'] for r in rows[:3]],['error','skipped_after_error','skipped_after_error'])

    def test_incorrect_output_is_still_measured(self):
        rows,attempts=self.exercise('raise SystemExit(2)')
        self.assertEqual(attempts,'xxx')
        self.assertEqual([r['status'] for r in rows[:3]],['incorrect']*3)


if __name__=='__main__':unittest.main()
