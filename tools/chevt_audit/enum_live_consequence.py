"""Enumerate the complete set of chapter-event-handler indices that are
*referenced as a dispatch target* by FDFIELD.DAT across all 30 chapters.

The 90-entry dispatch table data_fd2_battle_ai_post_action_consequence_table
(0x51B91) is reached at runtime by FOUR paths, each sourcing its table index
from a different sub-region of the per-chapter tile_event_data_table blob
(FDFIELD resource chapter_id*3+1):

  P1 tile_event  (fd2_handle_tile_event_interaction @0x19511):
       +0x53 3-byte tile_pickup slots; when kind not in {0=item,1=gold} the
       slot is an EVENT and the u16 param is the table index.
  P3 turn_event  (fd2_fire_chapter_turn_events_for_phase @0x1A85A):
       +0x03 3-byte (turn, event_code, phase) hooks; fires when
       turn==turn_counter && phase==arg -> table[event_code](0).
  P4 tile_step   (fd2_check_tile_event_post_action -> latch @0x51A8F):
       +0x33 2-byte (consequence_idx, event_type) hooks; when
       consequence_idx!=0xFF the value is latched and dispatched next AI loop.
  P2 kill_drop   (fd2_process_battle_drop_entries @0x1AC1A, drop type 2):
       char_spawn_record +0x16 kind / +0x17 param; when the collected drop
       entry type==2 the u16 value is the table index. Only spawning records
       (race_id != 0xFF) can ever die and fire their drop.

Field extraction reuses the byte-verified decoders in tools/decoders/. This
script only adds the per-path liveness filter + aggregation. It emits ONLY
handler indices + raw evidence (it is a worklist builder, not a classifier);
mapping index -> handler address is done separately against the live Ghidra
table read.

CLI:
    python tools/chevt_audit/enum_live_consequence.py            # aggregate
    python tools/chevt_audit/enum_live_consequence.py --by-chapter
    python tools/chevt_audit/enum_live_consequence.py --json OUT.json
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "decoders"))

import fdfield_event_decoder as fev  # noqa: E402
import fdfield_char_spawn_decoder as fcs  # noqa: E402

TABLE_SIZE = 90  # 360 bytes / 4; verified via Ghidra read of 0x51B91


class Ref:
    """One dispatch-target reference of a consequence index."""

    def __init__(self, chapter: int, path: str, local_idx: int, index: int,
                 raw: str, note: str = ""):
        self.chapter = chapter
        self.path = path
        self.local_idx = local_idx
        self.index = index
        self.raw = raw
        self.note = note

    def to_dict(self) -> dict:
        return {"chapter": self.chapter, "path": self.path,
                "local_idx": self.local_idx, "index": self.index,
                "raw": self.raw, "note": self.note}

    def __str__(self) -> str:
        n = f"  {self.note}" if self.note else ""
        return (f"ch{self.chapter:02d} {self.path} slot[{self.local_idx:2d}] "
                f"-> idx 0x{self.index:02X}  raw[{self.raw}]{n}")


def enum_chapter(blob: bytes, offsets: list[int], chapter_n: int) -> list[Ref]:
    refs: list[Ref] = []
    entry = fcs.chapter_entry(blob, offsets, chapter_n)
    char_count = entry[2]

    # ---- P1: tile_event (+0x53), kind not in {0,1} and not all-0xFF sentinel
    for i in range(fcs.TILE_PICKUP_COUNT):
        tp = fcs.decode_tile_pickup(entry, i)
        if not tp:
            continue
        kind, param = tp["kind"], tp["param"]
        if kind == 0xFF and param == 0xFFFF:
            continue  # empty slot
        if kind in (0, 1):
            continue  # item / gold, not an event dispatch
        note = "" if param < TABLE_SIZE else "INDEX_OOR"
        refs.append(Ref(chapter_n, "P1_tile_event", i, param,
                        f"kind=0x{kind:02X} param=0x{param:04X}", note))

    # ---- P3: turn_event (+0x03), reachable iff turn not in {0,0xFF}
    for i, (turn, code, phase) in enumerate(fev.decode_turn_events(entry)):
        if turn == 0 and code == 0 and phase == 0:
            continue  # all-zero sentinel
        raw = f"turn={turn} code=0x{code:02X} phase={phase}"
        if turn == 0xFF:
            # never fires (turn_counter never 0xFF); record for completeness
            refs.append(Ref(chapter_n, "P3_turn_event", i, code, raw,
                            "DEAD_turn0xFF"))
        elif turn == 0:
            # turn_counter starts at 1 -> turn 0 never fires
            refs.append(Ref(chapter_n, "P3_turn_event", i, code, raw,
                            "DEAD_turn0"))
        else:
            note = "" if code < TABLE_SIZE else "INDEX_OOR"
            refs.append(Ref(chapter_n, "P3_turn_event", i, code, raw, note))

    # ---- P4: tile_step (+0x33) latch source, consequence_idx != 0xFF
    for i in range(fcs.TILE_STEP_EVENT_COUNT):
        h = fcs.decode_tile_step_event(entry, i)
        if not h or h["consequence_idx"] == 0xFF:
            continue
        idx = h["consequence_idx"]
        note = "" if idx < TABLE_SIZE else "INDEX_OOR"
        refs.append(Ref(chapter_n, "P4_tile_step", i, idx,
                        f"cons_idx=0x{idx:02X} evt_type=0x{h['event_type']:02X}",
                        note))

    # ---- P2: kill_drop (record +0x16 kind==2), spawning records only
    for i in range(char_count):
        r = fcs.decode_record(entry, i)
        if not r:
            break
        if r["pickup_kind"] != 2:
            continue
        param = r["pickup_param"]
        spawns = r["race_id"] != 0xFF
        note = "" if spawns else "NONSPAWN_race0xFF"
        if param >= TABLE_SIZE:
            note = (note + ";INDEX_OOR").lstrip(";")
        refs.append(Ref(chapter_n, "P2_kill_drop", i, param,
                        f"kind=2 param=0x{param:04X} race=0x{r['race_id']:02X}",
                        note))
    return refs


def is_live(ref: Ref) -> bool:
    """True iff this reference can actually dispatch its index at runtime."""
    if ref.note.startswith("DEAD") or ref.note == "NONSPAWN_race0xFF":
        return False
    if "INDEX_OOR" in ref.note:
        return False
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--by-chapter", action="store_true")
    ap.add_argument("--json", type=str, help="write full evidence to JSON")
    args = ap.parse_args()

    blob = fev.FDFIELD.read_bytes()
    offsets = fev.read_offset_table(blob)

    all_refs: list[Ref] = []
    for n in range(1, 31):
        all_refs.extend(enum_chapter(blob, offsets, n))

    live = [r for r in all_refs if is_live(r)]
    dead = [r for r in all_refs if not is_live(r)]

    live_by_index: dict[int, list[Ref]] = {}
    for r in live:
        live_by_index.setdefault(r.index, []).append(r)

    if args.by_chapter:
        for n in range(1, 31):
            chap = [r for r in all_refs if r.chapter == n]
            if not chap:
                continue
            print(f"=== Chapter {n} ===")
            for r in chap:
                print("  " + str(r))
            print()

    print("# ==== LIVE referenced consequence indices (dispatch-reachable) ====")
    for idx in range(TABLE_SIZE):
        if idx in live_by_index:
            paths = sorted({r.path for r in live_by_index[idx]})
            chaps = sorted({r.chapter for r in live_by_index[idx]})
            print(f"  idx 0x{idx:02X}  LIVE  x{len(live_by_index[idx]):2d}  "
                  f"paths={','.join(paths)}  chapters={chaps}")
    live_set = sorted(live_by_index)
    print(f"\n# LIVE index set ({len(live_set)}): "
          f"{[f'0x{i:02X}' for i in live_set]}")

    print("\n# ==== DEAD / non-dispatch references (present but never fire) ====")
    for r in dead:
        print("  " + str(r))

    if args.json:
        out = {
            "table_size": TABLE_SIZE,
            "live_indices": live_set,
            "live_refs": [r.to_dict() for r in live],
            "dead_refs": [r.to_dict() for r in dead],
        }
        Path(args.json).write_text(json.dumps(out, indent=2), encoding="utf-8")
        print(f"\n# wrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
