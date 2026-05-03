"""Phase A — enumerate the LOW-confidence functions whose param count
classify could not pin down, and capture per-function signals so the
follow-on per-category resolution can proceed.

Pipeline phase: this is the follow-up to Phase 7 of
`tools/calling_convention_audit/`. Phase 7 emits HIGH/MEDIUM
recommendations into `param_count_recommendations.json` and silently
drops the LOW set. This script reads the same audit + recs files and
recovers the LOW set with full signal context, splitting them into the
six categories used by the manual signature pass:

    1. dispatch_callee  — caller_count=0 + name matches a known
       function-pointer table group (chapter_NN_post_action / init /
       end / chapter_event_handler_* / cast_*). These are already
       confirmed `void __cdecl func(void)`; they appear in the LOW set
       only because Phase 7 has no caller signal — we filter them out.
    2. spell_handler_id — name prefix `spell_handler_id_`. Has stack
       arg reads in body.
    3. execute          — name prefix `execute_`.
    4. fun_low          — still-named `FUN_*` after Phase 6. Subset of
       all FUN_* — only the ones that landed in the Phase 7 LOW bucket.
    5. unmatched_no_caller — caller_count=0 AND name doesn't match a
       dispatch group (i.e. the function-pointer dispatch isn't in our
       known list, OR the function is genuinely uncalled).
    6. mixed_signal     — caller_count>0 but caller signals disagree
       (`fastcall: callers disagree` or `cdecl: only N/M clean`).
    7. other            — anything else that fell into LOW.

I/O (workdir defaults to `<repo>/workspace/lowconf_signature/`):
    reads:  ../calling_convention_audit/audit.json
            ../calling_convention_audit/recommendations.json
    writes: lowconf_inventory.csv
            lowconf_inventory.json (fuller per-entry context)
            lowconf_summary.txt    (category counts + a few samples)

Usage:
    python tools/lowconf_signature/inventory.py [--workdir DIR]

The CSV columns are the seven shared by per-category resolution scripts:
    address,name,category,caller_count,stack_arg_reads,reg_ghosts,
    return_signal,reason,current_cc,current_param_count,last_insn,
    set_eax,set_edx,set_ecx,add_esp_seen,add_esp_max
"""

from __future__ import annotations

import argparse
import collections
import csv
import json
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "lowconf_signature"
AUDIT_DIR = REPO_ROOT / "workspace" / "calling_convention_audit"

PINNED_SKIP = {"0x00036cd7", "0x0004b502"}

DISPATCH_NAME_PATTERNS = [
    re.compile(r"^chapter_\d{2}_post_action$"),
    re.compile(r"^chapter_\d{2}_init$"),
    re.compile(r"^chapter_\d{2}_end$"),
    re.compile(r"^chapter_event_handler_"),
    re.compile(r"^cast_"),
]

FRAGMENT_ADDRS = {
    "0x000114fb",
    "0x00010b43",
    "0x00010c49",
    "0x00011011",
    "0x00011452",
    "0x00013994",
    "0x00015983",
}


def is_dispatch_callee(name: str) -> bool:
    return any(p.search(name) for p in DISPATCH_NAME_PATTERNS)


def categorize(audit: dict, rec_cc: str) -> tuple[str, str]:
    """Return (category, reason). Mirrors the LOW branches of
    param_count_classify.expected_param_count, but emits a category."""
    name = audit["name"]
    addr = audit["addr"]
    n_callers = audit["caller_count"]
    add_esp_seen = audit["callers_add_esp_seen"]
    set_eax = audit["callers_set_eax"]
    set_edx = audit["callers_set_edx"]
    set_ecx = audit["callers_set_ecx"]

    if addr in FRAGMENT_ADDRS:
        return ("fragment", "decompiler fragment - skip")
    if addr in PINNED_SKIP:
        return ("pinned", "pinned addr")
    if audit["is_thunk"]:
        return ("thunk", "thunk")

    # Dispatch callee filter: caller_count=0 + recognised name pattern.
    if n_callers == 0 and is_dispatch_callee(name):
        return ("dispatch_callee", "function-pointer dispatch table callee (already void)")

    if name.startswith("spell_handler_id_"):
        return ("spell_handler_id", "name prefix spell_handler_id_")
    if name.startswith("execute_"):
        return ("execute", "name prefix execute_")
    if name.startswith("FUN_"):
        return ("fun_low", "still-named FUN_* in LOW bucket")

    if n_callers == 0:
        return ("unmatched_no_caller", "no caller, no recognised name pattern")

    # Mixed signal: caller_count>0 but classify rejected.
    if rec_cc == "__fastcall":
        if set_eax not in (0, n_callers):
            return ("mixed_signal", f"fastcall eax mixed {set_eax}/{n_callers}")
        if set_edx not in (0, n_callers):
            return ("mixed_signal", f"fastcall edx mixed {set_edx}/{n_callers}")
        if set_ecx not in (0, n_callers):
            return ("mixed_signal", f"fastcall ecx mixed {set_ecx}/{n_callers}")
    if rec_cc == "__cdecl":
        if 0 < add_esp_seen < n_callers:
            return ("mixed_signal", f"cdecl partial cleanup {add_esp_seen}/{n_callers}")

    return ("other", f"LOW under {rec_cc} not matching prior buckets")


