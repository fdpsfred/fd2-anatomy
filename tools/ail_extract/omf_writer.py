"""OMF 32-bit (Easy OMF-386, USE32) writer.

A self-contained encoder for emitting Watcom-compatible OMF binary modules
(.obj). Covers the record types needed by AIL extraction:

    0x80 THEADR     module name header
    0x88 COMENT     comment (Watcom CC tag goes here)
    0x8B MODEND32   module end (32-bit, no start address)
    0x8C EXTDEF     external symbol table
    0x91 PUBDEF32   32-bit public symbol table
    0x96 LNAMES     name table (segment/class/group names)
    0x99 SEGDEF32   32-bit segment definition
    0x9A GRPDEF     group definition (e.g. DGROUP)
    0x9D FIXUPP32   32-bit fixup records (paired with the most recent LEDATA32)
    0xA1 LEDATA32   32-bit logical enumerated data (segment bytes)

Format references:
  - Microsoft OMF v1.1 spec
  - Watcom Easy OMF-386 extension (32-bit length / offset fields with the
    32-bit record-type pair 0x99 / 0x91 / 0xA1 / 0xA3 / 0x9D / 0x8B)

All record bodies include the final 1-byte checksum: sum of all record
bytes (type + length + body + checksum) ≡ 0 (mod 256).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

REC_THEADR    = 0x80
REC_COMENT    = 0x88
REC_MODEND32  = 0x8B
REC_EXTDEF    = 0x8C
REC_PUBDEF32  = 0x91
REC_LNAMES    = 0x96
REC_SEGDEF32  = 0x99
REC_GRPDEF    = 0x9A
REC_FIXUPP32  = 0x9D
REC_LEDATA32  = 0xA1


def encode_record(rec_type: int, payload: bytes) -> bytes:
    """Wrap payload bytes in an OMF record (type + length + payload + checksum).
    length = len(payload) + 1 (for checksum byte)."""
    length = len(payload) + 1
    if length > 0xFFFF:
        raise ValueError(f"OMF record too large: {length} bytes")
    header = bytes([rec_type, length & 0xFF, (length >> 8) & 0xFF])
    body = header + payload
    cs = (-sum(body)) & 0xFF
    return body + bytes([cs])


def enc_index(n: int) -> bytes:
    """OMF index encoding: 1-byte if < 0x80, else 2-byte big-endian-ish with
    MSB set on first byte."""
    if n < 0 or n > 0x7FFF:
        raise ValueError(f"OMF index out of range: {n}")
    if n < 0x80:
        return bytes([n])
    return bytes([(n >> 8) | 0x80, n & 0xFF])


def enc_name(s: str) -> bytes:
    """Length-prefixed ASCII name (max 255 chars)."""
    b = s.encode("ascii")
    if len(b) > 255:
        raise ValueError(f"OMF name too long: {len(b)} > 255")
    return bytes([len(b)]) + b


def enc_u32(v: int) -> bytes:
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def enc_u16(v: int) -> bytes:
    return (v & 0xFFFF).to_bytes(2, "little")


# -- SEGDEF32 ACBP encoding ---------------------------------------------------
# bits: A(3) C(3) B(1) P(1)
#  A=5 (dword align, 4-byte)
#  C=2 (public combine)
#  B=0 (size <= 4GB, no overflow)
#  P=1 (USE32 segment)
def acbp(align_dword: bool = True, combine_public: bool = True,
         use32: bool = True) -> int:
    a = 5 if align_dword else 1  # 5=dword, 1=byte
    c = 2 if combine_public else 0
    b = 0
    p = 1 if use32 else 0
    return (a << 5) | (c << 2) | (b << 1) | p


# -- FIXUPP32 subrecord builder ----------------------------------------------
def fixupp_subrecord(
    *,
    self_relative: bool,
    location: int,        # 0..15 (LOC field; 9 = offset32)
    data_record_offset: int,  # 0..0x3FF (10-bit field)
    frame_method: int,    # 0..5 (F=0 forms only; F0=SEG, F1=GRP, F2=EXT, F4=target, F5=target_no_disp)
    target_method: int,   # 0..3 if P=0 (with disp); 4..7 if P=1 (no disp)
    target_index: int,
    target_displacement: Optional[int] = None,
    frame_index: Optional[int] = None,
) -> bytes:
    """Build a FIXUPP32 subrecord.

    LOCAT is 2 bytes big-endian-ish:
      bit 15:    1 (this is a FIXUP subrec, not a THREAD subrec)
      bit 14:    M (1 = segment-relative, 0 = self-relative)
      bits 13-10: LOC (4-bit location type)
      bits 9-0:  data record offset (10-bit)
    FIX DATA byte:
      bit 7: F (1=thread, 0=specified)
      bits 6-4: Frame method (or thread number if F=1)
      bit 3: T (1=thread, 0=specified)
      bits 2: P (1 if target displacement omitted)
      bits 1-0: Target method (0..3 if P=0, 4..7 if P=1) — but the field
                is just bits 2..0 here; spec encodes Target field as
                bits 2..0 with P being bit 2, low 2 bits being method.
    """
    if not (0 <= data_record_offset <= 0x3FF):
        raise ValueError(f"FIXUPP32 data_record_offset out of 10-bit range: 0x{data_record_offset:x}")
    if not (0 <= location <= 15):
        raise ValueError(f"FIXUPP32 LOC out of range: {location}")

    M = 0 if self_relative else 1
    locat_hi = 0x80 | (M << 6) | ((location & 0x0F) << 2) | ((data_record_offset >> 8) & 0x3)
    locat_lo = data_record_offset & 0xFF
    locat = bytes([locat_hi, locat_lo])

    # FIX DATA byte: F=0, T=0 (we never use threads)
    # bits 6-4 = frame_method (0..5)
    # bits 2-0 = target field: P(1) + method(2)
    if frame_method < 0 or frame_method > 7:
        raise ValueError(f"FIXUPP32 frame_method out of range: {frame_method}")
    if target_method < 0 or target_method > 7:
        raise ValueError(f"FIXUPP32 target_method out of range: {target_method}")
    fix_data = (frame_method << 4) | (target_method & 0x07)

    out = bytearray(locat)
    out.append(fix_data)

    # Frame Datum (only if frame method requires it: 0=SEG, 1=GRP, 2=EXT need index;
    # 4=target frame, 5=target_no_disp don't need a frame datum)
    if frame_method in (0, 1, 2):
        if frame_index is None:
            raise ValueError(f"FIXUPP32 frame_method {frame_method} needs frame_index")
        out.extend(enc_index(frame_index))

    # Target Datum (always present)
    out.extend(enc_index(target_index))

    # Target Displacement (only if target_method < 4 / P=0)
    if target_method < 4:
        if target_displacement is None:
            raise ValueError(f"FIXUPP32 target_method {target_method} needs target_displacement")
        out.extend(enc_u32(target_displacement))
    return bytes(out)


# -----------------------------------------------------------------------------
@dataclass
class Segment:
    name_idx: int
    class_idx: int
    overlay_idx: int = 1   # reserved name "" at LNAMES[1]
    length: int = 0
    acbp_byte: int = field(default_factory=acbp)


@dataclass
class PubDef:
    seg_idx: int   # 1-based segment index (relative to this module's SEGDEFs)
    name: str
    offset: int    # 32-bit offset within the segment


@dataclass
class LeData:
    seg_idx: int
    offset: int
    data: bytes
    fixupp_subs: list[bytes] = field(default_factory=list)


@dataclass
class Group:
    name_idx: int  # LNAMES idx of group name (e.g. DGROUP)
    seg_idxs: list[int] = field(default_factory=list)


class OmfModule:
    """Builder for a single OMF .obj. Append items via add_* helpers,
    call build() to get the final bytes."""

    def __init__(self, module_name: str, comment_tag: Optional[str] = None):
        self.module_name = module_name
        self.comment_tag = comment_tag
        # LNAMES is 1-indexed; LNAMES[1] is conventionally the empty string ""
        # (used as overlay name for segments and class name for unnamed).
        self.lnames: list[str] = [""]
        self.segments: list[Segment] = []
        self.groups: list[Group] = []
        self.extdefs: list[str] = []         # 1-based EXTDEF index
        self.pubdefs: list[PubDef] = []
        self.ledatas: list[LeData] = []

    def add_name(self, name: str) -> int:
        """Add to LNAMES (or reuse), return 1-based index."""
        if name in self.lnames:
            return self.lnames.index(name)
        self.lnames.append(name)
        return len(self.lnames) - 1

    def add_segment(self, seg_name: str, class_name: str,
                    length: int, dword_align: bool = True,
                    combine_public: bool = True) -> int:
        seg_name_idx = self.add_name(seg_name)
        class_idx = self.add_name(class_name)
        overlay_idx = self.add_name("")  # LNAMES[1] = "" by convention
        s = Segment(
            name_idx=seg_name_idx,
            class_idx=class_idx,
            overlay_idx=overlay_idx,
            length=length,
            acbp_byte=acbp(align_dword=dword_align,
                           combine_public=combine_public, use32=True),
        )
        self.segments.append(s)
        return len(self.segments)

    def add_group(self, group_name: str, seg_idxs: list[int]) -> int:
        name_idx = self.add_name(group_name)
        self.groups.append(Group(name_idx=name_idx, seg_idxs=list(seg_idxs)))
        return len(self.groups)

    def add_extdef(self, name: str) -> int:
        """Add to EXTDEF (or reuse), return 1-based index."""
        if name in self.extdefs:
            return self.extdefs.index(name) + 1
        self.extdefs.append(name)
        return len(self.extdefs)

    def add_pubdef(self, seg_idx: int, name: str, offset: int) -> None:
        self.pubdefs.append(PubDef(seg_idx=seg_idx, name=name, offset=offset))

    def add_ledata(self, seg_idx: int, offset: int, data: bytes,
                   fixupp_subs: Optional[list[bytes]] = None) -> None:
        self.ledatas.append(LeData(seg_idx=seg_idx, offset=offset, data=data,
                                   fixupp_subs=list(fixupp_subs or [])))

    # -- Record builders ------------------------------------------------------
    def _build_theadr(self) -> bytes:
        return encode_record(REC_THEADR, enc_name(self.module_name))

    def _build_coment(self) -> bytes:
        # Watcom-specific COMENT class 0x9F = "WATCOM CC" — tells linker the
        # file came from a Watcom toolchain. Class 0xA1 = "_LIB" entry.
        # For now emit a generic "no list" comment if a tag was specified.
        if not self.comment_tag:
            return b""
        # COMENT format: cmt_type (1B), cmt_class (1B), data...
        payload = bytes([0x00, 0x9F]) + self.comment_tag.encode("ascii")
        return encode_record(REC_COMENT, payload)

    def _build_lnames(self) -> bytes:
        # All names in one LNAMES record (max 1024 bytes per record; if too
        # many, split — but AIL .obj never crosses that).
        payload = b"".join(enc_name(n) for n in self.lnames[1:])  # skip [0] sentinel
        # Actually we need LNAMES indexes to be 1-based; lnames[0] = "" is the
        # sentinel; emit lnames[1..] one by one and the first emitted gets idx 1.
        # But our self.add_name pre-stored "" at idx 0; for downstream record
        # encoding lnames[1] is at OMF idx 1. Make sure we emit lnames[1..n] in
        # order so OMF idx matches list idx.
        return encode_record(REC_LNAMES, payload) if payload else b""

    def _build_segdefs(self) -> bytes:
        out = b""
        for seg in self.segments:
            payload = bytes([seg.acbp_byte]) + enc_u32(seg.length) \
                      + enc_index(seg.name_idx) \
                      + enc_index(seg.class_idx) \
                      + enc_index(seg.overlay_idx)
            out += encode_record(REC_SEGDEF32, payload)
        return out

    def _build_grpdefs(self) -> bytes:
        out = b""
        for grp in self.groups:
            payload = enc_index(grp.name_idx)
            for s in grp.seg_idxs:
                payload += bytes([0xFF]) + enc_index(s)  # 0xFF = "segment idx follows"
            out += encode_record(REC_GRPDEF, payload)
        return out

    def _build_extdef(self) -> bytes:
        if not self.extdefs:
            return b""
        # Chunk into ≤1024-byte records (wlib 9.5a has small per-record
        # buffer; over-large EXTDEF causes 'Unexpected end of file').
        out = b""
        chunk = b""
        for name in self.extdefs:
            entry = enc_name(name) + enc_index(0)
            if len(chunk) + len(entry) > 1000:
                out += encode_record(REC_EXTDEF, chunk)
                chunk = b""
            chunk += entry
        if chunk:
            out += encode_record(REC_EXTDEF, chunk)
        return out

    def _build_pubdef32_for_segment(self, seg_idx: int,
                                    pubs: list[PubDef]) -> bytes:
        """Emit PUBDEF32 records for (group=0, seg_idx) combination.

        Splits into multiple records if the single-record payload would
        exceed ~16KB. wlib 9.5a (v3.0) has a smaller record-buffer limit
        than the OMF spec's 64KB; a single huge PUBDEF32 (with hundreds of
        symbols from ail_data_all.obj) causes wlib to misparse subsequent
        bytes as empty-named symbols and eventually 'Unexpected end of
        file'. Splitting keeps each record well below the limit.
        """
        # wlib 9.5a has small per-record buffer; cap each PUBDEF32 body
        # at ~1000 bytes. Each entry = 1 (name len) + name + 4 (offset) +
        # 1-2 (type) ≈ 40-100 bytes. Chunk by accumulated payload size.
        out = b""
        # base group=0, base seg, type_idx=0 (no type)
        header = enc_index(0) + enc_index(seg_idx)
        payload = header
        for p in pubs:
            entry = enc_name(p.name) + enc_u32(p.offset) + enc_index(0)
            if len(payload) + len(entry) > 1000:
                out += encode_record(REC_PUBDEF32, payload)
                payload = header
            payload += entry
        if len(payload) > len(header):
            out += encode_record(REC_PUBDEF32, payload)
        return out

    def _build_pubdefs(self) -> bytes:
        # Group by seg_idx
        by_seg: dict[int, list[PubDef]] = {}
        for p in self.pubdefs:
            by_seg.setdefault(p.seg_idx, []).append(p)
        out = b""
        for seg_idx in sorted(by_seg.keys()):
            out += self._build_pubdef32_for_segment(seg_idx, by_seg[seg_idx])
        return out

    def _build_ledatas_and_fixupps(self) -> bytes:
        out = b""
        for ld in self.ledatas:
            ledata_payload = enc_index(ld.seg_idx) + enc_u32(ld.offset) + ld.data
            out += encode_record(REC_LEDATA32, ledata_payload)
            if ld.fixupp_subs:
                fxp_payload = b"".join(ld.fixupp_subs)
                out += encode_record(REC_FIXUPP32, fxp_payload)
        return out

    def _build_modend(self) -> bytes:
        # MODEND32: module-type byte (bit 7=main=0, bit 6=start_addr=0)
        return encode_record(REC_MODEND32, bytes([0x00]))

    def build(self) -> bytes:
        out  = self._build_theadr()
        out += self._build_coment()
        out += self._build_lnames()
        out += self._build_segdefs()
        out += self._build_grpdefs()
        out += self._build_extdef()
        out += self._build_pubdefs()
        out += self._build_ledatas_and_fixupps()
        out += self._build_modend()
        return out
