#!/usr/bin/env python3
"""Reconcile Stage-2 rename worklist against LIVE Ghidra state.

Read-only audit. Source of truth for "applied" = live Ghidra dump
(workspace/src_refine/ghidra_func_params.tsv: addr<TAB>name<TAB>p1<TAB>p2...),
which must be regenerated from the running Ghidra immediately before use.

For every worklist item it decides, from the LIVE names, whether the symbol
rename is applied and whether every proposed parameter rename is applied; an
item is DONE only when both hold. Emits a pending list (the true remaining
work) plus a ledger cross-check. Derives nothing and applies nothing.

Outputs workspace/src_refine/stage2_pending.json and prints a summary.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WL = ROOT / "workspace/src_refine/stage2_worklist.json"
DUMP = ROOT / "workspace/src_refine/ghidra_func_params.tsv"
GLOBALS = ROOT / "workspace/src_refine/ghidra_global_names.tsv"
LEDGER = ROOT / "tools/src_refine/data/stage2_progress.json"
OUT = ROOT / "workspace/src_refine/stage2_pending.json"

wl = json.loads(WL.read_text(encoding="utf-8"))
items = wl["items"]

# live Ghidra: address -> (name, [param names])
live = {}
for line in DUMP.read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    cols = line.split("\t")
    live[cols[0].lower()] = (cols[1], cols[2:])
# globals/data symbols (no params): address -> (name, [])
if GLOBALS.exists():
    for line in GLOBALS.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        cols = line.split("\t")
        live[cols[0].lower()] = (cols[1], [])

ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
ledger_addrs = {str(e["address"]).lower() for e in ledger["completed"] if "address" in e}

pending = []
done = []
anomalies = []      # ledger says done but live says pending
need_global = []    # global/data symbol not in function dump

for idx, it in enumerate(items, start=1):
    a = it["address"].lower()
    rec = live.get(a)
    name_pending = False
    param_pending = []
    note = ""

    if rec is None:
        # not a function in the dump (likely a global/data symbol)
        need_global.append({"n": idx, "address": a, "kind": it["kind"],
                            "name_current": it["name_current"],
                            "name_final": it["name_final"],
                            "name_rename": it["name_rename"],
                            "param_renames": it["param_renames"]})
        continue

    live_name, live_params = rec
    live_pset = set(live_params)

    if it["name_rename"] and live_name != it["name_final"]:
        name_pending = True

    for pr in it["param_renames"]:
        prop, cur = pr["proposed"], pr["current"]
        if prop not in live_pset:
            param_pending.append({"current": cur, "proposed": prop,
                                  "type": pr.get("type")})
        elif cur in live_pset:
            note += f" WARN both {cur}&{prop} live;"

    entry = {"n": idx, "address": a, "kind": it["kind"], "home": it["home"],
             "name_current": it["name_current"], "name_final": it["name_final"],
             "live_name": live_name, "live_params": live_params,
             "name_pending": name_pending, "param_pending": param_pending,
             "note": note.strip()}

    if name_pending or param_pending:
        pending.append(entry)
        if a in ledger_addrs:
            anomalies.append({"n": idx, "address": a,
                              "why": "ledger=done but live pending",
                              "name_pending": name_pending,
                              "param_pending": param_pending})
    else:
        done.append(entry)

# split pending by what they need
p_name_only = [p for p in pending if p["name_pending"] and not p["param_pending"]]
p_param_only = [p for p in pending if p["param_pending"] and not p["name_pending"]]
p_both = [p for p in pending if p["name_pending"] and p["param_pending"]]

summary = {
    "total_items": len(items),
    "done": len(done),
    "pending": len(pending),
    "pending_name_only": len(p_name_only),
    "pending_param_only": len(p_param_only),
    "pending_both": len(p_both),
    "need_global_check": len(need_global),
    "ledger_anomalies": len(anomalies),
}

OUT.write_text(json.dumps({
    "summary": summary,
    "pending": pending,
    "need_global": need_global,
    "anomalies": anomalies,
}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

print(json.dumps(summary, indent=2))
if anomalies:
    print("\n!!! LEDGER ANOMALIES (ledger=done, live=pending):")
    for x in anomalies:
        print(f"  n={x['n']} {x['address']} name_pending={x['name_pending']} "
              f"param_pending={[p['proposed'] for p in x['param_pending']]}")
if need_global:
    print("\n--- NEED GLOBAL CHECK (not in function dump) ---")
    for g in need_global:
        print(f"  n={g['n']} {g['address']} {g['kind']} "
              f"{g['name_current']} -> {g['name_final']} "
              f"name_rename={g['name_rename']} params={len(g['param_renames'])}")
print(f"\nFirst 12 pending (n: what):")
for p in pending[:12]:
    bits = []
    if p["name_pending"]:
        bits.append(f"NAME {p['name_current']}->{p['name_final']}")
    if p["param_pending"]:
        bits.append("PARAM " + ",".join(f"{x['current']}->{x['proposed']}"
                                        for x in p["param_pending"]))
    print(f"  {p['n']:>3} {p['address']} [{'; '.join(bits)}]")
print(f"\nwrote {OUT}")
