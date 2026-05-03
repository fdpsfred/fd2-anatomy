"""Phase B/C — derive per-function target prototype for 84 remaining LOW
entries (fun_low, unmatched_no_caller, mixed_signal) and emit
`apply_plan.tsv` for downstream MCP-driven set_function_prototype calls.

Pipeline phase: this is the bulk-apply planner that follows the LOW
inventory built by `inventory.py`. The two `spell_handler_id` and
`execute` categories are handled inline by the operator (they share a
"strip 3 phantom reg params" pattern); this script handles the
remaining heterogeneous LOW set with rule-based decisions.

I/O:
    reads:  workspace/lowconf_signature/lowconf_inventory.json
    writes: workspace/lowconf_signature/apply_plan.tsv
            (columns: addr<TAB>cc<TAB>prototype<TAB>rule<TAB>note)

Decision rules (priority order):

R-stdcall-zero  cc=__stdcall + reads=(F,F,F) + pcur=0
                ratify void __stdcall func(void)

R-stdcall-keep  cc=__stdcall + last_insn=RET N + N>0
                stack_count = N/4; emit void __stdcall func(<param_N>...)

R-cdecl-strip3  cc=__cdecl + reads=(F,F,F) + pcur >= 3
                phantom 3 reg params; emit (pcur - 3) cdecl params

R-cdecl-keep    cc=__cdecl + reads=(F,F,F) + pcur < 3
                ratify current cdecl(<param_N>...) shape

R-fastcall-FFF  cc=__fastcall + reads=(F,F,F)
                Ghidra mis-classified; convert to void __cdecl func(void)
                if pcur == 3 (3 phantom reg with no real args).
                If pcur > 3, treat as cdecl with (pcur - 3) stack args.

R-fastcall-K    cc=__fastcall + reads has K trues (K reg args genuine)
                preserve fastcall, keep K reg + (pcur - K) stack args.

R-cdecl-reads-true  cc=__cdecl + reads has any True
                rare: __cdecl that body reads EAX/EDX/ECX. Likely
                Ghidra confusion; emit void __fastcall func(<K reg args>)
                with K = number of True reads.

The script does NOT call Ghidra; it only writes the TSV. Apply step is
done via MCP calls from the operator.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "lowconf_signature"

CATEGORIES = {"fun_low", "unmatched_no_caller", "mixed_signal"}


def truthy_count(rec: dict) -> int:
    return sum(1 for k in ("reads_eax", "reads_edx", "reads_ecx") if rec[k])


def make_param_list(n: int) -> str:
    if n <= 0:
        return "void"
    return ", ".join(f"int param_{i+1}" for i in range(n))


def make_proto(name: str, ret: str, cc: str, params: str) -> str:
    if not params:
        params = "void"
    return f"{ret} {cc} {name}({params})"


def decide(rec: dict) -> tuple[str, str, str, str] | None:
    """Return (rule, cc, prototype, note) or None to skip."""
    name = rec["name"]
    cc = rec["current_cc"]
    pcur = rec["param_count_current"]
    last_kind = rec["last_insn_kind"]
    last_n = rec["last_insn_n"]
    reads = (rec["reads_eax"], rec["reads_edx"], rec["reads_ecx"])
    rt = truthy_count(rec)

    # __stdcall rules
    if cc == "__stdcall":
        if reads == (False, False, False) and pcur == 0:
            return ("R-stdcall-zero", "__stdcall",
                    f"void __stdcall {name}(void)", "ratify zero-arg stdcall")
        if last_kind in ("RET", "RET0", "RET_N") and last_n > 0:
            n = last_n // 4
            return ("R-stdcall-keep", "__stdcall",
                    f"void __stdcall {name}({make_param_list(n)})",
                    f"RET {last_n} -> {n} stack")
        # Fallback: existing param count, stdcall
        return ("R-stdcall-asis", "__stdcall",
                f"void __stdcall {name}({make_param_list(pcur)})",
                f"ratify {pcur} stdcall args")

    # __cdecl rules
    if cc == "__cdecl":
        if reads == (False, False, False):
            if pcur >= 3:
                n = pcur - 3
                return ("R-cdecl-strip3", "__cdecl",
                        f"void __cdecl {name}({make_param_list(n)})",
                        f"strip 3 phantom reg, {n} cdecl args")
            return ("R-cdecl-keep", "__cdecl",
                    f"void __cdecl {name}({make_param_list(pcur)})",
                    f"ratify {pcur} cdecl args")
        # cdecl + body reads regs -> reads can be false positives from
        # globals/locals; ratify existing cdecl shape rather than guess.
        return ("R-cdecl-reads-true-ratify", "__cdecl",
                f"void __cdecl {name}({make_param_list(pcur)})",
                f"ratify {pcur} cdecl args; reads {reads} likely false positive")

    # __fastcall rules
    if cc == "__fastcall":
        if reads == (False, False, False):
            if pcur == 3:
                return ("R-fastcall-FFF-3", "__cdecl",
                        f"void __cdecl {name}(void)",
                        "phantom 3 reg, no body reads -> 0-arg cdecl")
            if pcur > 3:
                n = pcur - 3
                return ("R-fastcall-FFF-strip", "__cdecl",
                        f"void __cdecl {name}({make_param_list(n)})",
                        f"strip 3 phantom reg, {n} cdecl args")
            # pcur < 3 with fastcall + no body reads -> already minimal
            return ("R-fastcall-FFF-min", "__cdecl",
                    f"void __cdecl {name}({make_param_list(pcur)})",
                    f"flip to cdecl, {pcur} args")
        # fastcall + body reads K regs: ratify the existing pcur as
        # fastcall args. Ghidra's pre-Phase-7 count already reflects K
        # reg + stack from earlier classification; trust it over the
        # raw `reads_*` flags (which can fire on globals/locals).
        return ("R-fastcall-K-ratify", "__fastcall",
                f"void __fastcall {name}({make_param_list(pcur)})",
                f"ratify {pcur} fastcall args (body reads {reads})")

    return None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR)
    args = ap.parse_args()
    workdir = args.workdir

    inv = json.loads((workdir / "lowconf_inventory.json").read_text(encoding="utf-8"))
    rows = []
    for rec in inv["full_records"]:
        if rec.get("category") not in CATEGORIES:
            continue
        decision = decide(rec)
        if decision is None:
            continue
        rule, cc, proto, note = decision
        rows.append({
            "addr": rec["addr"],
            "name": rec["name"],
            "category": rec["category"],
            "old_cc": rec["current_cc"],
            "new_cc": cc,
            "old_pcur": rec["param_count_current"],
            "rule": rule,
            "prototype": proto,
            "note": note,
        })

    rows.sort(key=lambda r: (r["category"], int(r["addr"], 16)))

    tsv_path = workdir / "apply_plan.tsv"
    with tsv_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(
            f, fieldnames=["addr", "name", "category", "old_cc",
                           "new_cc", "old_pcur", "rule", "prototype", "note"],
            delimiter="\t",
        )
        w.writeheader()
        w.writerows(rows)

    by_rule: dict[str, int] = {}
    for r in rows:
        by_rule[r["rule"]] = by_rule.get(r["rule"], 0) + 1
    print(f"total entries: {len(rows)}")
    for rule, n in sorted(by_rule.items()):
        print(f"  {rule}: {n}")
    print(f"wrote {tsv_path.name}")


if __name__ == "__main__":
    main()
