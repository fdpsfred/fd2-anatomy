"""Compute pending worklist sites = pcrel32_worklist - ail_fixups_synth.

Per [[feedback_kb_writing_rules]] / [[feedback_audit_source_of_truth]]：避免引入
獨立 hole list 檔案（會與 worklist / synth drift），改在需要時即時 derive。

Pending site 定義 = 出現在 pcrel32_worklist.jsonl 但 inst_addr 未在
ail_fixups_synth.jsonl 出現過的 site。

Usage:
  python tools/ail_extract/list_pending_sites.py
    # 列出所有 pending site，stdout 印統計 + 分布

  python tools/ail_extract/list_pending_sites.py --snapshot
    # 額外寫一份 snapshot 到 workspace/ail_extract/pcrel32_pending.jsonl
    # （每次跑會覆寫，不入 source control，僅供瀏覽）

  python tools/ail_extract/list_pending_sites.py --next
    # 只印下一個 pending site（最小 site_idx）
"""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
WORKLIST = WS / "pcrel32_worklist.jsonl"
SYNTH = WS / "ail_fixups_synth.jsonl"
SNAPSHOT = WS / "pcrel32_pending.jsonl"


def load_jsonl(p: Path):
    out = []
    if not p.exists():
        return out
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def main():
    args = sys.argv[1:]
    snapshot = "--snapshot" in args
    next_only = "--next" in args

    worklist = load_jsonl(WORKLIST)
    synth = load_jsonl(SYNTH)
    done_inst = {v["inst_addr"] for v in synth}

    pending = [s for s in worklist if s["inst_addr"] not in done_inst]
    pending.sort(key=lambda s: s["site_idx"])

    if next_only:
        # Optional integer arg right after --next prints first N pending lines.
        k = 1
        if "--next" in args:
            i = args.index("--next")
            if i + 1 < len(args) and args[i + 1].isdigit():
                k = int(args[i + 1])
        if not pending:
            print("ALL DONE")
            return
        for n in pending[:k]:
            print(f"site_idx={n['site_idx']} inst_addr={n['inst_addr']} opcode={n['opcode']} parent={n['parent_fn']} target={n['target_addr']}")
        return

    total = len(worklist)
    done = len(synth)
    print(f"Worklist total:   {total}")
    print(f"Verdicted (synth): {done}")
    print(f"Pending:          {len(pending)}")
    print()

    if not pending:
        print("ALL DONE")
        return

    # Buckets
    holes = [s for s in pending if s["site_idx"] <= max(v["site_idx"] for v in synth)]
    tail = [s for s in pending if s["site_idx"] > max(v["site_idx"] for v in synth)]
    print(f"  Holes (site_idx <= max verdicted): {len(holes)}")
    print(f"  Tail  (site_idx >  max verdicted): {len(tail)}")
    print()

    print("Opcode distribution (pending):")
    for k, n in Counter(s["opcode"] for s in pending).most_common():
        print(f"  {k}: {n}")
    print()

    if holes:
        print("Hole inst_addr range:", holes[0]["inst_addr"], "..", holes[-1]["inst_addr"])
        unique_targets = Counter(s["target_addr"] for s in holes)
        print(f"Hole unique targets: {len(unique_targets)}")
        for addr, cnt in unique_targets.most_common(5):
            print(f"  {addr}: {cnt}")

    if snapshot:
        with SNAPSHOT.open("w", encoding="utf-8") as f:
            for s in pending:
                f.write(json.dumps(s, ensure_ascii=False) + "\n")
        print()
        print(f"Snapshot written: {SNAPSHOT}")


if __name__ == "__main__":
    main()
