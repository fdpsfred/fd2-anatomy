"""Format per-chapter section-3 markdown (敵人配置 / 寶物) for chapters/chapter_NN.md,
from the verified encounter JSON (workspace/kb_overhaul/encounters/chNN.json) plus
enemy / item / character name maps parsed from the finalized assets/ canon.

Name maps (parsed live from assets/, never hard-coded):
  enemy_data_idx -> name : from assets/enemies.md rows keyed by FD2.EXE VA address,
        idx = (row_addr - first_row_addr) / stride. This is gap-aware: enemies.md
        documents 60 of 68 slots; the 8 undocumented (idx 6,7,60-64,67) stay unmapped.
  item_id        -> name : from assets/items.md '完整裝備一覽' table (hex id -> name).
  player char_id -> name : from assets/characters.md roster table (for team-0 units
        that use a player-class template, e.g. neutral rabble).

Any enemy idx / item id with no name is reported to stderr so the 5D injection never
silently prints a raw index. Run --check first; only inject after it is clean.

CLI:
    python tools/kb_overhaul/gen_ch_section3.py --check          # coverage report
    python tools/kb_overhaul/gen_ch_section3.py --chapter 5      # print blocks
    python tools/kb_overhaul/gen_ch_section3.py --all --write    # write md blocks
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
ENC_DIR = REPO_ROOT / "workspace" / "kb_overhaul" / "encounters"
OUT_DIR = REPO_ROOT / "workspace" / "kb_overhaul" / "section3"
ENEMIES_MD = REPO_ROOT / "assets" / "enemies.md"
ITEMS_MD = REPO_ROOT / "assets" / "items.md"
CHARS_MD = REPO_ROOT / "assets" / "characters.md"

HEX_ADDR_ROW = re.compile(r"^\|\s*([0-9A-Fa-f]{4,6})\s*\|\s*([^|]+?)\s*\|")
HEX_ID_ROW = re.compile(r"^\|\s*([0-9A-Fa-f]{2})\s*\|\s*([^|]+?)\s*\|")
CHAR_ROW = re.compile(r"^\|\s*(\d+)\s*\|\s*([^|]+?)\s*\|")


def load_enemy_names() -> dict[int, str]:
    """enemy_data_idx -> name, keyed by FD2.EXE VA row address (gap-aware)."""
    rows: list[tuple[int, str]] = []
    for line in ENEMIES_MD.read_text(encoding="utf-8").splitlines():
        m = HEX_ADDR_ROW.match(line)
        if not m:
            continue
        addr_s, name = m.group(1), m.group(2).strip()
        if name in ("名稱",) or not re.fullmatch(r"[0-9A-Fa-f]+", addr_s):
            continue
        rows.append((int(addr_s, 16), name))
    if not rows:
        raise RuntimeError("no enemy rows parsed from enemies.md")
    base = min(a for a, _ in rows)
    stride = 0x0A  # enemy_data entry stride (10 bytes), documented in enemies.md
    out: dict[int, str] = {}
    for addr, name in rows:
        if (addr - base) % stride != 0:
            raise RuntimeError(f"enemy row addr {addr:#x} not stride-aligned")
        out[(addr - base) // stride] = name
    return out


def load_item_names() -> dict[int, str]:
    """item_id -> name from the '完整裝備一覽' table (hex id -> name)."""
    out: dict[int, str] = {}
    in_table = False
    for line in ITEMS_MD.read_text(encoding="utf-8").splitlines():
        if line.startswith("## 完整裝備一覽"):
            in_table = True
            continue
        if in_table and line.startswith("## "):
            break
        if not in_table:
            continue
        m = HEX_ID_ROW.match(line)
        if not m:
            continue
        id_s, name = m.group(1), m.group(2).strip()
        if name in ("物品",):
            continue
        out[int(id_s, 16)] = name
    return out


def load_char_names() -> dict[int, str]:
    """player char_id -> name from the characters.md roster table (col 0 = decimal id)."""
    out: dict[int, str] = {}
    for line in CHARS_MD.read_text(encoding="utf-8").splitlines():
        m = CHAR_ROW.match(line)
        if not m:
            continue
        # strip a trailing "(0x..)" the name column sometimes carries
        name = re.sub(r"\s*\(0x[0-9A-Fa-f]+\)\s*$", "", m.group(2).strip())
        out[int(m.group(1))] = name
    return out


def enemy_label(idx: int, enemy_names: dict[int, str]) -> tuple[str, bool]:
    name = enemy_names.get(idx)
    if name is None:
        return f"enemy_data[{idx}]（未命名）", False
    return f"{name}", True


def item_label(item_id: int, item_names: dict[int, str]) -> tuple[str, bool]:
    name = item_names.get(item_id)
    if name is None:
        return f"item 0x{item_id:02X}（未命名）", False
    return f"{name} (0x{item_id:02X})", True


def fmt_chapter(d: dict, enemy_names, item_names, char_names,
                missing: dict) -> str:
    n = d["chapter_n"]
    lines: list[str] = []
    # --- 敵人配置 ---
    lines.append("## 敵人配置")
    lines.append("")
    reserved = d["reserved_record_count"]
    note = (f"本章 FDFIELD entry {d['fdfield_idx']} 共 {d['spawning_record_count']} "
            f"個會生成的 spawn 記錄")
    if reserved:
        note += f"（另有 {reserved} 筆 race_id 0xFF 保留記錄不生成）"
    note += "。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。"
    lines.append(note)
    lines.append("")
    lines.append("| enemy_data | 敵人 | 等級 | 數量 | AI |")
    lines.append("|---|---|---|---|---|")
    for u in d["enemy_units"]:
        if u["enemy_data_idx"] is not None:
            lbl, ok = enemy_label(u["enemy_data_idx"], enemy_names)
            idx_s = str(u["enemy_data_idx"])
            if not ok:
                missing["enemy"].add(u["enemy_data_idx"])
        else:
            cid = u["player_char_id"]
            cname = char_names.get(cid, f"char 0x{cid:02X}")
            lbl = f"{cname}（玩家職模板）"
            idx_s = f"char 0x{cid:02X}"
        ai = " / ".join(u["ai_classes"])
        lines.append(f"| {idx_s} | {lbl} | LV{u['level']} | ×{u['count']} | {ai} |")
    if d["npc_units"]:
        lines.append("")
        lines.append("友軍 NPC（team 1，戰場自走）：")
        lines.append("")
        lines.append("| enemy_data | 單位 | 等級 | 數量 |")
        lines.append("|---|---|---|---|")
        for u in d["npc_units"]:
            if u["enemy_data_idx"] is not None:
                lbl, ok = enemy_label(u["enemy_data_idx"], enemy_names)
                idx_s = str(u["enemy_data_idx"])
                if not ok:
                    missing["enemy"].add(u["enemy_data_idx"])
            else:
                cid = u["player_char_id"]
                lbl = char_names.get(cid, f"char 0x{cid:02X}")
                idx_s = f"char 0x{cid:02X}"
            lines.append(f"| {idx_s} | {lbl} | LV{u['level']} | ×{u['count']} |")

    # --- 寶物 ---
    lines.append("")
    lines.append("## 寶物")
    lines.append("")
    items = [t for t in d["map_treasures"] if t["type"] == "item"]
    golds = [t for t in d["map_treasures"] if t["type"] == "gold"]
    decoys = [t for t in d["map_treasures"] if t["type"] == "gold_decoy"]
    if not (items or golds):
        lines.append("本章 tile_pickup 表無道具 / 金錢寶物。")
    else:
        lines.append("地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：")
        lines.append("")
        if items:
            labs = []
            for t in items:
                lbl, ok = item_label(t["item_id"], item_names)
                if not ok:
                    missing["item"].add(t["item_id"])
                labs.append(lbl)
            lines.append(f"- 道具：{'、'.join(labs)}")
        if golds:
            lines.append(f"- 金錢：{'、'.join(str(t['gold']) for t in golds)}")
    if decoys:
        lines.append(f"- 空寶箱（0 金錢誘餌）×{len(decoys)}")
    if d["map_events"]:
        ev = "、".join(f"tile[{e['tile_event_id']}]" for e in d["map_events"])
        lines.append("")
        lines.append(f"（tile_pickup 另有 {len(d['map_events'])} 筆 kind≥2 = 劇情事件觸發點"
                     f"（{ev}），非寶物，見 §FDFIELD event script。）")
    drops_item = [dr for dr in d["enemy_drops"] if dr["type"] == "item"]
    drops_gold = [dr for dr in d["enemy_drops"] if dr["type"] == "gold"]
    if drops_item or drops_gold:
        lines.append("")
        lines.append("敵人掉落（擊殺帶有掉落的敵人可得）：")
        lines.append("")
        if drops_item:
            ids = sorted({dr["item_id"] for dr in drops_item})
            labs = []
            for i in ids:
                lbl, ok = item_label(i, item_names)
                if not ok:
                    missing["item"].add(i)
                labs.append(lbl)
            lines.append(f"- 道具：{'、'.join(labs)}")
        if drops_gold:
            amts = sorted({dr["gold"] for dr in drops_gold})
            lines.append(f"- 金錢：{'、'.join(str(a) for a in amts)}")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chapter", type=int)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--check", action="store_true", help="coverage report only")
    ap.add_argument("--write", action="store_true", help="write per-chapter md")
    args = ap.parse_args()

    enemy_names = load_enemy_names()
    item_names = load_item_names()
    char_names = load_char_names()
    sys.stderr.write(f"# name maps: {len(enemy_names)} enemy, {len(item_names)} item, "
                     f"{len(char_names)} char\n")

    chapters = ([args.chapter] if args.chapter else list(range(1, 31)))
    missing = {"enemy": set(), "item": set()}
    if args.write:
        OUT_DIR.mkdir(parents=True, exist_ok=True)
    for nn in chapters:
        d = json.loads((ENC_DIR / f"ch{nn:02d}.json").read_text(encoding="utf-8"))
        block = fmt_chapter(d, enemy_names, item_names, char_names, missing)
        if args.write:
            (OUT_DIR / f"ch{nn:02d}.md").write_text(block + "\n", encoding="utf-8")
        elif not args.check:
            print(f"===== chapter {nn} =====")
            print(block)
            print()
    if missing["enemy"] or missing["item"]:
        sys.stderr.write(f"# UNMAPPED enemy idx: {sorted(missing['enemy'])}\n")
        sys.stderr.write(f"# UNMAPPED item id: {sorted(hex(i) for i in missing['item'])}\n")
    else:
        sys.stderr.write("# coverage OK: every referenced enemy idx + item id has a name\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
