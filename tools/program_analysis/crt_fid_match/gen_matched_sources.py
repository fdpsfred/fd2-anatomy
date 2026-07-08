#!/usr/bin/env python3
"""matched_function_sources.md 產生器。

從 rebuild_info/crt/lookup_9.5a.json 的 by_address 生成
rebuild_info/crt/matched_function_sources.md 的 generated view：一張逐列表
（每個 FD2.LE CRT function 對應到含 matching obj 的 Watcom lib 與版本）加一張
Aggregate by source lib + version 彙總表。表格內容全部由 JSON 機械產生，勿手改。

正典計數以 lookup_9.5a.json 的 by_address 條目數為準。

用法:
    python gen_matched_sources.py           # 寫回 rebuild_info/crt/matched_function_sources.md
    python gen_matched_sources.py --stdout   # 印到 stdout（供 diff）
    python gen_matched_sources.py --check     # 只比對現檔是否與重生結果一致（CI/regression）
"""
import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
LOOKUP = REPO_ROOT / "rebuild_info" / "crt" / "lookup_9.5a.json"
OUT = REPO_ROOT / "rebuild_info" / "crt" / "matched_function_sources.md"


def addr_key(k: str) -> int:
    return int(k, 16)


def load_entries():
    d = json.loads(LOOKUP.read_text(encoding="utf-8"))
    ba = d["by_address"]
    # 逐列表依 FD2 位址遞增排序（by_address 檔內順序非遞增）。
    return [(k, ba[k]) for k in sorted(ba.keys(), key=addr_key)], ba


def render_rows(entries) -> list:
    lines = []
    lines.append("| FD2 addr | lib symbol | body | verified | source obj | source lib(s) / versions |")
    lines.append("|---|---|---:|---|---|---|")
    for k, e in entries:
        addr = "0x%08x" % addr_key(k)
        # 名稱欄用凍結的 `name`（見 #31：disputed entry 的 current_name 另記，name 欄保留正典）。
        name = e["name"]
        body = e.get("body_size", "")
        verified = e.get("verified", "")
        obj = e.get("source_obj", "")
        summary = e.get("source_summary", "")
        lines.append(f"| `{addr}` | `{name}` | {body} | {verified} | `{obj}` | {summary} |")
    return lines


def render_aggregate(entries) -> list:
    # 每個 function 依其 source_libs 內每個唯一 (lib, version) 計一次；example 取
    # 該群組內位址最小的前 5 個 name。entries 已依位址遞增，故 append 順序即位址序。
    groups = {}  # (lib, version) -> [name, ...]
    for k, e in entries:
        seen = set()
        for L in e.get("source_libs", []):
            key = (L.get("lib"), L.get("version"))
            if key in seen:
                continue
            seen.add(key)
            groups.setdefault(key, []).append(e["name"])

    lines = []
    lines.append("## 依 source lib + 版本彙總")
    lines.append("")
    lines.append(
        "每個 (lib, 版本) 列出「該 lib 在該 Watcom 9.5x 子版本內有一個 obj 貢獻」的 FD2 function。"
    )
    lines.append("")
    lines.append("| lib | version | function count | examples |")
    lines.append("|---|---|---:|---|")
    for (lib, ver) in sorted(groups.keys()):
        names = groups[(lib, ver)]
        cnt = len(names)
        shown = names[:5]
        ex = ", ".join(f"`{n}`" for n in shown)
        if cnt > 5:
            ex += f" (+{cnt - 5} more)"
        lines.append(f"| {lib} | {ver} | {cnt} | {ex} |")
    return lines


def build(entries, ba) -> str:
    total = len(ba)
    out = []
    out.append("# Matched Function Sources（CRT 符號 ↔ Watcom lib 對照，generated view）")
    out.append("")
    out.append("**本檔是由 `rebuild_info/crt/lookup_9.5a.json` 的 `source_libs` 欄產生的")
    out.append("generated view，請勿手改表格內容**；重生方式見")
    out.append("`tools/program_analysis/crt_fid_match/_index.md`。每列把一個 FD2.LE function")
    out.append("address 對應到含 matching obj 的 Watcom lib 與版本，供 build pipeline 決定哪個")
    out.append(".obj 該 EXTDEF 哪個 lib 解析。手寫的 CRT 命名約定與 `crt_equivalent_*` / `fd2_*`")
    out.append("primitive 具名清單在 `symbol_inventory.md`。")
    out.append("")
    out.append(f"Total entries（= `lookup_9.5a.json` 的 `by_address` 條目數）: **{total}**")
    out.append("")
    out.append(
        f"> 本檔的逐列表與彙總數字是 `lookup_9.5a.json` 的生成檢視，正典計數以該 JSON 為準"
        f"（`by_address` = {total}）。表格由 `tools/program_analysis/crt_fid_match/gen_matched_sources.py`"
        f" 重生（見該 `_index.md`）；重生後逐列/彙總必與 {total} 一致。"
    )
    out.append("")
    out.extend(render_rows(entries))
    out.append("")
    out.append("註：`__int7 @ 0x49D98`（emu387.obj）body 內部的")
    out.append("`crt_emu387_int7_fptan_opcode_worker_4c630 @ 0x4C630` 是 `__int7` 的內部")
    out.append("subroutine，無獨立 PUBDEF，靠連結同一 `__int7` module 解析，不 emit C source。")
    out.append("歸屬詳見 `symbol_inventory.md` 的 fptan worker 段。")
    out.append("")
    out.extend(render_aggregate(entries))
    out.append("")
    out.append("## 讀表說明")
    out.append("")
    out.append("- 同一個 FD2 function 若對應 ≥ 1 個 (lib, version)，代表該 obj 的 SHA 在多個")
    out.append("  Watcom 9.5x 版本間相同（obj 位元組跨版本未變時的常態）。")
    out.append("- 某 obj 同時出現在多個 lib（例如 `dosinite.obj` 同時在 MATH387R 與 MATH387S，")
    out.append("  位元組相同、ABI-independent），代表該 obj 為 register-call 與 stack-call 兩種")
    out.append("  lib 變體共用，build pipeline 用任一 lib EXTDEF 都能正確解析。")
    out.append("- 「obj 只出現在 9.5+9.5a」是 FD2 連結版本的判定訊號（例如 `dosinite.obj` /")
    out.append("  `ftos.obj`）；完整的版本判定結論見 `fid_match.md`。")
    out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stdout", action="store_true", help="印到 stdout 不寫檔")
    ap.add_argument("--check", action="store_true", help="比對現檔是否與重生結果一致")
    args = ap.parse_args()

    entries, ba = load_entries()
    text = build(entries, ba)

    if args.check:
        cur = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if cur == text:
            print("OK: matched_function_sources.md 與重生結果一致")
            return 0
        print("DRIFT: matched_function_sources.md 與重生結果不一致，請跑 gen_matched_sources.py 重生", file=sys.stderr)
        return 1

    if args.stdout:
        sys.stdout.write(text)
        return 0

    OUT.write_text(text, encoding="utf-8")
    print(f"wrote {OUT} ({len(entries)} rows)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
