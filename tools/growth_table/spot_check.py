#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Emit a human-readable spot-check of the generated growth tables (UTF-8 markdown)
so values can be eyeballed against the game / walkthrough without the web page."""
import json, os
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
d = json.load(open(os.path.join(ROOT, "workspace", "growth_table", "growth_data.json"), encoding="utf-8"))
byid = {c["char_id"]: c for c in d["characters"]}
STATS = ["hp", "mp", "ap", "dp", "dx"]
SL = {"hp": "HP", "mp": "MP", "ap": "AP", "dp": "DP", "dx": "DX"}

def rowsof(seg): return {r["lv"]: r for r in seg["rows"]}

def fmt(r, s):
    mn, mx = r["min"][s], r["max"][s]
    return str(mn) if mn == mx else f"{mn}~{mx}"

def seg_table(seg, levels, out):
    rs = rowsof(seg)
    levels = [L for L in levels if L in rs]
    out.append("| 等級 | " + " | ".join(SL[s] for s in STATS) + " |")
    out.append("|---|" + "---|" * len(STATS))
    for L in levels:
        r = rs[L]
        out.append(f"| LV{L} | " + " | ".join(fmt(r, s) for s in STATS) + " |")
    out.append("")

def dump_char(cid, base_levels, promo_levels, out):
    c = byid[cid]
    out.append(f"## {c['name']}（char_id {cid} · {c['base_job_name']} · {c['race_name']}族 · 初始 LV{c['join_level']} · 上限 LV{c['max_level']}）")
    out.append(f"基礎值 HP{c['base_stats']['hp']} MP{c['base_stats']['mp']} AP{c['base_stats']['ap']} DP{c['base_stats']['dp']} DX{c['base_stats']['dx']}　（數值格式 `最小~最大`，相等時只顯示一個）\n")
    for seg in c["segments"]:
        if seg["kind"] == "base":
            out.append(f"### 基礎職業：{seg['job_name']}（LV{seg['from_level']}→{seg['to_level']}）")
            seg_table(seg, base_levels, out)
        else:
            item = f"（需 {seg['required_item_name']}）" if seg.get("required_item_name") else ""
            out.append(f"### 轉職：{seg['job_name']}{item}（承接基礎職 LV40，LV1→40，移動力+{seg['mv_bonus']}）")
            seg_table(seg, promo_levels, out)
    if c.get("buggy_promotions"):
        for b in c["buggy_promotions"]:
            out.append(f"> ⚠ 機制上可轉「{b['job_name']}」但實際轉職會 crash，不列入。\n")

out = ["# 成長數值 spot-check（節錄里程碑等級）\n",
       "來源：FD2.LE 逆向工程；公式與資料經對抗式驗證（byte-exact + 獨立重算 0 誤差）。\n"]
dump_char(0, [1, 10, 20, 30, 40], [1, 10, 20, 30, 40], out)         # 索爾：劍士→劍聖/英雄
dump_char(9, [1, 10, 20, 30, 40], [1, 10, 20, 30, 40], out)         # 悠妮：法師→大法師/聖者/召喚師
dump_char(17, [21, 30, 40], [1, 20, 40], out)                      # 米亞斯多德：LV21 加入 劍士→龍劍士
dump_char(30, [1, 20, 40, 60, 80, 99], [], out)                    # 蓋亞：LV99
dump_char(16, [2, 20, 40], [], out)                                # 凱拉斯：不可轉職(crash)

path = os.path.join(ROOT, "workspace", "growth_table", "spot_check.md")
open(path, "w", encoding="utf-8").write("\n".join(out))
print("wrote", path)
