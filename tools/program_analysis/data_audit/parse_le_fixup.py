"""Parse FD2.LE LE FIXUP table → forward + reverse pointer-resolution index.

Output (`tools/program_analysis/data_audit/data/le_fixups.json`):

    {
      "header": {
        "num_pages": 71, "page_size": 4096,
        "objects": [{"idx":1, "base_addr":"00010000", "virt_size":"0003ebd9",
                     "pg_first":1, "pg_count":63}, ...],
        "fixup_page_table_off":  "0000022f",
        "fixup_record_table_off":"0000034f",
        "fixup_section_bytes":   "0000e02a"
      },
      "source_addr_to_target": {
        "<source_addr_8hex>": {
          "target_addr": "<8hex>", "src_type": 7,
          "src_obj": 1, "trg_obj": 2
        },
        ...
      },
      "target_addr_to_sources": {
        "<target_addr_8hex>": ["<source_addr_8hex>", ...],
        ...
      },
      "stats": {
        "records": <N>, "pages": <P>, "objects": <O>,
        "trg32_count": <n>, "trg16_count": <n>,
        "src_types": {"7": <n>}
      }
    }

For FD2.LE specifically: 7944 records, all src_type=0x07 (32-bit offset),
all internal-reference (trg_type=0), no source-list, no additive, no
16-bit objnum. The decoder still handles the other variants defensively
(raising on unknown shapes) so it stays robust if used on other LE files.

Used by `build_xref_graph.py` (B5 / B6 classification refinement) and
the per-item pointer-indirect chase (plan §H' 手段 B).

## Self-tests

Plan §I originally assumed 0x47638 dispatch table A would produce 128
fixup records (one per pointer slot). That assumption was incorrect:
LE FIXUP records exist only for non-zero pointer slots — the loader has
no work to do for slots that are zero at load time. Empirical ground
truth for FD2.LE (verified by reading the post-load bytes):

  0x47638..0x47838 (table A, 128 dword slots): 60 non-zero entries
  0x47838..0x47A38 (table B, 128 dword slots): 72 non-zero entries

Hence the self-tests verify

1. 0x47638 dispatch table A: 60 fixup sources at 4-byte stride covering
   the 60 non-zero slots.
2. 0x47838 dispatch table B: 72 fixup sources at 4-byte stride covering
   the 72 non-zero slots.
3. 0x53896 tzname_ptr_array (2 dword slots, both populated): 2 fixup
   sources → targets ∈ .object2 (matches the two `tzname[]` string
   buffers at 0x53858 / 0x53877).
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
OUT = WORKSPACE / "le_fixups.json"

LE_FILE = REPO / "fd2_game_files" / "FD2.LE"
RAW_IMAGE = WORKSPACE / "le_image_raw.bin"


def open_source() -> tuple[str, bytes]:
    if LE_FILE.exists():
        return ("file", LE_FILE.read_bytes())
    if RAW_IMAGE.exists():
        return ("ghidra", RAW_IMAGE.read_bytes())
    raise SystemExit(
        "ERROR: neither fd2_game_files/FD2.LE nor data/le_image_raw.bin found."
    )


def parse_le_header(blob: bytes) -> dict:
    """Decode the LE header fields used by the fixup table walker."""
    if blob[:2] == b"MZ":
        new_hdr_off = struct.unpack_from("<I", blob, 0x3C)[0]
    else:
        new_hdr_off = 0
    sig = blob[new_hdr_off:new_hdr_off + 2]
    if sig not in (b"LE", b"LX"):
        raise SystemExit(
            f"ERROR: expected 'LE'/'LX' magic at offset {new_hdr_off:#x}, got {sig!r}"
        )

    le = blob[new_hdr_off:]
    return {
        "new_hdr_off":             new_hdr_off,
        "magic":                   sig.decode("ascii"),
        "num_pages":               struct.unpack_from("<I", le, 0x14)[0],
        "page_size":               struct.unpack_from("<I", le, 0x28)[0],
        "object_table_off":        struct.unpack_from("<I", le, 0x40)[0],
        "num_objects":             struct.unpack_from("<I", le, 0x44)[0],
        "object_pagemap_off":      struct.unpack_from("<I", le, 0x48)[0],
        "fixup_page_table_off":    struct.unpack_from("<I", le, 0x68)[0],
        "fixup_record_table_off":  struct.unpack_from("<I", le, 0x6C)[0],
        "data_pages_off":          struct.unpack_from("<I", le, 0x80)[0],
    }


def parse_object_table(blob: bytes, hdr: dict) -> list[dict]:
    """Decode LE object table — each 24-byte record."""
    base = hdr["new_hdr_off"] + hdr["object_table_off"]
    objs = []
    for i in range(hdr["num_objects"]):
        rec = blob[base + i * 24: base + (i + 1) * 24]
        v_size, base_addr, flags, pg_first, pg_count, _rsv = struct.unpack("<IIIIII", rec)
        objs.append({
            "idx":        i + 1,
            "base_addr":  base_addr,
            "virt_size":  v_size,
            "flags":      flags,
            "pg_first":   pg_first,
            "pg_count":   pg_count,
        })
    return objs


def find_owning_object(objs: list[dict], page: int) -> dict:
    """Return the object record whose page range covers `page` (1-based)."""
    for o in objs:
        if o["pg_first"] <= page < o["pg_first"] + o["pg_count"]:
            return o
    raise ValueError(f"page {page} not covered by any object")


def parse_fixup_records(blob: bytes, hdr: dict, objects: list[dict]) -> dict:
    """Walk fixup_page_table + fixup_record_table → forward & reverse index.

    Only LE-internal-reference, src_type 0x07 (32-bit offset) is observed
    in FD2.LE. Other variants raise so misuse on a non-FD2 binary fails
    loudly rather than silently producing a broken index.
    """
    fpt = hdr["new_hdr_off"] + hdr["fixup_page_table_off"]
    frt = hdr["new_hdr_off"] + hdr["fixup_record_table_off"]
    num_pages = hdr["num_pages"]
    page_size = hdr["page_size"]

    # Snapshot page-table offsets (relative to fixup record table start)
    pg_off = [struct.unpack_from("<I", blob, fpt + i * 4)[0] for i in range(num_pages + 1)]

    forward: dict[str, dict] = {}
    reverse: dict[str, list[str]] = {}
    stats = {
        "records":         0,
        "pages":           num_pages,
        "objects":         len(objects),
        "trg16_count":     0,
        "trg32_count":     0,
        "src_negative":    0,
        "src_overflow":    0,
        "src_types":       {},
        "skipped_records": 0,
    }

    for pg in range(1, num_pages + 1):
        seg_start = frt + pg_off[pg - 1]
        seg_end   = frt + pg_off[pg]
        if seg_start == seg_end:
            continue
        owning = find_owning_object(objects, pg)
        page_base_addr = owning["base_addr"] + (pg - owning["pg_first"]) * page_size

        p = seg_start
        while p < seg_end:
            src   = blob[p]
            flags = blob[p + 1]
            src_type  = src & 0x0F
            src_flags = src & 0xF0
            trg_type  = flags & 0x03
            cur = p + 2

            stats["src_types"][src_type] = stats["src_types"].get(src_type, 0) + 1

            if src_flags & 0x20:  # SOURCE_LIST
                raise NotImplementedError(
                    f"source-list fixup not seen in FD2.LE; encountered at +{p - frt:#x}"
                )

            srcoff = struct.unpack_from("<h", blob, cur)[0]
            cur += 2

            if trg_type != 0:
                raise NotImplementedError(
                    f"only internal-ref (trg_type=0) is expected in FD2.LE; "
                    f"got trg_type={trg_type} at +{p - frt:#x}"
                )

            if flags & 0x40:
                objnum = struct.unpack_from("<H", blob, cur)[0]
                cur += 2
            else:
                objnum = blob[cur]
                cur += 1

            if src_type == 0x02:
                trg_off = None
            else:
                if flags & 0x10:
                    trg_off = struct.unpack_from("<I", blob, cur)[0]
                    cur += 4
                    stats["trg32_count"] += 1
                else:
                    trg_off = struct.unpack_from("<H", blob, cur)[0]
                    cur += 2
                    stats["trg16_count"] += 1

            if flags & 0x04:  # ADDITIVE
                add_sz = 4 if (flags & 0x20) else 2
                cur += add_sz

            stats["records"] += 1
            if srcoff < 0:
                stats["src_negative"] += 1
            elif srcoff >= page_size:
                stats["src_overflow"] += 1

            if src_type != 0x07 or trg_off is None:
                # Defensive: not a 32-bit offset reloc. Skip for index purposes
                # but still log in stats.
                stats["skipped_records"] += 1
                p = cur
                continue

            src_addr = page_base_addr + srcoff
            trg_obj  = objects[objnum - 1]
            trg_addr = trg_obj["base_addr"] + trg_off

            src_hex = f"{src_addr:08x}"
            trg_hex = f"{trg_addr:08x}"

            forward[src_hex] = {
                "target_addr": trg_hex,
                "src_type":    src_type,
                "src_obj":     owning["idx"],
                "trg_obj":     trg_obj["idx"],
                "page":        pg,
                "page_srcoff": srcoff,
            }
            reverse.setdefault(trg_hex, []).append(src_hex)
            p = cur

        if p != seg_end:
            raise RuntimeError(
                f"page {pg} fixup walk did not land on segment end: "
                f"p={p:#x} end={seg_end:#x}"
            )

    return {
        "header": {
            "num_pages":              hdr["num_pages"],
            "page_size":              hdr["page_size"],
            "fixup_page_table_off":   f"{hdr['fixup_page_table_off']:08x}",
            "fixup_record_table_off": f"{hdr['fixup_record_table_off']:08x}",
            "fixup_section_bytes":    f"{pg_off[num_pages]:08x}",
            "objects": [
                {
                    "idx":        o["idx"],
                    "base_addr":  f"{o['base_addr']:08x}",
                    "virt_size":  f"{o['virt_size']:08x}",
                    "pg_first":   o["pg_first"],
                    "pg_count":   o["pg_count"],
                }
                for o in objects
            ],
        },
        "source_addr_to_target": forward,
        "target_addr_to_sources": reverse,
        "stats": stats,
    }


def self_test(out: dict, blob: bytes, hdr: dict, objs: list[dict]) -> int:
    """Three plan §I ground-truth checks. Returns 0 on full pass.

    Ground truth (post-load byte content at the addresses below) was
    obtained via Ghidra MCP `read_memory` and confirms that LE FIXUP
    only records non-zero pointer slots, not every potential slot.
    """
    fwd = out["source_addr_to_target"]

    EXPECTED_DISPATCH_A_COUNT = 60   # non-zero dwords in 0x47638..0x47838
    EXPECTED_DISPATCH_B_COUNT = 72   # non-zero dwords in 0x47838..0x47A38

    failed = 0

    # Test 1: dispatch table A
    sources_a = sorted(int(s, 16) for s in fwd if 0x47638 <= int(s, 16) < 0x47838)
    if len(sources_a) != EXPECTED_DISPATCH_A_COUNT:
        print(f"self-test 1 FAIL: 0x47638 dispatch_a expected {EXPECTED_DISPATCH_A_COUNT} "
              f"sources, got {len(sources_a)}")
        failed += 1
    elif not all((a - 0x47638) % 4 == 0 for a in sources_a):
        print("self-test 1 FAIL: dispatch_a sources not aligned to 4-byte stride")
        failed += 1
    else:
        print(f"self-test 1 PASS: 0x47638 dispatch table A = {len(sources_a)} non-zero "
              f"pointer slots, 4-byte aligned")

    # Test 2: dispatch table B
    sources_b = sorted(int(s, 16) for s in fwd if 0x47838 <= int(s, 16) < 0x47A38)
    if len(sources_b) != EXPECTED_DISPATCH_B_COUNT:
        print(f"self-test 2 FAIL: 0x47838 dispatch_b expected {EXPECTED_DISPATCH_B_COUNT} "
              f"sources, got {len(sources_b)}")
        failed += 1
    elif not all((a - 0x47838) % 4 == 0 for a in sources_b):
        print("self-test 2 FAIL: dispatch_b sources not aligned to 4-byte stride")
        failed += 1
    else:
        print(f"self-test 2 PASS: 0x47838 dispatch table B = {len(sources_b)} non-zero "
              f"pointer slots, 4-byte aligned")

    # Test 3: tzname_ptr_array — 2 entries × 4B
    sources_tz = sorted(int(s, 16) for s in fwd if 0x53896 <= int(s, 16) < 0x5389E)
    if len(sources_tz) != 2:
        print(f"self-test 3 FAIL: 0x53896 tzname_ptr_array expected 2 sources, got {len(sources_tz)}")
        failed += 1
    else:
        targets = sorted(fwd[f"{a:08x}"]["target_addr"] for a in sources_tz)
        if not all(0x50000 <= int(t, 16) < 0x556B0 for t in targets):
            print(f"self-test 3 FAIL: tzname_ptr_array targets not all in .object2: {targets}")
            failed += 1
        else:
            print(f"self-test 3 PASS: 0x53896 tzname_ptr_array = 2 sources → targets {targets}")

    return 0 if failed == 0 else 1


def main() -> int:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    mode, blob = open_source()
    print(f"loaded LE image bytes ({len(blob)} B) via {mode}")

    hdr = parse_le_header(blob)
    print(f"LE header: magic={hdr['magic']}  pages={hdr['num_pages']}  "
          f"objs={hdr['num_objects']}  page_size={hdr['page_size']}")

    objs = parse_object_table(blob, hdr)
    for o in objs:
        print(f"  obj{o['idx']}: base=0x{o['base_addr']:08x}  "
              f"size=0x{o['virt_size']:08x}  pg=[{o['pg_first']}..{o['pg_first']+o['pg_count']-1}]")

    out = parse_fixup_records(blob, hdr, objs)
    print(f"\nparsed {out['stats']['records']} fixup records "
          f"(trg16={out['stats']['trg16_count']}, trg32={out['stats']['trg32_count']}, "
          f"skipped={out['stats']['skipped_records']})")
    print(f"  src_negative={out['stats']['src_negative']}, "
          f"src_overflow={out['stats']['src_overflow']}")

    OUT.write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\nwritten: {OUT.relative_to(REPO)}")

    return self_test(out, blob, hdr, objs)


if __name__ == "__main__":
    sys.exit(main())
