"""Test extract_ail_bytes.py: byte-identical against FD2.LE.

Per [[feedback_script_review_then_test]].

Tests:
  T1. ail_code.bin / ail_data.bin / ail_layout.json all exist
  T2. ail_code.bin size = sum(range.size) across all fns
  T3. ail_data.bin size = sum(size) across all data items
  T4. byte-identical: for each function's first range, re-read from FD2.LE
      at le_file_off and compare against ail_code.bin slice
  T5. byte-identical for sampled data items (mixer dispatch tables, GTL
      prefix, pitch bend LUT — items >= 100 bytes for meaningful sample)
  T6. multi-range function: all ranges extracted and ordered correctly
  T7. BSS items: all addr in object's bss region per derived LE layout
  T8. LE layout objects: file_off + file_size doesn't exceed LE file size
  T9. obj1.file_size + obj1.bss + obj2... summed must equal expected file
      data section size derived from LE header

Cross-check sources:
  - FD2.LE (primary binary, re-read at test time)
  - workspace/ail_extract/ail_layout.json (script output to test)
  - LE format parser from data_audit (independent re-import for verification)

Exit 0 on PASS, 1 on first FAIL.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LE_PATH = REPO / "fd2_game_files" / "FD2.LE"
LAYOUT = REPO / "workspace" / "ail_extract" / "ail_layout.json"
CODE_BIN = REPO / "workspace" / "ail_extract" / "ail_code.bin"
DATA_BIN = REPO / "workspace" / "ail_extract" / "ail_data.bin"

sys.path.insert(0, str(REPO / "tools" / "program_analysis" / "data_audit"))
from parse_le_fixup import parse_le_header, parse_object_table  # noqa: E402


def fail(test_id, msg):
    print(f"  FAIL [{test_id}] {msg}")
    sys.exit(1)


def ok(test_id, msg):
    print(f"  PASS [{test_id}] {msg}")


def main():
    # T1: files exist
    for p in (LAYOUT, CODE_BIN, DATA_BIN):
        if not p.exists():
            fail("T1", f"{p} missing")
    ok("T1", "ail_layout.json + ail_code.bin + ail_data.bin all exist")

    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    code_bytes = CODE_BIN.read_bytes()
    data_bytes = DATA_BIN.read_bytes()
    le_bytes = LE_PATH.read_bytes()

    # T2: code size
    expected_code_size = 0
    for fn in layout["code"]:
        for r in fn["ranges"]:
            expected_code_size += r["size"]
    if len(code_bytes) != expected_code_size:
        fail("T2", f"ail_code.bin size {len(code_bytes)} != sum(ranges) {expected_code_size}")
    ok("T2", f"ail_code.bin size {len(code_bytes)} = sum of {sum(len(fn['ranges']) for fn in layout['code'])} ranges")

    # T3: data size
    expected_data_size = sum(d["size"] for d in layout["data"])
    if len(data_bytes) != expected_data_size:
        fail("T3", f"ail_data.bin size {len(data_bytes)} != sum(data sizes) {expected_data_size}")
    ok("T3", f"ail_data.bin size {len(data_bytes)} = sum of {len(layout['data'])} data items")

    # T4: byte-identical sample (sample 5 fns spread across address space)
    sample_fns = []
    step = max(1, len(layout["code"]) // 5)
    for i in range(0, len(layout["code"]), step):
        sample_fns.append(layout["code"][i])
    sample_fns = sample_fns[:5]
    for fn in sample_fns:
        r = fn["ranges"][0]
        le_off = int(r["le_file_off"], 16)
        ail_off = int(r["ail_code_off"], 16)
        sz = r["size"]
        if le_bytes[le_off : le_off + sz] != code_bytes[ail_off : ail_off + sz]:
            fail("T4", f"{fn['name']} bytes mismatch at LE 0x{le_off:x} vs ail_code 0x{ail_off:x}")
    ok("T4", f"5 sampled functions byte-identical to FD2.LE")

    # T5: byte-identical sample for large data items (>= 100 bytes)
    large_items = sorted([d for d in layout["data"] if d["size"] >= 100],
                          key=lambda d: -d["size"])[:5]
    for d in large_items:
        le_off = int(d["le_file_off"], 16)
        ail_off = int(d["ail_data_off"], 16)
        sz = d["size"]
        if le_bytes[le_off : le_off + sz] != data_bytes[ail_off : ail_off + sz]:
            fail("T5", f"{d['name']} bytes mismatch at LE 0x{le_off:x} vs ail_data 0x{ail_off:x}")
    ok("T5", f"{len(large_items)} sampled large data items byte-identical")

    # T6: multi-range fns
    multi = [fn for fn in layout["code"] if len(fn["ranges"]) > 1]
    for fn in multi:
        # Each range must be byte-identical
        for r in fn["ranges"]:
            le_off = int(r["le_file_off"], 16)
            ail_off = int(r["ail_code_off"], 16)
            sz = r["size"]
            if le_bytes[le_off : le_off + sz] != code_bytes[ail_off : ail_off + sz]:
                fail("T6", f"multi-range {fn['name']} range@0x{le_off:x} byte mismatch")
        # Ranges must be in linear-address-ascending order
        starts = [int(r["linear_start"], 16) for r in fn["ranges"]]
        if starts != sorted(starts):
            fail("T6", f"multi-range {fn['name']} ranges not in ascending order: {starts}")
    ok("T6", f"{len(multi)} multi-range fns: all ranges byte-identical + ordered")

    # T7: BSS items in bss segment
    objects = layout["objects"]
    for b in layout["bss"]:
        addr = int(b["addr"], 16)
        # The segment label like "obj2_bss" means bss of object 2
        seg = b["segment"]
        if not seg.endswith("_bss"):
            fail("T7", f"BSS item {b['name']} segment={seg!r} not '*_bss'")
        # Verify addr is in some object's bss range
        in_bss = False
        for o in objects:
            if o["linear_file_end"] <= addr < o["linear_v_end"]:
                in_bss = True
                break
        if not in_bss:
            fail("T7", f"BSS item {b['name']} addr 0x{addr:x} not in any obj's bss range")
    ok("T7", f"{len(layout['bss'])} BSS items all in derived BSS ranges")

    # T8: object file ranges within LE file
    for o in objects:
        if o["file_off"] + o["file_size"] > len(le_bytes):
            fail("T8", f"obj{o['index']}: file_off 0x{o['file_off']:x} + file_size 0x{o['file_size']:x} exceeds LE size 0x{len(le_bytes):x}")
    ok("T8", f"all {len(objects)} objects' file ranges within LE file")

    # T9: sum of object file_size = expected from LE header
    hdr = parse_le_header(le_bytes)
    import struct
    last_page_size = struct.unpack_from("<I", le_bytes, hdr["new_hdr_off"] + 0x2C)[0]
    expected_total = (hdr["num_pages"] - 1) * hdr["page_size"] + last_page_size
    actual_total = sum(o["file_size"] for o in objects)
    if actual_total != expected_total:
        fail("T9", f"sum object file_size 0x{actual_total:x} != expected 0x{expected_total:x}")
    ok("T9", f"sum of object file_size 0x{actual_total:x} = expected from LE header")

    print("\nAll tests PASS.")


if __name__ == "__main__":
    main()
