"""Render a verification report from FidDb verify queues + observed evidence.

Two modes:

* **Scaffold mode** (no --observations): emits a markdown skeleton — one
  section per queue entry listing expected behavior signatures from
  verify_rules.RULES, plus empty `Observed` and `Verdict` slots to be filled.

* **Finalize mode** (with --observations): reads recorded observations
  (callees, asm, optional decompiled C) and runs verify_rules.apply_rule()
  per entry. Emits the final markdown report with PASS/FAIL verdicts and the
  failure list when applicable.

Observations JSON schema (keyed by 8-hex address):

    {
      "00047328": {
        "callees": ["_gmtime"],
        "asm":     "<full disassembly text>",
        "decomp":  "<optional decompiled pseudocode>"
      },
      ...
    }

Usage:
    # Scaffold first
    python tools/crt_fid_match/verify_crt_samples.py \\
        --queue workspace/crt_fid_match/sample_queue.json \\
        --queue workspace/crt_fid_match/conflict_queue.json \\
        --out rebuild_info/crt_verify_report.md

    # Then finalize after recording observations
    python tools/crt_fid_match/verify_crt_samples.py \\
        --queue workspace/crt_fid_match/sample_queue.json \\
        --queue workspace/crt_fid_match/conflict_queue.json \\
        --observations workspace/crt_fid_match/observations_phaseB.json \\
        --out rebuild_info/crt_verify_report.md
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_rules import RULES, apply_rule  # noqa: E402


def render_entry(e: dict, observation: dict | None) -> tuple[str, str]:
    """Render one queue entry. Returns (markdown, verdict_label)."""
    addr = e["address"]
    sym = e["top_matched_name"]
    rule = RULES.get(sym)

    lines: list[str] = []
    lines.append(f"### `{addr}` `{sym}` — score {e['top_score']:.2f}")
    lines.append("")
    lines.append(f"- current_name: `{e['current_name']}`")
    lines.append(f"- body_size: {e['body_size']}")
    lines.append(f"- source_obj: `{e['top_source_obj']}`")
    if rule:
        lines.append(f"- expected: {rule.notes}")
        details = []
        if rule.body_size_range:
            details.append(f"body_size_range={rule.body_size_range}")
        if rule.instructions_any:
            details.append(f"instructions_any={rule.instructions_any}")
        if rule.instructions_all:
            details.append(f"instructions_all={rule.instructions_all}")
        if rule.callees_required_any:
            details.append(f"callees_required_any={rule.callees_required_any}")
        if rule.callees_required_all:
            details.append(f"callees_required_all={rule.callees_required_all}")
        if rule.callees_forbidden:
            details.append(f"callees_forbidden={rule.callees_forbidden}")
        if rule.int21_ah_any:
            details.append(f"int21_ah_any={[hex(x) for x in rule.int21_ah_any]}")
        if rule.is_leaf is not None:
            details.append(f"is_leaf={rule.is_leaf}")
        for d in details:
            lines.append(f"  - {d}")
    else:
        lines.append("- expected: *(no rule defined — ad-hoc review)*")
    lines.append("")

    verdict = "TBD"
    if observation is None:
        lines.append("**Observed**:")
        lines.append("- callees: `<TBD>`")
        lines.append("- key instructions: `<TBD>`")
        lines.append("")
        lines.append("**Verdict**: `<TBD>`")
    else:
        callees = observation.get("callees", [])
        asm_text = observation.get("asm", "")
        decomp = observation.get("decomp", "")
        body = e["body_size"]

        lines.append("**Observed**:")
        lines.append(f"- callees: `{callees}`")
        if "key_instructions" in observation:
            lines.append(f"- key instructions: `{observation['key_instructions']}`")
        if "notes" in observation:
            lines.append(f"- notes: {observation['notes']}")
        lines.append("")

        if rule is None:
            verdict_label = observation.get("manual_verdict", "AD-HOC-TBD")
            verdict = verdict_label
            lines.append(f"**Verdict** (ad-hoc): `{verdict_label}`")
            if "manual_reason" in observation:
                lines.append(f"- reason: {observation['manual_reason']}")
        else:
            failures = apply_rule(sym, asm_text, callees, body, decomp)
            if not failures:
                verdict = "PASS"
                lines.append("**Verdict**: **PASS** (all rule conditions satisfied)")
            elif failures == ["no_rule"]:
                verdict = "AD-HOC"
                lines.append("**Verdict**: AD-HOC (no rule defined)")
            else:
                verdict = "FAIL"
                lines.append("**Verdict**: **FAIL**")
                for f in failures:
                    lines.append(f"- {f}")
            # Optional manual override
            if observation.get("manual_verdict"):
                lines.append("")
                lines.append(f"**Manual override**: `{observation['manual_verdict']}`")
                if "manual_reason" in observation:
                    lines.append(f"- reason: {observation['manual_reason']}")
                verdict = observation["manual_verdict"]
    lines.append("")

    return "\n".join(lines), verdict


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--queue", action="append", required=True, type=Path,
                    help="Queue JSON from build_crt_lookup.py (repeatable).")
    ap.add_argument("--observations", type=Path, default=None,
                    help="Observations JSON keyed by address. If omitted, "
                         "emits scaffold only.")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--title", default="CRT FidDb verification report")
    args = ap.parse_args()

    obs: dict[str, dict] = {}
    if args.observations:
        obs = json.loads(args.observations.read_text(encoding="utf-8"))

    seen: set[str] = set()
    entries: list[dict] = []
    queue_summaries: list[str] = []
    for q in args.queue:
        data = json.loads(q.read_text(encoding="utf-8"))
        purpose = data.get("purpose", "")
        queue_summaries.append(f"- `{q}`: {data.get('count', 0)} entries — {purpose}")
        for e in data["entries"]:
            if e["address"] in seen:
                continue
            seen.add(e["address"])
            entries.append(e)

    entries.sort(key=lambda x: x["address"])

    body_lines: list[str] = []
    verdicts: dict[str, int] = {"PASS": 0, "FAIL": 0, "AD-HOC": 0,
                                "TBD": 0, "AD-HOC-TBD": 0}
    for e in entries:
        ob = obs.get(e["address"])
        section, verdict = render_entry(e, ob)
        verdicts[verdict] = verdicts.get(verdict, 0) + 1
        body_lines.append(section)

    out_lines: list[str] = []
    out_lines.append(f"# {args.title}")
    out_lines.append("")
    out_lines.append("## Sources")
    out_lines.append("")
    out_lines.extend(queue_summaries)
    out_lines.append("")
    out_lines.append(f"Total unique entries: **{len(entries)}**")
    out_lines.append("")
    if args.observations:
        out_lines.append("## Verdict summary")
        out_lines.append("")
        for k in ("PASS", "FAIL", "AD-HOC", "TBD", "AD-HOC-TBD"):
            v = verdicts.get(k, 0)
            if v:
                out_lines.append(f"- {k}: {v}")
        out_lines.append("")
    else:
        out_lines.append("*Scaffold mode — fill in `Observed` and `Verdict` sections, "
                         "then re-run with `--observations`.*")
        out_lines.append("")
    out_lines.append("## Entries")
    out_lines.append("")
    out_lines.extend(body_lines)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(out_lines), encoding="utf-8")
    mode = "finalize" if args.observations else "scaffold"
    print(f"[{mode}] Wrote {args.out} ({len(entries)} entries)")
    if args.observations:
        for k, v in verdicts.items():
            if v:
                print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
