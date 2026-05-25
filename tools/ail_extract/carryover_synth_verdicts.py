"""Carry-over A3b synth verdicts from the pre-review snapshot to the
post-review pcrel32 worklist.

Justification (vs strict per-item processing):
- The CP-2b verdict per site is a manually-classified 5-way dispatch on
  (target_addr, target_kind, extdef_target).
- The post-review change is purely metadata on parent_fn / parent_fn_entry
  (12 AIL fns gained leading align-NOPs that were carved into binary_artifact
  pool). No site's `inst_addr` moved, no site's `target_addr` is now
  invalid (verified earlier: 0 sites target the carved-out NOP prefix).
- Therefore the verdict + extdef_target carry over verbatim, keyed by
  `inst_addr` (binary truth), and only the parent_fn / parent_fn_entry
  metadata get refreshed from the new worklist.

This tool is NOT a Python-derived target classifier — it never re-classifies
a verdict; it only renames metadata around the existing decision.

Inputs:
  workspace/ail_extract/pcrel32_worklist.jsonl  (post-review, from
                                                  enumerate_pcrel32_sites.py)
  workspace/ail_extract/ail_fixups_synth.jsonl  (pre-review verdicts)

Output (overwritten):
  workspace/ail_extract/ail_fixups_synth.jsonl  (same path; backed up to
                                                  audit/ail_fixups_synth.pre_carryover.jsonl)

Validation gates (HARD FAIL if any fails — no graceful fallback):
  1. New worklist must have exactly the same set of `inst_addr` as old synth
     (else: a site appeared/disappeared, must be investigated per-item).
  2. Each old verdict's `target_addr` must equal the new worklist's
     `target_addr` for the same `inst_addr` (else: target moved, verdict
     stale).
  3. Site count must match (1408 expected).
"""
from __future__ import annotations

import json
import shutil
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
WORKLIST = WS / "pcrel32_worklist.jsonl"
SYNTH = WS / "ail_fixups_synth.jsonl"
BACKUP = WS / "audit" / "ail_fixups_synth.pre_carryover.jsonl"


def load_jsonl(p: Path) -> list[dict]:
    return [json.loads(l) for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]


def main():
    BACKUP.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(SYNTH, BACKUP)
    print(f"backup pre-carryover synth -> {BACKUP}")

    worklist = load_jsonl(WORKLIST)
    old_synth = load_jsonl(SYNTH)

    old_by_inst = {s["inst_addr"]: s for s in old_synth}
    new_by_inst = {s["inst_addr"]: s for s in worklist}

    only_old = sorted(set(old_by_inst) - set(new_by_inst))
    only_new = sorted(set(new_by_inst) - set(old_by_inst))
    common = sorted(set(old_by_inst) & set(new_by_inst))

    if only_old or only_new:
        print(f"FAIL: site_set diff old={len(old_by_inst)} new={len(new_by_inst)}")
        print(f"  only_in_old (gone): {len(only_old)}")
        for a in only_old[:5]: print(f"    - {a}  verdict={old_by_inst[a].get('verdict')}")
        print(f"  only_in_new (added): {len(only_new)}")
        for a in only_new[:5]: print(f"    + {a}")
        raise SystemExit(1)

    target_mismatches = []
    for inst_addr in common:
        o, n = old_by_inst[inst_addr], new_by_inst[inst_addr]
        if o["target_addr"] != n["target_addr"]:
            target_mismatches.append((inst_addr, o["target_addr"], n["target_addr"]))
    if target_mismatches:
        print(f"FAIL: {len(target_mismatches)} sites have target_addr diff:")
        for ia, ot, nt in target_mismatches[:10]:
            print(f"  {ia}: target {ot} -> {nt}")
        raise SystemExit(1)

    # Carry over: build a new synth with refreshed metadata, preserving the
    # decision (verdict / extdef_target / evidence / target_kind / extra
    # verdict-specific fields like crt_lookup_obj).
    refreshed_count = 0
    refreshed = []
    for site in worklist:
        old = old_by_inst[site["inst_addr"]]
        merged = {
            "site_idx": site["site_idx"],
            "inst_addr": site["inst_addr"],
            "opcode": site["opcode"],
            "disp32": site["disp32"],
            "target_addr": site["target_addr"],
            "parent_fn": site["parent_fn"],
            "parent_fn_entry": site["parent_fn_entry"],
            "inst_size": site["inst_size"],
            "disp32_offset_in_inst": site["disp32_offset_in_inst"],
        }
        # Preserve all verdict-specific fields from old (verdict, extdef_target,
        # target_kind, evidence, plus AIL_to_CRT's crt_lookup_obj, plus mid-fn
        # extras).
        for k in old:
            if k in merged: continue
            merged[k] = old[k]
        if (site["parent_fn"] != old["parent_fn"] or
            site["parent_fn_entry"] != old["parent_fn_entry"]):
            refreshed_count += 1
        refreshed.append(merged)

    refreshed.sort(key=lambda s: s["site_idx"])

    with SYNTH.open("w", encoding="utf-8") as f:
        for s in refreshed:
            f.write(json.dumps(s, ensure_ascii=False) + "\n")

    # Tally verdict distribution preserved
    from collections import Counter
    dist = Counter(s["verdict"] for s in refreshed)
    print(f"wrote {SYNTH} ({len(refreshed)} sites)")
    print(f"  parent_fn / parent_fn_entry metadata refreshed: {refreshed_count}")
    print(f"  verdict distribution: {dict(dist)}")
    # mid-fn case is written with verdict="unknown" + target_kind=
    # "ail_internal_mid_fn_label" (per plan A3b convention).
    expected = {"AIL_to_AIL": 757, "AIL_to_CRT": 313, "unknown": 244, "AIL_to_fd2common": 94}
    if dict(dist) != expected:
        print(f"FAIL: verdict distribution diff from CP-2b frozen {expected}")
        raise SystemExit(1)
    print("PASS: verdict distribution matches CP-2b frozen "
          "{AIL_to_AIL=757, AIL_to_CRT=313, unknown(mid-fn)=244, AIL_to_fd2common=94}")


if __name__ == "__main__":
    main()
