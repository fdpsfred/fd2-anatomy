"""Phase B — AIL_* public function self-print verification.

Reads:
  workspace/ail_audit/ail_decomp_dump.json     (Java-side batch decompile + refs)
  workspace/function_review/registry.json      (canonical per-fn registry)
  workspace/function_review/raw/ail_strings.txt (AIL_* string addr -> content)
  workspace/ail_audit/crt_globals_map.json     (known CRT globals to subtract)

Writes:
  workspace/ail_audit/phase_b_public_verify.json
  workspace/ail_audit/phase_b_summary.md
  workspace/ail_audit/ail_globals_map.json
  workspace/ail_audit/ail_string_xrefs.json

Verdict assignment (per plan):
  confirmed_public        — has 1 fprintf with self-print pattern; log helper called
  confirmed_public_multi  — >=2 fprintf, at least one self-print
  public_wrapper          — has self-print + only-_inner callee shape (1-2 fprintf,
                            tail dispatch to AIL_internal_<name>_inner)
  confirmed_public_trivial— no fprintf, instr_count <= 6, leaf (no callees)
  suspect_public          — caller>0 but no clear pattern (manual review)
  defer_F                 — no fprintf + 0 callers (defer to Phase F)

Self-print = decompile contains fprintf(*, "<self-name>(...", ...) where
<self-name> matches function name. Log helper = callee 0x3794c.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
FN_REVIEW = REPO / "workspace" / "function_review"

DUMP = WORK / "ail_decomp_dump.json"
REGISTRY = FN_REVIEW / "registry.json"
AIL_STRINGS = FN_REVIEW / "raw" / "ail_strings.txt"
CRT_GLOBALS = WORK / "crt_globals_map.json"

OUT_VERIFY = WORK / "phase_b_public_verify.json"
OUT_SUMMARY = WORK / "phase_b_summary.md"
OUT_GLOBALS = WORK / "ail_globals_map.json"
OUT_STRING_XREFS = WORK / "ail_string_xrefs.json"

LOG_HELPER_ADDR = "0003794c"  # AIL_internal_log_print_timestamp_prefix

# fprintf(<arg>, "<format>", ...) pattern in Ghidra decompile output
FPRINTF_RE = re.compile(r'fprintf\s*\(\s*[^,]+,\s*"((?:[^"\\]|\\.)*)"')


def parse_ail_strings():
    out = {}
    pat = re.compile(r"^([0-9a-fA-F]{8}):\s*\"(.*)\"$")
    for line in AIL_STRINGS.read_text(encoding="utf-8").splitlines():
        m = pat.match(line.strip())
        if m:
            out[m.group(1).lower()] = m.group(2)
    return out


def crt_global_filter():
    """Return a function (linear_addr_int) -> True if it's a known CRT global."""
    cg = json.loads(CRT_GLOBALS.read_text(encoding="utf-8"))
    iob_lo = int(cg["__iob"]["linear_start"], 16)
    iob_hi = int(cg["__iob"]["linear_end"], 16)
    errno_addr = int(cg["errno"]["linear"], 16)
    closed_addr = int(cg["_ClosedStreams_head"]["linear"], 16)
    open_addr = int(cg["_OpenStreams_head"]["linear"], 16)
    crt_singles = {errno_addr, closed_addr, open_addr}

    def is_crt(addr_int):
        if iob_lo <= addr_int < iob_hi:
            return True
        if addr_int in crt_singles:
            return True
        return False

    return is_crt, {
        "__iob_range": [f"0x{iob_lo:X}", f"0x{iob_hi:X}"],
        "errno": f"0x{errno_addr:X}",
        "_ClosedStreams_head": f"0x{closed_addr:X}",
        "_OpenStreams_head": f"0x{open_addr:X}",
    }


def find_self_print(name: str, dec: str):
    """Return list of (string, kind) tuples for fprintf calls in decompile.

    kind: "self" if string starts with "<name>(", else "other".
    """
    out = []
    if not dec:
        return out
    for m in FPRINTF_RE.finditer(dec):
        s = m.group(1)
        if s.startswith(name + "("):
            out.append((s, "self"))
        else:
            out.append((s, "other"))
    return out


