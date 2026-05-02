"""Phase 4 — apply ONE batch of cc/rename changes from recommendations.json (resumable).

Pipeline phase: this is step 4 of 5 in the calling-convention audit. See
`_index.md` in this directory for the full pipeline. The actual Ghidra
mutation work happens in `ghidra_apply.java`; this Python script is the
orchestrator that handles batching, sha verification, and progress
bookkeeping.

I/O (workdir defaults to `<repo>/workspace/calling_convention_audit/`):
    reads:  recommendations.json, progress.json
    writes: batch_input.tsv (when mode=prepare)
            progress.json + rename_log.json + errors.json (when mode=consume)

Usage (loop, two modes per cycle):
    python tools/calling_convention_audit/apply_batch.py prepare \\
        [--workdir DIR] [--batch-size 50]
    # ... orchestrator now runs ghidra_apply.java via mcp__ghidra__run_script_inline ...
    python tools/calling_convention_audit/apply_batch.py consume [--workdir DIR]

Each `prepare` slices recommendations.json[last_processed_idx+1 : +N] and
writes batch_input.tsv with columns:
    addr<TAB>target_cc<TAB>suggested_name<TAB>set_varargs<TAB>recs_idx
where target_cc may be "-" meaning "no cc change, only rename". The Ghidra
side reads that TSV and writes batch_result.tsv. `consume` then merges the
result into progress.json + rename_log.json + errors.json.

Cross-session resume: each session reads progress.json and continues from
last_processed_idx + 1. SHA verification ensures audit/recommendations have
not drifted; aborts cleanly otherwise.

Status check:
    python tools/calling_convention_audit/apply_batch.py status [--workdir DIR]
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "calling_convention_audit"


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def load_progress(progress_path: Path) -> dict:
    if not progress_path.exists():
        sys.exit(f"{progress_path} missing — run classify.py first")
    return json.loads(progress_path.read_text(encoding="utf-8"))


def save_progress(progress_path: Path, p: dict) -> None:
    p["updated_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    progress_path.write_text(json.dumps(p, indent=2, ensure_ascii=False), encoding="utf-8")


def verify_shas(progress: dict, audit_path: Path, rec_path: Path) -> None:
    cur_audit = sha256_file(audit_path)
    cur_recs = sha256_file(rec_path)
    if progress.get("audit_sha") != cur_audit:
        sys.exit(f"audit.json sha drift — re-run dump+classify before apply.\n  expected={progress['audit_sha']}\n  current ={cur_audit}")
    if progress.get("recs_sha") != cur_recs:
        sys.exit(f"recommendations.json sha drift — re-run classify before apply.\n  expected={progress['recs_sha']}\n  current ={cur_recs}")


def append_jsonl(path: Path, items: list[dict]) -> None:
    if not items:
        return
    existing = []
    if path.exists():
        try:
            existing = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            existing = []
    existing.extend(items)
    path.write_text(json.dumps(existing, indent=2, ensure_ascii=False), encoding="utf-8")


def cmd_prepare(workdir: Path, batch_size: int) -> None:
    progress_path = workdir / "progress.json"
    audit_path = workdir / "audit.json"
    rec_path = workdir / "recommendations.json"
    batch_in_path = workdir / "batch_input.tsv"

    progress = load_progress(progress_path)
    verify_shas(progress, audit_path, rec_path)
    if progress["phase"] != "phase4_apply":
        sys.exit(f"phase is {progress['phase']!r}, expected phase4_apply")

    recs = json.loads(rec_path.read_text(encoding="utf-8"))["recommendations"]
    start = progress["last_processed_idx"] + 1
    end = min(start + batch_size, len(recs))

    if start >= len(recs):
        progress["phase"] = "phase5_verify"
        save_progress(progress_path, progress)
        print("ALL_DONE: phase advanced to phase5_verify")
        return

    lines = []
    skipped = 0
    for i in range(start, end):
        r = recs[i]
        needs_cc = r["recommended_cc"] != r["current_cc"]
        needs_rename = bool(r["suggested_name"])
        if not needs_cc and not needs_rename:
            skipped += 1
            continue
        new_name = r["suggested_name"] or ""
        set_varargs = "true" if r.get("set_varargs") else "false"
        target_cc = r["recommended_cc"] if needs_cc else "-"
        lines.append(f"{r['addr']}\t{target_cc}\t{new_name}\t{set_varargs}\t{i}")

    batch_in_path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")

    progress["_batch"] = {
        "start_idx": start,
        "end_idx": end,
        "skipped_in_slice": skipped,
        "queued_count": len(lines),
    }
    save_progress(progress_path, progress)
    print(f"PREPARED: idx [{start}, {end}) -> {len(lines)} actions queued (skipped {skipped} no-op rows)")
    print(f"  next: orchestrator runs ghidra_apply.java to consume {batch_in_path}")


def cmd_consume(workdir: Path) -> None:
    progress_path = workdir / "progress.json"
    rec_path = workdir / "recommendations.json"
    batch_in_path = workdir / "batch_input.tsv"
    batch_out_path = workdir / "batch_result.tsv"
    errors_path = workdir / "errors.json"
    rename_log_path = workdir / "rename_log.json"

    progress = load_progress(progress_path)
    if "_batch" not in progress:
        sys.exit("no _batch metadata in progress.json — did `prepare` run?")
    if not batch_out_path.exists():
        sys.exit(f"missing {batch_out_path} — Ghidra apply step did not produce output")

    batch_meta = progress["_batch"]
    end_idx = batch_meta["end_idx"]

    applied_cc = 0
    applied_renames = 0
    errors = []
    rename_records = []
    for raw in batch_out_path.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        parts = raw.split("\t")
        addr, status = parts[0], parts[1]
        actions = parts[2] if len(parts) > 2 else ""
        msg = parts[3] if len(parts) > 3 else ""
        if status == "ok":
            if "cc_changed" in actions: applied_cc += 1
            if "renamed" in actions:
                applied_renames += 1
                rename_records.append({"addr": addr, "change": msg, "at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")})
        elif status == "skipped":
            pass  # nothing changed (Ghidra already at target state); not an error
        else:
            errors.append({"addr": addr, "status": status, "message": msg, "actions_attempted": actions})

    progress["last_processed_idx"] = end_idx - 1
    progress["applied_cc_changes"] = progress.get("applied_cc_changes", 0) + applied_cc
    progress["applied_renames"] = progress.get("applied_renames", 0) + applied_renames
    progress["errors_count"] = progress.get("errors_count", 0) + len(errors)
    progress["program_saved_at_idx"] = end_idx - 1
    progress.pop("_batch", None)

    if errors:
        append_jsonl(errors_path, errors)
    if rename_records:
        append_jsonl(rename_log_path, rename_records)

    save_progress(progress_path, progress)
    batch_in_path.unlink(missing_ok=True)
    batch_out_path.unlink(missing_ok=True)

    recs = json.loads(rec_path.read_text(encoding="utf-8"))["recommendations"]
    if progress["last_processed_idx"] + 1 >= len(recs):
        progress["phase"] = "phase5_verify"
        save_progress(progress_path, progress)
        print(f"CONSUMED: applied cc={applied_cc} renames={applied_renames} errors={len(errors)} -> phase advanced to phase5_verify")
    else:
        print(f"CONSUMED: applied cc={applied_cc} renames={applied_renames} errors={len(errors)} (next idx={progress['last_processed_idx']+1}/{len(recs)})")


def cmd_status(workdir: Path) -> None:
    progress_path = workdir / "progress.json"
    rec_path = workdir / "recommendations.json"
    progress = load_progress(progress_path)
    recs_count = json.loads(rec_path.read_text(encoding="utf-8"))["total"]
    print(json.dumps({
        "phase": progress["phase"],
        "last_processed_idx": progress["last_processed_idx"],
        "total": recs_count,
        "applied_cc_changes": progress.get("applied_cc_changes", 0),
        "applied_renames": progress.get("applied_renames", 0),
        "errors_count": progress.get("errors_count", 0),
        "remaining": recs_count - (progress["last_processed_idx"] + 1),
    }, indent=2))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR,
                    help=f"directory holding audit.json/recommendations.json/progress.json (default: {DEFAULT_WORKDIR})")
    ap.add_argument("--batch-size", type=int, default=50)
    ap.add_argument("mode", choices=("prepare", "consume", "status"))
    args = ap.parse_args()

    if args.mode == "prepare":
        cmd_prepare(args.workdir, args.batch_size)
    elif args.mode == "consume":
        cmd_consume(args.workdir)
    else:
        cmd_status(args.workdir)


if __name__ == "__main__":
    main()
