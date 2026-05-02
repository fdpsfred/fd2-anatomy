---
name: fd2-knowledge
description: Knowledge base for the DOS game "Flame Dragon Knights 2" (FD2 / 炎龍騎士團2) reverse engineering project. Invoke this skill whenever analyzing decompiled C++ from Ghidra (FD2.LE / FD2.EXE) and you encounter - (1) memory offsets or data addresses like 0x540AC, 0x792C1, 0x7ADB5 that may correspond to game data tables; (2) fixed-size struct loops where the stride hints at an FD2 entry (23 bytes = item, 7 = spell, 24 = character base, 11 = growth, 10 = enemy, 12 = spell learning, 80/0x50 = runtime character); (3) numeric constants that could be item IDs (00h-D6h), spell IDs (00h-23h), job IDs (00h-1Ah), portrait IDs (00h-41h), or chapter numbers (1-30); (4) damage / hit-rate / experience calculations; (5) FDFIELD.DAT / FDSHAP.DAT file-format parsing; (6) any question about character stats, equipment effects, shop inventory, enemy formations, walkthrough events, or class progression. Primary interface is `python query.py <subcommand>`; fall back to reading `data/*.md` only when a query command doesn't fit.
---

# FD2 Knowledge Base

Fast structured access to Flame Dragon Knights 2 strategy-guide data so you
can cross-reference it against Ghidra-decompiled code in the FLAME2 project.

## When to invoke

Run `python .claude/skills/fd2-knowledge/query.py <subcommand>` whenever
decompiled code shows:

- **Any hex memory address** → try `offset <hex>` first.
- **A loop with stride 23 / 7 / 24 / 11 / 10 / 12 / 80 (0x50)** → struct size
  hint; try `struct <name>`.
- **A hex byte compared against small numbers** (e.g. `if (x == 0x0B)`) →
  likely an item / spell / job / portrait ID.
- **Math that looks like a damage / hit / EXP formula** → `formula`.

Prefer this skill over reading `Strategy_Guide/*.htm` directly — the HTML is
hard to parse and has Chinese wide-character alignment that only renders in a
monospace terminal.

## Subcommands (cheat sheet)

```
python query.py offset <hex>             # Reverse-lookup any address (EXE or LE)
python query.py item   <id|name>         # 0Bh / 0x0B / 11 / 炎龍劍
python query.py item   --list
python query.py spell  <id|name>         # e.g. 02 / 炎龍術
python query.py spell  --list
python query.py char   <name>            # 索爾, 悠妮, ...
python query.py char   --job <name>      # list all chars of a job
python query.py char   --list
python query.py chapter <N>              # 1..30 full chapter
python query.py chapter <N> --enemies    # just enemy formation
python query.py chapter <N> --treasures  # just treasures
python query.py chapter <N> --events     # just events
python query.py job    <id|name>         # 09 / 劍聖
python query.py job    --list
python query.py portrait <id|name>       # 20h = 索爾(劍聖) etc.
python query.py enemy  <name>            # per-level stats row
python query.py struct <name>            # item_entry / runtime_char ...
python query.py struct --list
python query.py formula [name]           # damage/hit/exp formulas
python query.py hack   <N|keyword>       # EXE byte-pattern hack
python query.py grep   <keyword>         # full-text fallback
```

All output is markdown — read it verbatim.

## ID normalization

All `<id>` arguments accept any of these forms:
- `0Bh` (FD2 guide style)
- `0x0B` (C-style)
- `11` (decimal)
- `B` / `0B` (bare hex, 2 digits)

If a bare 2-digit number is ambiguous (e.g. `20` could be decimal 20 or 0x20),
both interpretations are tried — but prefer the `0Xh` or `0xXX` form to be
explicit.

## Worked examples

### Example 1 — offset reverse-lookup
Ghidra shows: `for (i = 0; i < 215; i++) { process(DAT_000540ac + i*23); }`
→ `python query.py offset 540AC`
→ Identifies **item_effect_table** at `FD2.LE 0x540AC` / `FD2.EXE 0x792C1`,
  23 bytes/entry, signature `0B 01 0A 00 5F 00`.

### Example 2 — offset fuzzy match (inside a struct)
Ghidra shows access to `0x540B0`.
→ `python query.py offset 540B0`
→ Reports "nearest: item_effect_table + 4 bytes → field offset 4 within entry"
  (which by the layout is the **HIT** field of item ID 00h).

### Example 3 — ID decoding
Code: `if (item_type == 0x06) { ... }`
→ `python query.py grep "06=杖"` or check `data/items.md` — type 06 is 杖 (staff).

### Example 4 — damage math
Code computes `max_dmg = ap - dp; actual = max_dmg * 9 / 10 + rand();`
→ `python query.py formula 物理攻擊傷害`
→ Returns the canonical formula: 實際傷害 = 最大傷害*0.9 ～ 最大傷害-1.

### Example 5 — runtime struct
Code dereferences `char_ptr + 0x40`.
→ `python query.py struct runtime_char`
→ Returns 80-byte layout showing `+0x40 = MT (力量, affects AP)`.

### Example 6 — chapter cross-reference
Code handles chapter 17 treasure chest "心眼之書".
→ `python query.py chapter 17 --treasures`
→ Returns treasure list including 心眼之書; also see `hack` that changes it to 領悟之書.

## ⚠️ Version caveat — critical

The offsets in the strategy guides were recorded against **one specific
version** of `FD2.EXE`. The game has shipped in multiple variants (繁中 / 簡中
/ 日文, different patch levels). When comparing to Ghidra:

1. **Addresses may drift** between versions — the `FD2.LE` address
   `0x540AC` in the guide may sit at `0x540XX` or even different kilobytes
   away in a different build.
2. **`search_bytes` (signatures) are the reliable cross-version anchor.**
   Every `offsets.json` entry has a `search_bytes` field — use that in
   Ghidra's byte search to find the actual address in *your* binary.
3. **`entry_size` and byte layouts are stable** across versions and serve as
   your primary structure-validation signal.
4. **ID numbers are stable**: item IDs 00h-D6h, spell IDs 00h-23h, job IDs
   00h-1Ah, portrait IDs 00h-41h work across versions.
5. **Names may differ by language**: the guide is 繁中; a 日文 build will
   have Japanese names. Compare by ID, not by name.
6. **When reporting a mismatch**, don't say "the data is wrong." Say:
   > Strategy guide records `0x540AC`; Ghidra shows `0x540XX`, a Δ of Y
   > bytes — likely a version offset. The signature `0B 01 0A 00 5F 00`
   > should locate the actual table start in this binary.

This project's `FD2.LE` was extracted from the user's specific `FD2.EXE` by
stripping DOS/4GW. Addresses there follow a consistent layout per that
build, but may not match the guide's addresses exactly.

## Data conventions

- Hex IDs shown with both `0Xh` (FD2 style) and decimal for clarity.
- Addresses labelled `exe_addr_sample` / `le_addr_sample` to make the
  version dependency explicit.
- Chinese names kept verbatim; do not translate unless asked.
- Raw `<pre>` tables in `data/*.md` are byte-accurate — prefer them over
  prose summaries when any numeric detail matters.

## Rebuilding the index

If `Strategy_Guide/*.htm` is updated, rebuild with:

```
python .claude/skills/fd2-knowledge/build_index.py --verify
```

See `README.md` for details.
