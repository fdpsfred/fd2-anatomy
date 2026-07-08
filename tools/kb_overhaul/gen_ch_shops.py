"""Format per-chapter §商店 markdown for chapters/chapter_NN.md, decoded from the
chapter_intro_metadata table (the authoritative shop-item source; consumer
fd2_load_chapter_shop_item_ids / fd2_run_chapter_intro_menu_main in src/ui_menu).

Ground truth (all verified against Ghidra + src, see workspace/kb_overhaul/batch5_shop_findings.md):
  - table = data_fd2_chapter_intro_metadata_table @ 0x6238D, 26 entries x 31 B.
    entry layout: +0 bCategory, +1 bHotkey_state, +2 bHotkey_scancode,
                  +3 weapons[12], +15 items[8], +23 mystery[8]  (0xFF = empty slot)
  - entry_index = chapter_n - 1  (entry0 = ch1 starting gear .. entry25 = ch26 endgame gear).
  - shop is shown only for STORY chapters (data_fd2_chapter_per_chapter_category_table
    @ 0x526B9, chapter_id byte == 0). BATTLE chapters (byte != 0 = ch23,24,25,28,29,30)
    skip the intro menu -> no shop, even if their entry is populated.
  - the three shops map to the intro-menu cursor_state: weapons(+3)=武器店, items(+15)=道具店,
    mystery(+23)=神秘商店 opened by the hidden hotkey at +2 scancode.

Table bytes are read from workspace/kb_overhaul/intro_metadata.hex and
chapter_category.hex (live Ghidra dumps saved as intermediate data; a sentinel
self-check catches any drift). Item names come from assets/items.md.

CLI:
    python tools/kb_overhaul/gen_ch_shops.py --check
    python tools/kb_overhaul/gen_ch_shops.py --chapter 5
    python tools/kb_overhaul/gen_ch_shops.py --all --write
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
WS = REPO_ROOT / "workspace" / "kb_overhaul"
OUT_DIR = WS / "shops"
INTRO_HEX = WS / "intro_metadata.hex"
CAT_HEX = WS / "chapter_category.hex"

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_ch_section3 as s3  # reuse load_item_names  # noqa: E402

ENTRY_STRIDE = 31
WEAPONS_OFF, WEAPONS_N = 3, 12
ITEMS_OFF, ITEMS_N = 15, 8
MYSTERY_OFF, MYSTERY_N = 23, 8
SCANCODE_OFF = 2
N_ENTRIES = 26


def load_bytes(p: Path) -> bytes:
    return bytes.fromhex(p.read_text(encoding="utf-8").strip())


def scancode_label(sc: int) -> str:
    """F-key combo for a BIOS scancode. Shift+F1..F10 = 0x54..0x5D,
    Ctrl+F1..F10 = 0x5E..0x67, Alt+F1..F10 = 0x68..0x71."""
    if 0x54 <= sc <= 0x5D:
        return f"Shift+F{sc - 0x53}"
    if 0x5E <= sc <= 0x67:
        return f"Ctrl+F{sc - 0x5D}"
    if 0x68 <= sc <= 0x71:
        return f"Alt+F{sc - 0x67}"
    return f"scancode 0x{sc:02X}"


def slot_items(entry: bytes, off: int, n: int, item_names, missing) -> list[str]:
    out = []
    for i in range(n):
        b = entry[off + i]
        if b == 0xFF:
            continue
        name = item_names.get(b)
        if name is None:
            missing.add(b)
            out.append(f"item 0x{b:02X}（未命名）")
        else:
            out.append(f"{name} (0x{b:02X})")
    return out


def fmt_shop(n: int, intro: bytes, cat: bytes, item_names, missing) -> str:
    lines = ["## 商店", ""]
    chapter_id = n - 1
    is_battle = cat[chapter_id] != 0 if chapter_id < len(cat) else False
    if is_battle:
        lines.append("無章內商店（battle 章，不走 intro 商店選單）。")
        return "\n".join(lines)
    if n > N_ENTRIES:
        # story chapter beyond the 26-entry intro table (only ch27)
        lines.append("本章無獨立 intro_metadata entry（表僅 26 筆，對應第 1..26 章）；"
                     "商店機制屬特例，見 §特殊機制。")
        return "\n".join(lines)
    base = chapter_id * ENTRY_STRIDE
    entry = intro[base:base + ENTRY_STRIDE]
    if all(b == 0 for b in entry):
        # all-zero entry = empty placeholder (no 0xFF terminators); the slots are
        # NOT a shop selling item 0x00 x12. Only ch22 among story chapters.
        lines.append("本章 intro_metadata entry 為空，無商店品項。")
        return "\n".join(lines)
    weapons = slot_items(entry, WEAPONS_OFF, WEAPONS_N, item_names, missing)
    items = slot_items(entry, ITEMS_OFF, ITEMS_N, item_names, missing)
    mystery = slot_items(entry, MYSTERY_OFF, MYSTERY_N, item_names, missing)
    if not (weapons or items or mystery):
        lines.append("本章 intro entry 為空，無商店品項。")
        return "\n".join(lines)
    hotkey = scancode_label(entry[SCANCODE_OFF])
    lines.append(f"story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（{hotkey}）"
                 f"開啟的神秘商店。品項（chapter_intro_metadata entry {chapter_id}）：")
    lines.append("")
    if weapons:
        lines.append(f"- **武器店**：{'、'.join(weapons)}")
    if items:
        lines.append(f"- **道具店**：{'、'.join(items)}")
    if mystery:
        lines.append(f"- **神秘商店（{hotkey}）**：{'、'.join(mystery)}")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chapter", type=int)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()

    intro = load_bytes(INTRO_HEX)
    cat = load_bytes(CAT_HEX)
    # sentinel self-checks against known Ghidra ground truth
    assert list(intro[0:3]) == [0x00, 0x00, 0x54], "intro entry0 sentinel mismatch"
    assert len(intro) == N_ENTRIES * ENTRY_STRIDE, f"intro size {len(intro)}"
    assert list(cat[22:30]) == [1, 1, 1, 0, 0, 1, 1, 1], "category tail sentinel mismatch"
    item_names = s3.load_item_names()

    chapters = ([args.chapter] if args.chapter else list(range(1, 31)))
    missing: set[int] = set()
    if args.write:
        OUT_DIR.mkdir(parents=True, exist_ok=True)
    for n in chapters:
        block = fmt_shop(n, intro, cat, item_names, missing)
        if args.write:
            (OUT_DIR / f"ch{n:02d}.md").write_text(block + "\n", encoding="utf-8")
        elif not args.check:
            print(f"===== chapter {n} =====")
            print(block)
            print()
    if args.check:
        story = [n for n in range(1, 31) if (n - 1 < len(cat) and cat[n - 1] == 0)]
        battle = [n for n in range(1, 31) if (n - 1 < len(cat) and cat[n - 1] != 0)]
        sys.stderr.write(f"# story chapters: {story}\n# battle chapters: {battle}\n")
    if missing:
        sys.stderr.write(f"# UNMAPPED item id: {sorted(hex(i) for i in missing)}\n")
    else:
        sys.stderr.write("# item coverage OK\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