INNER_CALL_RE = re.compile(r"\bAIL_internal_[A-Za-z0-9_]+_inner\b")


def classify(name: str, fn: dict, log_helper_called: bool, fprintfs):
    self_prints = [s for s, k in fprintfs if k == "self"]
    total_prints = len(fprintfs)
    in_degree = len(fn.get("callers", []))
    callees_names = [c["name"] for c in fn.get("callees", [])]
    # Some wrappers tail-jump to *_inner, which Ghidra's getCalledFunctions
    # excludes — also scan the decompile body for AIL_internal_*_inner usage.
    dec = fn.get("decompile", "") or ""
    decompile_inner_calls = set(INNER_CALL_RE.findall(dec))
    has_inner = (any(n.endswith("_inner") for n in callees_names)
                 or len(decompile_inner_calls) > 0)
    instr_count = fn.get("instr_count", 0)
    callees_count = len(fn.get("callees", []))

    if self_prints:
        if total_prints >= 2:
            return "confirmed_public_multi"
        if has_inner and total_prints == 1:
            return "public_wrapper"
        return "confirmed_public"
    # No self-print
    if total_prints == 0:
        if in_degree == 0:
            return "defer_F"
        if instr_count <= 6 and callees_count == 0:
            return "confirmed_public_trivial"
        return "suspect_public"
    # Has prints but none self — odd
    return "suspect_public"


