#!/usr/bin/env python3
"""gen_scenario.py -- synthesize a starting FD2.SAV that jumps to a chapter.

Ports the game's two save codecs (src/save/save.c, verified by a round-trip
self-test against the stock save):
  - crypt    : XOR involution, state=0xA5, per byte state=ROL16(state+0x9014,3),
               byte ^= state & 0xFF  (fd2_save_crypt_buffer @ 0x4dbd8)
  - checksum : u32 sum of bytes[0 .. len-5]  (fd2_save_compute_checksum @ 0x4dbb9)

A jump save = the stock save decrypted, chapter_id byte (+0x30C5) set to the
target, checksum recomputed (+0x59C7), re-encrypted. CONTINUE then quick-loads
chapter N (its FDFIELD map + the stock party roster) -> lets each chapter's
init/render be exercised without playing through. Party positions come from the
stock save, so this is for init/render/loader coverage (P2), not for asserting a
correct per-chapter roster (that needs the play-through mother-chain).

Usage:
  python tools/fd2_play/gen_scenario.py --validate
  python tools/fd2_play/gen_scenario.py --chapter 4 --sav-out workspace/fd2_play/sav/ch05.sav
"""
import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SAV_SIZE = 0x59CB
OFF_CHAPTER = 0x30C5
OFF_CKSUM = 0x59C7
# CONTINUE slot array (see resource_info/save_format.md): 4 slots of 0xA28
# bytes at +0x312B; each slot's chapter byte sits at slot+0xA00. The main-menu
# CONTINUE path (fd2_main_menu_dispatcher) reads chapter/roster from a SLOT and
# then runs the chapter INIT handler, whereas the quick-load path reads
# OFF_CHAPTER from the main body and resumes the saved battle without init.
OFF_SLOTS = 0x312B
SLOT_SIZE = 0xA28
SLOT_CHAPTER = 0xA00


def resolve_stock_sav():
    import os
    env = os.environ.get("FD2_GAME_DIR")
    for cand in ([Path(env)] if env else []) + [ROOT / "fd2_game_files",
                 Path(r"C:\Users\fdpsf\Documents\fd2-anatomy\fd2_game_files")]:
        p = cand / "FD2.SAV"
        if p.is_file():
            return p
    raise SystemExit("stock FD2.SAV not found")


def crypt(data):
    """XOR involution -- encrypt and decrypt are the same call."""
    out = bytearray(data)
    state = 0xA5
    for i in range(len(out)):
        tmp = (state + 0x9014) & 0xFFFF
        state = ((tmp << 3) | (tmp >> 13)) & 0xFFFF
        out[i] ^= state & 0xFF
    return bytes(out)


def checksum(plain):
    """u32 sum of bytes[0 .. len-5] (excludes the 4 trailing checksum bytes)."""
    return sum(plain[:len(plain) - 4]) & 0xFFFFFFFF


def validate(stock_enc):
    if len(stock_enc) != SAV_SIZE:
        return False, "stock save size %d != %d" % (len(stock_enc), SAV_SIZE)
    plain = crypt(stock_enc)
    stored = int.from_bytes(plain[OFF_CKSUM:OFF_CKSUM + 4], "little")
    calc = checksum(plain)
    if stored != calc:
        return False, "checksum mismatch: stored %#x calc %#x" % (stored, calc)
    if crypt(plain) != stock_enc:
        return False, "crypt is not an involution (re-encrypt != original)"
    return True, "chapter_id=%d checksum=%#x (involution+checksum OK)" % (
        plain[OFF_CHAPTER], stored)


def make_jump(stock_enc, chapter):
    """Patch BOTH chapter bytes: the main body one (quick-load path resumes
    the saved battle at chapter N) and CONTINUE slot 0's (CONTINUE path runs
    chapter N's init handler with slot 0's roster)."""
    plain = bytearray(crypt(stock_enc))
    plain[OFF_CHAPTER] = chapter & 0xFF
    plain[OFF_SLOTS + SLOT_CHAPTER] = chapter & 0xFF
    cs = checksum(plain)
    plain[OFF_CKSUM:OFF_CKSUM + 4] = cs.to_bytes(4, "little")
    return crypt(bytes(plain))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--validate", action="store_true")
    ap.add_argument("--chapter", type=int, default=-1,
                    help="0-based chapter_id to jump to (0 = chapter 1)")
    ap.add_argument("--sav-out", default="")
    args = ap.parse_args()

    stock = resolve_stock_sav().read_bytes()

    if args.validate:
        ok, msg = validate(stock)
        print(("OK  " if ok else "FAIL ") + msg)
        return 0 if ok else 1

    if args.chapter < 0 or not args.sav_out:
        raise SystemExit("need --chapter N --sav-out PATH (or --validate)")
    ok, msg = validate(stock)
    if not ok:
        raise SystemExit("stock save failed validation, refusing: " + msg)
    out = make_jump(stock, args.chapter)
    # self-check the synthesized save decrypts to the requested chapter + valid cksum
    chk = crypt(out)
    assert chk[OFF_CHAPTER] == (args.chapter & 0xFF)
    assert int.from_bytes(chk[OFF_CKSUM:OFF_CKSUM + 4], "little") == checksum(chk)
    sp = Path(args.sav_out)
    if not sp.is_absolute():
        sp = ROOT / args.sav_out
    sp.parent.mkdir(parents=True, exist_ok=True)
    sp.write_bytes(out)
    print("wrote %s (chapter_id=%d, %d bytes)" % (sp, args.chapter, len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
