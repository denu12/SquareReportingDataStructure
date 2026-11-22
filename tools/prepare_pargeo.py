"""Qualify a legacy Parlay call in a build copy; preserve captured headers."""
from pathlib import Path
import shutil
import sys


def prepare(source, destination):
    source,destination=Path(source),Path(destination)
    shutil.copytree(source/'include',destination,dirs_exist_ok=True)
    path=destination/'pargeo/kdTree.h'
    text=path.read_text()
    old='scan_inplace(make_slice(Sums), addm<size_t>())'
    assert text.count(old)==1,'ParGeo header differs from its captured version'
    # The public API adapts the legacy monoid. The internal overload does not.
    path.write_text(text.replace(old,'parlay::scan_inplace(make_slice(Sums), addm<size_t>())'))


if __name__=='__main__':prepare(*sys.argv[1:])
