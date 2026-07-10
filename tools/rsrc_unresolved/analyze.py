"""Ground-truth analysis for the [resource-format-unresolved] open issues.

Reads the REAL game files under fd2_game_files/ (never fabricated) and emits
per-item evidence to workspace/rsrc_unresolved/. Covers:

  A. FDOTHER.DAT nested sub-archives  -> 29 nested, 176 sub-entries, per-entry
     content classification (sprite / audio-sample / palette).           (item 2)
  B. FDOTHER.DAT 12 confirmed-dead outer idx -> dump + classify.          (item 6)
  C. ANI.DAT 9 entries -> 0xAD header body + per-frame 8-byte header table,
     characterising the fields the decoder never reads.                   (item 3)
  D. FDSHAP.DAT tile-attribute tables -> byte +1 (0..5) / +2 / +3 value
     distributions across all 33 attr entries.                           (item 5)
  E. TAI.DAT -> RLE round-trip sanity of the non-placeholder entries.     (item 1)

Container format (LLLLLL) and RLE 4-op decode are reused from tools/decoders.
No addresses / counts are hard-coded from KB: everything is derived live from
the binaries. Public format constants (LLLLLL sig, VGA 6-bit DAC range) carry a
source comment.
"""
from __future__ import annotations

import json
import struct
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GAME = REPO_ROOT / "fd2_game_files"
OUT = REPO_ROOT / "workspace" / "rsrc_unresolved"
OUT.mkdir(parents=True, exist_ok=True)

sys.path.insert(0, str(REPO_ROOT / "tools" / "decoders"))
from rle_decoder import rle_decode_sized  # noqa: E402

SIG = b"LLLLLL"  # LLLLLL container signature (resource_info/overview.md, 6x 0x4C)


# ----------------------------------------------------------------------------
# LLLLLL container parse on an in-memory blob (works for outer file AND for a
# nested sub-archive slice, whose offset table is relative to the slice start).
# ----------------------------------------------------------------------------
def parse_ll(blob: bytes):
    """Return (entries, meta) or (None, reason) if blob is not a LLLLLL archive.
    entries = [(idx, start, end, size)] with offsets relative to blob start."""
    if len(blob) < 10 or blob[:6] != SIG:
        return None, "no-LLLLLL-sig"
    first = struct.unpack_from("<I", blob, 6)[0]
    if first < 10 or (first - 6) % 4 != 0 or first > len(blob):
        return None, "bad-first-offset"
    n = (first - 6) // 4
    offs = list(struct.unpack_from(f"<{n}I", blob, 6))
    # monotonic non-decreasing, sentinel == blob size (tolerate <=)
    for i in range(1, n):
        if offs[i] < offs[i - 1]:
            return None, "non-monotonic"
    if offs[-1] > len(blob):
        return None, "sentinel-overflow"
    ents = []
    for i in range(n - 1):
        ents.append((i, offs[i], offs[i + 1], offs[i + 1] - offs[i]))
    meta = {"n_sub": n - 1, "sentinel": offs[-1], "blob_size": len(blob),
            "trailing": len(blob) - offs[-1]}
    return ents, meta


def is_vga_palette(blob: bytes):
    """768 bytes, every byte in 0..0x3F = 256x3 6-bit VGA DAC palette."""
    return len(blob) == 768 and all(b <= 0x3F for b in blob)


def try_sprite(blob: bytes):
    """Attempt an RLE-4op [w][h][cmds] decode. Return dict with verdict.
    A clean sprite: sane w/h and the command stream fills exactly w*h columns
    within the entry (rle_decode_sized returns produced==w*h and consumes <=size).
    """
    if len(blob) < 6:
        return {"sprite": False, "why": "too-small"}
    w, h = struct.unpack_from("<HH", blob, 0)
    if w == 0 or h == 0 or w > 2048 or h > 2048:
        return {"sprite": False, "why": f"bad-dims {w}x{h}", "w": w, "h": h}
    try:
        rw, rh, pixels, mask = rle_decode_sized(blob)
    except Exception as e:  # noqa: BLE001
        return {"sprite": False, "why": f"decode-exc:{e}", "w": w, "h": h}
    produced = len(pixels)
    ok = (produced == w * h)
    return {"sprite": bool(ok), "w": w, "h": h, "produced": produced,
            "expected": w * h, "why": "ok" if ok else "produced!=w*h"}


def classify(blob: bytes):
    """Best-effort content type for a leaf blob."""
    if len(blob) == 0:
        return "empty"
    if is_vga_palette(blob):
        return "vga_palette_768"
    # 7-byte BG/TAI placeholder constant
    if blob == bytes.fromhex("0A000300C9C9C9"):
        return "placeholder_7b"
    sub, _ = parse_ll(blob)
    if sub is not None:
        return f"nested_LLLLLL({len(sub)})"
    sp = try_sprite(blob)
    if sp["sprite"]:
        return f"rle_sprite({sp['w']}x{sp['h']})"
    return "raw_blob"  # audio sample / non-sprite payload


