"""
Query the FD2 knowledge base. Python 3.12 stdlib only.

Examples:
    python query.py offset 540AC
    python query.py item 0Bh
    python query.py item 炎龍劍
    python query.py spell 02
    python query.py char 索爾
    python query.py char --job 劍聖
    python query.py chapter 1
    python query.py chapter 1 --enemies
    python query.py job 09
    python query.py portrait 20
    python query.py enemy 薩卡
    python query.py struct runtime_char
    python query.py formula physical_damage
    python query.py hack 3
    python query.py grep 炎龍術
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

# Force UTF-8 stdout on Windows so Chinese renders correctly
try:
    sys.stdout.reconfigure(encoding="utf-8")
except (AttributeError, OSError):
    pass

SKILL_ROOT = Path(__file__).resolve().parent
DATA = SKILL_ROOT / "data"
INDEX = SKILL_ROOT / "index"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_json(name: str) -> dict:
    return json.loads((INDEX / name).read_text(encoding="utf-8"))


def parse_id(s: str) -> int:
    """Accept 0Bh / 0x0B / 11 / B (2-digit hex default)."""
    s = s.strip().lower()
    if s.endswith("h"):
        return int(s[:-1], 16)
    if s.startswith("0x"):
        return int(s, 16)
    if re.fullmatch(r"\d+", s):
        # For numbers that look decimal we keep them decimal only if >= 100 or
        # > the max possible 2-hex-digit interpretation is unlikely;
        # most ID lookups in FD2 are small, so prefer decimal when possible.
        return int(s, 10)
    # fallback to hex interpretation
    return int(s, 16)


def parse_id_flexible(s: str) -> list[int]:
    """Return a list of plausible interpretations (dec + hex)."""
    s = s.strip().lower()
    out: list[int] = []
    if s.endswith("h"):
        out.append(int(s[:-1], 16))
    elif s.startswith("0x"):
        out.append(int(s, 16))
    elif re.fullmatch(r"\d+", s):
        out.append(int(s, 10))
        # also try hex if ambiguous (e.g. "11" could be 0x11)
        try:
            h = int(s, 16)
            if h != int(s, 10):
                out.append(h)
        except ValueError:
            pass
    else:
        try:
            out.append(int(s, 16))
        except ValueError:
            pass
    return out


def hex2(n: int) -> str:
    return f"{n:02X}"


def read_md_section(md_path: Path, anchor: str | None = None,
                    heading_prefix: str | None = None) -> str:
    """Return a section of the md file around an anchor or a heading."""
    if not md_path.exists():
        return ""
    text = md_path.read_text(encoding="utf-8")
    if anchor:
        m = re.search(rf'<a id="{re.escape(anchor)}"></a>', text)
        if m:
            # Return 500 chars around the anchor
            start = max(0, m.start() - 200)
            end = min(len(text), m.end() + 500)
            return text[start:end]
    if heading_prefix:
        i = text.find(heading_prefix)
        if i >= 0:
            j = text.find("\n## ", i + 1)
            return text[i:j] if j > 0 else text[i:]
    return text


# ---------------------------------------------------------------------------
# Subcommand implementations
# ---------------------------------------------------------------------------

def cmd_offset(args) -> int:
    """Reverse-lookup a memory address against offsets.json."""
    offsets = load_json("offsets.json")
    target_hex = args.address.lstrip("0").lstrip("x").lstrip("X")
    if target_hex.endswith(("h", "H")):
        target_hex = target_hex[:-1]
    try:
        target = int(target_hex, 16)
    except ValueError:
        print(f"Cannot parse address: {args.address}")
        return 2
    key = f"{target:X}"

    # Exact match
    hit = offsets.get(key)
    if hit and "alias_of" in hit:
        hit = offsets.get(hit["alias_of"])
        key = list(offsets.keys())[list(offsets.values()).index(hit)] if hit else key
    if hit:
        print_offset_entry(target, key, hit, offsets)
        return 0

    # Fuzzy: find nearest <= key
    candidates = sorted(
        (int(k, 16), k, v) for k, v in offsets.items()
        if "alias_of" not in v
    )
    nearest = None
    for addr_int, k, v in candidates:
        if addr_int <= target:
            nearest = (addr_int, k, v)
        else:
            break
    print(f"# Address `0x{target:X}` — no exact match in offsets.json\n")
    if nearest:
        addr_int, k, v = nearest
        delta = target - addr_int
        print(f"Nearest known offset: **{v['name']}** at `0x{k}` "
              f"(+ {delta} bytes / 0x{delta:X})\n")
        print_offset_entry(addr_int, k, v, offsets, prefix="(nearest) ")
        if v.get("entry_size") and delta < v["entry_size"] * 10:
            print(f"\n⚠️ This may be an offset into the `{v['name']}` table "
                  f"(entry size {v['entry_size']} bytes → entry index "
                  f"{delta // v['entry_size']}, field offset "
                  f"{delta % v['entry_size']} within entry).")
    else:
        print("No known offsets below this address.")

    print("\n⚠️ Addresses vary between FD2.EXE versions. Use the `search_bytes` "
          "signature from data/offsets.md to locate the actual address in your "
          "binary via Ghidra's byte search.")
    return 0


def print_offset_entry(target: int, key: str, v: dict, all_offsets: dict,
                       prefix: str = "") -> None:
    name = v.get("name", "?")
    print(f"## {prefix}Offset 0x{key} — {name}\n")
    print("| Field | Value |")
    print("|---|---|")
    print(f"| Name | `{name}` |")
    print(f"| Title (zh) | {v.get('title', '-')} |")
    print(f"| FD2.LE (sample) | `{v.get('le_addr_sample') or '-'}` |")
    print(f"| FD2.EXE (sample) | `{v.get('exe_addr_sample') or '-'}` |")
    print(f"| Entry size | {v.get('entry_size', '-')} bytes |")
    print(f"| Signature | `{v.get('search_bytes') or '-'}` |")
    print(f"| Version note | {v.get('version_note', '-')} |")
    print()
    print("See `data/offsets.md` for the full layout of this table.")


def cmd_item(args) -> int:
    items = load_json("items.json")
    if args.list:
        print(f"# All items ({len(items)} entries)\n")
        for k, v in sorted(items.items(), key=lambda kv: int(kv[0], 16)):
            print(f"- `{v['id_hex']}` ({v['id_dec']:3d}) {v['name']}")
        return 0

    query = args.target
    # Try ID first
    if re.match(r"^(0x|0X)?[0-9A-Fa-f]{1,2}h?$", query):
        for cand in parse_id_flexible(query):
            key = hex2(cand)
            if key in items:
                print_item(items[key])
                return 0
    # Fall back to name substring
    hits = [v for v in items.values() if query in v["name"]]
    if not hits:
        print(f"No item matches `{query}`")
        return 1
    for v in hits:
        print_item(v)
        print()
    return 0


def print_item(v: dict) -> None:
    print(f"## Item {v['id_hex']} ({v['id_dec']}) — {v['name']}\n")
    fields = [
        ("AP", v.get("AP")), ("HIT", v.get("HIT")),
        ("DP", v.get("DP")), ("EV", v.get("EV")),
        ("暴擊%", v.get("crit_pct")), ("中毒%", v.get("poison_pct")),
        ("雙擊%", v.get("double_pct")),
        ("使用法術", v.get("on_use_spell")),
        ("使用距離", v.get("on_use_range")),
        ("使用範圍", v.get("on_use_aoe")),
        ("標籤", v.get("tag")),
    ]
    print("| Field | Value |")
    print("|---|---|")
    for k, val in fields:
        if val is not None:
            print(f"| {k} | {val} |")
    print()
    print(f"Raw: `{v.get('raw_desc', '')}`\n")
    print(f"See `data/items.md` for the full table.")


def cmd_spell(args) -> int:
    spells = load_json("spells.json")
    if args.list:
        print(f"# All spells ({len(spells)} entries)\n")
        for k, v in sorted(spells.items(), key=lambda kv: int(kv[0], 16)):
            print(f"- `{v['id_hex']}` ({v['id_dec']:3d}) {v['name']} "
                  f"[{v.get('category', '-')}]")
        return 0

    query = args.target
    if re.match(r"^(0x|0X)?[0-9A-Fa-f]{1,2}h?$", query):
        for cand in parse_id_flexible(query):
            key = hex2(cand)
            if key in spells:
                print_spell(spells[key])
                return 0
    hits = [v for v in spells.values() if query in v["name"]]
    if not hits:
        print(f"No spell matches `{query}`")
        return 1
    for v in hits:
        print_spell(v)
        print()
    return 0


def print_spell(v: dict) -> None:
    print(f"## Spell {v['id_hex']} ({v['id_dec']}) — {v['name']}  "
          f"*{v.get('category', '-')}*\n")
    print("| Field | Value |")
    print("|---|---|")
    for k, label in [
        ("distance", "距離"), ("aoe", "範圍"),
        ("mp", "MP"), ("max_damage", "最大傷害"),
        ("hit_pct", "命中%"),
    ]:
        if v.get(k) is not None:
            print(f"| {label} | {v[k]} |")
    print()
    print("See `data/spells.md` for the full table.")


def cmd_char(args) -> int:
    chars = load_json("characters.json")
    if args.list:
        names = sorted(chars["by_name"].keys())
        print(f"# All characters ({len(names)})\n")
        for n in names:
            rows = chars["by_name"][n]
            jobs = ", ".join(r.get("job") or "基礎" for r in rows)
            print(f"- **{n}** — {jobs}")
        return 0

    if args.job:
        jquery = args.job
        matches = []
        for rows in chars["by_name"].values():
            for r in rows:
                if r.get("job") and jquery in r["job"]:
                    matches.append(r)
        if not matches:
            print(f"No characters with job containing `{jquery}`")
            return 1
        print(f"# Characters with job containing `{jquery}` ({len(matches)})\n")
        for r in matches:
            print_char_row(r)
            print()
        return 0

    query = args.target
    if not query:
        print("Specify a name or use --list / --job")
        return 2
    if query not in chars["by_name"]:
        matches = [n for n in chars["by_name"] if query in n]
        if not matches:
            print(f"No character matches `{query}`")
            return 1
        if len(matches) > 1:
            print(f"Multiple matches: {', '.join(matches)}")
        query = matches[0]

    rows = chars["by_name"][query]
    print(f"# Character {query} ({len(rows)} job entries)\n")
    for r in rows:
        print_char_row(r)
        print()
    return 0


def print_char_row(r: dict) -> None:
    label = f"{r['name']}  {r.get('job') or '(基礎/未轉職)'}"
    print(f"## {label}")
    print(f"`{r['address_sample']}` | raw bytes: `{r['raw_bytes']}`\n")
    print("| 屬性 | 最小成長 | 最大成長+1 |")
    print("|---|---|---|")
    for stat in ["AP", "DP", "DX", "HP", "MP"]:
        lo = r.get(f"{stat}_min")
        hi = r.get(f"{stat}_max_plus1")
        print(f"| {stat} | {lo} | {hi} |")
    mg = r.get("MG_index")
    if mg is not None:
        print(f"\n法術習得等級資料索引: `{mg}` (0xFF = 無)")


def cmd_chapter(args) -> int:
    chapters = load_json("chapters.json")
    n = int(args.number)
    if not (1 <= n <= 30):
        print(f"Chapter must be 1-30 (got {n})")
        return 2
    entry = chapters.get(str(n))
    if not entry:
        print(f"No chapter {n}")
        return 1

    md_path = SKILL_ROOT / entry["md_file"]
    text = md_path.read_text(encoding="utf-8")

    if args.enemies:
        print_section(text, "敵方", "友方")
    elif args.treasures:
        print_section(text, "寶物", "事件")
    elif args.events:
        print_section(text, "事件", "說明")
    else:
        print(text)
    return 0


def print_section(text: str, start_label: str, end_label: str) -> None:
    """Given md text of a chapter, print only the block starting at start_label
    and ending at end_label."""
    # md is inside a ``` block; print the whole md but highlight the section.
    m = re.search(
        rf"(?m)^{start_label}：([\s\S]*?)(?=^(?:{end_label}|說明|加入|備註|[^\s]):|\Z)",
        text,
    )
    if m:
        print(f"## {start_label}\n\n```\n{start_label}：{m.group(1).rstrip()}\n```")
    else:
        # Fallback: just print the whole chapter
        print(text)


def cmd_job(args) -> int:
    jobs = load_json("jobs.json")
    if args.list:
        print(f"# All jobs ({len(jobs)})\n")
        for k, v in sorted(jobs.items()):
            print(f"- `{v['id_hex']}` {v['name']}"
                  + (f" (魔抗 {v['magic_resist_pct']}%)"
                     if v.get("magic_resist_pct") is not None else "")
                  + (f" (暴擊 {v['crit_pct']}%)"
                     if v.get("crit_pct") is not None else ""))
        return 0

    query = args.target
    if re.match(r"^(0x|0X)?[0-9A-Fa-f]{1,2}h?$", query):
        for cand in parse_id_flexible(query):
            key = hex2(cand)
            if key in jobs:
                print_job(jobs[key])
                return 0
    hits = [v for v in jobs.values() if query in v["name"]]
    if not hits:
        print(f"No job matches `{query}`")
        return 1
    for v in hits:
        print_job(v)
        print()
    return 0


def print_job(v: dict) -> None:
    print(f"## Job {v['id_hex']} ({v['id_dec']}) — {v['name']}\n")
    print("| Field | Value |")
    print("|---|---|")
    mr = v.get("magic_resist_pct")
    cr = v.get("crit_pct")
    print(f"| 魔抗 | {'--' if mr is None else f'{mr}%'} |")
    print(f"| 暴擊率 | {'--' if cr is None else f'{cr}%'} |")


def cmd_portrait(args) -> int:
    portraits = load_json("portraits.json")
    if args.list:
        for k, v in sorted(portraits.items()):
            print(f"- `{v['id_hex']}` {v['name']}")
        return 0
    query = args.target
    if re.match(r"^(0x|0X)?[0-9A-Fa-f]{1,2}h?$", query):
        for cand in parse_id_flexible(query):
            key = hex2(cand)
            if key in portraits:
                v = portraits[key]
                print(f"## Portrait {v['id_hex']} ({v['id_dec']}) — {v['name']}")
                return 0
    hits = [v for v in portraits.values() if query in v["name"]]
    if not hits:
        print(f"No portrait matches `{query}`")
        return 1
    for v in hits:
        print(f"- `{v['id_hex']}` {v['name']}")
    return 0


def cmd_enemy(args) -> int:
    enemies = load_json("enemies.json")
    query = args.target
    hits = [v for v in enemies.values() if query in v["name"]]
    if not hits:
        print(f"No enemy/ally matches `{query}`")
        return 1
    for v in hits:
        print(f"## {v['name']} ({v['side']})\n")
        print(f"`{v['address_sample']}`\n")
        print("| Field | Value |")
        print("|---|---|")
        for f in ["RA", "CL", "HP_per_lvl", "MP_per_lvl", "AP_per_lvl",
                  "DP_per_lvl", "DX_per_lvl", "MV", "EX_per_lvl"]:
            print(f"| {f} | {v.get(f)} |")
        print()
    return 0


# Struct layouts — these are the canonical byte layouts from the strategy
# guides; kept inline so `struct runtime_char` works without hitting md.
STRUCTS: dict[str, dict] = {
    "runtime_char": {
        "name": "runtime_char",
        "size": 0x50,
        "source": "memory_hack.htm",
        "description": "運行時人物資料（每位 80 bytes = 0x50）",
        "see": "data/memory_layout.md",
        "layout_summary": (
            "0x00-0x07: 保留\n"
            "0x08 XX YY: 所在座標\n"
            "0x0A Z1 Z2 Z3: 圖形／方向／跑步動作\n"
            "0x0D AA: 動作狀態 (00=未行動, 01=死亡, 80=行動完畢)\n"
            "0x0E BB: 陣營 (00=敵, 01=友, 02=己方)\n"
            "0x0F FA: 肖像編號\n"
            "0x10-0x11 NN NN: 人物姓名\n"
            "0x12-0x21 IT IT x8: 物品（每個 2 bytes: 狀態+編號）\n"
            "0x22-0x26 M1 M2 M3 M4 M5: 法術 (bit flags)\n"
            "0x27 RA: 種族編號\n"
            "0x28 CL: 職業編號\n"
            "0x29 LV: 等級\n"
            "0x2A A+: AP 增強 (00=無, 02=有)\n"
            "0x2B D+: DP 增強\n"
            "0x2C H+: HIT/EV 增強\n"
            "0x2D -H: 中毒狀態\n"
            "0x2E XA: 麻痹狀態\n"
            "0x2F XM: 封咒狀態\n"
            "0x3F MT: 力量 (影響 AP)\n"
            "0x40 MT (low): 力量\n"
            "0x41-0x42 DF: 耐力 (影響 DP)\n"
            "0x43 MV: 移動力\n"
            "0x44 EX: 經驗值\n"
            "0x46-0x47 DX: 速度\n"
            "0x48-0x4B H1 H2: 目前/最大 HP\n"
            "0x4C-0x4F M1 M2: 目前/最大 MP\n"
            "(加上 0x50+ 是 AP/DP/HT/EV 計算結果)"
        ),
    },
    "item_entry": {
        "name": "item_entry",
        "size": 23,
        "source": "code_hack2.htm §1",
        "description": "物品功效表單筆 (FD2.LE @ 0x540AC, FD2.EXE @ 0x792C1)",
        "see": "data/offsets.md#offset-540AC + data/items.md",
        "layout_summary": (
            "TY AP AP HT HT DP DP EV EV S1 S2 R1 R2 K1 K2 K3 K4 K5 K6 MM MM ?? ??\n"
            "TY: 物品類型 (01=劍 02=刀 03=槍 04=斧 05=弓 06=杖 07=爪 08=機械手臂 20h=道具)\n"
            "AP/HT/DP/EV: 16-bit 屬性增值\n"
            "S1: 附加屬性 (00=無 02=中毒 03=雙擊 04=暴擊)\n"
            "S2: 機率\n"
            "R1/R2: 攻擊距離 min/max\n"
            "K1..K6: 使用效果 (K1 見 data/items.md)\n"
            "MM: 價格 (16-bit)"
        ),
    },
    "spell_entry": {
        "name": "spell_entry",
        "size": 7,
        "source": "code_hack2.htm §3",
        "description": "法術功效表單筆 (FD2.LE @ 0x557FD, FD2.EXE @ 0x7AA11)",
        "see": "data/offsets.md#offset-557FD",
        "layout_summary": (
            "DA DA HT DS RN MP WH\n"
            "DA: 最大傷害 (或恢復力)\n"
            "HT: 命中率\n"
            "DS: 攻擊距離 (10h=1 表直線，否則低 4 bit 為距離)\n"
            "RN: 攻擊範圍 (最大 3)\n"
            "MP: 消耗法力\n"
            "WH: 作用對象 (00=敵方, 01=己方)"
        ),
    },
    "char_base_entry": {
        "name": "char_base_entry",
        "size": 24,
        "source": "code_hack2.htm §4",
        "description": "人物出場屬性 (FD2.LE @ 0x55BA1, FD2.EXE @ 0x7ADB5)",
        "see": "data/offsets.md#offset-55BA1 + data/characters_base.md",
        "layout_summary": (
            "-- -- -- -- -- RA CL LV HP HP MP MP MV MG MG MG\n"
            "MG IT IT IT IT IT IT AP AP DP DP DX DX -- -- --\n"
            "(尾端 1 byte -- 讓總長 24)\n"
            "實際出場屬性 = 基礎 + LV*每級成長"
        ),
    },
    "char_growth_entry": {
        "name": "char_growth_entry",
        "size": 11,
        "source": "code_hack2.htm §5",
        "description": "人物升級屬性 (FD2.LE @ 0x55EA1, FD2.EXE @ 0x7B0B5)",
        "see": "data/offsets.md#offset-55EA1 + data/characters_growth.md",
        "layout_summary": (
            "AP0 AP1 DP0 DP1 DX0 DX1 HP0 HP1 MP0 MP1 MG\n"
            "XX0/XX1 = 最小成長 / 最大成長+1\n"
            "MG = 法術習得等級資料索引 (0xFF 表無)"
        ),
    },
    "spell_learning_entry": {
        "name": "spell_learning_entry",
        "size": 12,
        "source": "code_hack2.htm §6",
        "description": "法術習得等級 (FD2.LE @ 0x564B3, FD2.EXE @ 0x7B6C7)",
        "see": "data/offsets.md#offset-564B3 + data/spell_learning.md",
        "layout_summary": (
            "(LV MG) x 6\n"
            "最多 6 組 (等級, 法術編號) 對，FFh 表空"
        ),
    },
    "enemy_entry": {
        "name": "enemy_entry",
        "size": 10,
        "source": "code_hack2.htm §8",
        "description": "敵人／友軍等級資訊 (FD2.LE @ 0x558F9, FD2.EXE @ 0x7AB0D)",
        "see": "data/offsets.md#offset-558F9 + data/enemies.md",
        "layout_summary": (
            "RA CL HP HP MP AP DP DX MV EX\n"
            "每級生命/法力/攻擊/防禦/速度, MV 固定, EX 為每級經驗值"
        ),
    },
}


def cmd_struct(args) -> int:
    if args.list:
        for v in STRUCTS.values():
            print(f"- `{v['name']}` ({v['size']} bytes): {v['description']}")
        return 0
    s = STRUCTS.get(args.target)
    if not s:
        # partial match
        hits = [v for v in STRUCTS.values() if args.target in v["name"]]
        if not hits:
            print(f"No struct matches `{args.target}`")
            print("Available: " + ", ".join(STRUCTS.keys()))
            return 1
        s = hits[0]
    print(f"## Struct `{s['name']}` — {s['size']} bytes\n")
    print(f"**Source:** {s['source']}\n")
    print(f"**Description:** {s['description']}\n")
    print("### Layout")
    print("```")
    print(s["layout_summary"])
    print("```")
    print(f"\nSee: `{s['see']}`")
    return 0


def cmd_formula(args) -> int:
    formulas = load_json("formulas.json")
    path = DATA / "formulas.md"
    text = path.read_text(encoding="utf-8") if path.exists() else ""

    if not args.name:
        print(text)
        return 0

    # keyword match
    found = False
    for line_no, line in enumerate(text.splitlines()):
        if args.name in line or args.name.lower() in line.lower():
            start = max(0, line_no - 2)
            end = min(len(text.splitlines()), line_no + 10)
            block = "\n".join(text.splitlines()[start:end])
            print("```")
            print(block)
            print("```\n")
            found = True
            break
    if not found:
        print(f"No formula matches `{args.name}`. Available names:")
        for k in formulas:
            print(f"  - {k}")
    return 0


def cmd_hack(args) -> int:
    path = DATA / "code_hacks.md"
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    if args.target.isdigit():
        n = int(args.target)
        # Find "N." as a list-item start
        m = re.search(rf"(?m)^{n}\.[^\n]+\n(?:(?!^\d+\.)[^\n]*\n?)*", text)
        if m:
            print(f"## Hack #{n}\n\n```\n{m.group(0).rstrip()}\n```")
            return 0
        print(f"No hack numbered {n}")
        return 1
    # keyword search
    hits = []
    for m in re.finditer(rf"(?m)^(\d+)\.[^\n]*{re.escape(args.target)}[^\n]*", text):
        hits.append((int(m.group(1)), m.group(0)))
    if not hits:
        print(f"No hack mentions `{args.target}`")
        return 1
    for n, line in hits:
        print(f"- Hack #{n}: {line.strip()}")
    return 0


def cmd_grep(args) -> int:
    pattern = args.pattern
    matches: list[tuple[Path, int, str]] = []
    for md in DATA.rglob("*.md"):
        lines = md.read_text(encoding="utf-8").splitlines()
        for i, line in enumerate(lines):
            if pattern in line:
                matches.append((md, i + 1, line))
    if not matches:
        print(f"No match for `{pattern}`")
        return 1
    for md, line_no, line in matches[:200]:
        rel = md.relative_to(SKILL_ROOT)
        print(f"{rel}:{line_no}: {line}")
    if len(matches) > 200:
        print(f"...({len(matches) - 200} more hits)")
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(
        prog="query.py",
        description="Query the FD2 knowledge base.",
    )
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("offset", help="reverse-lookup a memory address")
    p.add_argument("address", help="hex address (EXE or LE), e.g. 540AC, 0x792C1")
    p.set_defaults(func=cmd_offset)

    p = sub.add_parser("item", help="lookup item by ID or name")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_item)

    p = sub.add_parser("spell", help="lookup spell by ID or name")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_spell)

    p = sub.add_parser("char", help="lookup character by name")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--job", help="filter by job name (substring)")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_char)

    p = sub.add_parser("chapter", help="show chapter walkthrough")
    p.add_argument("number")
    p.add_argument("--enemies", action="store_true")
    p.add_argument("--treasures", action="store_true")
    p.add_argument("--events", action="store_true")
    p.set_defaults(func=cmd_chapter)

    p = sub.add_parser("job", help="lookup job by ID or name")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_job)

    p = sub.add_parser("portrait", help="lookup portrait by ID or name")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_portrait)

    p = sub.add_parser("enemy", help="lookup enemy/ally by name")
    p.add_argument("target")
    p.set_defaults(func=cmd_enemy)

    p = sub.add_parser("struct", help="show a byte-layout struct")
    p.add_argument("target", nargs="?", default="")
    p.add_argument("--list", action="store_true")
    p.set_defaults(func=cmd_struct)

    p = sub.add_parser("formula", help="show damage/hit/exp formulas")
    p.add_argument("name", nargs="?", default="")
    p.set_defaults(func=cmd_formula)

    p = sub.add_parser("hack", help="lookup FD2.EXE byte-pattern hack")
    p.add_argument("target")
    p.set_defaults(func=cmd_hack)

    p = sub.add_parser("grep", help="full-text search across data/")
    p.add_argument("pattern")
    p.set_defaults(func=cmd_grep)

    args = ap.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
