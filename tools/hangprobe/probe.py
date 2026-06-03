#!/usr/bin/env python3
"""probe.py - one-off investigation: what does DOSBox-X do when a DOS/4GW test
program (a) exits normally, (b) hard-crashes (NULL call), (c) hangs (infinite
loop)? Mirrors tests/build.bat's tail (compile -> link `system dos4g` -> run with
stdout redirect -> echo done -> exit) so the findings transfer to build_test.py.

For each case it launches DOSBox-X and samples once per second for up to --secs:
  - proc_alive : is the dosbox-x process still running (poll() is None)?
  - done       : did C:\\OUT\\DONE.TXT appear (batch reached `echo done`)?
  - testout    : size of C:\\OUT\\TEST.OUT (captured program stdout incl. crash dump)
Writes workspace/hangprobe/<case>_timeline.json and prints a compact summary.
Run with NO other DOSBox/build_test in flight (it owns its own mount + OUT).
"""
import json
import subprocess
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "hangprobe"
OUT = WORK / "OUT"
DOSBOX = r"C:\DOSBox-X\dosbox-x.exe"
WATCOM = r"C:\Users\fdpsf\Documents\WATCOM_9.5a"

CASES = {
    "norm": '#include <stdio.h>\nint main(void){ printf("normal run ok\\n"); return 0; }\n',
    "crash": "int main(void){ void (*fp)(void) = (void(*)(void))0; fp(); return 0; }\n",
    "hang": "int main(void){ volatile int x = 1; while (x) { } return 0; }\n",
    # beat: print+fflush, burn ~3s wall (time() 1s resolution) x3, then hang.
    # If host-side test.out grows in steps BEFORE we kill it, an in-DOSBox fflush
    # propagates to the host file -> a test.out-growth heartbeat is viable.
    "beat": (
        "#include <stdio.h>\n#include <time.h>\n"
        "static void burn(int s){ time_t t0=time(0); while(time(0)-t0 < s){} }\n"
        "int main(void){\n"
        "  printf(\"BEAT A\\n\"); fflush(stdout); burn(3);\n"
        "  printf(\"BEAT B\\n\"); fflush(stdout); burn(3);\n"
        "  printf(\"BEAT C\\n\"); fflush(stdout); burn(3);\n"
        "  for(;;){}\n  return 0;\n}\n"
    ),
    # beat2: each beat writes a heartbeat file with fopen/fprintf/FCLOSE (close
    # forces DOSBox to commit to the host file), burns ~3s, then hangs. If the
    # host-side HB.TXT updates between beats, an open/write/close-per-test
    # heartbeat IS host-observable -> viable hang detector.
    "beat2": (
        "#include <stdio.h>\n#include <time.h>\n"
        "static void burn(int s){ time_t t0=time(0); while(time(0)-t0 < s){} }\n"
        "static void hb(int i){ FILE*f=fopen(\"C:\\\\OUT\\\\HB.TXT\",\"w\"); "
        "if(f){ fprintf(f,\"beat %d\\n\", i); fclose(f); } }\n"
        "int main(void){\n"
        "  hb(1); burn(3);\n  hb(2); burn(3);\n  hb(3); burn(3);\n"
        "  for(;;){}\n  return 0;\n}\n"
    ),
}

CONF_TMPL = """[sdl]
output=surface
[cpu]
core=auto
cputype=pentium_mmx
cycles=max
[autoexec]
mount C "{work}"
mount D "{watcom}"
C:
if not exist C:\\OUT md C:\\OUT
set WATCOM=D:\\
set PATH=Z:\\;D:\\BIN;D:\\BINB
set INCLUDE=D:\\H
D:\\BIN\\WCC386.EXE C:\\PROG.C -bt=dos4g -3s -ms -s -zp4 -fo=C:\\OUT\\PROG.OBJ > C:\\OUT\\build.out
echo === link === >> C:\\OUT\\build.out
D:\\BIN\\WLINK.EXE system dos4g name C:\\OUT\\PROG.EXE option quiet file C:\\OUT\\PROG.OBJ >> C:\\OUT\\build.out
echo === run === >> C:\\OUT\\build.out
C:\\OUT\\PROG.EXE > C:\\OUT\\test.out
echo done > C:\\OUT\\done.txt
exit
"""


def find(name):
    if not OUT.is_dir():
        return None
    low = name.lower()
    for p in OUT.iterdir():
        if p.is_file() and p.name.lower() == low:
            return p
    return None


def run_case(case, secs):
    WORK.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    for p in OUT.iterdir():
        if p.is_file():
            p.unlink()
    (WORK / "PROG.C").write_text(CASES[case], encoding="latin-1")
    conf = WORK / ("%s.conf" % case)
    conf.write_text(CONF_TMPL.format(work=str(WORK), watcom=WATCOM), encoding="latin-1")

    start = time.time()
    proc = subprocess.Popen([DOSBOX, "-silent", "-conf", str(conf)])
    timeline = []
    done_at = exit_at = None
    while time.time() - start < secs:
        time.sleep(1.0)
        t = round(time.time() - start, 1)
        alive = proc.poll() is None
        dfile = find("done.txt")
        tfile = find("test.out")
        tsize = tfile.stat().st_size if tfile else None
        hbf = find("hb.txt")
        hb = hbf.read_text(encoding="latin-1", errors="replace").strip() if hbf else None
        timeline.append({"t": t, "alive": alive, "done": bool(dfile), "testout": tsize, "hb": hb})
        if dfile and done_at is None:
            done_at = t
        if not alive and exit_at is None:
            exit_at = t
        if not alive:  # process gone -> nothing more will change
            break

    killed = False
    if proc.poll() is None:
        proc.kill()
        killed = True

    bo = find("build.out")
    to = find("test.out")
    res = {
        "case": case,
        "secs": secs,
        "done_at": done_at,
        "proc_exit_at": exit_at,
        "killed_at_timeout": killed,
        "final_done_exists": bool(find("done.txt")),
        "test_out": (to.read_text(encoding="latin-1", errors="replace") if to else None),
        "build_out_tail": ("\n".join(bo.read_text(encoding="latin-1", errors="replace").splitlines()[-12:]) if bo else None),
        "timeline": timeline,
    }
    (WORK / ("%s_timeline.json" % case)).write_text(
        json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    return res


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--case", choices=list(CASES) + ["all"], default="all")
    ap.add_argument("--secs", type=int, default=25)
    a = ap.parse_args()
    cases = list(CASES) if a.case == "all" else [a.case]
    summary = []
    for c in cases:
        r = run_case(c, a.secs)
        summary.append({
            "case": c, "done_at": r["done_at"], "proc_exit_at": r["proc_exit_at"],
            "killed_at_timeout": r["killed_at_timeout"],
            "final_done_exists": r["final_done_exists"],
            "testout_len": (len(r["test_out"]) if r["test_out"] else 0),
        })
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
