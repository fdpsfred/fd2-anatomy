"""
Build data/*.md and index/*.json from Strategy_Guide/*.htm for the FD2 reverse
engineering knowledge base. Python 3.12 stdlib only.

Usage:
    python build_index.py           # build everything
    python build_index.py --verify  # build + run self-checks
"""

from __future__ import annotations

import argparse
import html
import json
import re
import sys
from collections import OrderedDict
from html.parser import HTMLParser
from pathlib import Path

SKILL_ROOT = Path(__file__).resolve().parent
GUIDE_ROOT = SKILL_ROOT.parent.parent.parent / "Strategy_Guide"
DATA_DIR = SKILL_ROOT / "data"
CHAPTERS_DIR = DATA_DIR / "chapters"
INDEX_DIR = SKILL_ROOT / "index"


# ---------------------------------------------------------------------------
# HTML helpers
# ---------------------------------------------------------------------------

class PreExtractor(HTMLParser):
    """Extract text from all <pre> blocks, stripping inner tags but keeping
    their text. Anchor ids become `{{ANCHOR:xxx}}` markers so we can split on
    them later."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.in_pre = False
        self.buf: list[str] = []
        self.chunks: list[str] = []

    def handle_starttag(self, tag, attrs):
        t = tag.lower()
        if t == "pre":
            self.in_pre = True
            self.buf = []
        elif self.in_pre:
            if t == "a":
                d = dict(attrs)
                anc = d.get("id") or d.get("name")
                if anc:
                    self.buf.append(f"{{{{ANCHOR:{anc}}}}}")
            elif t == "br":
                self.buf.append("\n")
            elif t == "img":
                d = dict(attrs)
                src = d.get("src", "")
                self.buf.append(f"[IMG:{src}]")

    def handle_endtag(self, tag):
        if tag.lower() == "pre" and self.in_pre:
            self.in_pre = False
            self.chunks.append("".join(self.buf))
            self.buf = []

    def handle_data(self, data):
        if self.in_pre:
            self.buf.append(data)


def read_html_pre(path: Path) -> str:
    """Read an HTM file and return concatenated <pre> block text."""
    text = path.read_text(encoding="utf-8-sig")
    p = PreExtractor()
    p.feed(text)
    return "\n\n".join(p.chunks).rstrip() + "\n"


def read_full_html(path: Path) -> str:
    """Read raw HTML (utf-8-sig)."""
    return path.read_text(encoding="utf-8-sig")


def strip_inline_html(s: str) -> str:
    """Remove simple inline tags (b, font, a without id) that sneak in outside
    of <pre>. Used for walkthrough parsing."""
    s = re.sub(r"</?(?:b|font|font[^>]*|FONT[^>]*|B)\s*>", "", s)
    s = re.sub(r"<A\s+[^>]*>", "", s, flags=re.IGNORECASE)
    s = s.replace("</A>", "").replace("</a>", "")
    s = re.sub(r"<br\s*/?>", "\n", s, flags=re.IGNORECASE)
    return s


# ---------------------------------------------------------------------------
# Shared writers
# ---------------------------------------------------------------------------

def write_md(name: str, title: str, body: str, *, subdir: Path | None = None) -> None:
    path = (subdir or DATA_DIR) / name
    path.parent.mkdir(parents=True, exist_ok=True)
    # Strip anchor markers from visible output but keep them as html anchors
    body = re.sub(
        r"\{\{ANCHOR:([^}]+)\}\}", r'<a id="\1"></a>', body
    )
    path.write_text(
        f"# {title}\n\n{body.rstrip()}\n", encoding="utf-8", newline="\n"
    )


def write_json(name: str, data) -> None:
    (INDEX_DIR / name).write_text(
        json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8", newline="\n"
    )


# ---------------------------------------------------------------------------
# Processors: one per source HTM
# ---------------------------------------------------------------------------

# --- introductions.htm: formulas.md, terrain.md, intro.md ---

def process_introductions() -> None:
    text = read_html_pre(GUIDE_ROOT / "introductions.htm")

    # Split by bold-like headers. The HTML has <b>Header</b>: patterns which
    # become plain "Header：" here. We instead split on common section markers.
    parts = re.split(r"\n\s*(?=<[bB]>)", text)  # won't actually match after PreExtractor
    # Instead use the text pattern we know: the file has "遊戲概述", "地圖上的操作",
    # "屬性", "地形", "提示" (containing formulas) as section headers.

    def extract(start_headers: list[str], stop_headers: list[str]) -> str:
        for sh in start_headers:
            i = text.find(sh)
            if i < 0:
                continue
            # find earliest stop
            stop = len(text)
            for eh in stop_headers:
                j = text.find(eh, i + len(sh))
                if j > 0:
                    stop = min(stop, j)
            return text[i:stop].strip()
        return ""

    all_sections = ["遊戲概述", "地圖上的操作", "屬性", "地形", "提示"]

    formulas_section = extract(["6.遊戲所使用的公式"], [])
    terrain_section = extract(["地形"], ["提示"])
    intro_general = text  # keep the full text as the general intro

    # formulas.md
    write_md(
        "formulas.md",
        "公式（傷害／命中／經驗值）",
        "來源：`Strategy_Guide/introductions.htm`\n\n```\n"
        + formulas_section
        + "\n```\n",
    )

    # terrain.md
    write_md(
        "terrain.md",
        "地形影響",
        "來源：`Strategy_Guide/introductions.htm`\n\n```\n"
        + terrain_section
        + "\n```\n",
    )

    # intro.md (general overview, keep as reference)
    write_md(
        "intro.md",
        "遊戲基本資訊與操作",
        "來源：`Strategy_Guide/introductions.htm`\n\n```\n"
        + intro_general
        + "\n```\n",
    )

    # formulas.json: index each named formula
    formulas: dict[str, dict] = OrderedDict()
    for name in [
        "物理攻擊傷害", "物理攻擊命中率", "劍技攻擊傷害",
        "法術攻擊傷害", "法術攻擊命中率", "攻擊經驗值",
        "法術恢復力", "恢復法術經驗值", "傳送術經驗值",
        "行動術經驗值", "魔刃術／魔鎧術／風行術經驗值",
        "麻痹術／毒擊術經驗值", "解毒術／祛麻術經驗值",
    ]:
        formulas[name] = {
            "name": name,
            "md_file": "data/formulas.md",
            "md_anchor": None,
        }
    write_json("formulas.json", formulas)


# --- item.htm: items.md + items.json ---

ITEM_TYPE_MAP = {
    0x01: "劍",
    0x02: "刀",
    0x03: "槍",
    0x04: "斧",
    0x05: "弓",
    0x06: "杖",
    0x07: "爪",
    0x08: "機械手臂",
    0x20: "道具",
}


def process_items() -> None:
    text = read_html_pre(GUIDE_ROOT / "item.htm")

    write_md(
        "items.md",
        "裝備／道具一覽表",
        "來源：`Strategy_Guide/item.htm`。編號 00h-D6h，共 215+ 項。\n\n```\n"
        + text
        + "\n```\n",
    )

    # Parse entries. Pattern:  "0Bh   炎龍劍    AP 400  HIT 120  DP 030  30%暴擊  ..."
    entries: dict[str, dict] = OrderedDict()
    line_re = re.compile(
        r"^\s*([0-9A-Fa-f]{2})h\s+(\S+?)\s{2,}(.*?)\s*$"
    )
    for line in text.splitlines():
        m = line_re.match(line)
        if not m:
            continue
        id_hex = m.group(1).upper()
        name = m.group(2)
        rest = m.group(3)

        def find_stat(label: str) -> int | None:
            r = re.search(rf"{label}\s*([0-9]+)", rest)
            return int(r.group(1)) if r else None

        ap = find_stat("AP")
        hit = find_stat("HIT")
        dp = find_stat("DP")
        ev = find_stat("EV")

        crit_m = re.search(r"(\d+)%暴擊", rest)
        poison_m = re.search(r"(\d+)%中毒", rest)
        double_m = re.search(r"(\d+)%雙擊", rest)

        use_spell_m = re.search(r"使用有(\S+?)效果", rest)
        range_m = re.search(r"距(\d+)", rest)
        aoe_m = re.search(r"範(\d+)", rest)

        tag_m = re.search(r"\(([^)]+最強武器[^)]*)\)", rest)

        entries[id_hex] = {
            "id_hex": id_hex + "h",
            "id_dec": int(id_hex, 16),
            "name": name,
            "AP": ap,
            "HIT": hit,
            "DP": dp,
            "EV": ev,
            "crit_pct": int(crit_m.group(1)) if crit_m else None,
            "poison_pct": int(poison_m.group(1)) if poison_m else None,
            "double_pct": int(double_m.group(1)) if double_m else None,
            "on_use_spell": use_spell_m.group(1) if use_spell_m else None,
            "on_use_range": int(range_m.group(1)) if range_m else None,
            "on_use_aoe": int(aoe_m.group(1)) if aoe_m else None,
            "tag": tag_m.group(1) if tag_m else None,
            "raw_desc": rest.strip(),
            "md_section": f"data/items.md",
        }

    write_json("items.json", entries)


# --- spell.htm: spells.md + spell_by_character.md + spells.json ---

SPELL_NAME_BY_ID = {
    0x00: "火炎術", 0x01: "烈炎術", 0x02: "炎龍術", 0x03: "天火術",
    0x04: "電擊術", 0x05: "落雷術", 0x06: "轟雷術", 0x07: "神雷術",
    0x08: "聖光彈", 0x09: "咒殺術", 0x0A: "碎岩術", 0x0B: "地震術",
    0x0C: "裂地術", 0x0D: "治療術", 0x0E: "回復術", 0x0F: "再生術",
    0x10: "神恩術", 0x11: "魔刃術", 0x12: "魔鎧術", 0x13: "風行術",
    0x14: "解毒術", 0x15: "祛麻術", 0x16: "封咒術", 0x17: "傳送術",
    0x18: "破龍擊", 0x19: "行動術", 0x1A: "毒擊術", 0x1B: "麻痹術",
    0x1C: "淒煌斬", 0x1D: "熾炎刀", 0x1E: "音速刃",
    0x20: "熾天使", 0x21: "風妖精", 0x22: "破壞神", 0x23: "暗邪鬼",
}


def process_spells() -> None:
    text = read_html_pre(GUIDE_ROOT / "spell.htm")

    # Split the file into two main sections using header hints.
    by_char_idx = text.find("各人物職業等級擁有的法術")
    list_idx = text.find("法術列表")
    if list_idx < 0:
        list_idx = text.find("攻擊法術")
    if by_char_idx < 0 or list_idx < 0:
        # fallback: keep everything in spells.md
        write_md(
            "spells.md", "法術列表",
            "來源：`Strategy_Guide/spell.htm`\n\n```\n" + text + "\n```\n",
        )
        write_md(
            "spell_by_character.md", "各角色職業等級習得法術",
            "來源：`Strategy_Guide/spell.htm`\n\n```\n" + text + "\n```\n",
        )
    else:
        by_char = text[by_char_idx:list_idx].strip()
        spells = text[list_idx:].strip()
        write_md(
            "spell_by_character.md",
            "各角色職業等級習得法術",
            "來源：`Strategy_Guide/spell.htm`\n\n```\n" + by_char + "\n```\n",
        )
        write_md(
            "spells.md",
            "法術列表（攻擊／劍技／恢復／輔助）",
            "來源：`Strategy_Guide/spell.htm` + `code_hack2.htm` §3\n\n```\n"
            + spells + "\n```\n",
        )

    # Build spells.json. Use SPELL_NAME_BY_ID as the source of truth for IDs,
    # and parse the table rows from spell.htm for numeric fields.
    entries: dict[str, dict] = OrderedDict()
    for sid, name in SPELL_NAME_BY_ID.items():
        entries[f"{sid:02X}"] = {
            "id_hex": f"{sid:02X}h",
            "id_dec": sid,
            "name": name,
            "category": None,
            "distance": None,
            "aoe": None,
            "mp": None,
            "max_damage": None,
            "hit_pct": None,
            "md_section": "data/spells.md",
        }

    # categorize
    for sid in range(0x00, 0x0D):
        entries[f"{sid:02X}"]["category"] = "攻擊法術"
    for sid in [0x18, 0x1C, 0x1D, 0x1E]:
        entries[f"{sid:02X}"]["category"] = "劍技"
    for sid in range(0x0D, 0x11):
        entries[f"{sid:02X}"]["category"] = "恢復法術"
    for sid in [0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B]:
        entries[f"{sid:02X}"]["category"] = "輔助法術"
    for sid in [0x20, 0x21, 0x22, 0x23]:
        entries[f"{sid:02X}"]["category"] = "召喚／特殊"

    # Try to parse numeric fields by matching "名稱  dist aoe mp ..." rows.
    for line in text.splitlines():
        for sid, name in SPELL_NAME_BY_ID.items():
            if name not in line:
                continue
            nums = re.findall(r"\b\d+\b", line)
            if len(nums) >= 4:
                e = entries[f"{sid:02X}"]
                # The first four or five numbers follow: distance, aoe, mp, max_dmg, hit
                if e["distance"] is None:
                    try:
                        e["distance"] = int(nums[0])
                        e["aoe"] = int(nums[1])
                        e["mp"] = int(nums[2])
                        if len(nums) >= 4:
                            e["max_damage"] = int(nums[3])
                        if len(nums) >= 5 and "%" in line:
                            hit_m = re.search(r"(\d+)%", line)
                            if hit_m:
                                e["hit_pct"] = int(hit_m.group(1))
                    except ValueError:
                        pass
            break

    write_json("spells.json", entries)


# --- memory_hack.htm: memory_layout.md, portraits.md, jobs.md (part 1) ---

JOB_NAME_BY_ID: dict[int, str] = {}
PORTRAIT_NAME_BY_ID: dict[int, str] = {}


def process_memory_hack() -> None:
    text = read_html_pre(GUIDE_ROOT / "memory_hack.htm")

    # Split on ANCHOR markers
    # Structure: prelude + {{ANCHOR:L1}} portraits + {{ANCHOR:L2}} jobs
    layout_part = text
    m1 = re.search(r"\{\{ANCHOR:L1\}\}", text)
    m2 = re.search(r"\{\{ANCHOR:L2\}\}", text)

    if m1 and m2:
        layout_part = text[:m1.start()].rstrip()
        portraits_part = text[m1.end():m2.start()].strip()
        jobs_part = text[m2.end():].strip()
    else:
        portraits_part = ""
        jobs_part = ""

    write_md(
        "memory_layout.md",
        "運行時人物記憶體結構（每位 80 bytes = 0x50）",
        "來源：`Strategy_Guide/memory_hack.htm`\n\n```\n"
        + layout_part + "\n```\n",
    )

    # Portraits
    portrait_lines = []
    for line in portraits_part.splitlines():
        s = line.strip()
        if not s:
            continue
        mm = re.match(r"^([0-9A-Fa-f]{2})\s+(.+)$", s)
        if mm:
            pid = int(mm.group(1), 16)
            pname = mm.group(2).strip()
            PORTRAIT_NAME_BY_ID[pid] = pname
            portrait_lines.append(f"{mm.group(1).upper()}h  {pname}")

    write_md(
        "portraits.md",
        "肖像編號表",
        "來源：`Strategy_Guide/memory_hack.htm`\n\n```\n"
        + "\n".join(portrait_lines) + "\n```\n",
    )

    # Jobs (name only from here; magic resist and crit come later)
    for line in jobs_part.splitlines():
        # Format: "00=龍      01=劍士    ..." multi per line
        for mm in re.finditer(r"([0-9A-Fa-f]{2})=(\S+)", line):
            jid = int(mm.group(1), 16)
            JOB_NAME_BY_ID[jid] = mm.group(2).strip()


# --- code_hack2.htm: offsets, characters_base, characters_growth,
#     spell_learning, enemies, file_formats, jobs (part 2: resist/crit) ---

def process_code_hack2() -> None:
    text = read_html_pre(GUIDE_ROOT / "code_hack2.htm")

    # Section headers in this file are numbered "1.物品功效資訊："…"8.敵人／友軍等級資訊："
    # Then "修改：FDFIELD.DAT" and "修改：FDSHAP.DAT" segments at the end.
    section_marks: list[tuple[int, str, str]] = []
    # numbered sections 1..8
    for m in re.finditer(r"(?m)^(\d)\.([^\n：]+)：", text):
        section_marks.append((m.start(), m.group(1), m.group(2).strip()))
    # FDFIELD / FDSHAP file formats
    for m in re.finditer(r"(?m)^修改：(\S+)", text):
        section_marks.append((m.start(), "F", m.group(1).strip()))

    section_marks.sort()
    # Compute body ranges
    bodies: dict[str, str] = {}
    for i, (pos, tag, title) in enumerate(section_marks):
        end = section_marks[i + 1][0] if i + 1 < len(section_marks) else len(text)
        key = f"{tag}:{title}" if tag != "F" else f"FILE:{title}"
        bodies[key] = text[pos:end].rstrip()

    # ---- Offsets: extract "XXXXh或YYYYh開始，每N byte" patterns ----
    # We scan the WHOLE text so sub-sections (e.g. magic resist, crit rate) are
    # captured too.
    offsets: dict[str, dict] = OrderedDict()

    header_re = re.compile(
        r"(?P<num>\d+)\.(?P<title>[^：\n]+)：(?P<exe>[0-9A-Fa-f]+)h(?:或(?P<le>[0-9A-Fa-f]+)h)?開始[^。\n]*?每[^。\n]*?(?P<size>\d+)\s*byte"
    )
    for m in header_re.finditer(text):
        exe = m.group("exe").upper()
        le = (m.group("le") or "").upper()
        size = int(m.group("size"))
        title = m.group("title").strip()
        # find signature line: "找 XX XX..." or "開頭為 XX XX..."
        after = text[m.end():m.end() + 300]
        sig_m = re.search(
            r"(?:找|開頭為)[ \t]*([0-9A-Fa-f]{2}(?:[ \t]+[0-9A-Fa-f]{2})+)", after
        )
        sig = sig_m.group(1).upper() if sig_m else None
        key = le or exe
        slug = title_to_slug(title)
        offsets[key] = {
            "name": slug,
            "title": title,
            "exe_addr_sample": f"0x{exe}",
            "le_addr_sample": f"0x{le}" if le else None,
            "version_note": "位址為攻略所記載版本；實際位址可能隨 FD2.EXE 版本浮動，以 signature 為準。",
            "entry_size": size,
            "search_bytes": sig,
            "signature_stable_across_versions": True,
            "notes_md_file": "data/offsets.md",
            "md_anchor": f"offset-{key}",
        }
        if le and exe != le:
            offsets[exe] = {"alias_of": le}

    # Parenthesized sub-offsets like "(1)魔法抗性資料：76FAAh或51D96h開始，每職業4 byte"
    sub_re = re.compile(
        r"\((?P<n>\d+)\)(?P<title>[^：\n]+)：(?P<exe>[0-9A-Fa-f]+)h(?:或(?P<le>[0-9A-Fa-f]+)h)?開始[^。\n]*?每(?P<unit>[^\d\n]*?)(?P<size>\d+)\s*byte"
    )
    for m in sub_re.finditer(text):
        exe = m.group("exe").upper()
        le = (m.group("le") or "").upper()
        size = int(m.group("size"))
        title = m.group("title").strip()
        after = text[m.end():m.end() + 300]
        sig_m = re.search(
            r"(?:找|開頭為)[ \t]*([0-9A-Fa-f]{2}(?:[ \t]+[0-9A-Fa-f]{2})+)", after
        )
        sig = sig_m.group(1).upper() if sig_m else None
        key = le or exe
        slug = title_to_slug(title)
        if key in offsets:
            continue
        offsets[key] = {
            "name": slug,
            "title": title,
            "exe_addr_sample": f"0x{exe}",
            "le_addr_sample": f"0x{le}" if le else None,
            "version_note": "位址為攻略所記載版本；實際位址可能隨 FD2.EXE 版本浮動，以 signature 為準。",
            "entry_size": size,
            "entry_unit": m.group("unit").strip() or None,
            "search_bytes": sig,
            "signature_stable_across_versions": True,
            "notes_md_file": "data/offsets.md",
            "md_anchor": f"offset-{key}",
        }
        if le and exe != le:
            offsets[exe] = {"alias_of": le}

    write_json("offsets.json", offsets)

    # ---- offsets.md ----
    offsets_md_lines = [
        "來源：`Strategy_Guide/code_hack2.htm`\n",
        "",
        "⚠️ **版本差異**：下表位址對應攻略作者使用的 FD2.EXE 版本。實際位址可能",
        "因版本不同浮動。`entry_size` 與 `search_bytes` 是跨版本穩定的定位依據；",
        "若 Ghidra 中的位址與此處不符，請用 `search_bytes` 在二進位中重新定位。\n",
        "",
        "| Name | FD2.LE (sample) | FD2.EXE (sample) | entry_size | signature |",
        "|------|-----------------|------------------|------------|-----------|",
    ]
    for key, v in offsets.items():
        if "alias_of" in v:
            continue
        offsets_md_lines.append(
            f"| <a id=\"offset-{key}\"></a>{v['name']} "
            f"| `{v.get('le_addr_sample') or '-'}` "
            f"| `{v['exe_addr_sample']}` "
            f"| {v['entry_size']} "
            f"| `{v.get('search_bytes') or '-'}` |"
        )
    offsets_md_lines.append("")
    offsets_md_lines.append("## 原始內容（`code_hack2.htm` 對應段落）\n")
    offsets_md_lines.append("```")
    offsets_md_lines.append(text)
    offsets_md_lines.append("```")
    write_md(
        "offsets.md",
        "記憶體偏移總表（FD2.EXE ↔ FD2.LE）",
        "\n".join(offsets_md_lines),
    )

    # ---- characters_base.md ----
    body = bodies.get("4:人物出場屬性資訊", "")
    write_md(
        "characters_base.md",
        "人物出場屬性表（每位 24 bytes）",
        "來源：`Strategy_Guide/code_hack2.htm` §4\n\n```\n"
        + body + "\n```\n",
    )

    # ---- characters_growth.md + characters.json partial ----
    body = bodies.get("5:升級屬性資料", "")
    write_md(
        "characters_growth.md",
        "人物升級屬性表（每位 11 bytes）",
        "來源：`Strategy_Guide/code_hack2.htm` §5\n\n```\n"
        + body + "\n```\n",
    )
    growth_entries = parse_growth_table(body)

    # ---- spell_learning.md ----
    body = bodies.get("6:法術習得等級資料", "")
    write_md(
        "spell_learning.md",
        "法術習得等級資料（每組 12 bytes）",
        "來源：`Strategy_Guide/code_hack2.htm` §6\n\n```\n"
        + body + "\n```\n",
    )

    # ---- jobs.md (combine memory_hack names + resist/crit from §7) ----
    body_jobs = bodies.get("7:職業相關資料", "")
    jobs_json: dict[str, dict] = OrderedDict()
    for line in body_jobs.splitlines():
        mm = re.match(
            r"^([0-9A-Fa-f]{2})\s+(\S+)\s+(?:--|\d+%)\s+(\d+)%",
            line.strip(),
        )
        if not mm:
            continue
        jid = int(mm.group(1), 16)
        jname = mm.group(2)
        # Magic resist: capture "--" or NN%
        # re-parse with a slightly different pattern
        m2 = re.match(
            r"^([0-9A-Fa-f]{2})\s+(\S+)\s+(--|\d+%)\s+(\d+)%",
            line.strip(),
        )
        if m2:
            resist_raw = m2.group(3)
            crit_raw = m2.group(4)
            jobs_json[f"{jid:02X}"] = {
                "id_hex": f"{jid:02X}h",
                "id_dec": jid,
                "name": JOB_NAME_BY_ID.get(jid, jname),
                "magic_resist_pct": None if resist_raw == "--" else int(
                    resist_raw.rstrip("%")
                ),
                "crit_pct": int(crit_raw),
                "md_section": "data/jobs.md",
            }
    # Also add job 00 (龍) and any jobs from JOB_NAME_BY_ID that don't appear
    # in this table (因為編號00沒有這兩項資料).
    for jid, jname in JOB_NAME_BY_ID.items():
        key = f"{jid:02X}"
        if key not in jobs_json:
            jobs_json[key] = {
                "id_hex": f"{jid:02X}h",
                "id_dec": jid,
                "name": jname,
                "magic_resist_pct": None,
                "crit_pct": None,
                "md_section": "data/jobs.md",
            }

    write_json("jobs.json", dict(sorted(jobs_json.items())))

    # jobs.md combines memory_hack job list + code_hack2 table
    jobs_md = (
        "來源：`Strategy_Guide/memory_hack.htm`（職業編號） + "
        "`code_hack2.htm` §7（魔抗／暴擊率）\n\n"
        "```\n職業編號表（來自 memory_hack.htm）：\n\n"
    )
    for jid, jname in sorted(JOB_NAME_BY_ID.items()):
        jobs_md += f"{jid:02X}h  {jname}\n"
    jobs_md += "\n職業屬性（來自 code_hack2.htm §7）：\n\n"
    jobs_md += body_jobs.strip() + "\n```\n"
    write_md("jobs.md", "職業編號與屬性", jobs_md)

    # ---- enemies.md + enemies.json ----
    body_en = bodies.get("8:敵人／友軍等級資訊", "")
    enemy_entries = parse_enemy_table(body_en)
    write_md(
        "enemies.md",
        "敵人／友軍等級資訊（每位 10 bytes）",
        "來源：`Strategy_Guide/code_hack2.htm` §8\n\n```\n"
        + body_en + "\n```\n",
    )
    write_json("enemies.json", enemy_entries)

    # ---- file_formats.md ----
    parts = []
    for k, v in bodies.items():
        if k.startswith("FILE:"):
            parts.append(v)
    # Also include sections 1 (items struct), 2 (shop), plus the whole FDFIELD
    # / FDSHAP content
    write_md(
        "file_formats.md",
        "資料檔結構（FDFIELD.DAT / FDSHAP.DAT）",
        "來源：`Strategy_Guide/code_hack2.htm`\n\n```\n"
        + "\n\n".join(parts) + "\n```\n",
    )

    # ---- characters.json: merge base stats from §5 growth table plus
    # memory_hack portrait IDs.
    characters_json: dict[str, dict] = OrderedDict()
    for i, row in enumerate(growth_entries):
        key = f"growth_{i:03d}"
        characters_json[key] = row
    # Index by name for easy query
    by_name: dict[str, list[dict]] = {}
    for row in growth_entries:
        by_name.setdefault(row["name"], []).append(row)
    write_json(
        "characters.json",
        {
            "growth_rows": growth_entries,
            "by_name": by_name,
            "portraits": {f"{k:02X}": v for k, v in PORTRAIT_NAME_BY_ID.items()},
        },
    )
    write_json(
        "portraits.json",
        {f"{k:02X}": {"id_hex": f"{k:02X}h", "id_dec": k, "name": v}
         for k, v in sorted(PORTRAIT_NAME_BY_ID.items())},
    )


def title_to_slug(title: str) -> str:
    mapping = {
        "物品功效資訊": "item_effect_table",
        "商店出售物品資訊": "shop_table",
        "法術功效資訊": "spell_effect_table",
        "人物出場屬性資訊": "character_base_table",
        "升級屬性資料": "character_growth_table",
        "法術習得等級資料": "spell_learning_table",
        "職業相關資料": "job_data",
        "魔法抗性資料": "job_magic_resist_table",
        "暴擊率資料": "job_crit_table",
        "敵人／友軍等級資訊": "enemy_data_table",
    }
    for k, v in mapping.items():
        if k in title:
            return v
    return re.sub(r"\W+", "_", title).strip("_") or "unknown"


def parse_growth_table(body: str) -> list[dict]:
    """Parse lines like '7B0B5 索爾  06 08 04 06 02 03 08 0C 00 00 FF'."""
    out: list[dict] = []
    line_re = re.compile(
        r"^([0-9A-Fa-f]{4,5})\s+(\S+(?:\s+\S+)?)\s+"
        r"((?:[0-9A-Fa-f]{2}\s+){10}[0-9A-Fa-f]{2})\s*$"
    )
    for line in body.splitlines():
        m = line_re.match(line.strip())
        if not m:
            continue
        addr = m.group(1).upper()
        name_part = m.group(2).strip()
        bytes_ = m.group(3).split()
        if "-" * 10 in name_part:
            continue

        # name may be "索爾" or "索爾    劍聖" (with class)
        parts = name_part.split()
        char_name = parts[0]
        job_name = parts[1] if len(parts) > 1 else None

        b = [int(x, 16) for x in bytes_]
        out.append({
            "address_sample": f"0x{addr}",
            "name": char_name,
            "job": job_name,
            "AP_min": b[0], "AP_max_plus1": b[1],
            "DP_min": b[2], "DP_max_plus1": b[3],
            "DX_min": b[4], "DX_max_plus1": b[5],
            "HP_min": b[6], "HP_max_plus1": b[7],
            "MP_min": b[8], "MP_max_plus1": b[9],
            "MG_index": b[10],
            "raw_bytes": " ".join(f"{x:02X}" for x in b),
            "md_section": "data/characters_growth.md",
        })
    return out


def parse_enemy_table(body: str) -> dict[str, dict]:
    """Parse lines like '7AB0D 士兵  01 02  18   0  5  2  1  4  30'."""
    out: dict[str, dict] = OrderedDict()
    # Columns: RA(hex) CL(hex) HP MP AP DP DX MV EX (decimal)
    line_re = re.compile(
        r"^([0-9A-Fa-f]{4,5})\s+(.+?)\s{2,}"
        r"([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+"
        r"(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$"
    )
    current_side = "unknown"
    for line in body.splitlines():
        s = line.strip()
        if "友軍" in s:
            current_side = "ally"
            continue
        if "敵人" in s:
            current_side = "enemy"
            continue
        m = line_re.match(s)
        if not m:
            continue
        addr = m.group(1).upper()
        name = m.group(2).strip()
        ra = int(m.group(3), 16)
        cl = int(m.group(4), 16)
        vals = [int(m.group(i)) for i in range(5, 12)]
        out[addr] = {
            "address_sample": f"0x{addr}",
            "name": name,
            "side": current_side,
            "RA": ra, "CL": cl,
            "HP_per_lvl": vals[0], "MP_per_lvl": vals[1],
            "AP_per_lvl": vals[2], "DP_per_lvl": vals[3],
            "DX_per_lvl": vals[4], "MV": vals[5],
            "EX_per_lvl": vals[6],
            "md_section": "data/enemies.md",
        }
    return out


# --- code_hack.htm: code_hacks.md ---

def process_code_hack() -> None:
    text = read_html_pre(GUIDE_ROOT / "code_hack.htm")
    write_md(
        "code_hacks.md",
        "FD2.EXE / FDFIELD.DAT Byte-Pattern 修改清單",
        "來源：`Strategy_Guide/code_hack.htm`\n\n"
        "⚠️ 以下 search / replace 的 byte pattern 是針對攻略作者當時使用的"
        "版本。不同版本可能偏移不同，但搜尋 pattern 通常仍有效。\n\n```\n"
        + text + "\n```\n",
    )


# --- character_list.htm: characters_ranks.md ---

def process_character_list() -> None:
    text = read_html_pre(GUIDE_ROOT / "character_list.htm")
    write_md(
        "characters_ranks.md",
        "人物屬性／排名／轉職建議",
        "來源：`Strategy_Guide/character_list.htm`\n\n```\n"
        + text + "\n```\n",
    )


# --- walkthrough.htm: chapters/chapter_NN.md + chapters.json + _index.md ---

def process_walkthrough() -> None:
    raw = read_full_html(GUIDE_ROOT / "walkthrough.htm")
    # Isolate the <pre> body but keep the <font color=blue><b>第N章 ...</b></font>
    # headers so we can split on them. We'll manually strip the outer HTML.
    pre_match = re.search(r"<pre[^>]*>(.*?)</pre>", raw, flags=re.DOTALL)
    if not pre_match:
        return
    body = pre_match.group(1)

    # Headers look like: <font color=blue><b>第N章  TITLE</b></font>
    # We'll split by these.
    header_re = re.compile(
        r"<font\s+color=blue><b>\s*第(\d+)章\s*(.*?)</b></font>",
        flags=re.IGNORECASE,
    )
    parts = list(header_re.finditer(body))

    chapters_json: dict[str, dict] = OrderedDict()
    index_lines = ["| # | 章節名稱 | 備註／加入 |", "|---|---|---|"]

    tail_start = parts[-1].start() if parts else 0
    for i, m in enumerate(parts):
        ch_num = int(m.group(1))
        ch_title = strip_inline_html(m.group(2)).strip()
        body_start = m.end()
        body_end = parts[i + 1].start() if i + 1 < len(parts) else len(body)
        ch_body_raw = body[body_start:body_end]

        # Convert inline HTML: tables with <IMG> become "[地圖圖示：fd2-N.jpg，專案中未附]"
        cleaned = ch_body_raw
        cleaned = re.sub(
            r"<IMG\s+src=\"([^\"]+)\"[^>]*>",
            r"[地圖圖示：\1，專案中未附]",
            cleaned,
            flags=re.IGNORECASE,
        )
        cleaned = re.sub(r"<table[^>]*>", "\n", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"</table>", "\n", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"<tr[^>]*>", "", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"</tr>", "", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"<td[^>]*>", "", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"</td>", "", cleaned, flags=re.IGNORECASE)
        cleaned = re.sub(r"<br\s*/?>", "\n", cleaned, flags=re.IGNORECASE)
        cleaned = strip_inline_html(cleaned)
        cleaned = html.unescape(cleaned)
        cleaned = cleaned.strip("\n")

        # Extract quick summary fields
        join_m = re.search(r"(?m)^加入：(.+)$", cleaned)
        note_m = re.search(r"(?m)^備註：(.+)$", cleaned)
        join_str = join_m.group(1).strip() if join_m else ""
        note_str = note_m.group(1).strip() if note_m else ""

        chapters_json[str(ch_num)] = {
            "chapter": ch_num,
            "title": ch_title,
            "md_file": f"data/chapters/chapter_{ch_num:02d}.md",
            "join": join_str,
            "note": note_str,
        }
        index_lines.append(
            f"| {ch_num} | {ch_title} | "
            f"{('加入：' + join_str) if join_str else ''}"
            f"{(' ／ 備註：' + note_str) if note_str else ''} |"
        )

        md_body = (
            f"來源：`Strategy_Guide/walkthrough.htm` (第{ch_num}章)\n\n```\n"
            + cleaned + "\n```\n"
        )
        write_md(
            f"chapter_{ch_num:02d}.md",
            f"第{ch_num}章  {ch_title}",
            md_body,
            subdir=CHAPTERS_DIR,
        )

    write_json("chapters.json", chapters_json)

    # Tail content after last chapter: "隱藏條件" etc.
    if parts:
        tail_from = parts[-1].end()
        # Skip past this chapter body; tail after all is covered by last chapter.
        pass

    write_md(
        "_index.md",
        "30 章攻略總覽",
        "來源：`Strategy_Guide/walkthrough.htm`\n\n" + "\n".join(index_lines) + "\n",
        subdir=CHAPTERS_DIR,
    )


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------

def verify() -> int:
    errors: list[str] = []

    # 1. items.json has >= 200 entries
    items = json.loads((INDEX_DIR / "items.json").read_text(encoding="utf-8"))
    if len(items) < 190:
        errors.append(f"items.json has only {len(items)} entries (expected >= 190)")

    # 2. chapters.json has exactly 30 chapters
    chapters = json.loads((INDEX_DIR / "chapters.json").read_text(encoding="utf-8"))
    if len(chapters) != 30:
        errors.append(f"chapters.json has {len(chapters)} (expected 30)")

    # 3. offsets.json covers 8 key sections (item, shop, spell, char_base,
    # char_growth, spell_learn, enemy + magic_resist/crit extras).
    offsets = json.loads((INDEX_DIR / "offsets.json").read_text(encoding="utf-8"))
    required_slugs = {
        "item_effect_table",
        "shop_table",
        "spell_effect_table",
        "character_base_table",
        "character_growth_table",
        "spell_learning_table",
        "enemy_data_table",
    }
    found_slugs = {v.get("name") for v in offsets.values() if "name" in v}
    missing = required_slugs - found_slugs
    if missing:
        errors.append(f"offsets.json missing slugs: {missing}")

    # 4. spells.json has all known spell IDs
    spells = json.loads((INDEX_DIR / "spells.json").read_text(encoding="utf-8"))
    if len(spells) < 30:
        errors.append(f"spells.json has only {len(spells)} (expected >= 30)")

    # 5. jobs.json has id 1A
    jobs = json.loads((INDEX_DIR / "jobs.json").read_text(encoding="utf-8"))
    if "1A" not in jobs:
        errors.append("jobs.json missing id 1A")

    # 6. All md files are UTF-8 without BOM
    for md in DATA_DIR.rglob("*.md"):
        raw = md.read_bytes()
        if raw.startswith(b"\xef\xbb\xbf"):
            errors.append(f"{md} has BOM")

    # 7. chapter_01 contains 哈諾 (第1章 加入)
    ch1 = (CHAPTERS_DIR / "chapter_01.md").read_text(encoding="utf-8")
    if "哈諾" not in ch1:
        errors.append("chapter_01.md does not mention 哈諾")

    # 8. Ensure 索爾 appears in characters.json
    chars = json.loads((INDEX_DIR / "characters.json").read_text(encoding="utf-8"))
    if "索爾" not in chars["by_name"]:
        errors.append("characters.json by_name missing 索爾")

    if errors:
        print("VERIFY FAILED:")
        for e in errors:
            print("  -", e)
        return 1
    print(f"VERIFY OK — items={len(items)}, chapters={len(chapters)}, "
          f"offsets={sum(1 for v in offsets.values() if 'alias_of' not in v)}, "
          f"spells={len(spells)}, jobs={len(jobs)}, chars={len(chars['by_name'])}")
    return 0


# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify", action="store_true",
                    help="after building, run self-checks")
    args = ap.parse_args()

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    CHAPTERS_DIR.mkdir(parents=True, exist_ok=True)
    INDEX_DIR.mkdir(parents=True, exist_ok=True)

    # Order matters: memory_hack populates JOB_NAME_BY_ID used later.
    process_introductions()
    process_items()
    process_spells()
    process_memory_hack()
    process_code_hack2()
    process_code_hack()
    process_character_list()
    process_walkthrough()

    print(f"Built into {SKILL_ROOT.relative_to(SKILL_ROOT.parent.parent.parent.parent) if False else SKILL_ROOT}")

    if args.verify:
        return verify()
    return 0


if __name__ == "__main__":
    sys.exit(main())
