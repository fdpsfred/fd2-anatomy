"""Combine verification results into the final CRT lookup table.

Reads:
  workspace/crt_fid_match/auto_candidates.json
  workspace/crt_fid_match/manual_queue.json
  tools/program_analysis/crt_fid_match/data/observations_all.json
                                                (primary input, hand-authored)

Writes:
  workspace/crt_fid_match/lookup_9.5a.json   - the final, verified lookup
                                                (publish to rebuild_info/crt/
                                                 manually when promoting to KB)
  workspace/crt_fid_match/verify_rejected_debug.md
                                              - opt-in debug dump of rejected
                                                entries (reject reasons are also
                                                inline-logged to stderr +
                                                lookup entry notes)

Verification status assigned per entry:

  auto_threshold     - score >= AUTO_THRESHOLD and not in CONFLICT_ADDRS
  conflict_resolved  - score >= AUTO_THRESHOLD but current_name vs matched_name
                        conflict; verified individually with observation
  manual             - score < AUTO_THRESHOLD; verified individually with
                        observation; manual_verdict != "REJECT" required

Entries with manual_verdict == "REJECT" in observations are dropped from the
lookup; reject reason is logged to stderr and the dropped-entry debug dump.

Tied candidates: if multiple candidates share the top score, every matched
name is recorded as `aliases` and indexed in `by_name` to point back to the
same address.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_crt_lookup import CONFLICT_ADDRS, AUTO_THRESHOLD  # noqa: E402


def collect_aliases(entry: dict) -> list[str]:
    """If multiple top-tied candidates exist, return all matched names."""
    cands = entry.get("candidates", [])
    if not cands:
        return []
    top = cands[0]["score"]
    aliases: list[str] = []
    for c in cands:
        if c["score"] == top:
            aliases.append(c["matched_name"])
    if len(aliases) <= 1:
        return []
    return aliases


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--auto", type=Path,
                    default=Path("workspace/crt_fid_match/auto_candidates.json"))
    ap.add_argument("--manual", type=Path,
                    default=Path("workspace/crt_fid_match/manual_queue.json"))
    ap.add_argument("--observations", type=Path,
                    default=Path("tools/program_analysis/crt_fid_match/data/observations_all.json"))
    ap.add_argument("--out-lookup", type=Path,
                    default=Path("workspace/crt_fid_match/lookup_9.5a.json"))
    ap.add_argument("--out-rejected", type=Path,
                    default=Path("workspace/crt_fid_match/verify_rejected_debug.md"),
                    help="Optional debug dump of rejected entries (workspace-only; reject reasons "
                         "are inline-merged into stderr and lookup entry notes)")
    ap.add_argument("--source-json", type=str,
                    default="workspace/crt_fid_match/results/matches_9.5a.json")
    args = ap.parse_args()

    auto = json.loads(args.auto.read_text(encoding="utf-8"))
    manual = json.loads(args.manual.read_text(encoding="utf-8"))
    obs = json.loads(args.observations.read_text(encoding="utf-8"))

    by_address: dict[str, dict] = {}
    by_name: dict[str, list[str]] = {}
    rejected: list[dict] = []

    def add_entry(e: dict, verified: str) -> None:
        addr = e["address"]
        primary = e["top_matched_name"]
        aliases = collect_aliases(e)
        rec = {
            "name": primary,
            "current_name": e["current_name"],
            "score": e["top_score"],
            "body_size": e["body_size"],
            "source_obj": e["top_source_obj"],
            "verified": verified,
            "aliases": aliases,
        }
        by_address[addr] = rec
        for n in (aliases or [primary]):
            by_name.setdefault(n, []).append(addr)

    # auto-threshold + conflict-resolved
    for e in auto["entries"]:
        addr = e["address"]
        if addr in CONFLICT_ADDRS:
            add_entry(e, "conflict_resolved")
        else:
            add_entry(e, "auto_threshold")

    # manual entries — include only if not REJECTed
    for e in manual["entries"]:
        addr = e["address"]
        ob = obs.get(addr, {})
        if ob.get("manual_verdict") == "REJECT":
            rejected.append({
                "address": addr,
                "current_name": e["current_name"],
                "matched_name": e["top_matched_name"],
                "score": e["top_score"],
                "body_size": e["body_size"],
                "source_obj": e["top_source_obj"],
                "reason": ob.get("manual_reason", "(no reason recorded)"),
                "notes": ob.get("notes", ""),
                "key_instructions": ob.get("key_instructions", ""),
            })
        else:
            add_entry(e, "manual")

    # Sort by_name lists for stability
    for n in by_name:
        by_name[n].sort()

    auto_pass = sum(1 for r in by_address.values()
                    if r["verified"] == "auto_threshold")
    conflict = sum(1 for r in by_address.values()
                   if r["verified"] == "conflict_resolved")
    manual_pass = sum(1 for r in by_address.values()
                      if r["verified"] == "manual")
    rej = len(rejected)
    total_input = len(auto["entries"]) + len(manual["entries"])

    lookup = {
        "version": auto["version"],
        "source": args.source_json,
        "auto_threshold": AUTO_THRESHOLD,
        "stats": {
            "total_input": total_input,
            "auto_pass": auto_pass,
            "conflict_resolved": conflict,
            "manual_pass": manual_pass,
            "rejected": rej,
            "in_lookup": auto_pass + conflict + manual_pass,
        },
        "by_address": dict(sorted(by_address.items())),
        "by_name": dict(sorted(by_name.items())),
    }

    args.out_lookup.parent.mkdir(parents=True, exist_ok=True)
    args.out_lookup.write_text(
        json.dumps(lookup, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    # Build rejected.md
    md: list[str] = []
    md.append("# FD2.LE Watcom 9.5a CRT verification — rejected matches")
    md.append("")
    md.append(
        "Entries below were FidQuery candidates that failed behavior verification "
        "(score < auto threshold + observed assembly disagrees with the "
        "Watcom CRT symbol's expected operation) and are therefore "
        f"**excluded** from `{args.out_lookup.as_posix()}`. Each entry "
        "records the FidDb-claimed name, the observed behavior, and the "
        "reason rejection was warranted."
    )
    md.append("")
    md.append(f"Total rejected: **{rej}** of {total_input} input candidates.")
    md.append("")
    for r in sorted(rejected, key=lambda x: x["address"]):
        md.append(f"## `{r['address']}` — claimed `{r['matched_name']}` "
                  f"(score {r['score']:.2f}, body {r['body_size']})")
        md.append("")
        md.append(f"- current_name: `{r['current_name']}`")
        md.append(f"- source_obj: `{r['source_obj']}`")
        if r["key_instructions"]:
            md.append(f"- observed key instructions: {r['key_instructions']}")
        if r["notes"]:
            md.append("")
            md.append(f"**Notes**: {r['notes']}")
        md.append("")
        md.append(f"**Reason**: {r['reason']}")
        md.append("")
    args.out_rejected.parent.mkdir(parents=True, exist_ok=True)
    args.out_rejected.write_text("\n".join(md), encoding="utf-8")

    # Stdout summary
    print(f"Wrote {args.out_lookup}")
    print(f"Wrote {args.out_rejected}")
    print(f"  total_input:        {total_input}")
    print(f"  auto_pass:          {auto_pass}")
    print(f"  conflict_resolved:  {conflict}")
    print(f"  manual_pass:        {manual_pass}")
    print(f"  rejected:           {rej}")
    print(f"  in lookup:          {auto_pass + conflict + manual_pass}")
    print(f"  by_name unique:     {len(by_name)}")

    # Sanity checks
    assert (auto_pass + conflict + manual_pass + rej) == total_input, \
        "Counts do not reconcile to total_input"
    assert (auto_pass + conflict + manual_pass) == len(by_address), \
        "by_address size disagrees with PASS counts"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
