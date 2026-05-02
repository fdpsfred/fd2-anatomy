"""FDFIELD.DAT char_spawn_records decoder — decodes the per-chapter character
spawn table embedded in each chapter's tile_event_data_table entry.

Each tile_event entry layout:
    +0..+2       header (shap_id_byte, party_member_count, active_char_count)
    +3..+0x32    16 x 3-byte turn-event hooks
    +0x33..+0x52 16 x 2-byte tile-step-event hooks (consequence_idx, event_type)
    +0x53..+0x82 16 x 3-byte tile_pickup table (kind, param_lo, param_hi)
    +0x83..      char_spawn_records (x active_char_count, stride 0x1A)

char_spawn_record (0x1A bytes; derived from init_runtime_char_for_battle @ 0x10C50):
    +0x00 bTeam              (0=enemy, 1=NPC, 2=player)
    +0x01 char_id            (<0x44 = player class; >=0x44 = enemy class)
    +0x02 ai_target_id
    +0x04 level
    +0x05..+0x0C inventory slots (0x05==0xFF means slot 0 special)
    +0x0D..+0x10 spell_bitmap (4 bytes)
    +0x11 ai_class+flags     (low 4 = ai_class 0..11; high 4 = flags)
    +0x12 ai_aux
    +0x13 ai_target_pos
    +0x15 race_id            (portrait_set conditional spawn filter)
    +0x16 pickup_kind
    +0x17..+0x18 pickup_param (u16 LE)

CLI:
    python tools/decoders/fdfield_char_spawn_decoder.py --chapter 1
    python tools/decoders/fdfield_char_spawn_decoder.py --all
    python tools/decoders/fdfield_char_spawn_decoder.py --no-pickup --chapter 5
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

# tools/decoders/fdfield_char_spawn_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fdfield_event_decoder as fd  # noqa: E402

CHAR_RECORDS_OFFSET = 0x83
RECORD_STRIDE = 0x1A
TILE_PICKUP_OFFSET = 0x53
TILE_PICKUP_COUNT = 16
TILE_STEP_EVENT_OFFSET = 0x33
TILE_STEP_EVENT_COUNT = 16
TILE_STEP_EVENT_STRIDE = 2

AI_CLASS_NAMES = {
    0: "default_attacker",
    1: "defensive_kiter",
    2: "aggressive_physical",
    3: "targeted_approach",
    4: "pass_turn",
    5: "item_pickup",
    6: "(unused?)",
    7: "charge_dash",
    8: "hard_skip",
    9: "advance_to_char",
    10: "hardcoded_attack",
    11: "smart_caster",
}


def decode_record(entry: bytes, record_idx: int) -> dict | None:
    base = CHAR_RECORDS_OFFSET + record_idx * RECORD_STRIDE
    if base + RECORD_STRIDE > len(entry):
        return None
    rec = entry[base:base + RECORD_STRIDE]
    inv_special = rec[0x05] == 0xFF
    inv_items: list[int] = []
    if inv_special:
        inv_items.append(rec[0x06])
    else:
        inv_items.extend([rec[0x05], rec[0x06]])
    for i in range(6):
        b = rec[0x07 + i]
        if b != 0xFF:
            inv_items.append(b)
    return {
        "team": rec[0x00],
        "char_id": rec[0x01],
        "ai_target_id": rec[0x02],
        "_pad03": rec[0x03],
        "level": rec[0x04],
        "inv_special": inv_special,
        "inv_raw": list(rec[0x05:0x0D]),
        "inv_items_packed": inv_items,
        "spell_bitmap": list(rec[0x0D:0x11]),
        "ai_class": rec[0x11] & 0x0F,
        "ai_class_flags": (rec[0x11] >> 4) & 0x0F,
        "ai_class_raw": rec[0x11],
        "ai_aux": rec[0x12],
        "ai_target_pos": rec[0x13],
        "_pad14": rec[0x14],
        "race_id": rec[0x15],
        "pickup_kind": rec[0x16],
        "pickup_param": rec[0x17] | (rec[0x18] << 8),
        "_pad19": rec[0x19],
        "_offset_in_entry": base,
    }


def decode_tile_pickup(entry: bytes, idx: int) -> dict | None:
    base = TILE_PICKUP_OFFSET + idx * 3
    if base + 3 > len(entry):
        return None
    return {"kind": entry[base], "param": entry[base + 1] | (entry[base + 2] << 8)}


def decode_tile_step_event(entry: bytes, idx: int) -> dict | None:
    base = TILE_STEP_EVENT_OFFSET + idx * TILE_STEP_EVENT_STRIDE
    if base + 2 > len(entry):
        return None
    return {"consequence_idx": entry[base], "event_type": entry[base + 1]}


def chapter_entry(blob: bytes, offsets: list[int], chapter_n: int) -> bytes:
    chapter_id = chapter_n - 1
    return fd.slice_resource(blob, offsets, fd.chapter_tile_event_idx(chapter_id))


def char_class_label(char_id: int) -> str:
    if char_id < 0x44:
        return f"player_class char_id 0x{char_id:02X}={char_id}"
    return f"enemy_class enemy_data[{char_id - 0x44}] (char_id 0x{char_id:02X})"


def fmt_record(idx: int, r: dict) -> list[str]:
    lines = []
    cl_lbl = char_class_label(r["char_id"])
    ai_name = AI_CLASS_NAMES.get(r["ai_class"], f"ai_{r['ai_class']}")
    flags = (f"flags=0x{r['ai_class_flags']:X}"
             if r["ai_class_flags"] else "flags=0")
    lines.append(
        f"  [{idx:2d}] @+0x{r['_offset_in_entry']:03X}  team={r['team']} "
        f"{cl_lbl} lv{r['level']} race={r['race_id']:2d}  ai={ai_name}({flags}) "
        f"aux=0x{r['ai_aux']:02X} target_pos=0x{r['ai_target_pos']:02X} "
        f"ai_target_id=0x{r['ai_target_id']:02X}"
    )
    if r["inv_items_packed"]:
        items = " ".join(f"0x{i:02X}" for i in r["inv_items_packed"])
        lines.append(
            f"        inv: {items}"
            + ("" if not r["inv_special"] else "  (inv_special:slot0_only)")
        )
    bitmap = " ".join(f"{b:08b}" for b in r["spell_bitmap"])
    if any(r["spell_bitmap"]):
        lines.append(f"        spells: {bitmap}")
    if r["pickup_kind"] != 0xFF or r["pickup_param"] != 0xFFFF:
        lines.append(
            f"        pickup_kind=0x{r['pickup_kind']:02X} "
            f"param=0x{r['pickup_param']:04X}"
        )
    pads = (r["_pad03"], r["_pad14"], r["_pad19"])
    if any(p != 0 for p in pads):
        lines.append(
            f"        padding bytes (+3/+0x14/+0x19): {pads} (non-zero)"
        )
    return lines


def cmd_chapter(blob: bytes, offsets: list[int], chapter_n: int,
                show_pickup: bool = True) -> None:
    chapter_id = chapter_n - 1
    entry = chapter_entry(blob, offsets, chapter_n)
    char_count = entry[2]
    print(f"=== Chapter {chapter_n} (binary id={chapter_id}, FDFIELD idx "
          f"{fd.chapter_tile_event_idx(chapter_id)}) ===")
    print(f"  entry size {len(entry)} bytes; char_count={char_count} "
          f"(records expected = {char_count} x 0x1A = {char_count * 0x1A} bytes; "
          f"records start +0x83, end +{0x83 + char_count * 0x1A:#x})")
    print(f"  +0x33..+0x52 tile-step-event x {TILE_STEP_EVENT_COUNT}:")
    actives_step: list[tuple[int, dict]] = []
    for i in range(TILE_STEP_EVENT_COUNT):
        h = decode_tile_step_event(entry, i)
        if h and h["consequence_idx"] != 0xFF:
            actives_step.append((i, h))
    if not actives_step:
        print("    (all 16 entries sentinel 0xFF/0x00 - no tile-step events)")
    else:
        for i, h in actives_step:
            handler_addr = (
                f"0x{fd.CHAPTER_EVENT_JUMP_TABLE[h['consequence_idx']]:08X}"
                if h['consequence_idx'] < len(fd.CHAPTER_EVENT_JUMP_TABLE)
                else "?"
            )
            print(
                f"    [{i:2d}] tile_event_id={i+1} "
                f"consequence_idx=0x{h['consequence_idx']:02X} "
                f"event_type=0x{h['event_type']:02X}  handler @ {handler_addr}"
            )
    if show_pickup:
        print(f"  +0x53..+0x82 tile_pickup x 16:")
        for i in range(TILE_PICKUP_COUNT):
            tp = decode_tile_pickup(entry, i)
            if not tp:
                continue
            if tp["kind"] == 0xFF and tp["param"] == 0xFFFF:
                continue
            print(f"    [{i:2d}] kind=0x{tp['kind']:02X} param=0x{tp['param']:04X}")
    print(f"  +0x83.. char_spawn_records x {char_count}:")
    for i in range(char_count):
        r = decode_record(entry, i)
        if not r:
            print(f"    [{i:2d}] (out of range)")
            break
        for line in fmt_record(i, r):
            print(line)
    used = CHAR_RECORDS_OFFSET + char_count * RECORD_STRIDE
    if used < len(entry):
        trailing = entry[used:]
        hex_short = " ".join(f"{b:02X}" for b in trailing[:32])
        print(f"  trailing bytes (+0x{used:X}..end, {len(trailing)} bytes): "
              f"{hex_short}{' ...' if len(trailing) > 32 else ''}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chapter", type=int)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--no-pickup", action="store_true",
                    help="skip tile_pickup output")
    args = ap.parse_args()
    blob = fd.FDFIELD.read_bytes()
    offsets = fd.read_offset_table(blob)
    print(f"# FDFIELD.DAT loaded: {len(blob)} bytes, "
          f"{len(offsets) - 1} resource entries")
    print()
    chapters = ([args.chapter] if args.chapter
                else (list(range(1, 31)) if args.all else None))
    if not chapters:
        ap.print_help()
        return 1
    for n in chapters:
        cmd_chapter(blob, offsets, n, show_pickup=not args.no_pickup)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
