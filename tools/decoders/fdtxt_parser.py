"""FDTXT.DAT byte-level parser — extracts dialog entries, page offsets, and
tokenises the dialog VM bytecode for FD2's text archive.

FDTXT.DAT layout:

    bytes 0..5    : "LLLLLL" magic signature (6 * 0x4C)
    bytes 6..N-1  : 35 little-endian u32 offsets — entries[0..33] start +
                    sentinel at index 34 (= file size)
    bytes N..EOF  : 34 entries' raw payloads, contiguous

Entry idx -> purpose:
    idx 0       : all_game_text         (global dialog library)
    idx 1..30   : current_chapter_text  (chapter_id+1)
    idx 31..33  : endgame extras        (chapter_id 30/31/32)

Per-entry layout:
    bytes [0 .. 2*N-1]  : N x u16 little-endian page offsets (entry-relative).
                          N = page_offsets[0] / 2.
    bytes [2*N .. ]     : page bytecode payloads, contiguous.

Page bytecode opcodes (u16 little-endian):
    0xFFFF  END                          terminate page
    0xFFFE  PAGE_BREAK                   wait-for-key
    0xFFFD  PARAGRAPH                    paragraph break + cinematic scroll
    0xFFFC  SUB_DIALOG_A                 recurse using last_action_sprite_id
    0xFFFB  SUB_DIALOG_B                 recurse using drop_dialog_swap_text_id
    0xFFFA  NUMBER                       runtime numeric substitution
    0xFFEF  PORTRAIT_LEFT_BY_ID  arg     show portrait_id arg, left slot
    0xFFEE  PORTRAIT_RIGHT_BY_ID arg     show portrait_id arg, right slot
    0xFFED  PORTRAIT_LEFT_BY_CHAR arg    show runtime_char[arg].bPortrait_id, left
    0xFFEC  PORTRAIT_RIGHT_BY_CHAR arg   same, right slot
    other (0x0000..0xFFEB)               TEXT glyph index into chinese_font_sheet

CLI:
    python tools/decoders/fdtxt_parser.py --info               # entry-table report
    python tools/decoders/fdtxt_parser.py --header             # dump file header bytes
    python tools/decoders/fdtxt_parser.py --dump-entry IDX     # hexdump entry IDX
    python tools/decoders/fdtxt_parser.py --pages IDX          # list page offsets
    python tools/decoders/fdtxt_parser.py --tokenize IDX P     # tokenize page P of entry IDX
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# tools/decoders/fdtxt_parser.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[2]
FDTXT_PATH = REPO_ROOT / "fd2_game_files" / "FDTXT.DAT"

MAGIC = b"LLLLLL"
MAGIC_LEN = 6
OFFSET_TABLE_START = 6
ENTRY_COUNT = 34
OFFSET_COUNT = ENTRY_COUNT + 1
HEADER_LEN = OFFSET_TABLE_START + OFFSET_COUNT * 4  # 146 = 0x92


@dataclass(frozen=True)
class EntryMeta:
    idx: int
    start: int
    end: int
    size: int
    purpose: str


def purpose_for(idx: int) -> str:
    if idx == 0:
        return "all_game_text (global)"
    if 1 <= idx <= 30:
        return f"chapter {idx} dialog (chapter_id={idx - 1})"
    if 31 <= idx <= 33:
        return f"endgame extras (chapter_id={idx - 1})"
    return "<out of range>"


class FdtxtArchive:
    """Parsed view of FDTXT.DAT."""

    def __init__(self, raw: bytes):
        if len(raw) < HEADER_LEN:
            raise ValueError(f"FDTXT too small: {len(raw)} < header {HEADER_LEN}")
        if raw[:MAGIC_LEN] != MAGIC:
            raise ValueError(
                f"FDTXT magic mismatch: got {raw[:MAGIC_LEN]!r}, expected {MAGIC!r}"
            )
        offsets = struct.unpack_from(f"<{OFFSET_COUNT}I", raw, OFFSET_TABLE_START)
        if offsets[-1] != len(raw):
            raise ValueError(
                f"FDTXT sentinel offset {offsets[-1]:#x} != file size {len(raw):#x}"
            )
        if offsets[0] != HEADER_LEN:
            raise ValueError(
                f"FDTXT first entry offset {offsets[0]:#x} != header end {HEADER_LEN:#x}"
            )
        for i in range(ENTRY_COUNT):
            if offsets[i] >= offsets[i + 1]:
                raise ValueError(
                    f"FDTXT non-monotone offsets at idx {i}: "
                    f"{offsets[i]:#x} >= {offsets[i + 1]:#x}"
                )
        self._raw = raw
        self._offsets = offsets

    @classmethod
    def from_default(cls) -> "FdtxtArchive":
        return cls.from_path(FDTXT_PATH)

    @classmethod
    def from_path(cls, path: Path) -> "FdtxtArchive":
        with open(path, "rb") as f:
            return cls(f.read())

    @property
    def file_size(self) -> int:
        return len(self._raw)

    @property
    def entry_count(self) -> int:
        return ENTRY_COUNT

    def entry_meta(self, idx: int) -> EntryMeta:
        if not 0 <= idx < ENTRY_COUNT:
            raise IndexError(f"entry idx {idx} out of range [0, {ENTRY_COUNT})")
        start = self._offsets[idx]
        end = self._offsets[idx + 1]
        return EntryMeta(idx=idx, start=start, end=end,
                         size=end - start, purpose=purpose_for(idx))

    def entry_bytes(self, idx: int) -> bytes:
        m = self.entry_meta(idx)
        return self._raw[m.start:m.end]

    def all_metas(self) -> list[EntryMeta]:
        return [self.entry_meta(i) for i in range(ENTRY_COUNT)]

    def page_offsets(self, idx: int) -> tuple[int, ...]:
        raw = self.entry_bytes(idx)
        if len(raw) < 2:
            return tuple()
        first = struct.unpack_from("<H", raw, 0)[0]
        if first % 2 != 0 or first > len(raw):
            raise ValueError(
                f"entry {idx}: bogus first page offset {first:#x} "
                f"(entry size {len(raw)})"
            )
        n = first // 2
        return struct.unpack_from(f"<{n}H", raw, 0)

    def page_count(self, idx: int) -> int:
        return len(self.page_offsets(idx))

    def page_bytes(self, idx: int, page: int) -> bytes:
        """Return bytes of one page, including the trailing 0xFFFF END opcode."""
        raw = self.entry_bytes(idx)
        offsets = self.page_offsets(idx)
        if not 0 <= page < len(offsets):
            raise IndexError(
                f"entry {idx} page {page} out of range [0, {len(offsets)})"
            )
        start = offsets[page]
        i = start
        while i + 1 < len(raw):
            op = struct.unpack_from("<H", raw, i)[0]
            i += 2
            if op == 0xFFFF:
                return raw[start:i]
        raise ValueError(
            f"entry {idx} page {page}: END opcode (0xFFFF) not found before EOF"
        )

    def tokenize_page(self, idx: int, page: int):
        return tokenize(self.page_bytes(idx, page))


OPCODES = {
    0xFFFF: ("END", 0),
    0xFFFE: ("PAGE_BREAK", 0),
    0xFFFD: ("PARAGRAPH", 0),
    0xFFFC: ("SUB_DIALOG_A", 0),
    0xFFFB: ("SUB_DIALOG_B", 0),
    0xFFFA: ("NUMBER", 0),
    0xFFEF: ("PORTRAIT_LEFT_BY_ID", 1),
    0xFFEE: ("PORTRAIT_RIGHT_BY_ID", 1),
    0xFFED: ("PORTRAIT_LEFT_BY_CHAR", 1),
    0xFFEC: ("PORTRAIT_RIGHT_BY_CHAR", 1),
}


@dataclass(frozen=True)
class Token:
    name: str
    value: int
    args: tuple
    offset: int

    def render_inline(self) -> str:
        if self.name == "GLYPH":
            v = self.value
            if 0x20 <= v <= 0x7E:
                return chr(v)
            return f"[GLYPH=0x{v:04X}]"
        if not self.args:
            return f"[{self.name}]"
        arg_str = ",".join(f"0x{a:04X}" for a in self.args)
        return f"[{self.name}={arg_str}]"


def tokenize(page_bytes: bytes):
    """Yield Tokens for a page bytecode stream. Stops after END (0xFFFF)."""
    i = 0
    n = len(page_bytes)
    while i + 1 < n:
        op_offset = i
        op = struct.unpack_from("<H", page_bytes, i)[0]
        i += 2
        if op in OPCODES:
            name, argc = OPCODES[op]
            if i + argc * 2 > n:
                raise ValueError(
                    f"truncated args for {name} at offset {op_offset:#x}"
                )
            args = struct.unpack_from(f"<{argc}H", page_bytes, i) if argc else ()
            i += argc * 2
            yield Token(name=name, value=op, args=args, offset=op_offset)
            if op == 0xFFFF:
                return
        else:
            yield Token(name="GLYPH", value=op, args=(), offset=op_offset)
    raise ValueError("page bytecode ended without END (0xFFFF) opcode")


def _hexdump(data: bytes, base: int = 0, max_bytes: Optional[int] = None) -> str:
    if max_bytes is not None:
        data = data[:max_bytes]
    lines = []
    for off in range(0, len(data), 16):
        chunk = data[off:off + 16]
        hex_part = " ".join(f"{b:02x}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{base + off:08x}: {hex_part:<48}  {ascii_part}")
    return "\n".join(lines)


def _cmd_info(_args):
    fdtxt = FdtxtArchive.from_default()
    print(f"FDTXT.DAT: {FDTXT_PATH}")
    print(f"Size     : {fdtxt.file_size} bytes (0x{fdtxt.file_size:x})")
    print(f"Magic    : {MAGIC.decode('ascii')}")
    print(f"Header   : {HEADER_LEN} bytes (0x{HEADER_LEN:x})")
    print(f"Entries  : {fdtxt.entry_count}")
    print()
    print(f"{'idx':>3}  {'start':>8}  {'end':>8}  {'size':>7}  purpose")
    print("-" * 78)
    for m in fdtxt.all_metas():
        print(f"{m.idx:>3}  {m.start:>#8x}  {m.end:>#8x}  {m.size:>7}  {m.purpose}")
    total = sum(m.size for m in fdtxt.all_metas())
    print()
    print(f"Total payload bytes : {total}")
    print(f"Header              : {HEADER_LEN}")
    print(f"Total + header      : {total + HEADER_LEN}")
    print(f"File size           : {fdtxt.file_size}")
    if total + HEADER_LEN == fdtxt.file_size:
        print("Sanity: PASS - payload + header == file size")
    else:
        print(f"Sanity: FAIL - diff {fdtxt.file_size - (total + HEADER_LEN)}")


def _cmd_dump_entry(args):
    fdtxt = FdtxtArchive.from_default()
    idx = args.dump_entry
    m = fdtxt.entry_meta(idx)
    raw = fdtxt.entry_bytes(idx)
    print(f"Entry {idx} - {m.purpose}")
    print(f"Range : [{m.start:#x}, {m.end:#x}) size {m.size}")
    print()
    limit = args.bytes if args.bytes else 256
    print(_hexdump(raw, base=0, max_bytes=limit))
    if limit < len(raw):
        print(f"... ({len(raw) - limit} more bytes)")


def _cmd_header(_args):
    raw = FDTXT_PATH.read_bytes()[:HEADER_LEN]
    print(_hexdump(raw, base=0))


def _cmd_pages(args):
    fdtxt = FdtxtArchive.from_default()
    idx = args.pages
    m = fdtxt.entry_meta(idx)
    offsets = fdtxt.page_offsets(idx)
    n = len(offsets)
    print(f"Entry {idx} - {m.purpose}")
    print(f"Size      : {m.size} bytes")
    print(f"Page count: {n}")
    print()
    print(f"{'page':>4}  {'offset':>8}  {'next':>8}  {'span':>5}")
    print("-" * 42)
    for p in range(n):
        start = offsets[p]
        end = offsets[p + 1] if p + 1 < n else m.size
        print(f"{p:>4}  {start:>#8x}  {end:>#8x}  {end - start:>5}")


def _cmd_tokenize(args):
    fdtxt = FdtxtArchive.from_default()
    idx, page = args.tokenize
    m = fdtxt.entry_meta(idx)
    print(f"Entry {idx} page {page} - {m.purpose}")
    raw = fdtxt.page_bytes(idx, page)
    print(f"Page bytes: {len(raw)} (incl END opcode)")
    print()
    counts: dict[str, int] = {}
    for tok in tokenize(raw):
        counts[tok.name] = counts.get(tok.name, 0) + 1
        print(f"  {tok.offset:>#6x}  {tok.value:#06x}  {tok.render_inline()}")
    print()
    print("Opcode counts:")
    for name, count in sorted(counts.items()):
        print(f"  {name:24s} {count:>5}")


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="FDTXT.DAT parser")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--info", action="store_true",
                   help="entry-table sanity report")
    g.add_argument("--dump-entry", type=int, metavar="IDX",
                   help="hexdump entry IDX")
    g.add_argument("--header", action="store_true",
                   help="dump file header bytes")
    g.add_argument("--pages", type=int, metavar="IDX",
                   help="list page offsets in entry IDX")
    g.add_argument("--tokenize", type=int, nargs=2, metavar=("IDX", "P"),
                   help="tokenize page P of entry IDX")
    p.add_argument("--bytes", type=int, default=0,
                   help="limit dump to N bytes (0 = 256)")
    return p


def main(argv=None):
    args = _build_parser().parse_args(argv)
    if args.info:
        _cmd_info(args)
    elif args.dump_entry is not None:
        _cmd_dump_entry(args)
    elif args.header:
        _cmd_header(args)
    elif args.pages is not None:
        _cmd_pages(args)
    elif args.tokenize is not None:
        _cmd_tokenize(args)


if __name__ == "__main__":
    main()
