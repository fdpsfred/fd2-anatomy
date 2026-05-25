"""Append a single CP-2b verdict + optional unknown record. ONE site per call.

Usage:
  python tools/ail_extract/append_verdict.py \
      --site-idx 61 \
      --verdict AIL_to_AIL_mid \
      --extdef-target L_AIL_install_DIG_INI_alt_df \
      --evidence "inst=JZ 0x38e1a"

  For AIL_to_AIL / AIL_to_CRT / AIL_to_fd2common: only synth is written.
  For AIL_to_AIL_mid: both synth (verdict='unknown', target_kind='ail_internal_mid_fn_label')
                     and unknown (category='ail_internal_mid_fn_label') are written.
  For unknown_other: synth(verdict='unknown'), unknown(category=given).

Per [[feedback_strict_one_at_a_time]] + [[feedback_ail_fixup_synth_per_item]]: this
script writes exactly ONE site's records. Do not call it in a loop from a wrapper
to bulk process; each invocation must correspond to a separate Ghidra MCP
verification round in its own user-facing message.
"""
import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
SYNTH = WS / "ail_fixups_synth.jsonl"
MIDFN = WS / "ail_fixups_midfn.jsonl"
WORKLIST = WS / "pcrel32_worklist.jsonl"


def load_jsonl(p):
    return [json.loads(l) for l in p.open(encoding="utf-8") if l.strip()]


def find_worklist_entry(site_idx):
    for line in WORKLIST.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        if rec.get("site_idx") == site_idx:
            return rec
    raise SystemExit(f"site_idx {site_idx} not in pcrel32_worklist.jsonl")


def already_done(inst_addr):
    for line in SYNTH.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        if json.loads(line)["inst_addr"] == inst_addr:
            return True
    return False


def total_counts():
    return sum(1 for _ in WORKLIST.open(encoding="utf-8") if _.strip()), \
           sum(1 for _ in SYNTH.open(encoding="utf-8") if _.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--site-idx", type=int, required=True)
    ap.add_argument("--verdict", required=True,
                    choices=["AIL_to_AIL", "AIL_to_CRT", "AIL_to_fd2common",
                             "AIL_to_AIL_mid", "unknown_other"])
    ap.add_argument("--extdef-target", required=True,
                    help="EXTDEF symbol name (L_<tgtfn>_alt_<offset> for mid; "
                         "or empty string for unknown_other)")
    ap.add_argument("--target-kind", default=None,
                    help="fn_entry_exact (default for AIL/CRT/fd2common); "
                         "ail_internal_mid_fn_label (for AIL_to_AIL_mid); "
                         "auto-set if omitted")
    ap.add_argument("--evidence", required=True)
    ap.add_argument("--crt-lookup-obj", default=None,
                    help="CRT .obj source (only for AIL_to_CRT)")
    # Mid-fn extras
    ap.add_argument("--target-within-fn", default=None,
                    help="Containing AIL fn name (AIL_to_AIL_mid only)")
    ap.add_argument("--target-fn-entry", default=None,
                    help="Containing AIL fn entry addr hex like 00038d3b "
                         "(AIL_to_AIL_mid only)")
    ap.add_argument("--target-offset-from-entry", default=None,
                    help="Hex offset like 0xdf (AIL_to_AIL_mid only)")
    # Pure-unknown
    ap.add_argument("--unknown-category", default=None,
                    help="Category for unknown_other unknown record")
    ap.add_argument("--unknown-reason", default=None)
    args = ap.parse_args()

    wl = find_worklist_entry(args.site_idx)
    if already_done(wl["inst_addr"]):
        print(f"[skip] site_idx={args.site_idx} inst={wl['inst_addr']} already in synth")
        return

    # Build synth record
    synth_rec = {
        "site_idx": args.site_idx,
        "inst_addr": wl["inst_addr"],
        "opcode": wl["opcode"],
        "disp32": wl["disp32"],
        "target_addr": wl["target_addr"],
        "parent_fn": wl["parent_fn"],
        "parent_fn_entry": wl["parent_fn_entry"],
    }

    if args.verdict == "AIL_to_AIL_mid":
        target_kind = args.target_kind or "ail_internal_mid_fn_label"
        synth_rec["verdict"] = "unknown"
        synth_rec["extdef_target"] = args.extdef_target
        synth_rec["target_kind"] = target_kind
        synth_rec["evidence"] = args.evidence + " (mid-fn → method B; see midfn.jsonl)"
    elif args.verdict == "unknown_other":
        synth_rec["verdict"] = "unknown"
        synth_rec["extdef_target"] = None
        synth_rec["target_kind"] = args.target_kind or "unknown"
        synth_rec["evidence"] = args.evidence + " (See midfn.jsonl)"
    else:
        synth_rec["verdict"] = args.verdict
        synth_rec["extdef_target"] = args.extdef_target
        synth_rec["target_kind"] = args.target_kind or "fn_entry_exact"
        synth_rec["evidence"] = args.evidence
        if args.verdict == "AIL_to_CRT" and args.crt_lookup_obj:
            synth_rec["crt_lookup_obj"] = args.crt_lookup_obj

    synth_rec["inst_size"] = wl["inst_size"]
    synth_rec["disp32_offset_in_inst"] = wl["disp32_offset_in_inst"]

    # Build mid-fn record (also catches unknown_other fallback)
    midfn_rec = None
    if args.verdict == "AIL_to_AIL_mid":
        for req in ("target_within_fn", "target_fn_entry", "target_offset_from_entry"):
            if not getattr(args, req.replace("-", "_")):
                raise SystemExit(f"AIL_to_AIL_mid requires --{req.replace('_','-')}")
        midfn_rec = {
            "site_idx": args.site_idx,
            "inst_addr": wl["inst_addr"],
            "opcode": wl["opcode"],
            "disp32": wl["disp32"],
            "target_addr": wl["target_addr"],
            "parent_fn": wl["parent_fn"],
            "parent_fn_entry": wl["parent_fn_entry"],
            "reason": "target_inside_AIL_fn_body_not_entry",
            "target_within_fn": args.target_within_fn,
            "target_fn_entry": args.target_fn_entry,
            "target_offset_from_entry": args.target_offset_from_entry,
            "evidence": args.evidence,
            "category": "ail_internal_mid_fn_label",
            "target_label_name": args.extdef_target,
        }
    elif args.verdict == "unknown_other":
        midfn_rec = {
            "site_idx": args.site_idx,
            "inst_addr": wl["inst_addr"],
            "opcode": wl["opcode"],
            "disp32": wl["disp32"],
            "target_addr": wl["target_addr"],
            "parent_fn": wl["parent_fn"],
            "parent_fn_entry": wl["parent_fn_entry"],
            "reason": args.unknown_reason or "uncategorized",
            "evidence": args.evidence,
            "category": args.unknown_category or "unknown",
        }

    with SYNTH.open("a", encoding="utf-8") as f:
        f.write(json.dumps(synth_rec, ensure_ascii=False) + "\n")
    if midfn_rec is not None:
        with MIDFN.open("a", encoding="utf-8") as f:
            f.write(json.dumps(midfn_rec, ensure_ascii=False) + "\n")

    total, done = total_counts()
    short_evidence = args.evidence[:60] + ("…" if len(args.evidence) > 60 else "")
    print(f"[{done}/{total}] site_idx={args.site_idx} "
          f"verdict={args.verdict} → {args.extdef_target}  ({short_evidence})")


if __name__ == "__main__":
    main()