def expected_param_count(a: dict, rec_cc: str) -> tuple[int | None, str]:
    """Recreate the LOW branch of param_count_classify."""
    n_callers = a["caller_count"]
    add_esp_seen = a["callers_add_esp_seen"]
    add_esp_max = a["callers_add_esp_max"]
    add_esp_min = a["callers_add_esp_min"]
    set_eax = a["callers_set_eax"]
    set_edx = a["callers_set_edx"]
    set_ecx = a["callers_set_ecx"]
    last_n = a["last_insn_n"]

    if rec_cc == "__stdcall":
        return (last_n // 4, "HIGH")

    if rec_cc == "__fastcall":
        if n_callers == 0:
            return (None, "LOW: fastcall no caller")
        reg = 0
        if set_eax == n_callers:
            reg = 1
            if set_edx == n_callers:
                reg = 2
                if set_ecx == n_callers:
                    reg = 3
        elif set_eax == 0:
            reg = 0
        else:
            return (None, f"LOW: fastcall eax mixed {set_eax}/{n_callers}")
        if reg < 2 and not (set_edx == 0 or set_edx == n_callers):
            return (None, f"LOW: fastcall edx mixed {set_edx}/{n_callers}")
        if reg < 3 and not (set_ecx == 0 or set_ecx == n_callers):
            return (None, f"LOW: fastcall ecx mixed {set_ecx}/{n_callers}")
        stack = last_n // 4
        return (reg + stack, "HIGH")

    if rec_cc == "__cdecl":
        if n_callers == 0:
            return (None, "LOW: cdecl no caller")
        if add_esp_seen == 0:
            return (0, "HIGH")
        if add_esp_seen == n_callers and add_esp_min == add_esp_max:
            return (add_esp_max // 4, "HIGH")
        if add_esp_seen >= max(2, n_callers // 2 + (n_callers % 2)):
            return (add_esp_max // 4, "MEDIUM")
        return (None, f"LOW: cdecl partial cleanup {add_esp_seen}/{n_callers}")

    return (None, f"LOW: unknown cc {rec_cc}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR)
    ap.add_argument("--audit-dir", type=Path, default=AUDIT_DIR)
    args = ap.parse_args()
    workdir = args.workdir
    audit_dir = args.audit_dir
    workdir.mkdir(parents=True, exist_ok=True)

    audit = json.loads((audit_dir / "audit.json").read_text(encoding="utf-8"))
    recs = json.loads((audit_dir / "recommendations.json").read_text(encoding="utf-8"))["recommendations"]
    audit_by = {f["addr"]: f for f in audit["functions"]}

    rows: list[dict] = []
    cat_counter: collections.Counter = collections.Counter()
    full: list[dict] = []

    for r in recs:
        addr = r["addr"]
        a = audit_by.get(addr)
        if a is None:
            continue
        rec_cc = r["recommended_cc"]
        expected, conf_or_reason = expected_param_count(a, rec_cc)
        if expected is not None:
            # HIGH/MEDIUM — Phase 7 already covered, skip.
            continue
        category, cat_reason = categorize(a, rec_cc)
        cat_counter[category] += 1
        if category in ("dispatch_callee", "fragment", "pinned", "thunk"):
            # Filtered out — record but don't emit to CSV.
            full.append({**a, "category": category, "reason": cat_reason, "rec_cc": rec_cc})
            continue
        row = {
            "address": addr,
            "name": a["name"],
            "category": category,
            "caller_count": a["caller_count"],
            "stack_arg_reads": "",
            "reg_ghosts": "",
            "return_signal": "",
            "reason": cat_reason,
            "current_cc": rec_cc,
            "current_param_count": a["param_count_current"],
            "last_insn": f"{a['last_insn_kind']}({a['last_insn_n']})",
            "set_eax": a["callers_set_eax"],
            "set_edx": a["callers_set_edx"],
            "set_ecx": a["callers_set_ecx"],
            "add_esp_seen": a["callers_add_esp_seen"],
            "add_esp_max": a["callers_add_esp_max"],
        }
        rows.append(row)
        full.append({**a, "category": category, "reason": cat_reason, "rec_cc": rec_cc})

    rows.sort(key=lambda r: (r["category"], int(r["address"], 16)))

    csv_path = workdir / "lowconf_inventory.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else [
            "address", "name", "category", "caller_count", "stack_arg_reads",
            "reg_ghosts", "return_signal", "reason", "current_cc",
            "current_param_count", "last_insn", "set_eax", "set_edx",
            "set_ecx", "add_esp_seen", "add_esp_max",
        ])
        w.writeheader()
        w.writerows(rows)

    json_path = workdir / "lowconf_inventory.json"
    json_path.write_text(json.dumps({
        "summary": {"total_low": sum(cat_counter.values()), **dict(cat_counter)},
        "filtered_out": ["dispatch_callee", "fragment", "pinned", "thunk"],
        "actionable_rows": rows,
        "full_records": full,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    summary_path = workdir / "lowconf_summary.txt"
    lines = [f"Total LOW: {sum(cat_counter.values())}"]
    for cat, n in cat_counter.most_common():
        lines.append(f"  {cat}: {n}")
    lines.append("")
    lines.append(f"Actionable (after dispatch/fragment filter): {len(rows)}")
    by_actionable = collections.Counter(r["category"] for r in rows)
    for cat, n in by_actionable.most_common():
        lines.append(f"  {cat}: {n}")
    summary_path.write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
