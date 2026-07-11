#!/usr/bin/env python3
"""make_pro_obj.py -- generate fd2cov.obj: the Watcom -ep profiling hook for FD2
coverage measurement.

`-ep` makes wcc386 emit `call __PRO` right after each function's
`push ebp; mov ebp,esp` prologue (verified by disassembly). On entry to __PRO,
[esp] holds the return address = (entered function entry + 8). __PRO is
register-safe (pushfd/pushad) and records that address as a set bit in
fd2cov_bitmap (bit index = addr>>2, 4-byte granularity). The bit set is
idempotent, so no dedup and no overflow.

Self-contained: this .obj defines BOTH __PRO (_TEXT) and fd2cov_bitmap (_BSS,
64 KB), the latter PUBDEF'd so the harness dump (tests/play/capture.c) can
fwrite it. The recorded addresses are RUNTIME (DOS4GW-relocated); offline they
are mapped back to functions via a reference symbol's runtime address + the
build .map. No src/ change: -ep is a build flag; __PRO/bitmap live here.

Built via the project's OMF writer (tools/ail_extract/omf_writer.py).
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ail_extract"))
import omf_writer as O

COV_BITS = 0x80000                 # addr>>2 bit space; covers runtime addr < 0x200000
BITMAP_BYTES = COV_BITS // 8       # 0x10000 = 64 KB

# __PRO machine code (register-safe). byte offsets:
#  00 9C              pushfd
#  01 60              pushad
#  02 8B 44 24 24     mov  eax,[esp+0x24]      ; ret addr = entered-func entry + 8
#  06 C1 E8 02        shr  eax,2               ; -> bit index (4-byte granularity)
#  09 3D <COV_BITS>   cmp  eax,COV_BITS        ; bounds check
#  0E 73 07           jae  L1                  ; skip if out of range
#  10 0F AB 05 <d32>  bts  [fd2cov_bitmap],eax ; set bit (disp32 @0x13 = fixup)
#  17 61              popad                    ; L1:
#  18 9D              popfd
#  19 C3              ret
CODE = (
    bytes([0x9C, 0x60, 0x8B, 0x44, 0x24, 0x24, 0xC1, 0xE8, 0x02, 0x3D])
    + COV_BITS.to_bytes(4, "little")
    + bytes([0x73, 0x07, 0x0F, 0xAB, 0x05, 0, 0, 0, 0, 0x61, 0x9D, 0xC3])
)
FIXUP_OFF = 0x13
assert len(CODE) == 0x1A, len(CODE)
assert CODE[0x10:0x13] == bytes([0x0F, 0xAB, 0x05])    # bts opcode + ModRM
assert CODE[FIXUP_OFF:FIXUP_OFF + 4] == bytes(4)        # disp32 placeholder


def build():
    m = O.OmfModule("fd2cov", comment_tag=None)
    text = m.add_segment("_TEXT", "CODE", length=len(CODE))     # seg 1
    bss = m.add_segment("_BSS", "BSS", length=BITMAP_BYTES)     # seg 2
    dgrp = m.add_group("DGROUP", [bss])                         # group 1
    m.add_pubdef(text, "__PRO", 0)
    m.add_pubdef(bss, "fd2cov_bitmap", 0)
    fx = O.fixupp_subrecord(
        self_relative=False, location=9, data_record_offset=FIXUP_OFF,
        frame_method=1, frame_index=dgrp,                      # FRAME = DGROUP
        target_method=0, target_index=bss, target_displacement=0,  # TARGET = _BSS:0
    )
    m.add_ledata(text, 0, CODE, [fx])
    return m.build()


def main():
    out = ROOT / "workspace" / "fd2_diff" / "fd2cov.obj"
    out.parent.mkdir(parents=True, exist_ok=True)
    data = build()
    out.write_bytes(data)
    print("wrote %s (%d bytes); COV_BITS=0x%x bitmap=%dKB"
          % (out, len(data), COV_BITS, BITMAP_BYTES // 1024))


if __name__ == "__main__":
    main()
