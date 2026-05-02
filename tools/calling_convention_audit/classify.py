"""Phase 2 — read audit.json, decide each function's recommended calling convention.

Pipeline phase: this is step 2 of 5 in the calling-convention audit. See
`_index.md` in this directory for the full pipeline overview, and
`program_info/calling_convention.md` for the ABI rules being applied.

I/O (workdir defaults to `<repo>/workspace/calling_convention_audit/`):
    reads:  audit.json
    writes: recommendations.json
            errors.json   (re-initialised; conflicts logged here)
            progress.json (phase advanced to phase4_apply, sha pinned)

Usage:
    python tools/calling_convention_audit/classify.py
    python tools/calling_convention_audit/classify.py --workdir /path/to/workdir

Classification cascade (first-match):

    1. PINNED addresses override all rules:
         0x36cd7 crt_frame_setup -> __stdcall
         0x4b502                 -> __fastcall (Borland CRT helper, EAX = struct ptr)
    2. is_thunk -> inherit from thunked_addr (resolved in pass 2)
    3. last_insn_kind == 'RETN':
         reads_eax/edx/ecx OR any callers_set_eax/edx/ecx > 0 -> __fastcall
         else -> __stdcall
    4. last_insn_kind == 'TAIL_JMP' with direct in-binary target that is a
       function entry -> inherit (pass 2). Otherwise -> __cdecl.
    5. caller_count > 0 AND callers_add_esp_seen == caller_count -> __cdecl
       (every recorded call site cleans the stack -> callee MUST be cdecl)
    6. caller_count > 0 AND callers_add_esp_seen == 0 AND any callers_set_*
       > 0 -> __fastcall (no caller cleans, at least one passes args via
       EAX/EDX/ECX)
    7. caller_count == 0 AND any reads_eax/edx/ecx -> __fastcall
    8. otherwise -> __cdecl  (Borland 32-bit default)

Naming check: any function name matching r'_(cdecl|stdcall|fastcall|thiscall)
(_|$)' whose suffix disagrees with recommended_cc gets a `suggested_name`
(suffix stripped).

Output recommendations.json sorted by entry-point address. progress.json
receives the sha256 of audit.json + recommendations.json so Phase 4 can
detect drift.
"""

from __future__ import annotations

import argparse
import collections
import datetime
import hashlib
import json
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "calling_convention_audit"

PINNED = {
    "0x00036cd7": ("__stdcall", "Borland CRT stack-probe helper (XCHG/CALL/MOV/RET 4)"),
    "0x0004b502": ("__fastcall", "Borland CRT helper, takes struct* in EAX, RET 12"),
}

CC_SUFFIX_RE = re.compile(r"_(cdecl|stdcall|fastcall|thiscall)(_|$)", re.IGNORECASE)


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def normalize_addr(a: str) -> str:
    if not a:
        return ""
    a = a.lower()
    if a.startswith("0x"):
        return a
    return "0x" + a


