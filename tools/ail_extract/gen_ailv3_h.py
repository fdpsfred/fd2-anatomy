#!/usr/bin/env python3
"""A6 — generate ailv3.h from ail_inventory.json.

Pulls every `AIL_*` function that is **not** `AIL_internal_*` (public API
surface only) and emits a Watcom 9.5a-compatible client header containing:

  - Prototype per public fn with `__cdecl` / `__watcall` cc keyword
  - Per-fn `#pragma aux <name> "*";` so wcc386 keeps the symbol byte-identical
    to the PUBDEF in `ailv3.lib` (Watcom otherwise wraps `__cdecl` names with
    underscore prefix/suffix, breaking link resolution)
  - Header preamble + extern "C" guards

Opaque-handle typedefs (`HSAMPLE` / `HDIGDRIVER` / ...) and callback typedefs
are NOT emitted here — Ghidra signatures presently expose handles as raw
`int` / `void *`. Type abstraction PENDING a Phase C signature audit that
maps each handle-using API to a vendor SDK typedef (per
[[feedback_no_unverified_claim]]: don't invent types without evidence).

Signature transforms applied:

  - `undefined4` → `int`     (Watcom 32-bit; observed in 2 public sigs)
  - `undefined2` → `short`
  - `undefined1` → `char`
  - `uint`       → `unsigned int`
  - `ushort`     → `unsigned short`
  - `uchar`      → `unsigned char`

Other types (`int`, `short`, `char`, `void`, `void *`) pass through.

Output: workspace/ail_extract/out/ailv3.h
"""
from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
INVENTORY = REPO / "workspace" / "ail_extract" / "ail_inventory.json"
OUT_PATH = REPO / "workspace" / "ail_extract" / "out" / "ailv3.h"

TYPE_SUBSTITUTIONS = [
    (re.compile(r"\bundefined4\b"), "int"),
    (re.compile(r"\bundefined2\b"), "short"),
    (re.compile(r"\bundefined1\b"), "char"),
    (re.compile(r"\buint\b"), "unsigned int"),
    (re.compile(r"\bushort\b"), "unsigned short"),
    (re.compile(r"\buchar\b"), "unsigned char"),
]

HANDLE_RETURN_TYPE: dict[str, str] = {
    "AIL_install_DIG_INI": "HDIGDRIVER",
    "AIL_install_DIG_driver_file": "HDIGDRIVER",
    "AIL_install_DIG_driver_image": "HDIGDRIVER",
    "AIL_install_MDI_INI": "HMDIDRIVER",
    "AIL_install_MDI_driver_file": "HMDIDRIVER",
    "AIL_install_MDI_driver_image": "HMDIDRIVER",
    "AIL_allocate_sample_handle": "HSAMPLE",
    "AIL_allocate_file_sample": "HSAMPLE",
    "AIL_allocate_sequence_handle": "HSEQUENCE",
    "AIL_register_timer": "HTIMER",
    "AIL_install_driver": "HDRIVER",
    "AIL_create_wave_synthesizer": "HWAVESYNTH",
    "AIL_get_IO_environment": "HDRIVER",
}

HANDLE_PARAM_NAMES: dict[str, str] = {
    "dig_driver": "HDIGDRIVER",
    "mdi_driver": "HMDIDRIVER",
    "sample": "HSAMPLE",
    "sequence": "HSEQUENCE",
    "driver_handle": "HDRIVER",
    "timer_handle": "HTIMER",
}

# Per-function argument-list overrides where Ghidra mis-typed a handle as a
# raw int with a generic param_N name, so the HANDLE_PARAM_NAMES mechanism
# cannot reach it. AIL_set_sample_address's first arg is the HSAMPLE handle:
# the worker at 0x41250 dereferences it (MOV [handle+8],start; MOV
# [handle+0x10],len), it is not an int. Leaving it int makes a -3s client that
# passes a void* HSAMPLE trip W113 (pointer type mismatch).
SIGNATURE_OVERRIDE: dict[str, str] = {
    "AIL_set_sample_address": "HSAMPLE sample, unsigned int start, unsigned int len",
    # xmi_data is a buffer pointer. FD2's resource layer (fd2_load_dat_resource)
    # represents all loaded-buffer pointers as a 32-bit value (uint32), so the
    # client passes one here; declaring it unsigned int (not void*) matches that
    # convention and avoids W113 without rippling void* through the whole
    # resource layer. Handles stay void* (opaque, AIL-only); data buffers are
    # the game's uint32 -- that split is intentional.
    "AIL_init_sequence": "HSEQUENCE sequence, unsigned int xmi_data, int sequence_idx",
}


def transform_signature(sig: str) -> str:
    out = sig
    for pat, repl in TYPE_SUBSTITUTIONS:
        out = pat.sub(repl, out)
    return out


def split_signature(sig: str, fn_name: str):
    """Split `<return_type> <fn_name>(<args>)` into (return_type, args_text).

    Handles return types containing spaces / `*` (e.g. `void *`).
    """
    idx = sig.find(fn_name)
    if idx < 0:
        raise ValueError(f"fn name {fn_name!r} not in signature {sig!r}")
    ret = sig[:idx].rstrip()
    rest = sig[idx + len(fn_name):]
    m = re.match(r"\s*\((.*)\)\s*$", rest)
    if not m:
        raise ValueError(f"could not parse arg list from {rest!r}")
    args = m.group(1).strip()
    return ret, args


