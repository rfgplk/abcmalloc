#!/usr/bin/env python3

# only for dev use

import glob
import os
import re

MICRON = "/code/C++/micron"
MTESTS = os.path.join(MICRON, "tests")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DTESTS = os.path.join(ROOT, "tests")

_inc = re.compile(r'^(\s*#\s*include\s*)"([^"]+)"(.*)$')
_src = re.compile(r'^((?:\.\./)+)src/(.*)$')
_abc = re.compile(r'^(?:memory/)?allocation/abcmalloc/(.+)$')

# these are mt which perform include spaghetti (they pull in tapi)
_DROP_RIGOR = {
    "abcmalloc_adversarial.cpp", "abcmalloc_arena_recycle.cpp", "abcmalloc_concurrent.cpp",
    "abcmalloc_mt.cpp", "abcmalloc_soak_mt.cpp",
}
# bad tests, will update later
_DROP_CORE = {"abcmalloc.cpp", "abcmalloc_main.cpp", "abcmalloc_thread.cpp"}

_ret = re.compile(r"^(\s*return\s+)(\d+)(\s*;.*)$")


def _invert_exit(line: str) -> str:
    m = _ret.match(line.rstrip("\n"))
    if m:
        return f"{m.group(1)}{'1' if m.group(2) == '0' else '0'}{m.group(3)}\n"
    return line


def convert(line: str) -> str:
    m = _inc.match(line.rstrip("\n"))
    if not m:
        return line if line.endswith("\n") else line + "\n"
    pre, path, post = m.group(1), m.group(2), m.group(3)
    sm = _src.match(path)
    if not sm: 
        return line if line.endswith("\n") else line + "\n"
    depth, rest = sm.group(1), sm.group(2)
    am = _abc.match(rest)
    if am: 
        return f'{pre}"{depth}src/{am.group(1)}"{post}\n'
    if rest == "cmalloc.hpp": 
        return f'{pre}"{depth}src/cmalloc.hpp"{post}\n'
    return f"{pre}<micron/{rest}>{post}\n" 


_UMBRELLA_BLOCK = (
    "// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines\n"
    "// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone\n"
    "// allocator instead of pulling their own in-tree copy.\n"
    '#include "../../src/cmalloc.hpp"\n'
)


def sync(src: str, dst: str, prepend: bool = False, invert_exit: bool = False) -> None:
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(src) as f:
        out = [convert(line) for line in f]
    if invert_exit:  # core/doctor: standard 0==success -> project 1==success
        out = [_invert_exit(line) for line in out]
    if os.path.basename(dst) == "snowball.hpp":  # failed require() -> 0==fail
        out = [line.replace("sys_exit(6)", "sys_exit(0)") for line in out]
    if prepend:
        out.insert(0, _UMBRELLA_BLOCK)
    with open(dst, "w") as f:
        f.write("".join(out))


def main() -> None:
    pairs = []  # (src, dst, prepend_umbrella)
    # rigor: non-threaded abc* tests only (refresh existing + add persistent)
    for p in sorted(glob.glob(os.path.join(MTESTS, "rigor", "abc*.cpp"))):
        b = os.path.basename(p)
        if b in _DROP_RIGOR:
            continue
        pairs.append((p, os.path.join(DTESTS, "rigor", b), True))
    # core: functional tests only (benches live in general/; stale tests excluded)
    for p in sorted(glob.glob(os.path.join(MTESTS, "core", "abcmalloc*.cpp"))):
        b = os.path.basename(p)
        if b.startswith("abcmalloc_bench_") or b in _DROP_CORE:
            continue
        pairs.append((p, os.path.join(DTESTS, "core", b), True))
    # general: refresh only benches that have a micron core counterpart (keep mirror-only ones)
    for p in sorted(glob.glob(os.path.join(DTESTS, "general", "*.cpp"))):
        b = os.path.basename(p)
        src = os.path.join(MTESTS, "core", b)
        if os.path.exists(src):
            pairs.append((src, p, True))
    # doctor: entire new suite for the doctor subsystem
    for p in sorted(glob.glob(os.path.join(MTESTS, "doctor", "*.cpp"))):
        pairs.append((p, os.path.join(DTESTS, "doctor", os.path.basename(p)), True))
    # shared harness: snowball + abc_rigor (used by the single-threaded soak/realloc
    # tests; its MT worker machinery is gated behind ABC_RIGOR_ST_ONLY). mock_allocators.hpp
    # is only used by the dropped threaded tests, so it is not shipped.
    for rel in ("snowball/snowball.hpp", "snowball/snowball_ext.hpp", "support/abc_rigor.hpp"):
        src = os.path.join(MTESTS, rel)
        if os.path.exists(src):
            pairs.append((src, os.path.join(DTESTS, rel), False))

    for src, dst, prepend in pairs:
        invert_exit = os.path.basename(os.path.dirname(dst)) in ("core", "doctor")
        sync(src, dst, prepend, invert_exit)
    print(f"synced {len(pairs)} test files -> {DTESTS}")


if __name__ == "__main__":
    main()