def base_classify(rec: dict) -> tuple[str, str]:
    """Return (cc, reason) without resolving thunk/tail-jmp inheritance. Pass 1."""
    addr = normalize_addr(rec["addr"])
    if addr in PINNED:
        cc, why = PINNED[addr]
        return cc, f"pinned: {why}"

    if rec["is_thunk"]:
        return "INHERIT_THUNK", f"thunk -> {rec['thunked_addr']}"

    kind = rec["last_insn_kind"]
    reads_any = rec["reads_eax"] or rec["reads_edx"] or rec["reads_ecx"]
    callers_set_reg = (
        rec["callers_set_eax"] + rec["callers_set_edx"] + rec["callers_set_ecx"]
    )

    if kind == "RETN":
        if reads_any or callers_set_reg > 0:
            return "__fastcall", f"RET {rec['last_insn_n']} + reg evidence"
        return "__stdcall", f"RET {rec['last_insn_n']} no reg evidence"

    if kind == "TAIL_JMP":
        tgt = rec["tail_jmp_target"]
        if tgt.startswith("0x") and ":not_function_entry" not in tgt and tgt != "indirect":
            return "INHERIT_TAIL", f"tail-jmp -> {tgt}"
        return "__cdecl", f"tail-jmp ({tgt or 'indirect'}) — default"

    cc_count = rec["caller_count"]
    add_esp_seen = rec["callers_add_esp_seen"]

    if cc_count > 0 and add_esp_seen == cc_count:
        return "__cdecl", f"all {cc_count} callers ADD ESP (cleanup)"

    if cc_count > 0 and add_esp_seen == 0 and callers_set_reg > 0:
        regs = []
        if rec["callers_set_eax"]: regs.append(f"EAX*{rec['callers_set_eax']}")
        if rec["callers_set_edx"]: regs.append(f"EDX*{rec['callers_set_edx']}")
        if rec["callers_set_ecx"]: regs.append(f"ECX*{rec['callers_set_ecx']}")
        return "__fastcall", f"no cleanup + callers set {','.join(regs)} (n={cc_count})"

    if cc_count == 0 and reads_any:
        regs = []
        if rec["reads_eax"]: regs.append("EAX")
        if rec["reads_edx"]: regs.append("EDX")
        if rec["reads_ecx"]: regs.append("ECX")
        return "__fastcall", f"no callers; prologue reads {','.join(regs)}"

    if kind == "RET0":
        return "__cdecl", f"RET0 default (callers={cc_count}, addEsp={add_esp_seen}, mixed/no-evidence)"

    if kind in ("OTHER", "NONE"):
        return "__cdecl", f"{kind} ({rec['last_insn_mnem']}) — default"

    return "__cdecl", "fallthrough default"


def resolve_inheritance(records: dict[str, dict], by_addr: dict[str, dict]) -> None:
    """Pass 2: resolve INHERIT_THUNK / INHERIT_TAIL by following the target's recommendation.

    Cycles broken at depth > 8; unresolved chains fall back to __cdecl.
    """
    MAX_DEPTH = 8
    changed = True
    iteration = 0
    while changed and iteration < MAX_DEPTH:
        changed = False
        iteration += 1
        for addr, rec in records.items():
            if rec["recommended_cc"] not in ("INHERIT_THUNK", "INHERIT_TAIL"):
                continue
            if rec["recommended_cc"] == "INHERIT_THUNK":
                tgt_addr = normalize_addr(by_addr[addr]["thunked_addr"])
            else:
                tgt_raw = by_addr[addr]["tail_jmp_target"]
                tgt_addr = normalize_addr(tgt_raw.split(":")[0])
            tgt = records.get(tgt_addr)
            if tgt is None:
                rec["recommended_cc"] = "__cdecl"
                rec["reason"] = f"{rec['reason']}; target {tgt_addr} not a known function -> default"
                changed = True
            elif tgt["recommended_cc"] not in ("INHERIT_THUNK", "INHERIT_TAIL"):
                rec["recommended_cc"] = tgt["recommended_cc"]
                rec["reason"] = f"{rec['reason']}; inherited from {tgt_addr} ({tgt['recommended_cc']})"
                changed = True
    for rec in records.values():
        if rec["recommended_cc"] in ("INHERIT_THUNK", "INHERIT_TAIL"):
            rec["recommended_cc"] = "__cdecl"
            rec["reason"] = f"{rec['reason']}; unresolved chain -> default"


