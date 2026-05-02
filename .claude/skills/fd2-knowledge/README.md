# fd2-knowledge (maintainer notes)

Knowledge-base skill for the FD2 reverse-engineering project. The user-
facing entry point is `SKILL.md`; this file is for whoever maintains the
skill itself.

## Layout

```
fd2-knowledge/
├── SKILL.md              # Trigger + cheat sheet for Claude
├── README.md             # This file
├── query.py              # Query CLI (Python 3.12 stdlib only)
├── build_index.py        # HTML → data/*.md + index/*.json
├── data/                 # Markdown (human + Claude readable)
│   ├── offsets.md        # ★ All memory offsets (EXE ↔ LE)
│   ├── items.md          # 200+ items with raw stats
│   ├── spells.md         # 35 spells, 4 categories
│   ├── characters_base.md, characters_growth.md, characters_ranks.md
│   ├── spell_learning.md, spell_by_character.md
│   ├── jobs.md, portraits.md, enemies.md
│   ├── memory_layout.md  # 80-byte runtime char struct
│   ├── formulas.md, terrain.md, intro.md
│   ├── code_hacks.md, file_formats.md
│   └── chapters/
│       ├── _index.md
│       └── chapter_01.md … chapter_30.md
└── index/                # JSON indexes consumed by query.py
    ├── offsets.json, items.json, spells.json, characters.json
    ├── jobs.json, portraits.json, enemies.json
    ├── chapters.json, formulas.json
```

## Rebuilding from source

Source HTML lives in `Strategy_Guide/*.htm` (UTF-8 with BOM). To
regenerate everything:

```
cd .claude/skills/fd2-knowledge
python build_index.py --verify
```

The `--verify` flag runs self-checks (item count, chapter count, required
offset slugs, encoding). On success prints a summary; on failure lists
missing / broken entries.

## Design decisions

1. **Stdlib only.** No `pip install` needed — Python 3.12's `html.parser`,
   `json`, `argparse`, `re`, `pathlib` cover everything. The HTML is
   simple `<pre>`-based content, not structured enough to warrant BeautifulSoup.
2. **Raw tables preserved.** `data/*.md` wraps the guide's `<pre>` content
   in markdown code fences. The original Chinese wide-character alignment
   is preserved (open these files in a monospace font to read comfortably).
   This is intentional: byte-layout tables lose meaning if reformatted.
3. **JSON for lookup, md for reading.** `index/*.json` has flat structured
   data (e.g. `items.json[0Bh].AP == 400`) so `query.py` can answer point
   queries cheaply. `data/*.md` is the ground truth that Claude reads when
   it needs surrounding context.
4. **30 chapters split per file.** `data/chapters/chapter_NN.md` is ~2 KB
   each. `python query.py chapter 17` returns only that chapter — not all
   60 KB of `walkthrough.htm`.
5. **Version caveat built in.** Every `offsets.json` entry has
   `exe_addr_sample` / `le_addr_sample` (not just `addr`), `version_note`,
   and `search_bytes`. `SKILL.md` explains to Claude how to reconcile a
   Ghidra-observed address against the guide's recorded address when they
   differ (which they will in any non-matching version).

## Adding a new source file

If someone drops a new HTM into `Strategy_Guide/`:

1. Add a `process_<newfile>()` function in `build_index.py` following the
   same pattern (read via `read_html_pre()` or `read_full_html()`, emit
   `data/<name>.md` via `write_md()`, optionally emit `index/<name>.json`
   via `write_json()`).
2. Call it from `main()`.
3. If it introduces a new kind of query, add a subcommand in `query.py`.
4. Update `SKILL.md` cheat sheet.
5. Add a verification check in `verify()`.

## Testing

```
python build_index.py --verify
python query.py offset 540AC
python query.py item 炎龍劍
python query.py spell 02
python query.py char 索爾
python query.py chapter 1
python query.py struct runtime_char
python query.py grep 炎龍術
```

These should all produce reasonable output. When making changes, run
`--verify` first; if it passes, spot-check a few subcommands.

## Character encoding note

- Source HTMLs are UTF-8 with BOM; read them with `encoding="utf-8-sig"`.
- Emitted `.md` and `.json` are UTF-8 without BOM (enforced by the
  `--verify` check).
- `query.py` forces `sys.stdout` to UTF-8 so Chinese renders correctly on
  Windows `cmd`. If you run it in PowerShell and see garbled output,
  `chcp 65001` first or set `PYTHONIOENCODING=utf-8`.
