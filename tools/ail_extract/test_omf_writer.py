"""Sanity test for omf_writer.py: emit a tiny no-fixup AIL fn .obj and
parse it back with the OMF record walker from omf_patch_segdef.py.

Pass criteria:
  1. Output bytes can be parsed as a complete sequence of OMF records.
  2. Record-type sequence matches what we emitted:
     THEADR, LNAMES, SEGDEF32, PUBDEF32, LEDATA32, MODEND32
  3. Every record's checksum verifies to zero (mod 256).
  4. LEDATA32 carries our 12-byte fn body verbatim.
  5. PUBDEF32 names our fn at offset 0.
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Import omf_writer from same dir
sys.path.insert(0, str(Path(__file__).parent))
from omf_writer import OmfModule  # noqa: E402

# Import omf_patch_segdef.py's record walker for cross-check parsing
spec = importlib.util.spec_from_file_location(
    "omf_patch_segdef",
    REPO / "tools" / "program_analysis" / "crt_fid_match" / "omf_patch_segdef.py",
)
patch_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch_mod)


def walk_records(data: bytes):
    """Yield (rec_type, rec_payload_without_checksum) tuples.

    rl in the record header is "payload + checksum length". The trailing
    1 byte of the record bytes is the checksum; strip it from the yielded
    payload (caller wants only the meaningful content)."""
    off = 0
    while off + 3 <= len(data):
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        if off + 3 + rl > len(data):
            raise ValueError(f"truncated record at off=0x{off:x}")
        # checksum verify on whole record bytes
        body = data[off : off + 3 + rl]
        if sum(body) & 0xFF != 0:
            raise ValueError(f"checksum mismatch at off=0x{off:x} rec=0x{rt:02x}")
        payload = data[off + 3 : off + 3 + rl - 1]   # drop trailing checksum
        yield rt, payload
        off += 3 + rl
        if rt in (0x8A, 0x8B):  # MODEND / MODEND32
            break
    if off != len(data):
        raise ValueError(f"trailing {len(data) - off} bytes after MODEND")


def main():
    # Build a minimal .obj for AIL_internal_sample_status_inner @ 0x41240 (12B)
    fn_name = "AIL_internal_sample_status_inner"
    fn_body = bytes.fromhex("8b44240485c074038b4004c3")
    assert len(fn_body) == 12, f"body len {len(fn_body)}"

    mod = OmfModule(module_name="ail_41240_AIL_internal_sample_status_inner")
    seg_idx = mod.add_segment("AIL_CODE", "CODE", length=len(fn_body))
    mod.add_pubdef(seg_idx=seg_idx, name=fn_name, offset=0)
    mod.add_ledata(seg_idx=seg_idx, offset=0, data=fn_body)
    out = mod.build()

    # Dump for visual inspection
    print(f"emitted {len(out)} bytes:")
    print(f"  hex: {out.hex()}")
    print()

    # Walk + verify
    records = list(walk_records(out))
    print(f"parsed {len(records)} OMF records:")
    expected_seq = [0x80, 0x96, 0x99, 0x91, 0xA1, 0x8B]
    actual_seq = [rt for rt, _ in records]
    print(f"  type sequence: {[hex(t) for t in actual_seq]}")
    print(f"  expected:      {[hex(t) for t in expected_seq]}")
    assert actual_seq == expected_seq, f"record type sequence mismatch"

    # THEADR: length-prefixed name should match module name
    theadr_payload = records[0][1]
    name_len = theadr_payload[0]
    theadr_name = theadr_payload[1 : 1 + name_len].decode("ascii")
    print(f"  THEADR name: {theadr_name!r}")
    assert theadr_name == "ail_41240_AIL_internal_sample_status_inner"

    # LNAMES: should contain "AIL_CODE", "CODE", "" (overlay)
    lnames_payload = records[1][1]
    print(f"  LNAMES payload bytes ({len(lnames_payload)}): {lnames_payload.hex()}")
    names = []
    p = 0
    while p < len(lnames_payload):
        nl = lnames_payload[p]; p += 1
        names.append(lnames_payload[p : p + nl].decode("ascii"))
        p += nl
    print(f"  LNAMES decoded: {names!r}")
    assert "AIL_CODE" in names and "CODE" in names

    # SEGDEF32: ACBP + length(4B) + name_idx + class_idx + overlay_idx
    seg_payload = records[2][1]
    acbp_byte = seg_payload[0]
    seg_len = int.from_bytes(seg_payload[1:5], "little")
    print(f"  SEGDEF32 ACBP=0x{acbp_byte:02x} (A={(acbp_byte>>5)&7} C={(acbp_byte>>2)&7} B={(acbp_byte>>1)&1} P={acbp_byte&1}) len={seg_len}")
    assert seg_len == 12

    # PUBDEF32: base_grp + base_seg + (name + offset32 + type_idx)*
    pub_payload = records[3][1]
    print(f"  PUBDEF32 payload ({len(pub_payload)}): {pub_payload.hex()}")
    p = 0
    base_grp = pub_payload[p]; p += 1
    base_seg = pub_payload[p]; p += 1
    print(f"    base_grp={base_grp} base_seg={base_seg}")
    nl = pub_payload[p]; p += 1
    pub_name = pub_payload[p : p + nl].decode("ascii"); p += nl
    pub_off = int.from_bytes(pub_payload[p : p + 4], "little"); p += 4
    pub_type = pub_payload[p]; p += 1
    print(f"    pub: name={pub_name!r} offset=0x{pub_off:x} type={pub_type}")
    assert pub_name == fn_name and pub_off == 0

    # LEDATA32: seg_idx + offset(4B) + data
    led_payload = records[4][1]
    p = 0
    led_seg = led_payload[p]; p += 1
    led_off = int.from_bytes(led_payload[p : p + 4], "little"); p += 4
    led_bytes = led_payload[p:]
    print(f"  LEDATA32 seg_idx={led_seg} off=0x{led_off:x} len={len(led_bytes)}")
    print(f"    data: {led_bytes.hex()}")
    assert led_bytes == fn_body

    # MODEND32: 1 byte module type (0x00 = not main, no start addr)
    mend_payload = records[5][1]
    print(f"  MODEND32 type byte=0x{mend_payload[0]:02x}")
    assert mend_payload[0] == 0x00

    # Quirky detector — our emission should NOT be flagged (we use 32-bit types)
    assert not patch_mod.is_quirky(out), "unexpected: emit flagged as Watcom-quirky"

    print()
    print("PASS: 5 checks (record sequence, checksums, name decoding, PUBDEF, LEDATA)")


if __name__ == "__main__":
    main()
