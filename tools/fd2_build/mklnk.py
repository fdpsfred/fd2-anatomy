#!/usr/bin/env python3
"""mklnk.py -- generate fd2.lnk, the src-only FD2.EXE wlink directive.

This is the build that the real game executable links from (no tests/), and in
its current incomplete state it doubles as the "oracle": the undefined symbols
it reports are exactly what real data/functions are still missing from src/.

Reuses tests/genbuild.src_compile_list() for the canonical src .obj set (same
objs the test build compiles), puts the obj holding fd2_main first, then the
Miles AIL vendor libs. Watcom CRT (clib3s/math387s/emu387/graph) is auto-pulled
by `system dos4g`, so it is not listed. Layer-2 target: no FAR_DATA / object
layout directives -- the linker places data freely (see KB rebuild_info/link).

Output: tests/fd2.lnk  (= DOSBox E:\\fd2.lnk). AIL libs are staged into
E:\\out by link_oracle.py before the link runs.
"""
import io, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import genbuild  # noqa: E402

AIL_LIBS = ["ailv3.lib", "fd2common.lib"]
MAIN_OBJ = "lifemain"  # src/life/main.c holds fd2_main; first file -> module name


def gen():
    src_list = genbuild.src_compile_list()              # [(rel, obj)]
    src_list = sorted(src_list, key=lambda t: t[1] != MAIN_OBJ)
    lines = ["system dos4g", "name E:\\out\\FD2.EXE"]
    for _, obj in src_list:
        lines.append(r"file E:\out\obj\%s.obj" % obj)
    for lib in AIL_LIBS:
        lines.append(r"library E:\out\%s" % lib)
    return "\n".join(lines) + "\n", len(src_list)


def main():
    txt, n = gen()
    out = os.path.join(ROOT, "tests", "fd2.lnk")
    if "--apply" in sys.argv:
        io.open(out, "w", encoding="utf-8", newline="\n").write(txt)
        print("wrote %s (%d src objs + %d AIL libs)" % (out, n, len(AIL_LIBS)))
    else:
        sys.stdout.write(txt)
        print("\n[dry-run] %d src objs; re-run with --apply to write tests/fd2.lnk" % n)


if __name__ == "__main__":
    main()
