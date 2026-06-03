#!/usr/bin/env python3
"""dump_real.py - extract ground-truth values from the REAL game files so the
resource-loader unit tests can assert on real parsed values (replacing the
former fabricated fixtures). Reads fd2_game_files/ directly — no hardcoded
sizes/offsets (the DAT/SAV layout constants below are the on-disk format the
loaders implement, taken from src/rsrc/rsrc.c + src/life/main.c).

DAT archive (src/rsrc/rsrc.c fd2_load_dat_resource):
  [0..5] 6-byte prefix; u32 offset table from file offset 6; resource `index`
  has start=u32@(6+index*4), end=u32@(6+(index+1)*4), size=end-start, payload
  at `start`.
FDICON.B24: 6-byte magic; flat u32 sprite-header table from offset 6
  (entry e = u32@(6+e*4)); a portrait p's 13 ints are entries [p*12 .. p*12+12].
FD2.SAV: 0x59CB bytes; documented field offsets below; checksum u32 @ +0x59C7.
"""
import json
import struct
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[2] / "fd2_game_files"


def u32(b, off):
    return struct.unpack_from("<I", b, off)[0]


def dat_resource(data, index):
    start = u32(data, 6 + index * 4)
    end = u32(data, 6 + (index + 1) * 4)
    size = end - start
    payload = data[start:start + size] if 0 <= start <= len(data) and size >= 0 else b""
    return {"index": index, "start": start, "end": end, "size": size,
            "first_bytes": list(payload[:8])}


def dump_dat(name, indices):
    data = (GAME / name).read_bytes()
    return {"file": name, "filesize": len(data),
            "resources": [dat_resource(data, i) for i in indices]}


def dump_fdicon(portrait_ids):
    data = (GAME / "FDICON.B24").read_bytes()
    out = {"file": "FDICON.B24", "filesize": len(data), "portraits": []}
    for p in portrait_ids:
        base = 6 + p * 12 * 4
        ents = [u32(data, base + i * 4) for i in range(13)]
        out["portraits"].append({"portrait_id": p, "offsets13": ents,
                                 "data_size_first": ents[1] - ents[0]})
    return out


def dump_sav():
    data = (GAME / "FD2.SAV").read_bytes()
    f = {
        "filesize": len(data),
        "tile_event_0_scene_id": data[0],
        "tile_event_1": data[1],
        "tile_event_2": data[2],
        "turn_counter_30C3": data[0x30C3],
        "party_count_30C4": data[0x30C4],
        "chapter_30C5": data[0x30C5],
        "view_window_origin_x_30C6": data[0x30C6],
        "view_window_origin_y_30C7": data[0x30C7],
        "cursor_world_x_30C8": data[0x30C8],
        "cursor_world_y_30C9": data[0x30C9],
        "cursor_screen_x_30CA": data[0x30CA],
        "cursor_screen_y_30CB": data[0x30CB],
        "menu_party_member_count_30CC": data[0x30CC],
        "party_total_gold_30CD": u32(data, 0x30CD),
        "game_speed_flag_30D1": data[0x30D1],
        "terrain_hud_enabled_30D2": data[0x30D2],
        "bgm_enabled_30D3": data[0x30D3],
        "sfx_enabled_30D4": data[0x30D4],
        "stored_checksum_59C7": u32(data, 0x59C7),
    }
    # saved runtime_char records at 0x12A3, 0x50 stride; portrait_id at +0x07
    f["roster_portrait_ids"] = [data[0x12A3 + k * 0x50 + 0x07] for k in range(8)]
    return f


def main():
    chapter = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    base = chapter * 3
    result = {
        "chapter_used_for_indices": chapter,
        "FDOTHER": dump_dat("FDOTHER.DAT", [0, 0xF, 0x10, 0x2A, 0x37]),
        "FDFIELD": dump_dat("FDFIELD.DAT", [0, base, base + 2]),
        "FDTXT": dump_dat("FDTXT.DAT", [0, chapter + 1]),
        "FDSHAP": dump_dat("FDSHAP.DAT", [0, 1]),
        "FDMUS": dump_dat("FDMUS.DAT", [0, 3, 5, 0x10]),
        "FDICON": dump_fdicon([0, 1, 5, 7, 0x40, 0x41]),
        "FD2_SAV": dump_sav(),
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
