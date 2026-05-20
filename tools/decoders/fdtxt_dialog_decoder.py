"""FDTXT.DAT dialog dump generator — renders every page's bytecode as a
markdown document with inline control-code annotations.

Notation produced:
    [OPCODE]            zero-arg control (END / PAGE_BREAK / PARAGRAPH / ...)
    [OPCODE=0xNNNN]     one-arg control (PORTRAIT_*=arg)
    {ascii char}        printable ASCII glyph (0x20..0x7E rendered as-is)
    <NNNN>              non-ASCII glyph (rendered as half-width angle bracket + hex id)

Optional readable mode (--readable) substitutes glyph IDs with the actual
Chinese characters using a JSON lookup file you supply via --glyph-table.
The expected JSON shape is {"lookup": {"0x0123": "字", ...}}.

CLI:
    python tools/decoders/fdtxt_dialog_decoder.py --idx 1
    python tools/decoders/fdtxt_dialog_decoder.py --stdout 0
    python tools/decoders/fdtxt_dialog_decoder.py --idx 1 --out chapter_01.md
    python tools/decoders/fdtxt_dialog_decoder.py --idx 1 --readable \
        --glyph-table /path/to/glyph_lookup.json
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# tools/decoders/fdtxt_dialog_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT_DIR = REPO_ROOT / "workspace" / "decoders" / "fdtxt_dialog"

# Make sibling fdtxt_parser importable
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fdtxt_parser import FdtxtArchive, Token, tokenize  # noqa: E402


READABLE_MODE = False
_glyph_lookup: dict[str, str] = {}

LINE_BREAK_OPCODES = {
    "PAGE_BREAK",
    "PARAGRAPH",
    "SUB_DIALOG_A",
    "SUB_DIALOG_B",
    "PORTRAIT_LEFT_BY_ID",
    "PORTRAIT_RIGHT_BY_ID",
    "PORTRAIT_LEFT_BY_CHAR",
    "PORTRAIT_RIGHT_BY_CHAR",
}


def load_glyph_lookup(path: Path) -> dict[str, str]:
    if not path.exists():
        raise FileNotFoundError(f"glyph table not found: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    return data.get("lookup", data)


def render_token(tok: Token) -> str:
    """Render a single token. Glyph ids 0x20..0x7E NOT treated as ASCII because
    FD2's font_sheet has Chinese glyphs at low fids too (e.g. 0x007D = 大 bitmap,
    not '}'). The only correct way to substitute Chinese is the glyph lookup."""
    if tok.name == "GLYPH":
        v = tok.value
        if READABLE_MODE:
            ch = _glyph_lookup.get(f"0x{v:04X}", "")
            if ch:
                return ch
            return f"<{v:04X}>"
        return f"<{v:04X}>"
    if not tok.args:
        return f"[{tok.name}]"
    arg_str = ",".join(f"0x{a:04X}" for a in tok.args)
    return f"[{tok.name}={arg_str}]"


def render_page(page_bytes: bytes) -> str:
    lines: list[str] = []
    current: list[str] = []

    def flush():
        if current:
            lines.append("".join(current))
            current.clear()

    for tok in tokenize(page_bytes):
        rendered = render_token(tok)
        if tok.name in LINE_BREAK_OPCODES:
            flush()
            lines.append(rendered)
        elif tok.name == "END":
            flush()
            lines.append(rendered)
            break
        else:
            current.append(rendered)
    flush()
    return "\n".join(lines)


def render_entry_markdown(archive: FdtxtArchive, idx: int) -> str:
    meta = archive.entry_meta(idx)
    n_pages = archive.page_count(idx)
    out: list[str] = []
    out.append(f"# FDTXT entry {idx} - {meta.purpose}")
    out.append("")
    out.append("## Entry metadata")
    out.append("")
    out.append(f"- idx: {idx}")
    out.append(f"- purpose: {meta.purpose}")
    out.append(f"- range: [0x{meta.start:x}, 0x{meta.end:x}) size {meta.size} bytes")
    out.append(f"- page_count: {n_pages}")
    out.append("")
    out.append("## Notation")
    out.append("")
    out.append("- `[OPCODE]` / `[OPCODE=0xNNNN]` - dialog VM control codes")
    out.append("- `{ascii char}` - printable ASCII glyph (atlas indices 0x20-0x7E)")
    if READABLE_MODE:
        out.append("- Chinese characters - readable mode: substituted from glyph lookup table")
        out.append("- `<NNNN>` - fallback when glyph_id has no lookup entry")
    else:
        out.append("- `<NNNN>` - non-ASCII glyph index into chinese_font_sheet (FDOTHER.DAT[4])")
    out.append("")
    out.append("## Pages")
    out.append("")
    page_offsets = archive.page_offsets(idx)
    for p in range(n_pages):
        start = page_offsets[p]
        end = page_offsets[p + 1] if p + 1 < n_pages else meta.size
        out.append(f"### Page {p}  (offset 0x{start:x}, span {end - start} bytes)")
        out.append("")
        out.append("```text")
        try:
            page_bytes = archive.page_bytes(idx, p)
            out.append(render_page(page_bytes))
        except Exception as e:
            out.append(f"<decode error: {e}>")
        out.append("```")
        out.append("")
    return "\n".join(out)


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="FDTXT.DAT dialog dump generator")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--idx", type=int, metavar="IDX",
                   help="render entry IDX to stdout or --out")
    g.add_argument("--stdout", type=int, metavar="IDX",
                   help="alias for --idx (printed to stdout)")
    g.add_argument("--list", action="store_true",
                   help="list all 34 entries with their page counts")
    p.add_argument("--out", type=Path,
                   help=f"write rendered markdown to file (default: stdout). "
                   f"When writing files, prefer workspace/decoders/fdtxt_dialog/")
    p.add_argument("--readable", action="store_true",
                   help="substitute glyph ids with Chinese chars from --glyph-table")
    p.add_argument("--glyph-table", type=Path,
                   help="JSON file with {'lookup': {'0xNNNN': 'char', ...}}")
    return p


def main(argv=None):
    global READABLE_MODE, _glyph_lookup
    args = _build_parser().parse_args(argv)
    if args.readable:
        if not args.glyph_table:
            sys.stderr.write(
                "--readable requires --glyph-table /path/to/lookup.json\n"
            )
            return 2
        _glyph_lookup = load_glyph_lookup(args.glyph_table)
        READABLE_MODE = True
        print(f"[readable mode] loaded {len(_glyph_lookup)} glyph entries",
              file=sys.stderr)

    archive = FdtxtArchive.from_default()

    if args.list:
        print(f"{'idx':>3}  {'pages':>5}  purpose")
        for m in archive.all_metas():
            try:
                n = archive.page_count(m.idx)
            except Exception:
                n = -1
            print(f"{m.idx:>3}  {n:>5}  {m.purpose}")
        return 0

    idx = args.idx if args.idx is not None else args.stdout
    md = render_entry_markdown(archive, idx)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(md + "\n", encoding="utf-8")
        print(f"wrote {args.out}", file=sys.stderr)
    else:
        print(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
