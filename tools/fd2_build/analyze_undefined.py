#!/usr/bin/env python3
"""analyze_undefined.py -- classify build_fd2's undefined symbols into an
actionable worklist, with the referencing .obj for each (home-file hint).

Reads workspace/fd2_build/exe/out/build.out (raw wcc386+wlink output written by
build_fd2.py). Buckets each distinct undefined symbol:

  vendor   -- libc / x87-math / Watcom-startup names (resolve by wiring an
              explicit CRT library into fd2.lnk; NOT a src/ gap)
  data     -- data_fd2_* game globals               -> Phase 1 data worklist
  func     -- fd2_* game functions                  -> Phase 2 (or still-stub fns)
  other    -- plain-named game globals/functions     -> investigate (name-pattern
              missed by the data_fd2_-only reconcile)

Cross-checks the data bucket against workspace/data_emit/worklist.tsv. Writes
workspace/fd2_build/game_worklist.tsv (non-vendor symbols + referencing objs).
"""
import io, os, re, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "workspace", "fd2_build", "exe", "out", "build.out")
WORKLIST = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")
OUT = os.path.join(ROOT, "workspace", "fd2_build", "game_worklist.tsv")

# libc names the Miles AIL lib + math lib pull from CLIB3S (vendor wiring, not a src gap)
LIBC = set("""isatty setbuf time localtime asctime strcpy fclose toupper strlen memset
strnicmp fgets memmove strncpy strncmp sprintf malloc free abs close open read write
lseek fopen fread fwrite fprintf fputs fputc fgetc fgets getc putc printf puts qsort
memcpy memcmp strcat strcmp stricmp strchr strrchr strstr atoi atol tolower getenv
filelength delay exit signal raise setjmp longjmp calloc realloc rand srand
sscanf vsprintf vfprintf ftell fseek fflush rewind remove rename system clock
getch getche putch kbhit int386 int386x int86 int86x intr outp inp outpw inpw
outpd inpd segread bdos intdos intdosx _dos_getvect _dos_setvect nosound sound""".split())


def is_vendor(sym):
    if sym in LIBC:
        return True
    if sym.startswith("__") or sym.startswith("___"):
        return True                                   # __8087 __hook387 __iob ...
    if len(sym) > 1 and sym[0] == "_" and sym[1].isupper():
        return True                                   # _Extender _HugeValue
    if sym == "_fltused_":
        return True
    return False


def main():
    refs = defaultdict(set)                            # sym -> {referencing obj}
    for line in io.open(RAW, encoding="latin-1"):
        m = re.search(r"file\s+(\S+).*undefined symbol\s+(\S+)", line)
        if m:
            refs[m.group(2).strip()].add(os.path.basename(m.group(1).strip().rstrip(":")))
            continue
        m = re.search(r"undefined symbol\s+(\S+)", line) or \
            re.search(r"(\S+)\s+is an undefined reference", line)
        if m:
            refs[m.group(1).strip()].add("?")

    syms = sorted(refs)
    vendor = [s for s in syms if is_vendor(s)]
    game = [s for s in syms if not is_vendor(s)]
    data = [s for s in game if s.startswith("data_fd2_")]
    func = [s for s in game if s.startswith("fd2_")]
    crt = [s for s in game if s.startswith(("crt_", "AIL_"))]
    other = [s for s in game if s not in set(data) | set(func) | set(crt)]

    # cross-check data bucket vs the reconcile worklist
    wl_fake, wl_real = set(), set()
    if os.path.exists(WORKLIST):
        with io.open(WORKLIST, encoding="utf-8") as f:
            f.readline()
            for ln in f:
                p = ln.rstrip("\n").split("\t")
                if len(p) >= 8:
                    (wl_fake if p[7] == "fake_in_testglob" else wl_real if p[7] == "real_in_src" else set()).add(p[2]) if p[7] in ("fake_in_testglob", "real_in_src") else None
    data_not_in_wl = [s for s in data if s not in wl_fake and s not in wl_real]

    print("=== build_fd2: %d distinct undefined symbols ===" % len(syms))
    print("  vendor (libc/math/startup) : %4d  -> wire explicit CRT lib in fd2.lnk" % len(vendor))
    print("  data_fd2_* (DATA worklist) : %4d  -> Phase 1" % len(data))
    print("  fd2_* functions            : %4d  -> Phase 2 / still-stub fns" % len(func))
    print("  crt_*/AIL_*                : %4d" % len(crt))
    print("  OTHER plain-named game sym : %4d  -> investigate (reconcile missed)" % len(other))
    print()
    print("  data_fd2_ undefined NOT in reconcile worklist: %d" % len(data_not_in_wl))
    for s in data_not_in_wl[:15]:
        print("     ", s, " <-", sorted(refs[s])[:3])
    print()
    print("--- fd2_* functions undefined (%d) ---" % len(func))
    for s in func:
        print("   %-46s <- %s" % (s, sorted(refs[s])[:4]))
    print()
    print("--- OTHER plain-named (%d), with referencing obj ---" % len(other))
    for s in other:
        print("   %-46s <- %s" % (s, sorted(refs[s])[:4]))

    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write("symbol\tbucket\treferencing_objs\n")
        for s in game:
            b = ("data" if s in set(data) else "func" if s in set(func)
                 else "crt" if s in set(crt) else "other")
            f.write("%s\t%s\t%s\n" % (s, b, ",".join(sorted(refs[s]))))
    print("\nWROTE %s (%d game symbols)" % (OUT, len(game)))


if __name__ == "__main__":
    main()
