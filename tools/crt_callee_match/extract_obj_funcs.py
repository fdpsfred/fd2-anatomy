"""Quick OMF-386 parser to extract function sizes from .obj files.

For each .obj in --in-dir, dump:
  { obj_name: [ { name, seg, offset, size } ... ] }

Function size = (next PUBDEF offset in same segment, or segment length) - this offset.
"""
from __future__ import annotations
import argparse, json, struct, sys
from pathlib import Path

# Record IDs we care about (post omf_patch_segdef, all are 32-bit forms)
REC_THEADR = 0x80
REC_LNAMES = 0x96
REC_SEGDEF = 0x98
REC_SEGDEF32 = 0x99
REC_PUBDEF = 0x90
REC_PUBDEF32 = 0x91
REC_MODEND = 0x8A
REC_MODEND32 = 0x8B


def read_index(buf: bytes, off: int) -> tuple[int, int]:
    b = buf[off]
    if b & 0x80:
        return (((b & 0x7F) << 8) | buf[off + 1]), off + 2
    return b, off + 1


def is_quirky(data: bytes) -> bool:
    """Watcom Easy OMF-386 quirk: 0x98 SEGDEF whose content[3] == 0
    (which under standard 16-bit length encoding would be name_idx, required >=1)."""
    off = 0
    n = len(data)
    while off + 3 <= n:
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        if off + 3 + rl > n:
            return False
        if rt == 0x98 and rl >= 9:
            if data[off + 3 + 3] == 0:
                return True
        off += 3 + rl
        if rt in (0x8A, 0x8B):
            break
    return False


def parse_obj(data: bytes) -> dict:
    """Walk OMF records. Returns dict with lnames, segments, pubdefs."""
    lnames: list[str] = [""]  # index 1-based
    segments: list[dict] = [None]  # index 1-based
    pubdefs: list[dict] = []
    quirky = is_quirky(data)

    off = 0
    n = len(data)
    while off + 3 <= n:
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        body_start = off + 3
        body_end = body_start + rl - 1  # last byte is checksum
        if body_end > n:
            break
        body = data[body_start:body_end]

        if rt == REC_LNAMES:
            i = 0
            while i < len(body):
                ln = body[i]
                lnames.append(body[i + 1 : i + 1 + ln].decode("latin-1", errors="replace"))
                i += 1 + ln
        elif rt in (REC_SEGDEF, REC_SEGDEF32):
            # 1 byte ACBP, optional frame/offset (if A=0), then SegLen, NameIdx, ClassIdx, OvlIdx
            i = 0
            acbp = body[i]; i += 1
            A = (acbp >> 5) & 0x7
            if A == 0:  # absolute seg: 2 byte FrameNum + 1 byte Offset
                i += 3
            seg_len_size = 4 if (rt == REC_SEGDEF32 or quirky) else 2
            if seg_len_size == 4:
                seg_len = struct.unpack_from("<I", body, i)[0]; i += 4
            else:
                seg_len = struct.unpack_from("<H", body, i)[0]; i += 2
            name_idx, i = read_index(body, i)
            class_idx, i = read_index(body, i)
            ovl_idx, i = read_index(body, i)
            segments.append({
                "name": lnames[name_idx] if name_idx < len(lnames) else f"<idx{name_idx}>",
                "class": lnames[class_idx] if class_idx < len(lnames) else f"<idx{class_idx}>",
                "len": seg_len,
            })
        elif rt in (REC_PUBDEF, REC_PUBDEF32):
            i = 0
            base_grp, i = read_index(body, i)
            base_seg, i = read_index(body, i)
            if base_seg == 0:
                # absolute frame number follows: 2 bytes
                i += 2
            off_size = 4 if (rt == REC_PUBDEF32 or quirky) else 2
            while i < len(body):
                ln = body[i]; i += 1
                name = body[i : i + ln].decode("latin-1", errors="replace"); i += ln
                if off_size == 4:
                    pub_off = struct.unpack_from("<I", body, i)[0]; i += 4
                else:
                    pub_off = struct.unpack_from("<H", body, i)[0]; i += 2
                # type index
                _, i = read_index(body, i)
                pubdefs.append({"name": name, "seg": base_seg, "offset": pub_off})
        # advance
        off = body_end + 1
        if rt in (REC_MODEND, REC_MODEND32):
            break
    return {"lnames": lnames, "segments": segments, "pubdefs": pubdefs}


def compute_func_sizes(parsed: dict) -> list[dict]:
    """For each PUBDEF in a CODE segment, size = next PUBDEF offset in same seg
    (sorted), or segment length, minus this offset."""
    segments = parsed["segments"]
    by_seg: dict[int, list[dict]] = {}
    for p in parsed["pubdefs"]:
        by_seg.setdefault(p["seg"], []).append(p)
    out = []
    for seg_idx, lst in by_seg.items():
        if seg_idx >= len(segments) or segments[seg_idx] is None:
            continue
        seg = segments[seg_idx]
        # only CODE segments (heuristic: class name containing CODE, or seg name _TEXT)
        cls = seg["class"].upper()
        nm = seg["name"].upper()
        is_code = ("CODE" in cls) or nm.endswith("_TEXT") or nm == "CODE"
        if not is_code:
            continue
        lst_sorted = sorted(lst, key=lambda x: x["offset"])
        for j, p in enumerate(lst_sorted):
            end = lst_sorted[j + 1]["offset"] if j + 1 < len(lst_sorted) else seg["len"]
            out.append({
                "name": p["name"],
                "seg_name": seg["name"],
                "seg_class": seg["class"],
                "offset": p["offset"],
                "size": end - p["offset"],
                "seg_len": seg["len"],
            })
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    result: dict[str, list[dict]] = {}
    failed: list[str] = []
    for fp in sorted(args.in_dir.glob("*.obj")):
        try:
            parsed = parse_obj(fp.read_bytes())
            funcs = compute_func_sizes(parsed)
            result[fp.name] = funcs
        except Exception as e:
            failed.append(f"{fp.name}: {e}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2))
    print(f"Parsed {len(result)} .obj  ({sum(len(v) for v in result.values())} funcs)  failed: {len(failed)}")
    for f in failed[:20]:
        print(f"  ! {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
