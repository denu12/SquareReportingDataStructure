"""Apply the inherited iterator fixes in the build tree, never in upstream."""
from pathlib import Path
import shutil
import sys

source,target=map(Path,sys.argv[1:])
shutil.copytree(source,target,dirs_exist_ok=True)
path=target/'pgm/pgm_index_variants.hpp'
text=path.read_text()
fixes=[
    ('            if (*it > zmax)\n', '            if (it != super->data.end() && *it > zmax)\n'),
    ('                    it = std::upper_bound(super->data.begin() + range.lo, super->data.begin() + range.hi, bmin);\n                    --it;',
     '                    it = std::lower_bound(super->data.begin() + range.lo, super->data.begin() + range.hi, bmin);\n                    continue;'),
    ('            auto range = super->pgm.search(zmin);',
     '            if (super->data.empty()) { this->it = super->data.end(); return; }\n            auto range = super->pgm.search(zmin);'),
    ('        reference operator*() const { return p; }',
     '        size_t source_position() const { return it - super->data.begin(); }\n        reference operator*() const { return p; }'),
]
for old,new in fixes:
    if text.count(old)!=1:raise SystemExit('PGM differs from the pinned revision')
    text=text.replace(old,new)
path.write_text(text)
