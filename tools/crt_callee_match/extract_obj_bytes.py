"""Extract LEDATA bytes for each function in an .obj, plus FIXUPP positions
that should be wildcarded when comparing against linked binary.

Output: JSON of {obj: {func_name: {offset, size, hex, fixups: [(pos, len)...]}}}
where pos is offset within the function (0..size) and len is 4 (rel32 or abs32).
"""
from __future__ import annotations
import argparse, json, struct, sys
from pathlib import Path


def read_index(buf, off):
    b = buf[off]
    if b & 0x80:
        return ((b & 0x7F) << 8) | buf[off + 1], off + 2
    return b, off + 1


def is_quirky(data):
    off = 0
    while off + 3 <= len(data):
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        if off + 3 + rl > len(data):
            return False
        if rt == 0x98 and rl >= 9 and data[off + 3 + 3] == 0:
            return True
        off += 3 + rl
        if rt in (0x8A, 0x8B):
            break
    return False


def parse_obj(data: bytes) -> dict:
    quirky = is_quirky(data)
    # Watcom 32-bit CLIB3S .obj files use loc_type=1/5 for 32-bit offsets in
    # USE32 segments (regardless of quirky-ness). We track this per-segment via
    # the SEGDEF ACBP P-bit (bit0). For simplicity, treat any seg with USE32
    # attribute as 32-bit-fixup; falls back to standard width if not USE32.
    seg_use32: dict[int, bool] = {}
    lnames = [""]
    segments = [None]  # 1-based
    pubdefs = []  # list of {name, seg, offset}
    # seg_data[seg_idx] = bytearray
    seg_data: dict[int, bytearray] = {}
    # fixups[seg_idx] = list of (pos_in_seg, len)
    fixups: dict[int, list[tuple[int, int]]] = {}
    # Last LEDATA processed (for FIXUPP record context)
    last_ledata_seg = None
    last_ledata_off = 0

    off = 0
    n = len(data)
    while off + 3 <= n:
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        body = data[off + 3 : off + 3 + rl - 1]

        if rt == 0x96:  # LNAMES
            i = 0
            while i < len(body):
                ln = body[i]
                lnames.append(body[i + 1 : i + 1 + ln].decode("latin-1", errors="replace"))
                i += 1 + ln

        elif rt in (0x98, 0x99):  # SEGDEF / SEGDEF32
            i = 0
            acbp = body[i]; i += 1
            A = (acbp >> 5) & 0x7
            # Watcom Easy OMF-386: 32-bit segments are signaled by record type
            # 0x99 (SEGDEF32) or by quirky 0x98 in a quirky file. The standard
            # ACBP P-bit is unreliable here.
            use32 = (rt == 0x99) or quirky
            if A == 0:
                i += 3
            seg_len_size = 4 if (rt == 0x99 or quirky) else 2
            if seg_len_size == 4:
                seg_len = struct.unpack_from("<I", body, i)[0]; i += 4
            else:
                seg_len = struct.unpack_from("<H", body, i)[0]; i += 2
            name_idx, i = read_index(body, i)
            class_idx, i = read_index(body, i)
            ovl_idx, i = read_index(body, i)
            segments.append({
                "name": lnames[name_idx] if name_idx < len(lnames) else f"<{name_idx}>",
                "class": lnames[class_idx] if class_idx < len(lnames) else f"<{class_idx}>",
                "len": seg_len,
            })
            seg_data[len(segments) - 1] = bytearray(seg_len)
            fixups[len(segments) - 1] = []
            seg_use32[len(segments) - 1] = use32

        elif rt in (0x90, 0x91):  # PUBDEF / PUBDEF32
            i = 0
            base_grp, i = read_index(body, i)
            base_seg, i = read_index(body, i)
            if base_seg == 0:
                i += 2
            off_size = 4 if (rt == 0x91 or quirky) else 2
            while i < len(body):
                ln = body[i]; i += 1
                name = body[i : i + ln].decode("latin-1", errors="replace"); i += ln
                if off_size == 4:
                    pub_off = struct.unpack_from("<I", body, i)[0]; i += 4
                else:
                    pub_off = struct.unpack_from("<H", body, i)[0]; i += 2
                _, i = read_index(body, i)
                pubdefs.append({"name": name, "seg": base_seg, "offset": pub_off})

        elif rt in (0xA0, 0xA1):  # LEDATA / LEDATA32
            i = 0
            seg_idx, i = read_index(body, i)
            data_off_size = 4 if (rt == 0xA1 or quirky) else 2
            if data_off_size == 4:
                data_off = struct.unpack_from("<I", body, i)[0]; i += 4
            else:
                data_off = struct.unpack_from("<H", body, i)[0]; i += 2
            payload = bytes(body[i:])
            if seg_idx in seg_data:
                seg_data[seg_idx][data_off : data_off + len(payload)] = payload
            last_ledata_seg = seg_idx
            last_ledata_off = data_off

        elif rt in (0x9C, 0x9D):  # FIXUPP / FIXUPP32
            # Parse FIXUPP records to find positions to wildcard.
            # Each subrecord starts with a leading byte. If high bit set: FIXUP record.
            # Fixup record format (most common):
            #   2 bytes: locat (top 4 bits) + data_record_offset (12 bits)
            #   1 byte:  fix_data
            #   variable: frame info, target info
            i = 0
            while i < len(body):
                first = body[i]
                if not (first & 0x80):
                    # THREAD subrecord — skip
                    flags = first
                    i += 1
                    method_or_thread = flags & 0x7
                    is_frame = (flags >> 6) & 1
                    # Index field follows (1 or 2 bytes)
                    if method_or_thread <= 3 or (method_or_thread == 4 and is_frame):
                        _, i = read_index(body, i)
                    continue
                # FIXUP subrecord. LOCAT field (2 bytes, big-endian):
                #   byte0: 1 M LLLL OO  (bit7=1 fixup, bit6=M mode,
                #                        bits5-2=location, bits1-0=offset hi)
                #   byte1: OOOOOOOO   (offset low 8 bits)
                if i + 2 >= len(body):
                    break
                location_type = (first >> 2) & 0xF
                data_record_offset = ((first & 0x3) << 8) | body[i + 1]
                fix_data = body[i + 2]
                i += 3
                # location_type: 0=lobyte, 1=offset16, 2=base, 3=ptr16:16, 4=hibyte,
                #                5=offset16(LE), 6=ptr16:32, 9=offset32, 11=ptr16:32(alt),
                #                13=offset32(LE)
                # In Watcom Easy OMF-386 quirky files, location_type 1 / 5 are
                # actually 32-bit offsets even though the type ID matches the
                # standard 16-bit OMF encoding (the segment is USE32).
                base_width = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 6: 6, 9: 4, 11: 4, 13: 4}.get(location_type, 4)
                # For Watcom 32-bit (CLIB3S) files: USE32 segs encode 32-bit
                # offsets via loc_type=1 / 5 (instead of standard 9 / 13).
                # Quirky files also do this. Detect via segment's P-bit.
                last_seg_use32 = seg_use32.get(last_ledata_seg, False) if last_ledata_seg else False
                if (quirky or last_seg_use32) and location_type in (1, 5):
                    width = 4
                else:
                    width = base_width
                # frame method (top 4 bits of fix_data) and target method (bottom 4 bits ish)
                F = (fix_data >> 7) & 1
                Frame = (fix_data >> 4) & 7
                T = (fix_data >> 3) & 1
                P = (fix_data >> 2) & 1
                Targt = fix_data & 3
                # Skip frame index if not from thread
                if not F:
                    if Frame in (0, 1, 2):  # SI(SEGDEF), GI(GRPDEF), EI(EXTDEF)
                        _, i = read_index(body, i)
                    # Frame=4 (target), 5 (target_segment) — no index
                # Skip target index if not from thread
                if not T:
                    _, i = read_index(body, i)
                # Skip displacement if P=0 (4 bytes for 32-bit, but actually depends)
                if not P:
                    # In 32-bit OMF (or quirky), displacement is 4 bytes
                    disp_size = 4 if quirky else 2
                    i += disp_size
                # Record fixup position in last LEDATA's segment
                if last_ledata_seg is not None:
                    abs_off = last_ledata_off + data_record_offset
                    fixups.setdefault(last_ledata_seg, []).append((abs_off, width))

        off = off + 3 + rl
        if rt in (0x8A, 0x8B):
            break

    # Build per-function output
    by_func: dict[str, dict] = {}
    by_seg = {}
    for p in pubdefs:
        by_seg.setdefault(p["seg"], []).append(p)
    for seg_idx, lst in by_seg.items():
        if seg_idx >= len(segments) or segments[seg_idx] is None:
            continue
        seg = segments[seg_idx]
        cls = seg["class"].upper()
        nm = seg["name"].upper()
        if not (("CODE" in cls) or nm.endswith("_TEXT") or nm == "CODE"):
            continue
        seg_bytes = bytes(seg_data.get(seg_idx, b""))
        seg_fixups = fixups.get(seg_idx, [])
        sorted_lst = sorted(lst, key=lambda x: x["offset"])
        for j, p in enumerate(sorted_lst):
            end = sorted_lst[j + 1]["offset"] if j + 1 < len(sorted_lst) else seg["len"]
            sz = end - p["offset"]
            func_bytes = seg_bytes[p["offset"]: end]
            # Filter fixups to those within this function
            func_fixups = [
                (pos - p["offset"], w)
                for pos, w in seg_fixups
                if p["offset"] <= pos < end
            ]
            by_func[p["name"]] = {
                "seg_idx": seg_idx,
                "seg_name": seg["name"],
                "offset": p["offset"],
                "size": sz,
                "hex": func_bytes.hex(),
                "fixups": func_fixups,
            }
        # Also include any "leading static" bytes (offset 0 to first PUBDEF)
        first_pub = sorted_lst[0]["offset"]
        if first_pub > 0:
            by_func["__static_pre_" + sorted_lst[0]["name"]] = {
                "seg_idx": seg_idx,
                "seg_name": seg["name"],
                "offset": 0,
                "size": first_pub,
                "hex": seg_bytes[:first_pub].hex(),
                "fixups": [(pos, w) for pos, w in seg_fixups if pos < first_pub],
            }

    return {
        "by_func": by_func,
        "all_seg_lengths": {i: s["len"] for i, s in enumerate(segments) if s},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objs", nargs="+", required=True, help="Specific .obj basenames to extract")
    ap.add_argument("--in-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    out = {}
    for name in args.objs:
        fp = args.in_dir / name
        if not fp.exists():
            # Try fuzzy match (just suffix)
            cands = list(args.in_dir.glob(f"*_{name}")) + list(args.in_dir.glob(f"*{name}"))
            if cands:
                fp = cands[0]
            else:
                print(f"NOT FOUND: {name}", file=sys.stderr)
                continue
        parsed = parse_obj(fp.read_bytes())
        out[fp.name] = parsed

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(out, indent=2))
    print(f"Wrote {args.out}: {len(out)} obj")


if __name__ == "__main__":
    main()
