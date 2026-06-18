# rebuild_info/emission/

把 Ghidra 內全 function 的 decompiled state 產出 C source 並重新 compile
為 functionally-equivalent FD2.LE 的 emit pipeline 規格與 metadata。
目標等價層級為 Layer 2（functionally-exact），不追求 Layer 3（byte-exact）。

## 規格

- `pool_routing.md` — 四 pool + 兩維度分類（ail / crt / fd2 / binary_artifact
  × link_vendor_lib / emit_fd2_source / skip_artifact）、entry chain、結局
  cinematic、binary_artifact pool 的 compiler-emit NOP 與 wlink alignment fill、
  CRT 程式碼地理位置
- `pipeline_spec.md` — emit 路由規則 E-1..E-9、fall-through pattern 的 6 種
  模式 (A..F)、binary 等價不變式（目標 = Layer 2 functionally-exact，
  不追求 Layer 3 byte-exact）、Watcom 9.5a string-pool dedup 規則。含實機 playtest
  萃取的規則：E-3b（絕對位址引用改 symbol）、E-8b（BSS tentative scalar 不保證順序/
  相鄰，reader-as-array 必 emit 真 array）、Layer-2 熱迴圈時序例外 —— 三者互引
  `../build_test/playtest_bugs.md`
- `calling_convention.md` — Watcom 32-bit cc ABI 規則（`__watcall` /
  `__cdecl` / `__stdcall`）、disasm signal-based 判斷規則

## 全程式 call graph

不在本資料夾常駐，要的時候即時重生：

```bash
# 1. 從 Ghidra MCP 抓 raw dumps 到 workspace/call_graph/raw/
# 2. python tools/program_analysis/build_call_graph.py
# 產出 workspace/call_graph/{call_graph.json, call_graph.dot, call_graph.md}
```

JSON / DOT schema 與 category × emit_action 對應規則寫在
`tools/program_analysis/build_call_graph.py` 的 `categorise()` /
`emit_action_for()`。
