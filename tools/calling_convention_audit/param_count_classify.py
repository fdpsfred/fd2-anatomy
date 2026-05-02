"""Phase 7 — derive expected stack/reg parameter count for each function from caller signals.

Pipeline phase: this is the cleanup pass that follows Phase 5 (cc verify).
Phase 6 (`ghidra_param_cleanup.java`) renames `arg_eax_in/edx_in/ecx_in`
storage-ghost parameter names to neutral `param_N`. Phase 7 (this script +
`ghidra_param_count_apply.java`) corrects the actual parameter COUNT to
match what callers really pass — Ghidra leaves the old phantom 3 reg-arg
slots in place when it changes a function from `__fastcall` to `__cdecl`.

I/O (workdir defaults to `<repo>/workspace/calling_convention_audit/`):
    reads:  audit.json, recommendations.json
    writes: param_count_recommendations.json
            param_count_input.tsv (input to ghidra_param_count_apply.java)

Usage:
    python tools/calling_convention_audit/param_count_classify.py [--workdir DIR]

Logic — caller behaviour is the source of truth for the original source
signature:

    __stdcall  : count = last_insn_n / 4   (callee pops N bytes -> N/4 stack args)  HIGH
    __fastcall : reg_count = max consecutive prefix of (EAX, EDX, ECX) that ALL
                 callers either all set or all skip.  stack_count = last_insn_n / 4.
                 total = reg_count + stack_count.  HIGH if all callers agree.
    __cdecl    : count = max(callers_add_esp_K) / 4  (caller-cleanup signature).
                 HIGH if all callers do ADD ESP and agree on K.
                 HIGH if NO caller does ADD ESP -> count = 0 (no stack args).
                 MEDIUM if mixed cleanup pattern.
                 LOW if 0 callers known.

Skipped (no recommendation emitted):
    - thunks (Ghidra inherits from target naturally)
    - pinned addresses (0x36cd7, 0x4b502 — manually verified)
    - LOW-confidence cases (avoid breaking ABI when signal is unreliable)
"""

from __future__ import annotations

import argparse
import collections
import datetime
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "calling_convention_audit"

PINNED_SKIP = {"0x00036cd7", "0x0004b502"}


def all_or_none(num: int, total: int) -> bool:
    return num == 0 or num == total


def expected_param_count(a: dict, r: dict) -> tuple[int | None, str, str]:
    """Return (expected_count, confidence, reason). expected_count=None means LOW (skip)."""
    cc = r["recommended_cc"]
    n_callers = a["caller_count"]
    add_esp_seen = a["callers_add_esp_seen"]
    add_esp_max = a["callers_add_esp_max"]
    add_esp_min = a["callers_add_esp_min"]
    set_eax = a["callers_set_eax"]
    set_edx = a["callers_set_edx"]
    set_ecx = a["callers_set_ecx"]
    last_n = a["last_insn_n"]

    if cc == "__stdcall":
        return (last_n // 4, "HIGH", f"stdcall RET {last_n} -> {last_n//4} stack args")

    if cc == "__fastcall":
        if n_callers == 0:
            return (None, "LOW", "fastcall: no caller signal")
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
            return (None, "LOW", f"fastcall: callers disagree (eax={set_eax}/{n_callers})")
        if reg < 2 and not all_or_none(set_edx, n_callers):
            return (None, "LOW", f"fastcall: edx mixed ({set_edx}/{n_callers})")
        if reg < 3 and not all_or_none(set_ecx, n_callers):
            return (None, "LOW", f"fastcall: ecx mixed ({set_ecx}/{n_callers})")
        stack = last_n // 4
        total = reg + stack
        return (total, "HIGH", f"fastcall: reg={reg} (eax/edx/ecx={set_eax}/{set_edx}/{set_ecx}/{n_callers}), stack RET{last_n}")

    if cc == "__cdecl":
        if n_callers == 0:
            return (None, "LOW", "cdecl: no caller signal")
        if add_esp_seen == 0:
            return (0, "HIGH", f"cdecl: no caller cleanup (n_callers={n_callers}) -> 0 stack args")
        if add_esp_seen == n_callers and add_esp_min == add_esp_max:
            return (add_esp_max // 4, "HIGH", f"cdecl: all {n_callers} callers ADD ESP {add_esp_max} -> {add_esp_max//4} args")
        if add_esp_seen >= max(2, n_callers // 2 + (n_callers % 2)):
            return (add_esp_max // 4, "MEDIUM", f"cdecl: {add_esp_seen}/{n_callers} clean, max={add_esp_max} (using max)")
        return (None, "LOW", f"cdecl: only {add_esp_seen}/{n_callers} clean stack")

    return (None, "LOW", f"unknown cc {cc}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR,
                    help=f"workdir (default: {DEFAULT_WORKDIR})")
    args = ap.parse_args()
    workdir = args.workdir

    audit_path = workdir / "audit.json"
    rec_path = workdir / "recommendations.json"
    out_rec = workdir / "param_count_recommendations.json"
    out_tsv = workdir / "param_count_input.tsv"

    audit = json.loads(audit_path.read_text(encoding="utf-8"))
    recs = json.loads(rec_path.read_text(encoding="utf-8"))["recommendations"]
    audit_by = {f["addr"]: f for f in audit["functions"]}

    rows = []
    skipped = collections.Counter()

    for r in recs:
        addr = r["addr"]
        if addr in PINNED_SKIP:
            skipped["pinned"] += 1
            continue
        a = audit_by[addr]
        if a["is_thunk"]:
            skipped["thunk"] += 1
            continue
        expected, confidence, reason = expected_param_count(a, r)
        current = a["param_count_current"]
        if expected is None:
            skipped["low_confidence"] += 1
            continue
        if expected == current:
            skipped["already_match"] += 1
            continue
        rows.append({
            "addr": addr,
            "name": a["name"],
            "cc": r["recommended_cc"],
            "current_count": current,
            "expected_count": expected,
            "delta": current - expected,
            "confidence": confidence,
            "reason": reason,
        })

    rows.sort(key=lambda x: int(x["addr"], 16))

    delta_dist = collections.Counter(r["delta"] for r in rows)
    conf_dist = collections.Counter(r["confidence"] for r in rows)

    rec_doc = {
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "total_in_recs": len(recs),
        "queued": len(rows),
        "skipped": dict(skipped),
        "delta_distribution": dict(sorted(delta_dist.items())),
        "confidence_distribution": dict(conf_dist),
        "recommendations": rows,
    }
    out_rec.write_text(json.dumps(rec_doc, indent=2, ensure_ascii=False), encoding="utf-8")

    out_tsv.write_text(
        "\n".join(f"{r['addr']}\t{r['expected_count']}\t{r['confidence']}" for r in rows) + ("\n" if rows else ""),
        encoding="utf-8",
    )

    print(f"queued: {len(rows)}")
    print(f"skipped: {dict(skipped)}")
    print(f"delta dist: {dict(sorted(delta_dist.items()))}")
    print(f"confidence dist: {dict(conf_dist)}")
    print(f"wrote {out_rec.name} and {out_tsv.name}")


if __name__ == "__main__":
    main()
