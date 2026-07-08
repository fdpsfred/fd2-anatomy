"""Generate per-chapter encounter + treasure structured data for chapters/ section 3
(敵人配置 / 寶物) from FDFIELD.DAT.

Built on the byte-verified decoders in tools/decoders/:
  - fdfield_char_spawn_decoder.decode_record  (record fields verified field-by-field
    against fd2_init_runtime_char_for_battle @0x10C50)
  - tile_pickup semantics verified against src/ui_menu/menufld.c:
        kind 0 = item   (param = item_id, fd2_add_item_to_inventory)
        kind 1 = gold   (param = amount added to party_total_gold; param 0 = decoy)
        kind >=2 = event (param = consequence_table index; NOT treasure)
  - enemy drop = record +0x16 kind / +0x17 param (same encoding; 0xFF = no drop)

Output: workspace/kb_overhaul/encounters/chNN.json  +  a combined markdown preview.

Cross-check invariant (enforced): for every chapter,
    len(enemy) + len(npc) + len(player) + len(other) == char_count
so no spawn record is silently dropped or double-counted.

Name annotation (enemy 中文名 / item 中文名) is OPTIONAL and only added when a
name map JSON is supplied via --names; without it, raw indices are emitted for
later annotation once assets/ is finalized.

CLI:
    python tools/kb_overhaul/gen_ch_encounters.py --all
    python tools/kb_overhaul/gen_ch_encounters.py --chapter 4 --preview
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

OUT_DIR = REPO_ROOT / "workspace" / "kb_overhaul" / "encounters"

TEAM_NAME = {0: "enemy", 1: "npc", 2: "player"}


def classify_pickup(kind: int, param: int) -> dict | None:
    """Interpret a (kind, param) pickup pair per menufld.c semantics.

    Returns None for the sentinel (kind 0xFF) and for empty item slots."""
    if kind == 0xFF:
        return None
    if kind == 0:
        # item pickup; param 0 with kind 0 is an empty/unused slot in the
        # 16-entry tile_pickup table, so treat param 0 as 'none' there.
        if param == 0:
            return None
        return {"type": "item", "item_id": param}
    if kind == 1:
        # gold; param 0 = decoy tile ("nothing here" dialog)
        if param == 0:
            return {"type": "gold_decoy", "gold": 0}
        return {"type": "gold", "gold": param}
    # kind >= 2: chapter-scripted event dispatch, not treasure
    return {"type": "event", "consequence_idx": param}


# A spawn record only ever spawns when a chapter handler calls
# fd2_load_chapter_portraits_and_dump_tmp(target_race_id) with target_race_id ==
# record.race_id (record +0x15). No handler ever passes 0xFF (verified: 68 call
# sites in src, none with a 0xFF arg), and that function is the sole caller of
# fd2_init_runtime_char_for_battle, so race_id == 0xFF records never spawn — they
# are reserved/padding slots and MUST be excluded from the enemy roster.
RESERVED_RACE_ID = 0xFF


def group_records(entry: bytes, char_count: int) -> dict:
    by_team: dict[int, list[dict]] = {0: [], 1: [], 2: [], "other": []}
    reserved: list[dict] = []
    drops: list[dict] = []
    for i in range(char_count):
        r = fcs.decode_record(entry, i)
        if not r:
            break
        team = r["team"]
        cid = r["char_id"]
        if r["race_id"] == RESERVED_RACE_ID:
            reserved.append({"idx": i, "team": team, "char_id": cid,
                             "level": r["level"]})
            continue
        rec = {
            "idx": i,
            "team": team,
            "char_id": cid,
            "enemy_data_idx": (cid - 0x44) if cid >= 0x44 else None,
            "player_char_id": cid if cid < 0x44 else None,
            "level": r["level"],
            "race_id": r["race_id"],
            "ai_class": r["ai_class"],
            "ai_class_name": fcs.AI_CLASS_NAMES.get(r["ai_class"], f"ai_{r['ai_class']}"),
        }
        (by_team[team] if team in by_team else by_team["other"]).append(rec)
        d = classify_pickup(r["pickup_kind"], r["pickup_param"])
        if d and d["type"] in ("item", "gold"):
            drops.append({"idx": i, "char_id": cid, **d})
    return by_team, drops, reserved


def aggregate_units(records: list[dict]) -> list[dict]:
    """Collapse identical (unit key, level) records into count rows."""
    order: list[tuple] = []
    acc: dict[tuple, dict] = {}
    for r in records:
        key = (r["enemy_data_idx"], r["player_char_id"], r["level"])
        if key not in acc:
            acc[key] = {
                "enemy_data_idx": r["enemy_data_idx"],
                "player_char_id": r["player_char_id"],
                "level": r["level"],
                "count": 0,
                "ai_classes": [],
                "race_ids": [],
            }
            order.append(key)
        acc[key]["count"] += 1
        if r["ai_class_name"] not in acc[key]["ai_classes"]:
            acc[key]["ai_classes"].append(r["ai_class_name"])
        if r["race_id"] not in acc[key]["race_ids"]:
            acc[key]["race_ids"].append(r["race_id"])
    return [acc[k] for k in order]


def chapter_data(blob: bytes, offsets: list[int], chapter_n: int) -> dict:
    chapter_id = chapter_n - 1
    entry = fcs.chapter_entry(blob, offsets, chapter_n)
    char_count = entry[2]
    by_team, drops, reserved = group_records(entry, char_count)

    # tile_pickup table (16 entries) -> treasures + events
    treasures: list[dict] = []
    events: list[dict] = []
    for i in range(fcs.TILE_PICKUP_COUNT):
        tp = fcs.decode_tile_pickup(entry, i)
        if not tp:
            continue
        d = classify_pickup(tp["kind"], tp["param"])
        if d is None:
            continue
        d = {"tile_event_id": i, **d}
        if d["type"] == "event":
            events.append(d)
        else:
            treasures.append(d)

    total = (sum(len(by_team[t]) for t in (0, 1, 2)) + len(by_team["other"])
             + len(reserved))
    if total != char_count:
        raise AssertionError(
            f"ch{chapter_n}: grouped {total} != char_count {char_count} (record loss/dup)"
        )

    return {
        "chapter_n": chapter_n,
        "chapter_id": chapter_id,
        "fdfield_idx": fev.chapter_tile_event_idx(chapter_id),
        "entry_size": len(entry),
        "party_member_count": entry[1],
        "char_spawn_count": char_count,
        "spawning_record_count": char_count - len(reserved),
        "reserved_record_count": len(reserved),
        "reserved_record_indices": [r["idx"] for r in reserved],
        "enemy_units": aggregate_units(by_team[0]),
        "npc_units": aggregate_units(by_team[1]),
        "player_units": aggregate_units(by_team[2]),
        "other_units": aggregate_units(by_team["other"]),
        "map_treasures": treasures,
        "map_events": events,
        "enemy_drops": drops,
    }


def preview_md(d: dict) -> str:
    lines = [f"### Chapter {d['chapter_n']} (FDFIELD idx {d['fdfield_idx']}, "
             f"{d['char_spawn_count']} spawn records)"]
    lines.append("")
    lines.append("敵人 (team 0):")
    for u in d["enemy_units"]:
        ai = "/".join(u["ai_classes"])
        lines.append(f"  enemy_data[{u['enemy_data_idx']}] LV{u['level']} x{u['count']}  ai={ai}")
    if d["npc_units"]:
        lines.append("友軍 NPC (team 1):")
        for u in d["npc_units"]:
            tag = (f"enemy_data[{u['enemy_data_idx']}]" if u["enemy_data_idx"] is not None
                   else f"player_char 0x{u['player_char_id']:02X}")
            lines.append(f"  {tag} LV{u['level']} x{u['count']}")
    if d["player_units"] or d["other_units"]:
        lines.append(f"其他 team: player={len(d['player_units'])} other={len(d['other_units'])}")
    lines.append("地圖寶物 (tile_pickup):")
    for t in d["map_treasures"]:
        if t["type"] == "item":
            lines.append(f"  tile[{t['tile_event_id']}] item 0x{t['item_id']:02X} ({t['item_id']})")
        elif t["type"] == "gold":
            lines.append(f"  tile[{t['tile_event_id']}] gold {t['gold']}")
        elif t["type"] == "gold_decoy":
            lines.append(f"  tile[{t['tile_event_id']}] (decoy, 0 gold)")
    if d["map_events"]:
        ev = ", ".join(f"tile[{e['tile_event_id']}]->consequence[{e['consequence_idx']}]"
                       for e in d["map_events"])
        lines.append(f"地圖事件 (kind>=2, 非寶物): {ev}")
    if d["enemy_drops"]:
        lines.append("敵人掉落 (record pickup):")
        for dr in d["enemy_drops"]:
            if dr["type"] == "item":
                lines.append(f"  rec[{dr['idx']}] enemy 0x{dr['char_id']:02X} drops item 0x{dr['item_id']:02X}")
            else:
                lines.append(f"  rec[{dr['idx']}] enemy 0x{dr['char_id']:02X} drops gold {dr['gold']}")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chapter", type=int)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--preview", action="store_true", help="print markdown preview")
    ap.add_argument("--write-json", action="store_true", help="write per-chapter JSON")
    args = ap.parse_args()

    blob = fev.FDFIELD.read_bytes()
    offsets = fev.read_offset_table(blob)
    chapters = ([args.chapter] if args.chapter
                else (list(range(1, 31)) if args.all else None))
    if not chapters:
        ap.print_help()
        return 1

    if args.write_json:
        OUT_DIR.mkdir(parents=True, exist_ok=True)
    total_records = 0
    for n in chapters:
        d = chapter_data(blob, offsets, n)
        total_records += d["char_spawn_count"]
        if args.write_json:
            (OUT_DIR / f"ch{n:02d}.json").write_text(
                json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")
        if args.preview:
            print(preview_md(d))
    if args.all:
        print(f"# cross-check OK: 30 chapters, {total_records} total spawn records "
              f"(all grouped==char_count)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
