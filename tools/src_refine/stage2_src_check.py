#!/usr/bin/env python3
"""Read-only: extract each src function's parameter names and compare to the
Stage-2 proposed names, so per-item applies know whether the src side already
matches (Ghidra-only sync) or needs a surgical src edit.

For every pending param item (workspace/src_refine/stage2_pending.json) it finds
the function DEFINITION in its home .c (the line bearing `name(`, walking
forward to the matching `)`), extracts the parameter identifiers, and reports
match / mismatch / not-found against the worklist's proposed names. Parses
nothing into renames and applies nothing.

Run after stage2_reconcile.py. Prints a summary + the mismatch/not-found list.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PEND = ROOT / "workspace/src_refine/stage2_pending.json"
OUT = ROOT / "workspace/src_refine/stage2_src_check.json"

pend = json.loads(PEND.read_text(encoding="utf-8"))["pending"]


def find_def_params(home, fname):
    """Return list of param identifiers from the C definition of fname, or None."""
    f = ROOT / home
    if not f.exists():
        return None, "home-missing"
    text = f.read_text(encoding="utf-8", errors="replace")
    # definition line: starts at col 0 (not indented => not a call/comment body),
    # contains the name followed by '(' ; exclude pure prototypes ending ';' before body
    pat = re.compile(r"(?m)^[A-Za-z_].*\b" + re.escape(fname) + r"\s*\(")
    m = pat.search(text)
    if not m:
        return None, "def-not-found"
    # capture from the '(' to the matching ')'
    i = text.index("(", m.start())
    depth = 0
    j = i
    while j < len(text):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                break
        j += 1
    arglist = text[i + 1:j].strip()
    if arglist in ("", "void"):
        return [], "ok"
    params = []
    for part in split_top(arglist):
        part = part.strip()
        if not part:
            continue
        # last identifier token = param name (handles `uint32 *p`, `char buf[]`)
        ids = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", part.replace("[", " ").replace("]", " "))
        # drop trailing array/type keywords already folded; name is last id
        params.append(ids[-1] if ids else part)
    return params, "ok"


def split_top(s):
    """Split on top-level commas (ignore commas inside parens/brackets)."""
    out, depth, cur = [], 0, ""
    for c in s:
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += c
    if cur:
        out.append(cur)
    return out


rows = []
for p in pend:
    if not p["param_pending"]:
        continue
    src_params, status = find_def_params(p["home"], p["live_name"])
    proposed = {pr["proposed"] for pr in p["param_pending"]}
    src_set = set(src_params or [])
    src_has_all_proposed = proposed.issubset(src_set) if src_params is not None else False
    # For a clean item the deliverable target is the live src param list; the
    # Ghidra sync renames each live var (positional) to the matching src name.
    live = p["live_params"]
    ghidra_map = None
    if src_has_all_proposed and src_params is not None and len(live) == len(src_params):
        ghidra_map = {live[i]: src_params[i]
                      for i in range(len(live)) if live[i] != src_params[i]}
    rows.append({
        "n": p["n"], "address": p["address"], "home": p["home"],
        "fname": p["live_name"], "status": status,
        "live_params": live,
        "src_params": src_params, "proposed": sorted(proposed),
        "src_matches": src_has_all_proposed,
        "ghidra_map": ghidra_map,
    })

clean = [r for r in rows if r["src_matches"]]
mismatch = [r for r in rows if not r["src_matches"] and r["status"] == "ok"]
notfound = [r for r in rows if r["status"] != "ok"]

OUT.write_text(json.dumps({"rows": rows}, indent=2, ensure_ascii=False) + "\n",
               encoding="utf-8")

clean_mapped = [r for r in clean if r["ghidra_map"] is not None]
clean_anom = [r for r in clean if r["ghidra_map"] is None]
print(f"param items checked : {len(rows)}")
print(f"  src matches proposed (Ghidra-only sync): {len(clean)}")
print(f"     with positional map ready           : {len(clean_mapped)}")
print(f"     LEN-MISMATCH live vs src (manual)   : {len(clean_anom)}")
print(f"  src MISMATCH (needs src edit)          : {len(mismatch)}")
print(f"  def not found / home missing           : {len(notfound)}")
if clean_anom:
    print("\n--- CLEAN but live/src param count differs (handle manually) ---")
    for r in clean_anom:
        print(f"  n={r['n']} {r['fname']} live={r['live_params']} src={r['src_params']}")
if mismatch:
    print("\n--- MISMATCH (proposed not all present in src def) ---")
    for r in mismatch:
        print(f"  n={r['n']} {r['fname']} ({r['home']})")
        print(f"     src={r['src_params']}  proposed={r['proposed']}")
if notfound:
    print("\n--- DEF NOT FOUND ---")
    for r in notfound:
        print(f"  n={r['n']} {r['fname']} status={r['status']} ({r['home']})")
print(f"\nwrote {OUT}")
