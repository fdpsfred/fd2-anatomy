"""FDFIELD.DAT chapter turn-event decoder — extracts the 16 (turn, event_code,
phase) hooks from each chapter's tile_event_data_table entry and looks each
event_code up against the chapter_event jump table at 0x00051B91.

FDFIELD entry chapter_id*3+1 (= "tile_event_data_table") layout:

    +0:        shap_id_byte             FDSHAP idx = byte * 2
    +1:        party_member_count
    +2:        active_char_count
    +3..+50:   16 x 3-byte turn-event hooks: (turn: u8, event_code: u8, phase: u8)
                  phase 0 = enemy_turn_intro
                  phase 1 = end_of_player_turn
                  phase 2 = new_player_turn_intro
                  event_code = idx into compiled handler table at 0x00051B91
    +51..:     more sub-sections (char spawn records etc.)

CLI:
    python tools/decoders/fdfield_event_decoder.py --chapter 1
    python tools/decoders/fdfield_event_decoder.py --all
    python tools/decoders/fdfield_event_decoder.py --dump-opcodes
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/fdfield_event_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[2]
FDFIELD = REPO_ROOT / "fd2_game_files" / "FDFIELD.DAT"

NUM_CHAPTERS = 30
TURN_EVENT_OFFSET = 3
TURN_EVENT_COUNT = 16
TURN_EVENT_STRIDE = 3
TURN_EVENT_BLOCK_SIZE = TURN_EVENT_COUNT * TURN_EVENT_STRIDE  # 48 bytes

# Chapter event jump table at 0x00051B91 in FD2.LE (88 entries).
# event_code 0xFF in chapter data is an empty slot (turn=0xFF never matches
# the turn counter which starts at 1).
CHAPTER_EVENT_JUMP_TABLE: list[int] = [
    0x000341DB, 0x000342B5, 0x0003431D, 0x00034377, 0x000343E2, 0x00034D68,
    0x00034422, 0x00034D72, 0x00034DCD, 0x000344C2, 0x00034E3B, 0x00034565,
    0x00034594, 0x00034E90, 0x000345EA, 0x0003462E, 0x00034696, 0x000346C8,
    0x00034F02, 0x00034716, 0x000347B1, 0x000347D9, 0x00034819, 0x00034844,
    0x000348FC, 0x00034924, 0x0003499B, 0x000349D9, 0x00034A0E, 0x00034A3C,
    0x00034A7A, 0x00034B5D, 0x00034BE2, 0x00034C1E, 0x00034C6C, 0x00034C76,
    0x00034CB3, 0x00034CCC, 0x00034F42, 0x00034F74, 0x00034FCB, 0x00034FF0,
    0x0003505F, 0x00035091, 0x000350A4, 0x000350B9, 0x000350CC, 0x00035112,
    0x000351C6, 0x000351E9, 0x00035261, 0x0003529A, 0x000352E2, 0x00035321,
    0x0003535D, 0x000353DA, 0x00035487, 0x000354DD, 0x000354FE, 0x00035641,
    0x00035675, 0x000356B7, 0x00035898, 0x000358C7, 0x000358EA, 0x0003599B,
    0x000359C8, 0x00035A2F, 0x00035A48, 0x00035AB8, 0x00035B05, 0x00035B6B,
    0x00035BF2, 0x00035C23, 0x00035C32, 0x00035C79, 0x00035D60, 0x00035EBE,
    0x00035ED2, 0x00035EE6, 0x00035F5A, 0x00035F6F, 0x00035F92, 0x00036088,
    0x000360C0, 0x000360D8, 0x000360E3, 0x000360EA, 0x000360F1, 0x000360F8,
]
JUMP_TABLE_BASE = 0x00051B91

PHASE_NAME = {
    0: "enemy_turn_intro",
    1: "end_of_player_turn",
    2: "new_player_turn_intro",
}


def read_offset_table(blob: bytes) -> list[int]:
    """Parse a DAT offset table.

    DAT layout: 6-byte header, then a u32 offset array. Offset[0] is at byte 6.
    The offset table extends until offset[0]; we stop reading once the cursor
    reaches offset[0]. Returns the full N+1 offset table.
    """
    offsets: list[int] = []
    cursor = 6
    first_off = None
    while True:
        if cursor + 4 > len(blob):
            break
        (off,) = struct.unpack_from("<I", blob, cursor)
        offsets.append(off)
        if first_off is None:
            first_off = off
        cursor += 4
        if cursor >= first_off:
            break
    return offsets


def slice_resource(blob: bytes, offsets: list[int], idx: int) -> bytes:
    if idx + 1 >= len(offsets):
        raise IndexError(
            f"resource idx {idx} out of range (offset table has {len(offsets)} entries)"
        )
    return blob[offsets[idx]:offsets[idx + 1]]


def decode_turn_events(entry: bytes) -> list[tuple[int, int, int]]:
    if len(entry) < TURN_EVENT_OFFSET + TURN_EVENT_BLOCK_SIZE:
        raise ValueError(
            f"tile_event entry too short ({len(entry)} bytes) - need at least "
            f"{TURN_EVENT_OFFSET + TURN_EVENT_BLOCK_SIZE}"
        )
    out: list[tuple[int, int, int]] = []
    for i in range(TURN_EVENT_COUNT):
        base = TURN_EVENT_OFFSET + i * TURN_EVENT_STRIDE
        out.append((entry[base], entry[base + 1], entry[base + 2]))
    return out


def chapter_tile_event_idx(chapter_id_zero_based: int) -> int:
    return chapter_id_zero_based * 3 + 1


def fmt_event(turn: int, event_code: int, phase: int) -> str:
    if turn == 0xFF and event_code == 0xFF and phase == 0:
        return "  (empty/sentinel)"
    phase_name = PHASE_NAME.get(phase, f"phase{phase}?")
    if event_code < len(CHAPTER_EVENT_JUMP_TABLE):
        addr = CHAPTER_EVENT_JUMP_TABLE[event_code]
        handler_str = f"  handler @ 0x{addr:08X}"
    elif event_code == 0xFF:
        handler_str = "  handler @ (out-of-range; 0xFF sentinel)"
    else:
        handler_str = f"  handler @ ? (event_code 0x{event_code:02X} > table size)"
    return (f"  turn={turn:3d}  event_code=0x{event_code:02X}  "
            f"phase={phase} ({phase_name}){handler_str}")


def cmd_chapter(blob: bytes, offsets: list[int], chapter_n: int) -> None:
    if chapter_n < 1 or chapter_n > NUM_CHAPTERS:
        sys.stderr.write(f"chapter must be in 1..{NUM_CHAPTERS}, got {chapter_n}\n")
        sys.exit(1)
    chapter_id = chapter_n - 1
    idx = chapter_tile_event_idx(chapter_id)
    entry = slice_resource(blob, offsets, idx)
    print(f"=== Chapter {chapter_n} (binary chapter_id={chapter_id}) ===")
    print(f"  FDFIELD idx     : {idx}")
    print(f"  tile_event size : {len(entry)} bytes")
    print(f"  +0 shap-related : 0x{entry[0]:02X}")
    print(f"  +1 party_count  : {entry[1]}")
    print(f"  +2 char_count   : {entry[2]}")
    events = decode_turn_events(entry)
    nonempty = [e for e in events if e != (0, 0, 0)]
    print(f"  +3..+50 turn-events: {len(nonempty)}/16 active")
    for turn, code, phase in events:
        print(fmt_event(turn, code, phase))


def cmd_all(blob: bytes, offsets: list[int]) -> None:
    for ch in range(1, NUM_CHAPTERS + 1):
        cmd_chapter(blob, offsets, ch)
        print()


def cmd_dump_opcodes(blob: bytes, offsets: list[int]) -> None:
    occurrences: dict[int, list[tuple[int, int, int]]] = {}
    for ch in range(1, NUM_CHAPTERS + 1):
        chapter_id = ch - 1
        entry = slice_resource(blob, offsets, chapter_tile_event_idx(chapter_id))
        for turn, code, phase in decode_turn_events(entry):
            if turn == 0 and code == 0 and phase == 0:
                continue
            occurrences.setdefault(code, []).append((ch, turn, phase))
    if not occurrences:
        print("(no non-empty events found across 30 chapters)")
        return
    print(f"# Distinct event_code values across 30 chapters: {len(occurrences)}")
    print(f"# Format: event_code (hits) [chapter:turn/phase, ...]")
    for code in sorted(occurrences):
        hits = occurrences[code]
        sample = ", ".join(f"ch{c}:t{t}/p{p}" for c, t, p in hits[:8])
        more = f" ...+{len(hits) - 8}" if len(hits) > 8 else ""
        print(f"  0x{code:02X} ({len(hits)})  [{sample}{more}]")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chapter", type=int,
                    help="dump turn events for one chapter (1..30)")
    ap.add_argument("--all", action="store_true",
                    help="dump all 30 chapters")
    ap.add_argument("--dump-opcodes", action="store_true",
                    help="aggregate event_code values across 30 chapters")
    args = ap.parse_args()
    if not (args.chapter or args.all or args.dump_opcodes):
        ap.print_help()
        return 1
    blob = FDFIELD.read_bytes()
    offsets = read_offset_table(blob)
    print(f"# FDFIELD.DAT loaded: {len(blob)} bytes, "
          f"{len(offsets) - 1} resource entries (offset table {len(offsets)} u32s)")
    print()
    if args.chapter:
        cmd_chapter(blob, offsets, args.chapter)
    if args.all:
        cmd_all(blob, offsets)
    if args.dump_opcodes:
        cmd_dump_opcodes(blob, offsets)
    return 0


if __name__ == "__main__":
    sys.exit(main())
