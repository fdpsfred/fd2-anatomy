#!/usr/bin/env python3
"""next_batch.py - scout the next emit-review batch from routing.json.

Emits a Workflow args.functions worklist (<=limit items) for emit_review.wf.js,
so continuing / resuming is one command (no hand-listing addresses).

The `reviewed` flag + per-function commits ARE the resume checkpoint: this always
returns the NEXT not-yet-done work, so it is safe to re-run after ANY interruption
(token/usage limit, crash). Re-running picks up exactly where it stopped.

Usage:
  python tools/emit/next_batch.py --stats
  python tools/emit/next_batch.py --mode review --limit 12 [--name summon] [--target anim/aniwalk.c]
  python tools/emit/next_batch.py --mode emit   --limit 12
Output: JSON on stdout. For a batch: {batchLabel, count, remaining_in_filter, functions:[...]}.

NOTE: this reads routing.json only (the durable truth). The main agent should still
reconcile against Ghidra live (search_functions) before a batch, per the project's
audit source-of-truth rule, and warn on any drift.
"""
import argparse
import json
from pathlib import Path

ROUTING = Path(__file__).resolve().parents[2] / 'src' / 'routing.json'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--mode', choices=['review', 'emit'], default='review')
    ap.add_argument('--limit', type=int, default=12)
    ap.add_argument('--name', default='', help='substring filter on function name')
    ap.add_argument('--target', default='', help='substring filter on target .c')
    ap.add_argument('--stats', action='store_true', help='print coverage only')
    ap.add_argument('--partition', default='',
                    help='path to a tools/emit/partitions/branch_N.json; restrict '
                         'scout + stats to its file-disjoint subfiles')
    a = ap.parse_args()

    d = json.loads(ROUTING.read_text(encoding='utf-8'))

    part_subfiles = None
    if a.partition:
        pj = json.loads(Path(a.partition).read_text(encoding='utf-8'))
        part_subfiles = set(pj.get('subfiles', []))

    # with a partition manifest, scope EVERYTHING (stats + scouting) to its subfiles
    # so each worktree sees only its own file-disjoint slice of the work.
    items = ({addr: v for addr, v in d.items() if v.get('target') in part_subfiles}
             if part_subfiles is not None else d)

    total = len(items)
    emitted = sum(1 for v in items.values() if v.get('done'))
    reviewed = sum(1 for v in items.values() if v.get('reviewed'))
    await_review = sum(1 for v in items.values() if v.get('done') and not v.get('reviewed'))
    await_emit = sum(1 for v in items.values() if not v.get('done'))

    if a.stats:
        out = {'total': total, 'emitted': emitted, 'reviewed': reviewed,
               'await_review': await_review, 'await_emit': await_emit}
        if part_subfiles is not None:
            out['scope'] = 'partition:%s' % Path(a.partition).stem
        print(json.dumps(out, indent=2))
        return

    def pick(v):
        if a.mode == 'review':
            if not (v.get('done') and not v.get('reviewed')):
                return False
        else:  # emit
            if v.get('done'):
                return False
        if a.name and a.name not in v['name']:
            return False
        if a.target and a.target not in v['target']:
            return False
        return True

    rows = sorted((addr, v) for addr, v in items.items() if pick(v))
    sel = rows[:a.limit]
    fns = [{'addr': addr, 'name': v['name'], 'target': v['target'], 'mode': a.mode}
           for addr, v in sel]
    label = a.mode
    if a.name:
        label += '-' + a.name
    if a.target:
        label += '-' + a.target.replace('/', '_').replace('.', '_')
    print(json.dumps({
        'batchLabel': label, 'count': len(fns),
        'remaining_in_filter': len(rows), 'functions': fns,
    }, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
