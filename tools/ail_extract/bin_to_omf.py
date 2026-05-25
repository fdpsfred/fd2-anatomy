"""A5 — Emit OMF .obj for every AIL fn / data item / fd2common module.

Pipeline inputs:
  ail_inventory.json     fn + data list (from dump_ail_set.py)
  ail_layout.json        per-fn / data file offsets
  ail_obj_grouping.json  per-obj membership (per-item fallback only)
  ail_code.bin           concatenated fn body bytes
  ail_data.bin           concatenated data bytes
  ail_fixups_synth.jsonl rel32 cross-fn 5-path verdict
  ail_fixups_midfn.jsonl mid-fn alt-entry details
  workspace/data_audit/le_fixups.json   LE FIXUP records (whole binary)
  rebuild_info/crt/lookup_9.5a.json     CRT symbol map

Consolidated mode (--mode consolidated, production):
  Emits 2 AIL .obj + 7 fd2common .obj:
    ail_code.obj          single _TEXT with all 428 fn
    ail_data.obj          single _DATA with all data items (compact, vendor-order)
    fd2common_*.obj       7 fd2common helpers (lock+unlock merged)

  ISR private stack safety is handled by Ghidra data item
  `data_ail_isr_private_stack_gap_8b` at vendor 0x535f4 (8B between
  use16_isr_temp_stack_buffer and saved_caller_ss). No external padding
  mechanism needed — the gap item is in the inventory and placed at its
  vendor position in the compact layout.

Per-item mode (--mode per-item, fallback):
  Emits per-fn .obj + per-data .obj + MERGE_GROUPS for vendor-overflow.

Fixup routing:
  • Consolidated intra-AIL rel32: literal disp32 (no FIXUPP)
  • Cross-module rel32 / LE FIXUP: EXTDEF + FIXUPP32
    (M=0/1, LOC=9, F=1 GRPDEF FLAT, P=1, TARGT=6 EXTDEF)
  • Mid-fn target: PUBDEF L_<fn>_alt_<offset> in target .obj

Per [[feedback_no_kb_hardcoded_values]]: fd2common / crt_equivalent function
sets are taken from a live Ghidra search, never from KB hardcoded addresses.
"""
from __future__ import annotations

import json
import sys
from collections import defaultdict
from pathlib import Path

# Use omf_writer.py in the same directory
sys.path.insert(0, str(Path(__file__).parent))
from omf_writer import (
    OmfModule, fixupp_subrecord, REC_THEADR, REC_COMENT, REC_MODEND32,
    REC_EXTDEF, REC_PUBDEF32, REC_LNAMES, REC_SEGDEF32, REC_GRPDEF,
    REC_FIXUPP32, REC_LEDATA32,
)

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
OBJ_DIR = WS / "objs"

# Inputs
INVENTORY = WS / "ail_inventory.json"
LAYOUT = WS / "ail_layout.json"
GROUPING = WS / "ail_obj_grouping.json"
CODE_BIN = WS / "ail_code.bin"
DATA_BIN = WS / "ail_data.bin"
TOOLS_DIR = Path(__file__).parent
SYNTH = TOOLS_DIR / "ail_fixups_synth.jsonl"
MIDFN = TOOLS_DIR / "ail_fixups_midfn.jsonl"
LE_FIXUPS = REPO / "workspace" / "data_audit" / "le_fixups.json"
CRT_LOOKUP = REPO / "rebuild_info" / "crt" / "lookup_9.5a.json"


def hex_int(s):
    return int(s, 16)


# --------------------------------------------------------------------- helpers

def load_jsonl(path: Path) -> list:
    return [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]