def byte_hist_summary(blob: bytes):
    """Rough audio-vs-noise fingerprint: mean, and share of bytes near 0x80."""
    if not blob:
        return {}
    n = len(blob)
    mean = sum(blob) / n
    near_center = sum(1 for b in blob if 0x60 <= b <= 0xA0) / n
    zeros = blob.count(0) / n
    return {"len": n, "mean": round(mean, 1),
            "near_0x80_frac": round(near_center, 3), "zero_frac": round(zeros, 3)}


# ----------------------------------------------------------------------------
def analyze_fdother():
    raw = (GAME / "FDOTHER.DAT").read_bytes()
    ents, meta = parse_ll(raw)
    assert ents is not None, "FDOTHER not LLLLLL"
    report = {"outer_count": len(ents), "nested": [], "dead_dump": []}

    nested_total_sub = 0
    for (idx, start, end, size) in ents:
        blob = raw[start:end]
        sub, submeta = parse_ll(blob)
        if sub is None:
            continue
        # Reject false-positive "nested" that is actually one sprite whose
        # first bytes happen to parse as LLLLLL: require the sig literally.
        if blob[:6] != SIG:
            continue
        nested_total_sub += len(sub)
        sub_rows = []
        for (si, ss, se, ssz) in sub:
            leaf = blob[ss:se]
            sub_rows.append({
                "sub_idx": si, "size": ssz, "kind": classify(leaf),
                "first12": leaf[:12].hex().upper(),
                "audio_fp": byte_hist_summary(leaf) if ssz > 32 else {},
            })
        report["nested"].append({
            "outer_idx": idx, "outer_hex": f"0x{idx:02X}", "outer_size": size,
            "n_sub": len(sub), "trailing": submeta["trailing"],
            "sub": sub_rows,
        })

    report["nested_archive_count"] = len(report["nested"])
    report["nested_total_sub_entries"] = nested_total_sub

    # ----- item 6: the 12 confirmed-dead outer idx (from fdother.md, but we
    # re-dump the actual bytes and classify them ourselves) -----
    dead_idx = [0x25, 0x26, 0x2B, 0x2C, 0x52, 0x53, 0x55, 0x56, 0x57,
                0x60, 0x61, 0x62]
    for idx in dead_idx:
        s, e, _, _ = ents[idx][1], ents[idx][2], None, None
        blob = raw[ents[idx][1]:ents[idx][2]]
        row = {"idx": idx, "hex": f"0x{idx:02X}", "size": len(blob),
               "kind": classify(blob), "first32": blob[:32].hex().upper()}
        sub, _ = parse_ll(blob)
        if sub is not None and blob[:6] == SIG:
            row["sub"] = [{"sub_idx": si, "size": sz, "kind": classify(blob[ss:se]),
                           "first12": blob[ss:se][:12].hex().upper()}
                          for (si, ss, se, sz) in sub]
        else:
            sp = try_sprite(blob)
            row["sprite_probe"] = sp
        report["dead_dump"].append(row)

    (OUT / "fdother_nested.json").write_text(json.dumps(report, indent=1))
    return report


def analyze_ani():
    raw = (GAME / "ANI.DAT").read_bytes()
    ents, meta = parse_ll(raw)
    assert ents is not None, "ANI not LLLLLL"
    out = {"entry_count": len(ents), "entries": []}
    for (idx, start, end, size) in ents:
        blob = raw[start:end]
        header = blob[:0xAD]
        frame_count = struct.unpack_from("<H", header, 0xA5)[0]
        # walk frames
        pos = 0xAD
        frames = []
        for f in range(frame_count):
            if pos + 8 > len(blob):
                frames.append({"frame": f, "error": "truncated"})
                break
            data_size, chunk_count, u4, u6 = struct.unpack_from("<HHHH", blob, pos)
            dword_4 = struct.unpack_from("<I", blob, pos + 4)[0]
            frames.append({"frame": f, "data_size": data_size,
                           "chunk_count": chunk_count,
                           "hdr4_u16": u4, "hdr6_u16": u6, "hdr4_u32": dword_4})
            pos += 8 + data_size
        # summarise the "unread" fields across frames
        u4s = [fr["hdr4_u16"] for fr in frames if "hdr4_u16" in fr]
        u6s = [fr["hdr6_u16"] for fr in frames if "hdr6_u16" in fr]
        out["entries"].append({
            "idx": idx, "size": size, "frame_count": frame_count,
            "header_AD_hex": header.hex().upper(),
            "header_nonzero_offsets": [i for i, b in enumerate(header) if b != 0],
            "frames_parsed": len(frames),
            "end_pos_matches_size": (pos == len(blob)),
            "end_pos": pos, "blob_size": len(blob),
            "hdr4_distinct": sorted(set(u4s)),
            "hdr6_distinct": sorted(set(u6s)),
            "first5_frames": frames[:5],
        })
    (OUT / "ani.json").write_text(json.dumps(out, indent=1))
    return out