def name_cleanup(name: str, recommended_cc: str) -> str | None:
    """If name has a cc suffix that disagrees with recommended_cc, return cleaned name."""
    m = CC_SUFFIX_RE.search(name)
    if not m:
        return None
    suffix_cc = "__" + m.group(1).lower()
    if suffix_cc.lower() == recommended_cc.lower():
        return None
    cleaned = CC_SUFFIX_RE.sub(lambda mm: mm.group(2) or "", name)
    cleaned = cleaned.rstrip("_")
    if not cleaned or cleaned == name:
        return None
    return cleaned


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR,
                    help=f"directory holding audit.json/recommendations.json/progress.json (default: {DEFAULT_WORKDIR})")
    args = ap.parse_args()
    workdir = args.workdir
    workdir.mkdir(parents=True, exist_ok=True)

    audit_path = workdir / "audit.json"
    rec_path = workdir / "recommendations.json"
    progress_path = workdir / "progress.json"

    audit = json.loads(audit_path.read_text(encoding="utf-8"))
    funcs = audit["functions"]

    records: dict[str, dict] = {}
    for f in funcs:
        addr = normalize_addr(f["addr"])
        cc, why = base_classify(f)
        records[addr] = {
            "addr": addr,
            "name": f["name"],
            "current_cc": f["current_cc"],
            "recommended_cc": cc,
            "reason": why,
            "is_thunk": f["is_thunk"],
            "last_insn_kind": f["last_insn_kind"],
            "last_insn_n": f["last_insn_n"],
            "param_count_current": f["param_count_current"],
            "set_varargs": False,
            "suggested_name": None,
            "pinned": addr in PINNED,
        }

    resolve_inheritance(records, {normalize_addr(f["addr"]): f for f in funcs})

    for rec in records.values():
        suggested = name_cleanup(rec["name"], rec["recommended_cc"])
        if suggested:
            rec["suggested_name"] = suggested

    sorted_recs = sorted(records.values(), key=lambda r: int(r["addr"], 16))

    n_cc_changes = sum(1 for r in sorted_recs if r["recommended_cc"] != r["current_cc"])
    n_renames = sum(1 for r in sorted_recs if r["suggested_name"])
    cc_dist = collections.Counter(r["recommended_cc"] for r in sorted_recs)
    current_dist = collections.Counter(r["current_cc"] for r in sorted_recs)

    rec_doc = {
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "total": len(sorted_recs),
        "current_cc_dist": dict(current_dist),
        "recommended_cc_dist": dict(cc_dist),
        "cc_change_count": n_cc_changes,
        "rename_count": n_renames,
        "recommendations": sorted_recs,
    }
    rec_path.write_text(json.dumps(rec_doc, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Wrote {rec_path}: {len(sorted_recs)} records, {n_cc_changes} cc changes, {n_renames} renames")
    print(f"current cc dist: {dict(current_dist)}")
    print(f"recommended cc dist: {dict(cc_dist)}")

    audit_sha = sha256_file(audit_path)
    recs_sha = sha256_file(rec_path)
    if progress_path.exists():
        progress = json.loads(progress_path.read_text(encoding="utf-8"))
    else:
        progress = {}
    progress.update({
        "phase": "phase4_apply",
        "ghidra_program": "FD2.LE",
        "batch_size": progress.get("batch_size", 50),
        "total": len(sorted_recs),
        "last_processed_idx": progress.get("last_processed_idx", -1),
        "applied_cc_changes": progress.get("applied_cc_changes", 0),
        "applied_renames": progress.get("applied_renames", 0),
        "auto_downgraded": progress.get("auto_downgraded", 0),
        "errors_count": progress.get("errors_count", 0),
        "audit_sha": audit_sha,
        "recs_sha": recs_sha,
        "program_saved_at_idx": progress.get("program_saved_at_idx", -1),
        "updated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
    })
    if progress.get("_last_recs_sha") and progress["_last_recs_sha"] != recs_sha:
        progress["last_processed_idx"] = -1
        progress["applied_cc_changes"] = 0
        progress["applied_renames"] = 0
    progress["_last_recs_sha"] = recs_sha
    progress_path.write_text(json.dumps(progress, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Updated {progress_path} (phase=phase4_apply)")


if __name__ == "__main__":
    main()