class BinToOmfBuilder:
    def __init__(self):
        self.inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
        self.grouping = json.loads(GROUPING.read_text(encoding="utf-8"))
        self.code_bin = CODE_BIN.read_bytes()
        self.data_bin = DATA_BIN.read_bytes()
        self.synth = load_jsonl(SYNTH)
        self.midfn = load_jsonl(MIDFN)
        self.le_fixups_raw = json.loads(LE_FIXUPS.read_text(encoding="utf-8"))
        self.crt_lookup_raw = json.loads(CRT_LOOKUP.read_text(encoding="utf-8"))

        self._build_indexes()

        # fd2common pool: 8 fns (per rebuild_info/crt/symbol_inventory.md)
        # 6 fd2_dpmi_* + crt_equivalent_get_eflags + crt_equivalent_get_eflags_thunk.
        # We read these from a live Ghidra-derived snapshot file under audit/
        # rather than KB-hardcoding the addresses (per
        # [[feedback_no_kb_hardcoded_values]]).
        snap = json.loads(
            (WS / "audit" / "ghidra_pool_snapshot.json").read_text(encoding="utf-8")
        )
        self.fd2common_by_addr = {}  # addr (linear int) -> (name, body_min, body_max, body_size, sig)
        for f in snap:
            if f["name"].startswith("fd2_dpmi_") or \
               f["name"] in ("crt_equivalent_get_eflags",
                             "crt_equivalent_get_eflags_thunk"):
                self.fd2common_by_addr[int(f["entry"], 16)] = f
        # Cross-check: 8 fns
        if len(self.fd2common_by_addr) != 8:
            raise SystemExit(
                f"FAIL: fd2common pool expected 8 fns, got {len(self.fd2common_by_addr)}: "
                f"{sorted(f['name'] for f in self.fd2common_by_addr.values())}"
            )
        # fd2common merge groups: fd2_dpmi_unlock_region contains a short
        # JMP (EB rel8) into fd2_dpmi_lock_region's shared tail. Short JMPs
        # have no OMF FIXUPP — the 1-byte displacement only works when both
        # functions are in the same .obj at vendor-relative offsets. Detect
        # by checking if one fn's body_max+1 == the other fn's body_min
        # (contiguous vendor layout) AND the fn body contains EB xx.
        self.fd2common_merge_primary = {}   # primary_addr -> [secondary_info, ...]
        self.fd2common_merge_secondary = set()  # addrs to skip in individual emit
        fd2c_by_name = {f["name"]: f for f in self.fd2common_by_addr.values()}
        lock_fn = fd2c_by_name.get("fd2_dpmi_lock_region")
        unlock_fn = fd2c_by_name.get("fd2_dpmi_unlock_region")
        if lock_fn and unlock_fn:
            lock_end = int(lock_fn["body_max"], 16) + 1
            unlock_start = int(unlock_fn["body_min"], 16)
            if lock_end == unlock_start:
                primary_addr = int(lock_fn["entry"], 16)
                secondary_addr = int(unlock_fn["entry"], 16)
                self.fd2common_merge_primary[primary_addr] = [unlock_fn]
                self.fd2common_merge_secondary.add(secondary_addr)

        # CRT data symbols: addr → Ghidra `data_crt_*` name. Dumped live by
        # the inline script that produced raw/crt_data_symbols.json. Used as
        # an EXTDEF fallback for LE FIXUP targets that fall on a CRT data
        # symbol (e.g. `data_crt_environ` = Watcom CLIB3S `_environ`).
        crt_data_path = WS / "raw" / "crt_data_symbols.json"
        self.crt_data_by_addr = {}
        if crt_data_path.exists():
            for rec in json.loads(crt_data_path.read_text(encoding="utf-8")):
                self.crt_data_by_addr[hex_int(rec["addr"])] = rec["name"]

        # ISR private stack gap is now a proper Ghidra data item
        # (data_ail_isr_private_stack_gap_8b @ 0x535f4, 8 bytes) in the
        # inventory. Consolidated mode preserves it naturally at its vendor
        # position between use16_isr_temp_stack_buffer and saved_caller_ss.
        # Per-item mode uses MERGE_GROUPS to keep it adjacent.
        # stack_pad.json is no longer needed.

        # Detect overlapping size-0 items — these are mid-data alt-labels of
        # a parent item (e.g., ail_mix_pitch_int @ 0x538b8 is at offset 4
        # within data_ail_mix_pitch_low @ 0x538b4 size=12). They MUST emit
        # as PUBDEFs WITHIN the parent's .obj at the correct offset, not as
        # standalone .obj (would resolve to wrong location at link time).
        # Map: parent_addr -> [(child_offset, child_name, child_size_bytes), ...]
        # child_size_bytes>0 → sized merge (vendor-overflow design pattern);
        # =0 → label-only.
        self.subsumed_by_parent = defaultdict(list)
        # Map: child_addr -> True if child is subsumed (skip own .obj emit)
        self.subsumed_children = set()
        sorted_items = sorted(self.inv["data_items"],
                              key=lambda d: (hex_int(d["addr"]), -d["size"]))
        # For each item with size > 0, find size-0 items inside its range.
        for parent in sorted_items:
            if parent["size"] <= 0:
                continue
            p_addr = hex_int(parent["addr"])
            p_end = p_addr + parent["size"]
            for child in sorted_items:
                if child is parent:
                    continue
                if child["size"] != 0:
                    continue
                c_addr = hex_int(child["addr"])
                if p_addr <= c_addr < p_end:
                    self.subsumed_by_parent[parent["addr"]].append(
                        (c_addr - p_addr, child["name"], 0)
                    )
                    self.subsumed_children.add(child["addr"])

        # Vendor-overflow merge groups: master_isr's main loop INCs pending
        # counter slots indexed by EDI=0..0x3c (16 iterations). The array
        # `data_ail_timer_slot_pending_trigger_count_16` (vendor 0x52b54)
        # is only 15 entries (60 bytes); slot 15's INC overflows by +60 to
        # vendor 0x52b90 = `data_ail_isr_nested_pending_count` — a deliberate
        # vendor layout trick where the array's 16th slot IS the nested
        # counter. master_isr's exit reads [0x52b90] to decide RETF chain vs
        # direct IRETD.
        #
        # In per-item arch, wlink scatters the two items, breaking the
        # overflow assumption. The INC then lands on whichever .obj wlink
        # placed at pending_count + 60 (observed: data_ail_timer_slot_state_
        # array_16, corrupting slot 0's state -> phantom dispatch -> crash).
        #
        # Fix: emit nested_pending as a sized child PUBDEF embedded in the
        # pending_count .obj at offset 60, so the merged .obj contributes
        # 64 contiguous bytes to seg2 and slot 15's INC lands on nested_pending
        # (vendor-equivalent behavior).
        MERGE_GROUPS = [
            ("00052b54", "00052b90", "data_ail_isr_nested_pending_count", 60, 4),
            ("000535f4", "000535fc", "data_ail_isr_saved_caller_ss", 8, 2),
        ]
        addr_to_item = {d["addr"]: d for d in self.inv["data_items"]}
        for parent_addr, child_addr, child_name_expected, child_off, child_size in MERGE_GROUPS:
            child = addr_to_item.get(child_addr)
            if child is None:
                continue  # child not present; skip merge
            if child["name"] != child_name_expected:
                raise SystemExit(
                    f"FAIL: MERGE_GROUP child name mismatch at {child_addr}: "
                    f"expected '{child_name_expected}' got '{child['name']}'"
                )
            if child["size"] != child_size:
                raise SystemExit(
                    f"FAIL: MERGE_GROUP child size mismatch at {child_addr}: "
                    f"expected {child_size} got {child['size']}"
                )
            self.subsumed_by_parent[parent_addr].append(
                (child_off, child["name"], child_size)
            )
            self.subsumed_children.add(child_addr)

        # fd2common rel32 sites (Ghidra-derived, instruction-aligned).
        # Naive byte-scan would mis-trigger on `C1 E8 <imm8>` (SHR with imm8)
        # and similar 2nd-byte-is-E8 patterns. Use the inline-dumped list
        # at raw/fd2common_rel32_sites.json instead.
        self.fd2common_rel32_by_fn = defaultdict(list)
        fd2c_sites = WS / "raw" / "fd2common_rel32_sites.json"
        if fd2c_sites.exists():
            for rec in json.loads(fd2c_sites.read_text(encoding="utf-8")):
                self.fd2common_rel32_by_fn[rec["parent_fn"]].append(rec)

        # Mid-data alt-offset labels: walk LE FIXUP records once, find every
        # target that falls strictly inside an AIL data item body (offset > 0).
        # Result: {data_addr_str: set((offset_int, label_name))}
        self.alt_data_per_item = defaultdict(set)
        for sa_int, rec in self.le_by_src.items():
            tgt = hex_int(rec["target_addr"])
            self._record_mid_data(tgt)
        # Also walk rel32 targets (rare for data, but cover the case).
        for s in self.synth:
            tgt = hex_int(s["target_addr"])
            self._record_mid_data(tgt)

        # Mid-fn alt-entry labels (CP-4 fix): unify the alt-entry set from
        # all four origin sources so target .obj PUBDEFs cover every EXTDEF
        # `resolve_le_target` / synth might emit on the caller side:
        #   (a) midfn.jsonl rel32 sites (already added by _build_indexes)
        #   (b) LE FIXUP records targeting mid-fn offsets
        #   (c) synth rel32 verdicts (any verdict whose target lands mid-fn)
        #   (d) fd2common-fn rel32 sites whose target is an AIL fn body mid
        # Without (b)-(d) the caller would EXTDEF `L_<fn>_alt_<off>` while
        # the target .obj has no matching PUBDEF -> wlink unresolved.
        for sa_int, rec in self.le_by_src.items():
            self._record_mid_fn(hex_int(rec["target_addr"]))
        for s in self.synth:
            self._record_mid_fn(hex_int(s["target_addr"]))
        for site_list in self.fd2common_rel32_by_fn.values():
            for site in site_list:
                self._record_mid_fn(hex_int(site["target_addr"]))

    def _record_mid_fn(self, tgt_addr: int):
        """If tgt_addr strictly inside an AIL fn body (offset > 0), record
        an alt-entry label on that fn (so its .obj emits a matching PUBDEF
        and the caller's EXTDEF resolves)."""
        for (mn, mx, name) in self.fn_ranges:
            if mn < tgt_addr <= mx:
                off = tgt_addr - mn
                label = f"L_{name}_alt_{off:x}"
                self.alts_per_fn[name].add((off, label))
                return
            if mn == tgt_addr:
                return  # exact entry: fn_by_entry path, not a mid-fn alt

    def _record_mid_data(self, tgt_addr: int):
        """If tgt_addr falls inside an AIL data item OR on its exact end
        (end-exclusive sentinel commonly used by fd2_dpmi_lock_region pairs),
        record an alt-offset label on that item."""
        for d in self.inv["data_items"]:
            d_addr = hex_int(d["addr"])
            d_size = d["size"]
            if d_size <= 0:
                continue
            if d_addr < tgt_addr <= d_addr + d_size:
                off = tgt_addr - d_addr
                label = f"L_{d['name']}_alt_{off:x}"
                self.alt_data_per_item[d["addr"]].add((off, label))
                return

    def _build_indexes(self):
        self.fn_by_entry = {hex_int(fn["entry"]): fn for fn in self.inv["functions"]}
        self.fn_by_name = {fn["name"]: fn for fn in self.inv["functions"]}
        self.data_by_addr = {hex_int(d["addr"]): d for d in self.inv["data_items"]}

        # Build address-range index: linear addr -> fn (for addr inside body)
        # Multi-range supported (init_runtime_defaults).
        self.fn_ranges = []
        for fn in self.inv["functions"]:
            for [rmin, rmax] in fn["body_ranges"]:
                self.fn_ranges.append((hex_int(rmin), hex_int(rmax), fn["name"]))
        self.fn_ranges.sort()

        # Per fn name → layout entry (with ail_code_off etc.)
        self.fn_layout_by_name = {c["name"]: c for c in self.layout["code"]}
        self.data_layout_by_addr = {d["addr"]: d for d in self.layout["data"]}
        self.bss_layout_by_addr = {b["addr"]: b for b in self.layout["bss"]}

        # synth indexed by parent_fn_name
        self.synth_by_parent = defaultdict(list)
        for s in self.synth:
            self.synth_by_parent[s["parent_fn"]].append(s)

        # midfn alt-entry PUBDEFs: target_fn_name → list of (offset_int, label_name)
        self.alts_per_fn = defaultdict(set)
        for m in self.midfn:
            offset_int = int(m["target_offset_from_entry"], 16)
            self.alts_per_fn[m["target_within_fn"]].add(
                (offset_int, m["target_label_name"])
            )

        # LE fixup: src_addr (int) → record
        self.le_by_src = {
            hex_int(sa): {**rec, "src_addr": sa}
            for sa, rec in self.le_fixups_raw["source_addr_to_target"].items()
        }

        # CRT lookup: by_address → {addr_str: symbol_name}
        self.crt_by_addr = {hex_int(a): rec for a, rec in
                            self.crt_lookup_raw.get("by_address", {}).items()}

        # ail_obj_grouping helpers
        self.fn_obj_by_name = {o["fn_name"]: o for o in self.grouping["fn_objs"]}
        self.shared_obj_by_addr = {o["data_addr"]: o for o in self.grouping["shared_objs"]}
        # Single-owner data item set per fn name
        self.single_owner_data_per_fn = defaultdict(list)
        for o in self.grouping["fn_objs"]:
            for d in o["owned_data"]:
                self.single_owner_data_per_fn[o["fn_name"]].append(d)

    # -- resolve LE fixup target → (kind, extdef_name, segment_offset) -------
    def resolve_le_target(self, target_addr: int):
        """Return (kind, extdef_name) for an LE fixup target_addr.

        kind ∈ {AIL_to_AIL, AIL_to_AIL_mid, AIL_data, AIL_data_mid,
                AIL_to_fd2common, AIL_to_CRT, AIL_to_CRT_data, unknown}

        For data target that's an AIL data item, the EXTDEF name is the
        data item's symbol name (shared data .obj will PUBDEF it).
        """
        # 1. AIL fn entry exact
        if target_addr in self.fn_by_entry:
            return "AIL_to_AIL", self.fn_by_entry[target_addr]["name"]
        # 2. AIL data exact
        if target_addr in self.data_by_addr:
            return "AIL_data", self.data_by_addr[target_addr]["name"]
        # 3. AIL fn body mid-fn
        for (mn, mx, name) in self.fn_ranges:
            if mn <= target_addr <= mx:
                off = target_addr - mn
                label = f"L_{name}_alt_{off:x}"
                return "AIL_to_AIL_mid", label
        # 4. AIL data mid-array or end-exclusive sentinel (lock_region pair
        #    passes addresses on the [start, end) boundary; end address is
        #    a valid alt label of the same data item).
        for d in self.inv["data_items"]:
            d_addr = hex_int(d["addr"])
            d_size = d["size"]
            if d_size > 0 and d_addr < target_addr <= d_addr + d_size:
                off = target_addr - d_addr
                label = f"L_{d['name']}_alt_{off:x}"
                return "AIL_data_mid", label
        # 5. fd2common fn entry
        if target_addr in self.fd2common_by_addr:
            return "AIL_to_fd2common", self.fd2common_by_addr[target_addr]["name"]
        # 6. CRT fn lookup
        if target_addr in self.crt_by_addr:
            return "AIL_to_CRT", self.crt_by_addr[target_addr].get("name") or \
                                 self.crt_by_addr[target_addr].get("symbol")
        # 7. CRT data extern (Watcom CLIB3S 9.5a PUBDEF for data symbols).
        # Heuristic: Ghidra symbol `data_crt_<X>` maps to Watcom PUBDEF `_<X>`
        # (Watcom convention). EXTDEF name resolution at wlink time. PENDING
        # VERIFY: confirm wlink resolves each CRT data extern; if not, list
        # them in open_issues and decide whether to emit a stub PUBDEF in
        # fd2common.lib for FD2-side rebuild.
        if target_addr in self.crt_data_by_addr:
            ghidra_name = self.crt_data_by_addr[target_addr]
            extern_name = "_" + ghidra_name[len("data_crt_"):]
            return "AIL_to_CRT_data", extern_name
        return "unknown", None

    # -- per-fn emit ----------------------------------------------------------
    def emit_fn_obj(self, fn_obj: dict) -> tuple[bytes, dict]:
        """Emit one fn .obj. Return (bytes, stats_dict)."""
        fn = self.fn_by_name[fn_obj["fn_name"]]
        fn_name = fn["name"]
        fn_entry = hex_int(fn["entry"])
        is_public = fn_obj["is_public"]
        layout = self.fn_layout_by_name[fn_name]

        mod = OmfModule(module_name=fn_obj["obj_name"])

        # --- Segments ---
        # AIL_CODE: total fn body size
        code_size = sum(r["size"] for r in layout["ranges"])
        code_seg_idx = mod.add_segment("_TEXT", "CODE", length=code_size)

        # AIL data is emitted in a separate consolidated .obj
        # (ail_data_all.obj) at vendor-mapped offsets. fn .objs only contain
        # code now — no owned_data PUBDEFs or LEDATAs. Cross-fn data
        # references go through EXTDEF/PUBDEF resolved by wlink at link time.
        #
        # Rationale: wlink places shared-data .objs in obj 2 in EXTDEF
        # resolution order, which is influenced by which client .obj
        # references each data symbol. When a client (test_aud.exe) does
        # NOT reference critical ISR globals like data_ail_isr_saved_
        # original_vector_selector, those globals end up placed adjacent
        # to AIL_internal_timer_isr_master's data segment. The master ISR
        # uses memory below data_ail_isr_saved_caller_ss as its private
        # stack (via `MOV ESP, &saved_caller_ss` at vendor 0x3e77e), and
        # callback stack pushes then corrupt the adjacent ISR globals →
        # IP=1 wild jump on cleanup when uninstall_timer_isr reads garbage.
        # Consolidating into one .obj makes obj 2 layout independent of
        # client EXTDEF order; the vendor's ~60-byte zero buffer below
        # saved_caller_ss is preserved via vendor-relative offsets.
        bss_seg_idx = None  # legacy alias, unused

        # GRPDEFs for Watcom dos4gw flat memory model. wlink emits a runtime
        # LE FIXUP relocation for every FIXUPP32 record whose frame references
        # the FLAT group; without the FLAT GRPDEF anchor the relocation is
        # link-time only and dos4gw never adds the object's load base, leaving
        # disp32 / imm32 as segment-relative offsets (= low DOS memory under
        # FLAT addressing). DGROUP first (idx 1, empty here — data is in
        # ail_data_all.obj), then FLAT (idx 2 — empty, matches wcc386 -mf
        # convention; we reference FLAT in every FIXUP).
        mod.add_group("DGROUP", [])
        flat_grp_idx = mod.add_group("FLAT", [])

        # --- Layout: linear -> AIL_CODE offset mapping ---
        # When the fn has multiple body ranges (body-fragmented via intra-fn
        # JMP, e.g. AIL_internal_init_runtime_defaults), all ranges share
        # AIL_CODE; their LEDATA32 offsets are computed cumulatively. Compute
        # this BEFORE PUBDEF emit because the fn entry's code-seg offset
        # depends on which range it falls in.
        range_starts = []  # (linear_start, linear_end_excl, code_offset_start)
        cumulative_off = 0
        for r in layout["ranges"]:
            r_lin = hex_int(r["linear_start"])
            range_starts.append((r_lin, r_lin + r["size"], cumulative_off))
            cumulative_off += r["size"]
        def linear_to_code_off(lin_addr):
            for (mn, mx_excl, base_off) in range_starts:
                if mn <= lin_addr < mx_excl:
                    return base_off + (lin_addr - mn)
            return None

        # --- PUBDEFs ---
        # 1. fn entry — PUBDEF for every fn so cross-fn callers can resolve
        # via EXTDEF at wlink time. Public AIL_* names also serve as the
        # client API surface; internal_ names are PUBDEF for in-lib link
        # only (gen_ailv3_h.py filters by name when emitting the client
        # header). `is_public` controls header visibility, not PUBDEF.
        # For body-fragmented fns where entry is in range[1+], use the actual
        # cumulative code-seg offset (not hardcoded 0). Hardcoding 0 lands
        # callers at range[0]'s start (typically an epilogue or other
        # non-entry block), skipping the real fn body entirely — observed as
        # AIL_internal_init_runtime_defaults's 18 set_preference calls
        # silently not running, leaving preferences[0..17] all zero.
        entry_code_off = linear_to_code_off(fn_entry)
        if entry_code_off is None:
            raise SystemExit(
                f"FAIL: fn entry 0x{fn_entry:x} ({fn_name}) not in any body range"
            )
        mod.add_pubdef(seg_idx=code_seg_idx, name=fn_name, offset=entry_code_off)

        # 2. mid-fn alt-entry labels in this fn's body (方法 B)
        for (off_int, label) in sorted(self.alts_per_fn.get(fn_name, set())):
            mod.add_pubdef(seg_idx=code_seg_idx, name=label, offset=off_int)

        # 3. owned data items: only PUBDEF if it's referenced externally
        # (shared-data are separate .obj; single-owner doesn't need PUBDEF
        # unless something cross-.obj references it. To keep simple: PUBDEF
        # every owned data so wlink can resolve from any direction.)
        # Build the per-segment LEDATA32 blocks below; track data item
        # offsets within each segment.
        # BSS items follow file-backed DATA items inside the unified AIL_DATA
        # segment (CP-4 fix: no separate AIL_BSS segment to avoid dos4gw
        # commit-chain hang).
        # Owned data items are emitted in ail_data_all.obj, not here.

        # --- Collect all code fixups as (seg_off_int, ext_name, self_rel) ---
        # (seg_off is offset within AIL_CODE segment, i.e. base_off + intra)
        fn_code_fixups = []
        for s in self.synth_by_parent.get(fn_name, []):
            inst_addr = hex_int(s["inst_addr"])
            disp_off = s["disp32_offset_in_inst"]
            disp_lin = inst_addr + disp_off
            disp_code_off = linear_to_code_off(disp_lin)
            if disp_code_off is None:
                raise SystemExit(
                    f"FAIL: rel32 site inst_addr={s['inst_addr']} parent_fn={fn_name} "
                    f"disp offset 0x{disp_lin:x} not in any range of fn body"
                )
            ext_name = s["extdef_target"]
            if not ext_name:
                raise SystemExit(
                    f"FAIL: rel32 site {s['inst_addr']} verdict={s['verdict']} "
                    f"has no extdef_target"
                )
            fn_code_fixups.append((disp_code_off, ext_name, True))

        for r in layout["ranges"]:
            r_lin_start = hex_int(r["linear_start"])
            r_lin_end_excl = hex_int(r["linear_end_excl"])
            base_off = next(b for (mn, mx_excl, b) in range_starts
                             if mn == r_lin_start)
            for sa_int, rec in self.le_by_src.items():
                if r_lin_start <= sa_int < r_lin_end_excl:
                    tgt_addr = hex_int(rec["target_addr"])
                    kind, ext_name = self.resolve_le_target(tgt_addr)
                    if ext_name is None:
                        raise SystemExit(
                            f"FAIL: LE FIXUP src=0x{sa_int:x} in {fn_name} "
                            f"target=0x{tgt_addr:x} unresolved"
                        )
                    src_code_off = base_off + (sa_int - r_lin_start)
                    fn_code_fixups.append((src_code_off, ext_name, False))

        fn_code_fixups.sort(key=lambda t: t[0])

        # --- Pre-scan for cross-range intra-fn E8/E9 disp32 ---
        # Body-fragmented fns (currently only AIL_internal_init_runtime_defaults
        # has num_ranges > 1) may have an E8/E9 instruction in one range whose
        # disp32 targets another range of the same fn. The original FD2.LE
        # disp32 is computed against the original linear addresses; in our
        # rebuild the ranges sit contiguously inside AIL_CODE instead of at
        # their original spread positions, so disp32 needs recomputing.
        # We patch the LEDATA bytes in-place (no FIXUP emit — both src and
        # target are in the same .obj segment, so a literal disp32 is the
        # right encoding). Bin-time disp32 zero-out (CP-4) only applies to
        # FIXUP positions; cross-range JMP disp32 must remain literal.
        cross_range_disp32_patches = {}  # code_seg_off -> new_disp32
        fixup_offsets = {seg_off for (seg_off, _, _) in fn_code_fixups}
        if len(layout["ranges"]) > 1:
            for r in layout["ranges"]:
                r_lin_start = hex_int(r["linear_start"])
                r_lin_end_excl = hex_int(r["linear_end_excl"])
                r_size = r["size"]
                r_code_off = hex_int(r["ail_code_off"])
                range_bytes = self.code_bin[r_code_off : r_code_off + r_size]
                base_off = next(b for (mn, mx_excl, b) in range_starts
                                 if mn == r_lin_start)
                i = 0
                while i <= r_size - 5:
                    opcode = range_bytes[i]
                    if opcode in (0xE8, 0xE9):
                        disp32 = int.from_bytes(
                            range_bytes[i+1:i+5], "little", signed=True)
                        inst_addr = r_lin_start + i
                        target_addr = inst_addr + 5 + disp32
                        tgt_code_off = linear_to_code_off(target_addr)
                        if (tgt_code_off is not None
                                and not (r_lin_start <= target_addr < r_lin_end_excl)):
                            inst_code_off = base_off + i
                            disp32_seg_off = inst_code_off + 1
                            if disp32_seg_off in fixup_offsets:
                                raise SystemExit(
                                    f"FAIL: cross-range disp32 at code_off "
                                    f"0x{disp32_seg_off:x} in {fn_name} "
                                    f"collides with FIXUP position"
                                )
                            new_disp32 = tgt_code_off - (inst_code_off + 5)
                            cross_range_disp32_patches[disp32_seg_off] = new_disp32
                            i += 5
                            continue
                    i += 1

        # --- Emit AIL_CODE LEDATA32 + FIXUPP32 per chunk (max CHUNK_SIZE bytes
        # per LEDATA32; data_record_offset is a 10-bit field max 0x3FF) ---
        CHUNK_SIZE = 1024
        for r in layout["ranges"]:
            r_lin_start = hex_int(r["linear_start"])
            r_size = r["size"]
            r_code_off = hex_int(r["ail_code_off"])
            range_bytes = self.code_bin[r_code_off : r_code_off + r_size]
            base_off = next(b for (mn, mx_excl, b) in range_starts
                             if mn == r_lin_start)
            for chunk_start in range(0, r_size, CHUNK_SIZE):
                chunk_end = min(chunk_start + CHUNK_SIZE, r_size)
                chunk_size = chunk_end - chunk_start
                # bytearray copy so we can zero out the disp32/imm32 bytes
                # at every fixup site (CP-4 fix). wlink resolves a FIXUPP32
                # record by computing (target + addend [- src - 4 if M=0]),
                # where addend = the existing 4 bytes in LEDATA at the fixup
                # location. If we leave the binary-time disp32 / imm32 in the
                # LEDATA, the resolved value becomes (post-link target) +
                # (binary-time disp32) — double-counted, jumps wild.
                chunk_bytes = bytearray(range_bytes[chunk_start:chunk_end])
                chunk_seg_off = base_off + chunk_start

                # Apply cross-range disp32 patches (CP-5 body-fragmented fix).
                for patch_seg_off, new_disp32 in cross_range_disp32_patches.items():
                    if chunk_seg_off <= patch_seg_off < chunk_seg_off + chunk_size:
                        local_off = patch_seg_off - chunk_seg_off
                        chunk_bytes[local_off:local_off + 4] = (
                            new_disp32 & 0xFFFFFFFF).to_bytes(4, "little")

                chunk_subs = []
                for (seg_off, ext_name, self_rel) in fn_code_fixups:
                    if chunk_seg_off <= seg_off < chunk_seg_off + chunk_size:
                        local_off = seg_off - chunk_seg_off
                        chunk_bytes[local_off:local_off + 4] = b"\x00\x00\x00\x00"
                        ext_idx = mod.add_extdef(ext_name)
                        sub = fixupp_subrecord(
                            self_relative=self_rel, location=9,
                            data_record_offset=local_off,
                            frame_method=1, frame_index=flat_grp_idx,
                            target_method=6, target_index=ext_idx,
                        )
                        chunk_subs.append(sub)
                mod.add_ledata(seg_idx=code_seg_idx, offset=chunk_seg_off,
                               data=bytes(chunk_bytes), fixupp_subs=chunk_subs)

        return mod.build(), {
            "kind": "fn",
            "fn_name": fn_name,
            "code_size": code_size,
            "data_size": 0,
            "bss_size": 0,
            "rel32_fixups": len(self.synth_by_parent.get(fn_name, [])),
            "extdefs": len(mod.extdefs),
            "pubdefs": len(mod.pubdefs),
        }

    # -- per-data-item .obj emit ---------------------------------------------
    def emit_data_item_obj(self, d: dict) -> tuple[bytes, dict, str]:
        """Emit one .obj for a single AIL data item.

        Returns (obj_bytes, stats_dict, obj_filename).
        """
        d_addr_str = d["addr"]
        d_addr = hex_int(d_addr_str)
        d_size = d["size"]
        d_name = d["name"]

        padding = 0

        sized_children = [
            (child_off, child_name, child_size)
            for (child_off, child_name, child_size) in
                self.subsumed_by_parent.get(d_addr_str, [])
            if child_size > 0
        ]
        tail_extension = 0
        for (child_off, _, child_size) in sized_children:
            tail_extension = max(tail_extension, child_off + child_size)
        if tail_extension > d_size:
            seg_length = tail_extension
        else:
            seg_length = d_size

        # 8.3 obj name: ail_data_<addr>_<name>.obj. Trim name if too long;
        # the THEADR record (which carries the long name) is what wlink uses
        # for symbol resolution, not the filename.
        obj_name = f"ail_data_{d_addr_str.lstrip('0').lower() or '0'}_{d_name}.obj"

        mod = OmfModule(module_name=obj_name)
        data_idx = mod.add_segment("_DATA", "DATA", length=seg_length)
        # GRPDEFs for Watcom flat-memory FIXUP frame anchoring (DGROUP +
        # empty FLAT; FIXUP records reference FLAT for absolute-mode fixups).
        mod.add_group("DGROUP", [data_idx])
        flat_grp_idx = mod.add_group("FLAT", [])

        # PUBDEF: data symbol at offset = padding (after the pad region).
        mod.add_pubdef(seg_idx=data_idx, name=d_name, offset=padding)
        for (alt_off, alt_label) in sorted(
                self.alt_data_per_item.get(d_addr_str, set())):
            mod.add_pubdef(seg_idx=data_idx, name=alt_label,
                           offset=padding + alt_off)
        # PUBDEFs for child items subsumed under this parent: size-0 mid-data
        # alt-labels (ail_mix_pitch_int @ pitch_low + 4) AND sized vendor-
        # overflow merges (nested_pending_count tail-attached to pending_count).
        for (child_off, child_name, _csize) in sorted(
                self.subsumed_by_parent.get(d_addr_str, [])):
            mod.add_pubdef(seg_idx=data_idx, name=child_name,
                           offset=padding + child_off)

        # LEDATA emit order: padding chunk(s) first (if any), then data.
        # OMF requires LEDATAs in ascending offset order within a segment.
        CHUNK_SIZE = 1024
        # Padding LEDATA(s)
        if padding > 0:
            pos = 0
            while pos < padding:
                take = min(CHUNK_SIZE, padding - pos)
                mod.add_ledata(seg_idx=data_idx, offset=pos,
                               data=b"\x00" * take, fixupp_subs=[])
                pos += take

        # Data LEDATA(s)
        is_bss = d_addr_str in self.bss_layout_by_addr
        if is_bss:
            # BSS items: zero-init bytes (CP-4 coalesce — emit as DATA class
            # so dos4gw doesn't page-commit-chain on own BSS selector).
            # BSS items: write zeros up to tail_extension (covers any
            # vendor-overflow merged children appended after parent body).
            zero_total = max(d_size, tail_extension)
            if zero_total > 0:
                pos = 0
                while pos < zero_total:
                    take = min(CHUNK_SIZE, zero_total - pos)
                    mod.add_ledata(seg_idx=data_idx, offset=padding + pos,
                                   data=b"\x00" * take, fixupp_subs=[])
                    pos += take
            extdef_count = 0
            fixup_count = 0
        else:
            d_layout = self.data_layout_by_addr.get(d_addr_str)
            if d_layout is None:
                if d_size == 0:
                    # Symbol-only label (no bytes). PUBDEF emitted above; no
                    # LEDATA needed. Skip data emit.
                    return mod.build(), {
                        "kind": "data_item",
                        "data_name": d_name,
                        "data_size": 0,
                        "padding": padding,
                        "seg_length": seg_length,
                        "extdefs": 0,
                        "pubdefs": len(mod.pubdefs),
                        "fixups": 0,
                    }, obj_name
                raise SystemExit(
                    f"FAIL: data item {d_name} @ {d_addr_str} size={d_size}"
                    f" not in data_layout or bss_layout"
                )
            d_file_off = hex_int(d_layout["ail_data_off"])
            d_bytes = bytearray(self.data_bin[d_file_off : d_file_off + d_size])

            # Collect LE FIXUPs whose src ∈ this item.
            item_fixups = []  # list of (drec_off_within_item, ext_name)
            for sa_int, rec in self.le_by_src.items():
                if d_addr <= sa_int < d_addr + d_size:
                    tgt_addr = hex_int(rec["target_addr"])
                    kind, ext_name = self.resolve_le_target(tgt_addr)
                    if ext_name is None:
                        raise SystemExit(
                            f"FAIL: LE FIXUP src=0x{sa_int:x} in data "
                            f"{d_name} target=0x{tgt_addr:x} unresolved"
                        )
                    drec_off = sa_int - d_addr
                    d_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
                    item_fixups.append((drec_off, ext_name))

            # Split data into CHUNK_SIZE chunks. Fixups within each chunk
            # use data_record_offset (10-bit field, max 0x3FF) relative to
            # chunk start.
            pos = 0
            while pos < d_size:
                take = min(CHUNK_SIZE, d_size - pos)
                chunk_bytes = bytes(d_bytes[pos : pos + take])
                chunk_subs = []
                for fixup_off, ext_name in item_fixups:
                    if pos <= fixup_off < pos + take:
                        ext_idx = mod.add_extdef(ext_name)
                        sub = fixupp_subrecord(
                            self_relative=False, location=9,
                            data_record_offset=fixup_off - pos,
                            frame_method=1, frame_index=flat_grp_idx,
                            target_method=6, target_index=ext_idx,
                        )
                        chunk_subs.append(sub)
                mod.add_ledata(seg_idx=data_idx, offset=padding + pos,
                               data=chunk_bytes, fixupp_subs=chunk_subs)
                pos += take
            # Zero-fill tail extension if any vendor-overflow merged children
            # extend past d_size.
            if tail_extension > d_size:
                tail_pos = d_size
                while tail_pos < tail_extension:
                    take = min(CHUNK_SIZE, tail_extension - tail_pos)
                    mod.add_ledata(seg_idx=data_idx, offset=padding + tail_pos,
                                   data=b"\x00" * take, fixupp_subs=[])
                    tail_pos += take
            extdef_count = len(mod.extdefs)
            fixup_count = len(item_fixups)

        return mod.build(), {
            "kind": "data_item",
            "data_name": d_name,
            "data_size": d_size,
            "padding": padding,
            "seg_length": seg_length,
            "extdefs": extdef_count,
            "pubdefs": len(mod.pubdefs),
            "fixups": fixup_count,
        }, obj_name

    # -- shared-data .obj emit (LEGACY, kept for grouping.json compatibility) -
    def emit_shared_obj(self, shared: dict) -> tuple[bytes, dict]:
        d_addr_str = shared["data_addr"]
        d_addr = hex_int(d_addr_str)
        d_size = shared["data_size"]
        d_name = shared["data_name"]
        segment = shared["segment"]  # "data" or "bss"

        mod = OmfModule(module_name=shared["obj_name"])

        if segment == "bss":
            # CP-4 fix: emit BSS items as zero-init DATA class so wlink
            # combines them into AIL_DATA (same selector as the rest of AIL
            # data). Standalone BSS-class segments land in their own selector,
            # and dos4gw 9.5a's commit-on-init for that selector triggers a
            # runaway page-allocation chain in DOSBox-X. File-backed zero
            # bytes carry the same runtime semantic (zero-initialised), with
            # the only cost being EXE size growth equal to total BSS size.
            data_idx = mod.add_segment("_DATA", "DATA", length=d_size)
            # GRPDEFs for Watcom flat-memory FIXUP frame anchoring (see
            # emit_fn_obj for rationale). No FIXUPs are emitted here (BSS
            # zero-init) but the groups must still exist for consistency
            # with the rest of the .lib's modules.
            mod.add_group("DGROUP", [data_idx])
            mod.add_group("FLAT", [])
            mod.add_pubdef(seg_idx=data_idx, name=d_name, offset=0)
            for (alt_off, alt_label) in sorted(
                    self.alt_data_per_item.get(d_addr_str, set())):
                mod.add_pubdef(seg_idx=data_idx, name=alt_label, offset=alt_off)
            mod.add_ledata(seg_idx=data_idx, offset=0, data=b"\x00" * d_size,
                           fixupp_subs=[])
            return mod.build(), {
                "kind": "shared_data",
                "data_name": d_name,
                "data_size": d_size,
                "segment": "bss_as_data",
                "extdefs": 0,
                "fixups": 0,
            }

        # file-backed data
        data_idx = mod.add_segment("_DATA", "DATA", length=d_size)
        # GRPDEFs for Watcom flat-memory FIXUP frame anchoring.
        mod.add_group("DGROUP", [data_idx])
        flat_grp_idx = mod.add_group("FLAT", [])
        mod.add_pubdef(seg_idx=data_idx, name=d_name, offset=0)
        # mid-data alt-offset labels
        for (alt_off, alt_label) in sorted(
                self.alt_data_per_item.get(d_addr_str, set())):
            mod.add_pubdef(seg_idx=data_idx, name=alt_label, offset=alt_off)
        d_layout = self.data_layout_by_addr.get(d_addr_str)
        if d_layout is None:
            raise SystemExit(f"FAIL: shared data {d_name} not in layout.data[]")
        d_file_off = hex_int(d_layout["ail_data_off"])
        d_bytes = bytearray(self.data_bin[d_file_off : d_file_off + d_size])
        # LE FIXUP records src ∈ this data
        le_subs = []
        for sa_int, rec in self.le_by_src.items():
            if d_addr <= sa_int < d_addr + d_size:
                tgt_addr = hex_int(rec["target_addr"])
                kind, ext_name = self.resolve_le_target(tgt_addr)
                if ext_name is None:
                    raise SystemExit(
                        f"FAIL: LE FIXUP src=0x{sa_int:x} in shared data {d_name} "
                        f"target=0x{tgt_addr:x} unresolved"
                    )
                ext_idx = mod.add_extdef(ext_name)
                drec_off = sa_int - d_addr
                d_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
                sub = fixupp_subrecord(
                    self_relative=False, location=9,
                    data_record_offset=drec_off,
                    frame_method=1, frame_index=flat_grp_idx,
                    target_method=6, target_index=ext_idx,
                )
                le_subs.append(sub)
        mod.add_ledata(seg_idx=data_idx, offset=0, data=bytes(d_bytes), fixupp_subs=le_subs)
        return mod.build(), {
            "kind": "shared_data",
            "data_name": d_name,
            "data_size": d_size,
            "segment": "data",
            "extdefs": len(mod.extdefs),
            "fixups": len(le_subs),
        }

    # -- fd2common .obj emit -------------------------------------------------
    def emit_fd2common_obj(self, fn_info: dict) -> tuple[bytes, dict]:
        """Byte-preserve emit of a fd2common helper: read fn body from LE
        binary (via existing extract_ail_bytes mechanism not available here,
        so we re-read from FD2.LE on-disk per-fn) → wrap in OMF .obj.

        fd2_dpmi_* are pure INT 31h wrappers, expected EXTDEFs = 0.
        crt_equivalent_get_eflags_thunk is a 5B JMP to crt_equivalent_get_eflags
        in the same .obj? No — they're separate .obj files within
        fd2common.lib. The thunk needs an EXTDEF for its jump target.
        """
        fn_name = fn_info["name"]
        entry_int = int(fn_info["entry"], 16)
        body_min = int(fn_info["body_min"], 16)
        body_max = int(fn_info["body_max"], 16)
        body_size = fn_info["body_size"]

        # Read fn body bytes from FD2.LE
        # We need linear → file_offset mapping; reuse the LE object table
        # in layout["objects"] (this map covers all 3 LE objects).
        objects = self.layout["objects"]
        # Map linear → file_off
        def linear_to_file_off(lin):
            for o in objects:
                if o["base"] <= lin < o["linear_file_end"]:
                    return o["file_off"] + (lin - o["base"])
            return None
        # Read directly from FD2.LE
        le_path = REPO / "fd2_game_files" / "FD2.LE"
        le_bytes_full = le_path.read_bytes()
        body_file_off = linear_to_file_off(body_min)
        body_bytes = bytearray(le_bytes_full[body_file_off : body_file_off + body_size])
        assert len(body_bytes) == body_size

        # Build .obj
        entry_lower = fn_info["entry"].lstrip("0").lower() or "0"
        slug = fn_name  # fd2common names are all safe ASCII
        obj_name = f"fd2common_{slug}.obj"
        mod = OmfModule(module_name=obj_name)
        code_idx = mod.add_segment("_TEXT", "CODE", length=body_size)
        # GRPDEFs for Watcom flat-memory FIXUP frame anchoring (DGROUP is
        # empty here — fd2common helpers own no data — but we still emit it
        # to keep .lib module structure homogeneous).
        mod.add_group("DGROUP", [])
        flat_grp_idx = mod.add_group("FLAT", [])
        mod.add_pubdef(seg_idx=code_idx, name=fn_name, offset=0)

        # Collect fixups: rel32 within body (from Ghidra-derived list) +
        # LE FIXUP src ∈ body.
        le_subs = []
        for site in self.fd2common_rel32_by_fn.get(fn_name, []):
            inst_addr = hex_int(site["inst_addr"])
            tgt_addr = hex_int(site["target_addr"])
            disp_off = site["disp32_offset_in_inst"]
            kind, ext_name = self.resolve_le_target(tgt_addr)
            if ext_name is None:
                raise SystemExit(
                    f"FAIL: fd2common {fn_name} rel32 at {site['inst_addr']} "
                    f"target=0x{tgt_addr:x} unresolved"
                )
            ext_idx = mod.add_extdef(ext_name)
            drec_off = (inst_addr - body_min) + disp_off
            body_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
            sub = fixupp_subrecord(
                self_relative=True, location=9,
                data_record_offset=drec_off,
                frame_method=1, frame_index=flat_grp_idx,
                target_method=6, target_index=ext_idx,
            )
            le_subs.append(sub)
        # LE FIXUP src ∈ body
        for sa_int, rec in self.le_by_src.items():
            if body_min <= sa_int < body_min + body_size:
                tgt = hex_int(rec["target_addr"])
                kind, ext_name = self.resolve_le_target(tgt)
                if ext_name is None:
                    raise SystemExit(
                        f"FAIL: fd2common {fn_name} LE FIXUP src=0x{sa_int:x} "
                        f"target=0x{tgt:x} unresolved"
                    )
                ext_idx = mod.add_extdef(ext_name)
                drec_off = sa_int - body_min
                body_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
                sub = fixupp_subrecord(
                    self_relative=False, location=9,
                    data_record_offset=drec_off,
                    frame_method=1, frame_index=flat_grp_idx,
                    target_method=6, target_index=ext_idx,
                )
                le_subs.append(sub)
        mod.add_ledata(seg_idx=code_idx, offset=0, data=bytes(body_bytes),
                       fixupp_subs=le_subs)
        return mod.build(), {
            "kind": "fd2common",
            "fn_name": fn_name,
            "body_size": body_size,
            "extdefs": len(mod.extdefs),
            "fixups": len(le_subs),
        }


    # -- fd2common merged .obj emit -------------------------------------------
    def emit_fd2common_merged_obj(self, primary_info: dict,
                                   secondaries: list) -> tuple[bytes, dict]:
        """Emit a merged .obj containing primary + secondary fd2common fns
        at vendor-relative offsets. Preserves short JMP (EB rel8) between
        them — the 1-byte displacement is correct when both bodies are at
        their vendor-relative positions within the .obj segment."""
        all_fns = [primary_info] + secondaries
        all_fns.sort(key=lambda f: int(f["body_min"], 16))

        # Compute contiguous range
        range_min = int(all_fns[0]["body_min"], 16)
        range_max = int(all_fns[-1]["body_max"], 16)
        total_size = range_max - range_min + 1

        # Read bytes from FD2.LE
        objects = self.layout["objects"]
        def linear_to_file_off(lin):
            for o in objects:
                if o["base"] <= lin < o["linear_file_end"]:
                    return o["file_off"] + (lin - o["base"])
            return None
        le_path = REPO / "fd2_game_files" / "FD2.LE"
        le_bytes_full = le_path.read_bytes()
        file_off = linear_to_file_off(range_min)
        body_bytes = bytearray(le_bytes_full[file_off:file_off + total_size])

        # Build .obj
        fn_names = [f["name"] for f in all_fns]
        obj_name = f"fd2common_{'_'.join(fn_names)}.obj"
        if len(obj_name) > 200:
            obj_name = f"fd2common_merged_{primary_info['name']}.obj"
        mod = OmfModule(module_name=obj_name)
        code_idx = mod.add_segment("_TEXT", "CODE", length=total_size)
        mod.add_group("DGROUP", [])
        flat_grp_idx = mod.add_group("FLAT", [])

        # PUBDEFs for each fn entry
        for fn in all_fns:
            entry_int = int(fn["entry"], 16)
            offset = entry_int - range_min
            mod.add_pubdef(seg_idx=code_idx, name=fn["name"], offset=offset)

        # Collect all rel32 fixups from ALL merged fns
        all_fixups = []  # (drec_off_in_segment, ext_name, self_rel)
        for fn in all_fns:
            fn_name = fn["name"]
            body_min_int = int(fn["body_min"], 16)
            body_size = fn["body_size"]
            # rel32 sites
            for site in self.fd2common_rel32_by_fn.get(fn_name, []):
                inst_addr = hex_int(site["inst_addr"])
                tgt_addr = hex_int(site["target_addr"])
                disp_off = site["disp32_offset_in_inst"]
                # Check if target is within the merged range (intra-merged)
                if range_min <= tgt_addr <= range_max:
                    continue  # Intra-merged: literal disp32, no fixup needed
                kind, ext_name = self.resolve_le_target(tgt_addr)
                if ext_name is None:
                    # Try fd2common fn entry
                    if tgt_addr in self.fd2common_by_addr:
                        ext_name = self.fd2common_by_addr[tgt_addr]["name"]
                    else:
                        raise SystemExit(
                            f"FAIL: merged fd2common {fn_name} rel32 at "
                            f"0x{inst_addr:x} target=0x{tgt_addr:x} unresolved")
                drec_off = (inst_addr - range_min) + disp_off
                body_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
                all_fixups.append((drec_off, ext_name, True))
            # LE FIXUP records
            for sa_int, rec in self.le_by_src.items():
                if body_min_int <= sa_int < body_min_int + body_size:
                    tgt = hex_int(rec["target_addr"])
                    kind, ext_name = self.resolve_le_target(tgt)
                    if ext_name is None:
                        raise SystemExit(
                            f"FAIL: merged fd2common {fn_name} LE FIXUP "
                            f"src=0x{sa_int:x} target=0x{tgt:x} unresolved")
                    drec_off = sa_int - range_min
                    body_bytes[drec_off:drec_off + 4] = b"\x00\x00\x00\x00"
                    all_fixups.append((drec_off, ext_name, False))

        all_fixups.sort(key=lambda t: t[0])
        # Emit LEDATA + FIXUPP
        CHUNK_SIZE = 1024
        for chunk_start in range(0, total_size, CHUNK_SIZE):
            chunk_end = min(chunk_start + CHUNK_SIZE, total_size)
            chunk = bytearray(body_bytes[chunk_start:chunk_end])
            chunk_subs = []
            for (drec_off, ext_name, self_rel) in all_fixups:
                if chunk_start <= drec_off < chunk_end:
                    loff = drec_off - chunk_start
                    ext_idx = mod.add_extdef(ext_name)
                    sub = fixupp_subrecord(
                        self_relative=self_rel, location=9,
                        data_record_offset=loff,
                        frame_method=1, frame_index=flat_grp_idx,
                        target_method=6, target_index=ext_idx,
                    )
                    chunk_subs.append(sub)
            mod.add_ledata(seg_idx=code_idx, offset=chunk_start,
                           data=bytes(chunk), fixupp_subs=chunk_subs)

        return mod.build(), {
            "kind": "fd2common_merged",
            "fn_names": fn_names,
            "total_size": total_size,
            "extdefs": len(mod.extdefs),
            "fixups": len(all_fixups),
        }

    # -- consolidated code emit ------------------------------------------------
    def emit_ail_code_consolidated(self) -> tuple[bytes, dict]:
        """Emit a single .obj containing ALL AIL function bodies in one
        _TEXT CODE segment.  Functions are sorted by vendor entry address
        (ascending) and concatenated.

        Intra-AIL rel32 (E8/E9/0F8x) whose target is another AIL fn or
        mid-fn label are resolved as literal disp32 within the segment —
        no FIXUPP needed (self-relative addressing is position-independent
        within the same segment).

        Cross-module rel32 (AIL→CRT, AIL→fd2common) and absolute LE FIXUP
        references keep EXTDEF + FIXUPP32 as in per-item mode.
        """
        # 1. Sort fns by vendor entry, compute consolidated offsets
        sorted_fns = sorted(self.inv["functions"],
                            key=lambda f: hex_int(f["entry"]))
        fn_consol_off = {}   # fn_name → offset in consolidated segment
        fn_entry_off = {}    # vendor_entry_int → offset in consolidated segment
        cum = 0
        for fn in sorted_fns:
            name = fn["name"]
            layout = self.fn_layout_by_name[name]
            body_size = sum(r["size"] for r in layout["ranges"])
            fn_consol_off[name] = cum
            fn_entry_int = hex_int(fn["entry"])
            # For multi-range fns, entry may not be at range[0].
            # Compute entry offset relative to the fn's base.
            range_starts_local = []
            local_cum = 0
            for r in layout["ranges"]:
                r_lin = hex_int(r["linear_start"])
                range_starts_local.append((r_lin, r_lin + r["size"], local_cum))
                local_cum += r["size"]
            entry_local = None
            for (mn, mx, base) in range_starts_local:
                if mn <= fn_entry_int < mx:
                    entry_local = base + (fn_entry_int - mn)
                    break
            if entry_local is None:
                raise SystemExit(
                    f"FAIL: consolidated fn {name} entry 0x{fn_entry_int:x} "
                    f"not in any body range")
            fn_entry_off[fn_entry_int] = cum + entry_local
            cum += body_size
        total_code_size = cum

        # 2. Build mid-fn label → consolidated offset mapping
        midfn_label_off = {}  # label_name → offset in consolidated segment
        for fn_name, alts in self.alts_per_fn.items():
            base = fn_consol_off[fn_name]
            for (intra_off, label) in alts:
                midfn_label_off[label] = base + intra_off

        # Helper: resolve an intra-AIL target to consolidated segment offset
        def resolve_intra_ail(target_addr: int):
            """Return consolidated segment offset if target is an AIL fn
            entry or mid-fn label, else None."""
            if target_addr in fn_entry_off:
                return fn_entry_off[target_addr]
            for (mn, mx, name) in self.fn_ranges:
                if mn < target_addr <= mx:
                    off = target_addr - mn
                    label = f"L_{name}_alt_{off:x}"
                    if label in midfn_label_off:
                        return midfn_label_off[label]
                    return fn_consol_off[name] + off
            return None

        # 3. Build OmfModule
        mod = OmfModule(module_name="ail_code.obj")
        code_seg_idx = mod.add_segment("_TEXT", "CODE", length=total_code_size)
        mod.add_group("DGROUP", [])
        flat_grp_idx = mod.add_group("FLAT", [])

        # 4. PUBDEFs for all fn entries + mid-fn alt labels
        for fn in sorted_fns:
            name = fn["name"]
            entry_int = hex_int(fn["entry"])
            mod.add_pubdef(seg_idx=code_seg_idx, name=name,
                           offset=fn_entry_off[entry_int])
        for fn_name, alts in sorted(self.alts_per_fn.items()):
            base = fn_consol_off[fn_name]
            for (intra_off, label) in sorted(alts):
                mod.add_pubdef(seg_idx=code_seg_idx, name=label,
                               offset=base + intra_off)

        # 5. Emit LEDATA + FIXUPP per fn (in vendor entry order)
        CHUNK_SIZE = 1024
        intra_resolved = 0
        extern_fixups = 0
        for fn in sorted_fns:
            name = fn["name"]
            layout = self.fn_layout_by_name[name]
            fn_base = fn_consol_off[name]

            # Build per-range linear→code_off mapping (local to this fn)
            range_starts_local = []
            local_cum = 0
            for r in layout["ranges"]:
                r_lin = hex_int(r["linear_start"])
                range_starts_local.append(
                    (r_lin, r_lin + r["size"], local_cum))
                local_cum += r["size"]

            def linear_to_fn_off(lin_addr):
                for (mn, mx, base) in range_starts_local:
                    if mn <= lin_addr < mx:
                        return base + (lin_addr - mn)
                return None

            # Collect all fixup positions for this fn:
            # (fn_local_off, ext_name_or_None, self_rel, is_intra_ail)
            fn_fixups = []

            # 5a. rel32 from synth records
            for s in self.synth_by_parent.get(name, []):
                inst_addr = hex_int(s["inst_addr"])
                disp_off = s["disp32_offset_in_inst"]
                disp_lin = inst_addr + disp_off
                fn_local = linear_to_fn_off(disp_lin)
                if fn_local is None:
                    raise SystemExit(
                        f"FAIL: consolidated rel32 inst={s['inst_addr']} "
                        f"fn={name} disp 0x{disp_lin:x} not in body")
                target_addr = hex_int(s["target_addr"])
                intra_off = resolve_intra_ail(target_addr)
                if intra_off is not None:
                    # Intra-AIL: literal disp32
                    fn_fixups.append(
                        (fn_local, None, True, True, intra_off))
                    intra_resolved += 1
                else:
                    ext_name = s["extdef_target"]
                    if not ext_name:
                        raise SystemExit(
                            f"FAIL: consolidated rel32 {s['inst_addr']} "
                            f"no extdef_target")
                    fn_fixups.append(
                        (fn_local, ext_name, True, False, None))
                    extern_fixups += 1

            # 5b. LE FIXUP records with src in this fn's body
            for r in layout["ranges"]:
                r_lin_start = hex_int(r["linear_start"])
                r_lin_end = hex_int(r["linear_end_excl"])
                r_local_base = next(
                    b for (mn, mx, b) in range_starts_local
                    if mn == r_lin_start)
                for sa_int, rec in self.le_by_src.items():
                    if r_lin_start <= sa_int < r_lin_end:
                        tgt_addr = hex_int(rec["target_addr"])
                        kind, ext_name = self.resolve_le_target(tgt_addr)
                        if ext_name is None:
                            raise SystemExit(
                                f"FAIL: consolidated LE FIXUP src=0x{sa_int:x} "
                                f"fn={name} target=0x{tgt_addr:x} unresolved")
                        fn_local = r_local_base + (sa_int - r_lin_start)
                        fn_fixups.append(
                            (fn_local, ext_name, False, False, None))
                        extern_fixups += 1

            fn_fixups.sort(key=lambda t: t[0])

            # 5c. Cross-range intra-fn disp32 patches (body-fragmented fns)
            cross_patches = {}
            fixup_positions = {f[0] for f in fn_fixups}
            if len(layout["ranges"]) > 1:
                for r in layout["ranges"]:
                    r_lin_start = hex_int(r["linear_start"])
                    r_size = r["size"]
                    r_code_off = hex_int(r["ail_code_off"])
                    rng_bytes = self.code_bin[r_code_off:r_code_off + r_size]
                    r_local_base = next(
                        b for (mn, mx, b) in range_starts_local
                        if mn == r_lin_start)
                    i = 0
                    while i <= r_size - 5:
                        op = rng_bytes[i]
                        if op in (0xE8, 0xE9):
                            d32 = int.from_bytes(
                                rng_bytes[i+1:i+5], "little", signed=True)
                            src_lin = r_lin_start + i
                            tgt_lin = src_lin + 5 + d32
                            tgt_fn_local = linear_to_fn_off(tgt_lin)
                            r_end = hex_int(r["linear_end_excl"])
                            if (tgt_fn_local is not None
                                    and not (r_lin_start <= tgt_lin < r_end)):
                                src_fn_local = r_local_base + i
                                disp_fn_local = src_fn_local + 1
                                if disp_fn_local not in fixup_positions:
                                    new_d32 = tgt_fn_local - (src_fn_local + 5)
                                    cross_patches[disp_fn_local] = new_d32
                                    i += 5
                                    continue
                        i += 1

            # 5d. Build LEDATA bytes for this fn, chunk by CHUNK_SIZE
            for r in layout["ranges"]:
                r_lin_start = hex_int(r["linear_start"])
                r_size = r["size"]
                r_code_off = hex_int(r["ail_code_off"])
                rng_bytes = self.code_bin[r_code_off:r_code_off + r_size]
                r_local_base = next(
                    b for (mn, mx, b) in range_starts_local
                    if mn == r_lin_start)
                for chunk_start in range(0, r_size, CHUNK_SIZE):
                    chunk_end = min(chunk_start + CHUNK_SIZE, r_size)
                    chunk = bytearray(rng_bytes[chunk_start:chunk_end])
                    chunk_fn_off = r_local_base + chunk_start
                    chunk_seg_off = fn_base + chunk_fn_off

                    # Apply cross-range patches
                    for p_fn_off, new_d32 in cross_patches.items():
                        if chunk_fn_off <= p_fn_off < chunk_fn_off + len(chunk):
                            loff = p_fn_off - chunk_fn_off
                            chunk[loff:loff+4] = (
                                new_d32 & 0xFFFFFFFF).to_bytes(4, "little")

                    chunk_subs = []
                    for (fn_off, ext_name, self_rel,
                         is_intra, intra_tgt_off) in fn_fixups:
                        if chunk_fn_off <= fn_off < chunk_fn_off + len(chunk):
                            loff = fn_off - chunk_fn_off
                            if is_intra:
                                # Literal disp32 for intra-AIL self-rel
                                src_seg = fn_base + fn_off
                                d32 = intra_tgt_off - (src_seg + 4)
                                chunk[loff:loff+4] = (
                                    d32 & 0xFFFFFFFF).to_bytes(4, "little")
                            else:
                                # EXTDEF + FIXUPP
                                chunk[loff:loff+4] = b"\x00\x00\x00\x00"
                                ext_idx = mod.add_extdef(ext_name)
                                sub = fixupp_subrecord(
                                    self_relative=self_rel, location=9,
                                    data_record_offset=loff,
                                    frame_method=1,
                                    frame_index=flat_grp_idx,
                                    target_method=6,
                                    target_index=ext_idx,
                                )
                                chunk_subs.append(sub)

                    mod.add_ledata(seg_idx=code_seg_idx,
                                   offset=chunk_seg_off,
                                   data=bytes(chunk),
                                   fixupp_subs=chunk_subs)

        return mod.build(), {
            "kind": "consolidated_code",
            "total_code_size": total_code_size,
            "fn_count": len(sorted_fns),
            "intra_ail_resolved": intra_resolved,
            "extern_fixups": extern_fixups,
            "extdefs": len(mod.extdefs),
            "pubdefs": len(mod.pubdefs),
        }

    # -- consolidated data emit ------------------------------------------------
    def emit_ail_data_consolidated(self) -> tuple[bytes, dict]:
        """Emit a single .obj containing ALL AIL data items in one _DATA
        segment.  Items are sorted by vendor address (ascending) and packed
        tightly (non-AIL gaps removed).  ISR stack padding is inserted
        before data_ail_isr_saved_caller_ss.

        Vendor-adjacent AIL items remain adjacent in the compact layout,
        so MERGE_GROUPS is automatically satisfied.
        """
        # 1. Sort data items by vendor addr, excluding subsumed children
        sorted_items = sorted(
            [d for d in self.inv["data_items"]
             if d["addr"] not in self.subsumed_children],
            key=lambda d: hex_int(d["addr"]))

        # 2. Compute compact offsets (no explicit padding — ISR stack gap
        #    is now a proper data item in the inventory)
        item_compact_off = {}  # d_addr_str → offset in compact segment
        cum = 0
        for d in sorted_items:
            d_addr = d["addr"]
            item_compact_off[d_addr] = cum
            effective_size = d["size"]
            for (child_off, _, child_size) in \
                    self.subsumed_by_parent.get(d_addr, []):
                if child_size > 0:
                    effective_size = max(effective_size,
                                        child_off + child_size)
            cum += effective_size
        total_data_size = cum

        # 3. Build OmfModule
        mod = OmfModule(module_name="ail_data.obj")
        data_seg_idx = mod.add_segment("_DATA", "DATA",
                                        length=total_data_size)
        mod.add_group("DGROUP", [data_seg_idx])
        flat_grp_idx = mod.add_group("FLAT", [])

        # 4. PUBDEFs for all items + subsumed children + mid-data alt labels
        for d in sorted_items:
            d_addr_str = d["addr"]
            compact = item_compact_off[d_addr_str]
            mod.add_pubdef(seg_idx=data_seg_idx, name=d["name"],
                           offset=compact)
            # Subsumed children (size-0 labels + vendor-overflow merges)
            for (child_off, child_name, _) in sorted(
                    self.subsumed_by_parent.get(d_addr_str, [])):
                mod.add_pubdef(seg_idx=data_seg_idx, name=child_name,
                               offset=compact + child_off)
            # Mid-data alt-offset labels
            for (alt_off, alt_label) in sorted(
                    self.alt_data_per_item.get(d_addr_str, set())):
                mod.add_pubdef(seg_idx=data_seg_idx, name=alt_label,
                               offset=compact + alt_off)

        # 5. Emit LEDATA + FIXUPP per item
        CHUNK_SIZE = 1024
        total_fixups = 0
        for d in sorted_items:
            d_addr_str = d["addr"]
            d_addr = hex_int(d_addr_str)
            d_size = d["size"]
            d_name = d["name"]
            compact = item_compact_off[d_addr_str]

            # Effective size (including vendor-overflow tail extension)
            effective_size = d_size
            for (child_off, _, child_size) in \
                    self.subsumed_by_parent.get(d_addr_str, []):
                if child_size > 0:
                    effective_size = max(effective_size,
                                        child_off + child_size)

            if d_size == 0:
                continue  # symbol-only label, PUBDEF emitted above

            # Determine if BSS or file-backed
            is_bss = d_addr_str in self.bss_layout_by_addr
            if is_bss:
                pos = 0
                while pos < effective_size:
                    take = min(CHUNK_SIZE, effective_size - pos)
                    mod.add_ledata(seg_idx=data_seg_idx,
                                   offset=compact + pos,
                                   data=b"\x00" * take, fixupp_subs=[])
                    pos += take
            else:
                d_layout = self.data_layout_by_addr.get(d_addr_str)
                if d_layout is None:
                    raise SystemExit(
                        f"FAIL: consolidated data {d_name} @ {d_addr_str} "
                        f"not in layout")
                d_file_off = hex_int(d_layout["ail_data_off"])
                d_bytes = bytearray(
                    self.data_bin[d_file_off:d_file_off + d_size])

                # LE FIXUPs within this item
                item_fixups = []
                for sa_int, rec in self.le_by_src.items():
                    if d_addr <= sa_int < d_addr + d_size:
                        tgt = hex_int(rec["target_addr"])
                        kind, ext_name = self.resolve_le_target(tgt)
                        if ext_name is None:
                            raise SystemExit(
                                f"FAIL: consolidated data LE FIXUP "
                                f"src=0x{sa_int:x} in {d_name} "
                                f"target=0x{tgt:x} unresolved")
                        drec_off = sa_int - d_addr
                        d_bytes[drec_off:drec_off+4] = b"\x00\x00\x00\x00"
                        item_fixups.append((drec_off, ext_name))
                        total_fixups += 1

                # Emit data bytes in chunks
                pos = 0
                while pos < d_size:
                    take = min(CHUNK_SIZE, d_size - pos)
                    chunk = bytes(d_bytes[pos:pos + take])
                    chunk_subs = []
                    for (fix_off, ext_name) in item_fixups:
                        if pos <= fix_off < pos + take:
                            ext_idx = mod.add_extdef(ext_name)
                            sub = fixupp_subrecord(
                                self_relative=False, location=9,
                                data_record_offset=fix_off - pos,
                                frame_method=1,
                                frame_index=flat_grp_idx,
                                target_method=6,
                                target_index=ext_idx,
                            )
                            chunk_subs.append(sub)
                    mod.add_ledata(seg_idx=data_seg_idx,
                                   offset=compact + pos,
                                   data=chunk, fixupp_subs=chunk_subs)
                    pos += take

                # Zero-fill tail extension for vendor-overflow merges
                if effective_size > d_size:
                    tail_pos = d_size
                    while tail_pos < effective_size:
                        take = min(CHUNK_SIZE, effective_size - tail_pos)
                        mod.add_ledata(seg_idx=data_seg_idx,
                                       offset=compact + tail_pos,
                                       data=b"\x00" * take, fixupp_subs=[])
                        tail_pos += take

        return mod.build(), {
            "kind": "consolidated_data",
            "total_data_size": total_data_size,
            "item_count": len(sorted_items),
            "padded_items": [],
            "fixups": total_fixups,
            "extdefs": len(mod.extdefs),
            "pubdefs": len(mod.pubdefs),
        }