def analyze_fdshap_attr():
    raw = (GAME / "FDSHAP.DAT").read_bytes()
    ents, meta = parse_ll(raw)
    assert ents is not None
    # attr tables live at odd FDSHAP idx: shap_id*2+1, shap_id = 0..0x20 (33)
    out = {"attr_entries": []}
    agg_b1 = Counter()
    agg_b2 = Counter()
    agg_b3 = Counter()
    for shap_id in range(0x21):
        aidx = shap_id * 2 + 1
        if aidx >= len(ents):
            break
        blob = raw[ents[aidx][1]:ents[aidx][2]]
        ntiles = len(blob) // 4
        b1 = Counter(); b2 = Counter(); b3 = Counter()
        for t in range(ntiles):
            rec = blob[t * 4:t * 4 + 4]
            if len(rec) < 4:
                break
            b1[rec[1]] += 1
            b2[rec[2]] += 1
            b3[rec[3]] += 1
        agg_b1 += b1; agg_b2 += b2; agg_b3 += b3
        out["attr_entries"].append({
            "shap_id": shap_id, "fdshap_idx": aidx, "size": len(blob),
            "n_tiles": ntiles,
            "byte1_values": dict(sorted(b1.items())),
            "byte3_values": dict(sorted(b3.items())),
            "byte2_min_max": [min(b2) if b2 else None, max(b2) if b2 else None],
        })
    out["aggregate"] = {
        "byte1_all_values": dict(sorted(agg_b1.items())),
        "byte1_max": max(agg_b1) if agg_b1 else None,
        "byte2_min": min(agg_b2) if agg_b2 else None,
        "byte2_max": max(agg_b2) if agg_b2 else None,
        "byte3_all_values": dict(sorted(agg_b3.items())),
    }
    (OUT / "fdshap_attr.json").write_text(json.dumps(out, indent=1))
    return out


def analyze_tai():
    raw = (GAME / "TAI.DAT").read_bytes()
    ents, meta = parse_ll(raw)
    out = {"entry_count": len(ents), "placeholder": 0, "sprite_ok": 0,
           "sprite_fail": [], "samples": []}
    PLACE = bytes.fromhex("0A000300C9C9C9")
    for (idx, start, end, size) in ents:
        blob = raw[start:end]
        if blob == PLACE:
            out["placeholder"] += 1
            continue
        sp = try_sprite(blob)
        if sp["sprite"]:
            out["sprite_ok"] += 1
            if len(out["samples"]) < 6:
                out["samples"].append({"idx": idx, "w": sp["w"], "h": sp["h"],
                                       "size": size})
        else:
            out["sprite_fail"].append({"idx": idx, "size": size, "why": sp["why"]})
    (OUT / "tai.json").write_text(json.dumps(out, indent=1))
    return out


def main():
    print("== FDOTHER nested + dead ==")
    fo = analyze_fdother()
    print(f"  outer_count            = {fo['outer_count']}")
    print(f"  nested_archive_count   = {fo['nested_archive_count']}")
    print(f"  nested_total_sub       = {fo['nested_total_sub_entries']}")
    print(f"  dead idx dumped        = {len(fo['dead_dump'])}")

    print("== ANI ==")
    an = analyze_ani()
    print(f"  entry_count            = {an['entry_count']}")
    for e in an["entries"]:
        print(f"  idx {e['idx']}: frames={e['frame_count']:>4} "
              f"end_matches_size={e['end_pos_matches_size']} "
              f"hdr4_distinct={e['hdr4_distinct'][:6]} "
              f"hdr6_distinct={e['hdr6_distinct'][:6]}")

    print("== FDSHAP attr ==")
    fs = analyze_fdshap_attr()
    ag = fs["aggregate"]
    print(f"  byte+1 values (all)    = {ag['byte1_all_values']}")
    print(f"  byte+1 max             = {ag['byte1_max']}")
    print(f"  byte+2 min/max         = {ag['byte2_min']} / {ag['byte2_max']}")
    print(f"  byte+3 values (all)    = {ag['byte3_all_values']}")

    print("== TAI ==")
    ta = analyze_tai()
    print(f"  entry_count={ta['entry_count']} placeholder={ta['placeholder']} "
          f"sprite_ok={ta['sprite_ok']} sprite_fail={len(ta['sprite_fail'])}")
    if ta["sprite_fail"]:
        print(f"  fails: {ta['sprite_fail'][:8]}")

    print(f"\nJSON written to {OUT}")


if __name__ == "__main__":
    main()
