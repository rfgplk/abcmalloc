#!/usr/bin/env python3

# only for dev use practically

import os
import re

SRC = "/code/C++/micron/src/memory/allocation/abcmalloc"
DST = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
ABC_REL = "memory/allocation/abcmalloc"  # SRC relative to micron/src

_inc = re.compile(r'^(\s*#\s*include\s*)"([^"]+)"(.*)$')


def convert(line: str) -> str:
    m = _inc.match(line.rstrip("\n"))
    if m and ".." in m.group(2):
        resolved = os.path.normpath(os.path.join(ABC_REL, m.group(2)))
        assert not resolved.startswith(".."), \
            f"include {m.group(2)!r} escapes micron/src (resolved={resolved!r})"
        return f"{m.group(1)}<micron/{resolved}>{m.group(3)}\n"
    return line if line.endswith("\n") else line + "\n"


def main() -> None:
    n = 0
    for name in sorted(os.listdir(SRC)):
        if not name.endswith(".hpp"):
            continue
        with open(os.path.join(SRC, name)) as f:
            out = [convert(line) for line in f]
        with open(os.path.join(DST, name), "w") as f:
            f.write("".join(out))
        n += 1
    print(f"synced {n} headers from {SRC} -> {DST}")


if __name__ == "__main__":
    main()
