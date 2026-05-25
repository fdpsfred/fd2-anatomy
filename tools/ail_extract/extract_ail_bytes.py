"""Extract AIL function / data bytes from FD2.LE.

Per [[feedback_no_kb_hardcoded_values]]: LE layout (object base / file offset /
file_size / virtual_size) is derived dynamically by importing the existing
`tools/program_analysis/data_audit/parse_le_fixup.py` helpers. No constants
from `rebuild_info/link/le_layout.md` are hardcoded here.

Per [[feedback_script_review_then_test]]: see test_extract_ail_bytes.py.

Inputs:
  - `fd2_game_files/FD2.LE` (primary binary)
  - `workspace/ail_extract/ail_inventory.json` (produced by dump_ail_set.py)

Outputs:
  - `workspace/ail_extract/ail_code.bin`   : AIL fn body bytes (concatenated per range)
  - `workspace/ail_extract/ail_data.bin`   : AIL data item bytes (file-backed only)
  - `workspace/ail_extract/ail_layout.json`: per-fn/data offset map + diagnostics
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LE_PATH = REPO / "fd2_game_files" / "FD2.LE"
INVENTORY = REPO / "workspace" / "ail_extract" / "ail_inventory.json"

CODE_OUT = REPO / "workspace" / "ail_extract" / "ail_code.bin"
DATA_OUT = REPO / "workspace" / "ail_extract" / "ail_data.bin"
LAYOUT_OUT = REPO / "workspace" / "ail_extract" / "ail_layout.json"

# Import LE header / object table parser from existing data_audit script.
# All LE constants (data_pages_off / object base / virt_size / page count) come
# from parsing FD2.LE at runtime; no values from KB.
sys.path.insert(0, str(REPO / "tools" / "program_analysis" / "data_audit"))
from parse_le_fixup import parse_le_header, parse_object_table  # noqa: E402


def hex_int(s: str) -> int:
    return int(s, 16)


def build_object_map(le_bytes: bytes) -> list[dict]:
    """Return list of objects with derived file_off and file_size per object.

    Each object dict:
        index           : 1-based obj index
        base            : linear address (relocation base)
        virtual_size    : virtual size from LE object table
        pg_first        : first page index (1-based)
        pg_count        : page count for this object
        file_off        : file offset where object's first page starts
        file_size       : total file-backed bytes for this object
                          (= sum of page sizes, last data page uses last_page_size)
        linear_file_end : base + file_size (exclusive end of file-backed range)
        linear_v_end    : base + virtual_size (exclusive end of virtual range)
    """
    hdr = parse_le_header(le_bytes)
    objs = parse_object_table(le_bytes, hdr)
    page_size = hdr["page_size"]
    num_pages_total = hdr["num_pages"]
    # For LE we don't have last_page_size in parse_le_header's return dict;
    # re-read directly from the parsed header. parse_le_fixup uses spec offsets
    # for fields it needs; the last_page_size is at LE-header offset 0x2C.
    import struct
    new_hdr_off = hdr["new_hdr_off"]
    last_page_size = struct.unpack_from("<I", le_bytes, new_hdr_off + 0x2C)[0]
    data_pages_off = hdr["data_pages_off"]

    enriched = []
    for o in objs:
        # file_off = data_pages_off + (pg_first - 1) * page_size
        file_off = data_pages_off + (o["pg_first"] - 1) * page_size
        # file_size: for each of pg_count pages, use last_page_size if it's
        # the very last data page (its phys page number equals num_pages_total),
        # else use page_size.
        size = 0
        for k in range(o["pg_count"]):
            phys_page = o["pg_first"] + k
            size += last_page_size if phys_page == num_pages_total else page_size
        enriched.append({
            "index":           o["idx"],
            "base":            o["base_addr"],
            "virtual_size":    o["virt_size"],
            "pg_first":        o["pg_first"],
            "pg_count":        o["pg_count"],
            "file_off":        file_off,
            "file_size":       size,
            "linear_file_end": o["base_addr"] + size,
            "linear_v_end":    o["base_addr"] + o["virt_size"],
        })
    return enriched


def linear_to_file_offset(addr: int, objects: list[dict]) -> tuple[int | None, str]:
    """Map a linear address to file offset (or None if BSS / zero-fill).

    Returns (file_off_or_None, segment_label) where segment_label is:
      "obj<N>_file" for file-backed bytes
      "obj<N>_bss"  for zero-fill region
      raises ValueError if outside any object.
    """
    for obj in objects:
        if obj["base"] <= addr < obj["linear_file_end"]:
            return obj["file_off"] + (addr - obj["base"]), f"obj{obj['index']}_file"
        if obj["linear_file_end"] <= addr < obj["linear_v_end"]:
            return None, f"obj{obj['index']}_bss"
    raise ValueError(f"linear addr 0x{addr:x} outside known objects")


def main():
    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
    le_bytes = LE_PATH.read_bytes()
    objects = build_object_map(le_bytes)

    print(f"LE file size: {len(le_bytes)} bytes")
    print(f"LE objects:")
    for o in objects:
        bss = o["virtual_size"] - o["file_size"]
        print(f"  obj{o['index']}: base=0x{o['base']:08x} "
              f"virt=0x{o['virtual_size']:x} file=0x{o['file_size']:x} bss=0x{bss:x} "
              f"file_off=0x{o['file_off']:x}")

    code_buf = bytearray()
    data_buf = bytearray()
    layout = {
        "objects": objects,
        "code": [],
        "data": [],
        "bss": [],
        "diagnostics": {},
    }

    # === FUNCTIONS ===
    multi_range_count = 0
    for fn in inv["functions"]:
        name = fn["name"]
        ranges_out = []
        for [rmin, rmax] in fn["body_ranges"]:
            r_start = hex_int(rmin)
            r_end_inclusive = hex_int(rmax)
            r_size = r_end_inclusive - r_start + 1
            file_off, segment = linear_to_file_offset(r_start, objects)
            if file_off is None:
                raise ValueError(f"function {name} @ {fn['entry']} body in BSS?!")
            if file_off + r_size > len(le_bytes):
                raise ValueError(f"function {name} bytes 0x{file_off:x}+{r_size} exceeds LE size")
            code_off = len(code_buf)
            code_buf.extend(le_bytes[file_off : file_off + r_size])
            ranges_out.append({
                "linear_start": f"0x{r_start:08x}",
                "linear_end_excl": f"0x{r_start + r_size:08x}",
                "le_file_off": f"0x{file_off:08x}",
                "ail_code_off": f"0x{code_off:08x}",
                "size": r_size,
                "segment": segment,
            })
        if len(ranges_out) > 1:
            multi_range_count += 1
        layout["code"].append({
            "name": name,
            "entry_addr": fn["entry"],
            "cc": fn["cc"],
            "signature": fn["signature"],
            "is_public": not name.startswith("AIL_internal_"),
            "ranges": ranges_out,
        })

    # === DATA ITEMS ===
    for item in inv["data_items"]:
        name = item["name"]
        addr = hex_int(item["addr"])
        size = item["size"]
        file_off, segment = linear_to_file_offset(addr, objects)
        if file_off is None:
            # BSS — no file bytes
            layout["bss"].append({
                "name": name,
                "addr": item["addr"],
                "size": size,
                "type": item["type"],
                "segment": segment,
            })
            continue
        if file_off + size > len(le_bytes):
            raise ValueError(f"data {name} @ {item['addr']} bytes 0x{file_off:x}+{size} exceeds LE size")
        data_off = len(data_buf)
        data_buf.extend(le_bytes[file_off : file_off + size])
        layout["data"].append({
            "name": name,
            "addr": item["addr"],
            "size": size,
            "type": item["type"],
            "le_file_off": f"0x{file_off:08x}",
            "ail_data_off": f"0x{data_off:08x}",
            "segment": segment,
        })

    layout["diagnostics"] = {
        "le_file_size": len(le_bytes),
        "ail_code_size": len(code_buf),
        "ail_data_size": len(data_buf),
        "function_count": len(layout["code"]),
        "data_count": len(layout["data"]),
        "bss_count": len(layout["bss"]),
        "multi_range_functions": multi_range_count,
    }

    CODE_OUT.write_bytes(bytes(code_buf))
    DATA_OUT.write_bytes(bytes(data_buf))
    LAYOUT_OUT.write_text(json.dumps(layout, indent=2, ensure_ascii=False), encoding="utf-8")

    print()
    print(f"Wrote {CODE_OUT}  ({len(code_buf)} bytes)")
    print(f"Wrote {DATA_OUT}  ({len(data_buf)} bytes)")
    print(f"Wrote {LAYOUT_OUT}")
    print(f"  function entries:   {len(layout['code'])}")
    print(f"  multi-range fns:    {multi_range_count}")
    print(f"  data items (file):  {len(layout['data'])}")
    print(f"  bss items (zero):   {len(layout['bss'])}")


if __name__ == "__main__":
    main()