# ============================================================================
def main():
    import argparse
    ap = argparse.ArgumentParser(
        description="Emit OMF .obj for AIL extraction pipeline")
    ap.add_argument("--mode", choices=["per-item", "consolidated"],
                    default="per-item",
                    help="per-item (default): one .obj per fn/data. "
                         "consolidated: single ail_code.obj + ail_data.obj")
    args = ap.parse_args()

    OBJ_DIR.mkdir(parents=True, exist_ok=True)
    # Clean stale objs from any previous mode
    for stale_pat in ("ail_*.obj", "fd2common_*.obj"):
        for stale in OBJ_DIR.glob(stale_pat):
            stale.unlink()

    builder = BinToOmfBuilder()

    if args.mode == "consolidated":
        print("=== CONSOLIDATED mode ===\n")
        # 1. ail_code.obj — single .obj for all 428 AIL fn
        print("Emitting ail_code.obj ...")
        code_bytes, code_stats = builder.emit_ail_code_consolidated()
        (OBJ_DIR / "ail_code.obj").write_bytes(code_bytes)
        print(f"  code: {code_stats['total_code_size']} bytes, "
              f"{code_stats['fn_count']} fns, "
              f"{code_stats['intra_ail_resolved']} intra-AIL resolved, "
              f"{code_stats['extern_fixups']} extern fixups, "
              f"{code_stats['extdefs']} extdefs, "
              f"{code_stats['pubdefs']} pubdefs")

        # 2. ail_data.obj — single .obj for all AIL data items
        print("Emitting ail_data.obj ...")
        data_bytes, data_stats = builder.emit_ail_data_consolidated()
        (OBJ_DIR / "ail_data.obj").write_bytes(data_bytes)
        print(f"  data: {data_stats['total_data_size']} bytes, "
              f"{data_stats['item_count']} items, "
              f"{data_stats['fixups']} fixups, "
              f"{data_stats['extdefs']} extdefs, "
              f"{data_stats['pubdefs']} pubdefs")
        if data_stats["padded_items"]:
            for name, pad in data_stats["padded_items"]:
                print(f"  ISR stack pad: {name} = {pad} bytes")

        # 3. fd2common .obj (with merge for short-JMP dependencies)
        print("Emitting fd2common .obj ...")
        fd2c_count = 0
        for entry, fn_info in sorted(builder.fd2common_by_addr.items()):
            if entry in builder.fd2common_merge_secondary:
                continue  # emitted as part of primary's merged .obj
            if entry in builder.fd2common_merge_primary:
                secondaries = builder.fd2common_merge_primary[entry]
                out_bytes, stats = builder.emit_fd2common_merged_obj(
                    fn_info, secondaries)
                names = [fn_info["name"]] + [s["name"] for s in secondaries]
                obj_name = f"fd2common_{'_'.join(names)}.obj"
                if len(obj_name) > 200:
                    obj_name = f"fd2common_merged_{fn_info['name']}.obj"
                out_path = OBJ_DIR / obj_name
                out_path.write_bytes(out_bytes)
                print(f"  {out_path.name}: {stats['total_size']}B merged, "
                      f"{stats['extdefs']} extdefs (fns: {names})")
                fd2c_count += 1
            else:
                out_bytes, stats = builder.emit_fd2common_obj(fn_info)
                out_path = OBJ_DIR / f"fd2common_{fn_info['name']}.obj"
                out_path.write_bytes(out_bytes)
                print(f"  {out_path.name}: {stats['body_size']}B, "
                      f"{stats['extdefs']} extdefs")
                fd2c_count += 1

        print(f"\n=== consolidated summary ===")
        print(f"  ail_code.obj: {len(code_bytes)} bytes")
        print(f"  ail_data.obj: {len(data_bytes)} bytes")
        print(f"  fd2common: {fd2c_count} .obj")

    else:
        # Per-item mode (original)
        print("=== PER-ITEM mode ===\n")
        stats_summary = {
            "fn_obj": 0, "data_item_obj": 0, "fd2common_obj": 0,
            "total_extdefs": 0, "total_pubdefs": 0, "total_fixups": 0,
            "total_bytes": 0, "total_padding": 0,
        }
        N = (len(builder.grouping["fn_objs"])
             + len(builder.inv["data_items"]) + 8)
        i = 0
        for fn_obj in builder.grouping["fn_objs"]:
            i += 1
            out_bytes, stats = builder.emit_fn_obj(fn_obj)
            out_path = OBJ_DIR / fn_obj["obj_name"]
            out_path.write_bytes(out_bytes)
            stats_summary["fn_obj"] += 1
            stats_summary["total_extdefs"] += stats["extdefs"]
            stats_summary["total_pubdefs"] += stats["pubdefs"]
            stats_summary["total_fixups"] += stats["rel32_fixups"]
            stats_summary["total_bytes"] += len(out_bytes)
            if i % 50 == 0 or i == N:
                print(f"  [{i}/{N}] emit {out_path.name}")
        padded_items = []
        subsumed = []
        for d in builder.inv["data_items"]:
            if d["addr"] in builder.subsumed_children:
                subsumed.append(d["name"])
                continue
            i += 1
            out_bytes, stats, obj_name = builder.emit_data_item_obj(d)
            out_path = OBJ_DIR / obj_name
            out_path.write_bytes(out_bytes)
            stats_summary["data_item_obj"] += 1
            stats_summary["total_extdefs"] += stats["extdefs"]
            stats_summary["total_pubdefs"] += stats["pubdefs"]
            stats_summary["total_fixups"] += stats["fixups"]
            stats_summary["total_bytes"] += len(out_bytes)
            stats_summary["total_padding"] += stats["padding"]
            if stats["padding"] > 0:
                padded_items.append((stats["data_name"], stats["padding"]))
            if i % 50 == 0 or i == N:
                print(f"  [{i}/{N}] emit {out_path.name}")
        for entry, fn_info in sorted(builder.fd2common_by_addr.items()):
            i += 1
            out_bytes, stats = builder.emit_fd2common_obj(fn_info)
            out_path = OBJ_DIR / f"fd2common_{fn_info['name']}.obj"
            out_path.write_bytes(out_bytes)
            stats_summary["fd2common_obj"] += 1
            stats_summary["total_extdefs"] += stats["extdefs"]
            stats_summary["total_bytes"] += len(out_bytes)
            if i % 50 == 0 or i == N:
                print(f"  [{i}/{N}] emit {out_path.name}")
        if padded_items:
            print(f"\n=== ISR stack-padded items ({len(padded_items)}) ===")
            for name, pad in padded_items:
                print(f"  {name}: {pad} bytes")
        if subsumed:
            print(f"\n=== Subsumed items ({len(subsumed)}) ===")
            for name in subsumed:
                print(f"  {name}: PUBDEF inside parent .obj")
        print()
        print("=== emit summary ===")
        for k, v in stats_summary.items():
            print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
