#!/usr/bin/env python3

# for dev use only

import glob
import os
import re
import shutil

MICRON = "/code/C++/micron"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

_inc = re.compile(r'^(\s*#\s*include\s*)"([^"]+)"(.*)$')
_src = re.compile(r'^((?:\.\./)+)src/(.*)$')
_abc = re.compile(r'^(?:memory/)?allocation/abcmalloc/(.+)$')

_DROP_BENCH = {"malloc_pathways_bench.cpp", "malloc_pathways_mt_bench.cpp"}


def convert(line: str) -> str | None:
    m = _inc.match(line.rstrip("\n"))
    if not m:
        return line if line.endswith("\n") else line + "\n"
    pre, path, post = m.group(1), m.group(2), m.group(3)
    sm = _src.match(path)
    if not sm:  # ../external/bbench/..., same-dir: leave as-is
        return line if line.endswith("\n") else line + "\n"
    rest = sm.group(2)
    if _abc.match(rest) or rest == "cmalloc.hpp":
        return None  # drop local allocator includes (installed allocator is used instead)
    return f"{pre}<micron/{rest}>{post}\n"


def main() -> None:
    # 1) vendor bbench verbatim
    dst_bb = os.path.join(ROOT, "external", "bbench")
    os.makedirs(dst_bb, exist_ok=True)
    n_bb = 0
    for h in sorted(glob.glob(os.path.join(MICRON, "external", "bbench", "*.hpp"))):
        shutil.copy2(h, os.path.join(dst_bb, os.path.basename(h)))
        n_bb += 1
    # 2) refresh only the (non-threaded) benches the mirror already ships
    n_b = 0
    for p in sorted(glob.glob(os.path.join(MICRON, "benches", "*.cpp"))):
        b = os.path.basename(p)
        dst = os.path.join(ROOT, "benches", b)
        if b in _DROP_BENCH or not os.path.exists(dst):
            continue
        with open(p) as f:
            out = [c for c in (convert(line) for line in f) if c is not None]
        with open(dst, "w") as f:
            f.write("".join(out))
        n_b += 1
    print(f"vendored {n_bb} bbench headers -> {dst_bb}; refreshed {n_b} benches")


if __name__ == "__main__":
    main()