def apply_handle_types(fn_name: str, ret: str, args: str) -> tuple[str, str]:
    """Replace raw int/void* with handle typedefs where applicable."""
    if fn_name in HANDLE_RETURN_TYPE:
        ret = HANDLE_RETURN_TYPE[fn_name]
    # Replace parameter types by matching "type name" patterns
    if args and args != "void":
        parts = [p.strip() for p in args.split(",")]
        new_parts = []
        for part in parts:
            tokens = part.rsplit(None, 1)
            if len(tokens) == 2:
                ptype, pname = tokens
                pname_bare = pname.lstrip("*").strip()
                if pname_bare in HANDLE_PARAM_NAMES:
                    new_parts.append(f"{HANDLE_PARAM_NAMES[pname_bare]} {pname_bare}")
                else:
                    new_parts.append(part)
            else:
                new_parts.append(part)
        args = ", ".join(new_parts)
    return ret, args


def emit_prototype(fn: dict) -> str:
    sig = transform_signature(fn["signature"])
    ret, args = split_signature(sig, fn["name"])
    ret, args = apply_handle_types(fn["name"], ret, args)
    if fn["name"] in SIGNATURE_OVERRIDE:
        args = SIGNATURE_OVERRIDE[fn["name"]]
    cc = fn.get("cc") or "__cdecl"
    if cc == "__cdecl":
        return f"extern {ret} __cdecl {fn['name']}({args});"
    else:
        return f"extern {ret} {fn['name']}({args});"


HEADER = """\
/* ailv3.h - Miles AIL Audio Interface Library v3 (rebuilt from FD2.LE)
 * Target  : Watcom C/C++ 9.5a, DOS/4G LE 32-bit flat
 * Source  : auto-generated by tools/ail_extract/gen_ailv3_h.py from
 *           workspace/ail_extract/ail_inventory.json -- DO NOT EDIT.
 *
 * Signature notes
 *   - `__cdecl` / `__watcall` keyword follows Ghidra audit of each function.
 *   - `#pragma aux <name> "*"` keeps PUBDEF symbol byte-identical with
 *     `ailv3.lib`. `modify [eax ebx ecx edx]` declares the vendor clobber set
 *     (EBX is clobbered without a PUSH; EAX/ECX/EDX are ordinary volatiles).
 *     The full list is required for -3s clients; a bare [ebx] is unsafe there.
 *   - `param_N` parameter names are Ghidra defaults where the original
 *     C names have not been recovered; clients pass arguments by position.
 */
#ifndef AILV3_H
#define AILV3_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle typedefs — Miles SDK convention. Internally these are
   pointers to vendor-allocated structs (or small integers for timers).
   In Watcom 32-bit flat model int and void* are both 32-bit, so the
   underlying representation is interchangeable. */
typedef void *       HDIGDRIVER;    /* DIG driver handle (from AIL_install_DIG_*) */
typedef void *       HMDIDRIVER;    /* MDI driver handle (from AIL_install_MDI_*) */
typedef void *       HSAMPLE;       /* sample playback handle */
typedef void *       HSEQUENCE;     /* XMIDI sequence handle */
typedef unsigned int HTIMER;        /* timer slot handle */
typedef void *       HDRIVER;       /* generic driver handle (from AIL_install_driver) */
typedef void *       HWAVESYNTH;    /* wave synthesizer handle */

"""

FOOTER = """\

#ifdef __cplusplus
}
#endif

#endif /* AILV3_H */
"""


def main():
    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
    pub = [
        f for f in inv["functions"]
        if f["name"].startswith("AIL_") and not f["name"].startswith("AIL_internal_")
    ]
    pub.sort(key=lambda f: f["name"])

    lines = [HEADER]
    for fn in pub:
        proto = emit_prototype(fn)
        lines.append(proto)
        # `"*"` keeps the PUBDEF name byte-identical with the lib.
        # The modify set lists ALL four caller-saved registers (eax ebx ecx
        # edx), not just ebx. AIL helpers clobber EBX without restoring it
        # (vendor optimiser stripped the PUSH EBX), and being ordinary calls
        # they also clobber the volatile EAX/ECX/EDX. Watcom reads the modify
        # set as EXACT: a bare "modify [ebx]" would mark EAX/ECX/EDX preserved,
        # which is only harmless under -3r (where they are arg-volatile anyway)
        # but corrupts a -3s client (it then keeps a live value in EDX/ECX
        # across the call). Listing all four is correct under both -3r and -3s.
        lines.append(f'#pragma aux {fn["name"]} "*" modify [eax ebx ecx edx];')
        lines.append("")
    lines.append(FOOTER)

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")

    cc_counts = {
        "__cdecl": sum(1 for f in pub if f.get("cc") == "__cdecl"),
        "__watcall": sum(1 for f in pub if f.get("cc") == "__watcall"),
    }
    print(f"emitted {len(pub)} public AIL_* prototypes -> {OUT_PATH}")
    print(f"  __cdecl  : {cc_counts['__cdecl']}")
    print(f"  __watcall: {cc_counts['__watcall']}")
    print(f"  size     : {OUT_PATH.stat().st_size} bytes")


if __name__ == "__main__":
    main()
