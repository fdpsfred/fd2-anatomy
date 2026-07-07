"""Generate STKDIAG.OBJ -- diagnostic replacement for CLIB3S's stk module.

Defines __CHK / __GRO / __STK / __STKOVERFLOW with the same semantics as
CLIB3S(stk), except the overflow path first prints
  PROBE=xxxxxxxx ESP=xxxxxxxx
(xxxxxxxx = return address into the probed function's prologue, i.e.
function_start+10, and the ESP at check time) before the standard
"Stack Overflow!" message and exit(1).  Linking this obj as a `file`
before the CRT libraries shadows CLIB3S(stk) entirely.

The SS-escape of the CLIB checker is dropped: in the DOS/4GW flat model the
app / AIL-ISR SS selector is identical, so the escape can never fire in FD2;
behavior on the abort path is unchanged, we only add the PROBE print.

Output: workspace/fd2_build/exe/out/obj/STKDIAG.OBJ
"""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "ail_extract"))
from omf_writer import OmfModule, fixupp_subrecord

OUT_OBJ = REPO / "workspace" / "fd2_build" / "exe" / "out" / "obj" / "STKDIAG.OBJ"

MSG = b"PROBE=XXXXXXXX ESP=XXXXXXXX\r\nStack Overflow!\r\n"
assert len(MSG) == 46
DIG1_OFF = 6          # first XXXXXXXX
DIG2_OFF = 19         # second XXXXXXXX

text = bytearray()

def emit(*bs):
    text.extend(bytes(bs))

# 0x00 __CHK: xchg eax,[esp+4]; call __STK; mov eax,[esp+4]; ret 4
emit(0x87, 0x44, 0x24, 0x04)
emit(0xE8, 0x0A, 0x00, 0x00, 0x00)      # call __STK @0x13 (next ip 0x09)
emit(0x8B, 0x44, 0x24, 0x04)
emit(0xC2, 0x04, 0x00)
# 0x10 __GRO: ret 4
emit(0xC2, 0x04, 0x00)
# 0x13 __STK:
emit(0x3B, 0xC4)                        # cmp eax,esp
emit(0x73, 0x0D)                        # jae __STKOVERFLOW (0x24)
emit(0x2B, 0xC4)                        # sub eax,esp
emit(0xF7, 0xD8)                        # neg eax
FIX_STACKLOW = len(text) + 2            # disp32 at 0x1D
emit(0x3B, 0x05, 0x00, 0x00, 0x00, 0x00)  # cmp eax,[_STACKLOW]
emit(0x76, 0x01)                        # jbe __STKOVERFLOW (0x24)
emit(0xC3)                              # ret
# 0x24 __STKOVERFLOW:
assert len(text) == 0x24
emit(0x8B, 0x44, 0x24, 0x04)            # mov eax,[esp+4]  (ret addr in probed fn)
FIX_DIG1 = len(text) + 1
emit(0xBF, 0x00, 0x00, 0x00, 0x00)      # mov edi,offset MSG+6
emit(0xE8, 0x26, 0x00, 0x00, 0x00)      # call hex8 (0x58; next ip 0x32)
emit(0x89, 0xE0)                        # mov eax,esp
FIX_DIG2 = len(text) + 1
emit(0xBF, 0x00, 0x00, 0x00, 0x00)      # mov edi,offset MSG+19
emit(0xE8, 0x1A, 0x00, 0x00, 0x00)      # call hex8 (next ip 0x3E)
FIX_MSG = len(text) + 1
emit(0xBA, 0x00, 0x00, 0x00, 0x00)      # mov edx,offset MSG
emit(0xB9, len(MSG), 0x00, 0x00, 0x00)  # mov ecx,len
emit(0xBB, 0x02, 0x00, 0x00, 0x00)      # mov ebx,2 (stderr)
emit(0xB4, 0x40)                        # mov ah,40h
emit(0xCD, 0x21)                        # int 21h
emit(0xB8, 0x01, 0x4C, 0x00, 0x00)      # mov eax,4C01h
emit(0xCD, 0x21)                        # int 21h
# 0x58 hex8: eax=value, edi=dest; clobbers ecx,dl
assert len(text) == 0x58
emit(0xB9, 0x08, 0x00, 0x00, 0x00)      # mov ecx,8
emit(0xC1, 0xC0, 0x04)                  # rol eax,4       (loop head 0x5D)
emit(0x8A, 0xD0)                        # mov dl,al
emit(0x80, 0xE2, 0x0F)                  # and dl,0Fh
emit(0x80, 0xC2, 0x30)                  # add dl,30h
emit(0x80, 0xFA, 0x39)                  # cmp dl,39h
emit(0x76, 0x03)                        # jbe +3
emit(0x80, 0xC2, 0x07)                  # add dl,7
emit(0x88, 0x17)                        # mov [edi],dl
emit(0x47)                              # inc edi
emit(0xE2, 0xE8)                        # loop 0x5D
emit(0xC3)                              # ret
assert len(text) == 0x76, hex(len(text))

mod = OmfModule("stkdiag")
text_idx = mod.add_segment("_TEXT", "CODE", length=len(text))
data_idx = mod.add_segment("_DATA", "DATA", length=len(MSG))
mod.add_group("DGROUP", [data_idx])
flat_idx = mod.add_group("FLAT", [])

mod.add_pubdef(text_idx, "__CHK", 0x00)
mod.add_pubdef(text_idx, "__GRO", 0x10)
mod.add_pubdef(text_idx, "__STK", 0x13)
mod.add_pubdef(text_idx, "__STKOVERFLOW", 0x24)

stacklow_idx = mod.add_extdef("_STACKLOW")

subs = [
    fixupp_subrecord(self_relative=False, location=9,
                     data_record_offset=FIX_STACKLOW,
                     frame_method=1, frame_index=flat_idx,
                     target_method=6, target_index=stacklow_idx),
]
for off, disp in ((FIX_DIG1, DIG1_OFF), (FIX_DIG2, DIG2_OFF), (FIX_MSG, 0)):
    subs.append(fixupp_subrecord(
        self_relative=False, location=9, data_record_offset=off,
        frame_method=1, frame_index=flat_idx,
        target_method=0, target_index=data_idx, target_displacement=disp))

mod.add_ledata(seg_idx=text_idx, offset=0, data=bytes(text), fixupp_subs=subs)
mod.add_ledata(seg_idx=data_idx, offset=0, data=MSG, fixupp_subs=[])

OUT_OBJ.write_bytes(mod.build())
print("wrote %s (%d bytes, text=0x%x)" % (OUT_OBJ, OUT_OBJ.stat().st_size, len(text)))