def main():
    dump = json.loads(DUMP.read_text(encoding="utf-8"))
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    ail_strings = parse_ail_strings()

    by_addr_dump = dump["by_addr"]
    reg_by_addr = {r["address"]: r for r in registry}

    # ---- Phase B classification: AIL_* (public, not AIL_internal_*) ----
    public_records = []
    for addr, fn in by_addr_dump.items():
        name = fn["name"]
        if not name.startswith("AIL_") or name.startswith("AIL_internal_"):
            continue

        callees_addrs = {c["addr"].lower() for c in fn.get("callees", [])}
        log_helper_called = LOG_HELPER_ADDR in callees_addrs

        fprintfs = find_self_print(name, fn.get("decompile", ""))
        verdict = classify(name, fn, log_helper_called, fprintfs)

        self_offsets = []
        for m in FPRINTF_RE.finditer(fn.get("decompile", "") or ""):
            s = m.group(1)
            if s.startswith(name + "("):
                # find this string's data addr (any AIL string == s)
                self_str_addr = next(
                    (sa for sa, sc in ail_strings.items() if sc == s), None
                )
                self_offsets.append({
                    "string": s,
                    "string_addr": self_str_addr,
                })

        public_records.append({
            "address": addr.lower(),
            "current_name": name,
            "verdict": verdict,
            "self_prints": self_offsets,
            "total_fprintfs": len(fprintfs),
            "fprintf_strings": [s for s, _ in fprintfs],
            "log_helper_called": log_helper_called,
            "has_inner_callee": any(
                c["name"].endswith("_inner") for c in fn.get("callees", [])
            ),
            "in_degree": len(fn.get("callers", [])),
            "out_degree": len(fn.get("callees", [])),
            "body_size": fn.get("body_size", 0),
            "instr_count": fn.get("instr_count", 0),
        })

    # ---- ail_globals_map.json ----
    is_crt_global, crt_summary = crt_global_filter()

    # Aggregate data refs from {confirmed AIL_* (any of confirmed_/public_wrapper/multi/trivial)} ∪ AIL_internal_*
    confirmed_verdicts = {
        "confirmed_public", "confirmed_public_multi",
        "public_wrapper", "confirmed_public_trivial",
    }
    confirmed_addrs = {r["address"] for r in public_records
                       if r["verdict"] in confirmed_verdicts}

    contributing_addrs = set(confirmed_addrs)
    for addr, fn in by_addr_dump.items():
        if fn["name"].startswith("AIL_internal_"):
            contributing_addrs.add(addr.lower())

    globals_agg = {}  # global_addr (hex8) -> {ref_count, ref_types, referenced_by}
    for addr in sorted(contributing_addrs):
        fn = by_addr_dump.get(addr) or by_addr_dump.get(addr.upper().rjust(8, "0"))
        if fn is None:
            # Try case variants
            for k in by_addr_dump:
                if k.lower() == addr.lower():
                    fn = by_addr_dump[k]
                    break
        if fn is None:
            continue
        for ref in fn.get("data_refs", []):
            tgt_hex = ref["to"].lower()
            if not re.fullmatch(r"[0-9a-f]{8}", tgt_hex):
                continue
            tgt_int = int(tgt_hex, 16)
            if tgt_hex in reg_by_addr:
                continue
            if is_crt_global(tgt_int):
                continue
            # Ignore string refs (these go to ail_string_xrefs.json) — keep them
            # as data globals too, but tag them.
            ent = globals_agg.setdefault(tgt_hex, {
                "ref_count": 0,
                "ref_types": set(),
                "referenced_by": set(),
                "is_string": tgt_hex in ail_strings,
            })
            ent["ref_count"] += 1
            ent["ref_types"].add(ref["type"])
            ent["referenced_by"].add(addr.lower())

    globals_out = {
        "_meta": {
            "input_function_count": len(contributing_addrs),
            "confirmed_public_count": len(confirmed_addrs),
            "ail_internal_count": len(contributing_addrs) - len(confirmed_addrs),
            "excluded_crt": crt_summary,
            "global_count": len(globals_agg),
        },
        "globals": {
            addr: {
                "ref_count": ent["ref_count"],
                "ref_types": sorted(ent["ref_types"]),
                "referenced_by": sorted(ent["referenced_by"]),
                "is_string": ent["is_string"],
            }
            for addr, ent in sorted(globals_agg.items())
        },
    }
    OUT_GLOBALS.write_text(
        json.dumps(globals_out, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    # ---- ail_string_xrefs.json (AIL-prefix strings -> referencing AIL functions) ----
    string_to_fns = {}  # string_addr -> {content, referenced_by: [fn_addrs]}
    for addr in sorted(contributing_addrs):
        fn = None
        for k in by_addr_dump:
            if k.lower() == addr.lower():
                fn = by_addr_dump[k]
                break
        if fn is None:
            continue
        for ref in fn.get("data_refs", []):
            tgt_hex = ref["to"].lower()
            if not re.fullmatch(r"[0-9a-f]{8}", tgt_hex):
                continue
            if tgt_hex in ail_strings:
                ent = string_to_fns.setdefault(tgt_hex, {
                    "content": ail_strings[tgt_hex],
                    "referenced_by": set(),
                    "ref_sites": [],
                })
                ent["referenced_by"].add(addr.lower())
                ent["ref_sites"].append({"from": ref["from"].lower(),
                                          "in_function": addr.lower()})

    ail_strings_out = {
        "_meta": {
            "ail_string_total": len(ail_strings),
            "with_xref_from_ail_function": len(string_to_fns),
        },
        "by_string_addr": {
            sa: {
                "content": ent["content"],
                "referenced_by_function": sorted(ent["referenced_by"]),
                "ref_sites": sorted(ent["ref_sites"], key=lambda x: x["from"]),
            }
            for sa, ent in sorted(string_to_fns.items())
        },
    }
    OUT_STRING_XREFS.write_text(
        json.dumps(ail_strings_out, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    # ---- phase_b_public_verify.json ----
    public_records.sort(key=lambda r: r["address"])
    verdict_counts = {}
    for r in public_records:
        verdict_counts[r["verdict"]] = verdict_counts.get(r["verdict"], 0) + 1

    OUT_VERIFY.write_text(
        json.dumps({
            "_meta": {
                "ail_public_total": len(public_records),
                "log_helper_addr": LOG_HELPER_ADDR,
                "verdict_counts": verdict_counts,
            },
            "records": public_records,
        }, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    # ---- phase_b_summary.md ----
    suspect = [r for r in public_records if r["verdict"] == "suspect_public"]
    defer_f = [r for r in public_records if r["verdict"] == "defer_F"]
    trivial = [r for r in public_records if r["verdict"] == "confirmed_public_trivial"]

    md = []
    md.append("# Phase B — AIL_* public self-print 驗證 summary\n")
    md.append(f"輸入：{len(public_records)} 個 `AIL_*` 公開函式（registry 內 "
              f"name 開頭 `AIL_` 但非 `AIL_internal_`）。\n")
    md.append("Verdict 分布：\n")
    for v in [
        "confirmed_public", "confirmed_public_multi", "public_wrapper",
        "confirmed_public_trivial", "suspect_public", "defer_F",
    ]:
        md.append(f"- `{v}`: {verdict_counts.get(v, 0)}")
    md.append("")

    if suspect:
        md.append(f"\n## suspect_public 列表（{len(suspect)} 個，需使用者裁定）\n")
        md.append("| addr | name | in_deg | out_deg | instr | fprintfs | log_helper |")
        md.append("|---|---|---:|---:|---:|---|---|")
        for r in suspect:
            md.append(
                f"| `0x{r['address']}` | `{r['current_name']}` | "
                f"{r['in_degree']} | {r['out_degree']} | {r['instr_count']} | "
                f"{r['total_fprintfs']} | {'Y' if r['log_helper_called'] else 'N'} |"
            )

    if defer_f:
        md.append(f"\n## defer_F 列表（{len(defer_f)} 個，移交 Phase F）\n")
        md.append("| addr | name | instr | callees |")
        md.append("|---|---|---:|---:|")
        for r in defer_f:
            md.append(
                f"| `0x{r['address']}` | `{r['current_name']}` | "
                f"{r['instr_count']} | {r['out_degree']} |"
            )

    if trivial:
        md.append(f"\n## confirmed_public_trivial 列表（{len(trivial)} 個）\n")
        md.append("| addr | name | instr | in_deg |")
        md.append("|---|---|---:|---:|")
        for r in trivial:
            md.append(
                f"| `0x{r['address']}` | `{r['current_name']}` | "
                f"{r['instr_count']} | {r['in_degree']} |"
            )

    md.append("\n## ail_globals_map.json 規模\n")
    md.append(f"- 投入計算的 AIL function: {globals_out['_meta']['input_function_count']} "
              f"（{globals_out['_meta']['confirmed_public_count']} confirmed_public + "
              f"{globals_out['_meta']['ail_internal_count']} AIL_internal_*）")
    md.append(f"- 識別出的 AIL global 位址: {globals_out['_meta']['global_count']}")
    md.append(f"- 已扣除 CRT global: {list(globals_out['_meta']['excluded_crt'].keys())}")

    md.append("\n## ail_string_xrefs.json 規模\n")
    md.append(f"- 已知 AIL-prefix 字串總數: {ail_strings_out['_meta']['ail_string_total']}")
    md.append(f"- 被 AIL function 引用的字串數: "
              f"{ail_strings_out['_meta']['with_xref_from_ail_function']}")

    OUT_SUMMARY.write_text("\n".join(md) + "\n", encoding="utf-8")

    # ---- console summary ----
    print(f"AIL_* public:           {len(public_records)}")
    for v in [
        "confirmed_public", "confirmed_public_multi", "public_wrapper",
        "confirmed_public_trivial", "suspect_public", "defer_F",
    ]:
        print(f"  {v:30s} {verdict_counts.get(v, 0)}")
    print(f"\nail_globals_map: {globals_out['_meta']['global_count']} globals")
    print(f"ail_string_xrefs: {ail_strings_out['_meta']['with_xref_from_ail_function']} strings")
    print(f"\nwrote:\n  {OUT_VERIFY.relative_to(REPO)}\n  "
          f"{OUT_SUMMARY.relative_to(REPO)}\n  "
          f"{OUT_GLOBALS.relative_to(REPO)}\n  "
          f"{OUT_STRING_XREFS.relative_to(REPO)}")


if __name__ == "__main__":
    main()
