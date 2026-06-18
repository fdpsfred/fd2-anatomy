export const meta = {
  name: 'fd2-data-emit',
  description: 'FD2 Phase 1 data landing: per-symbol caller analysis + real-byte emit + verify_real byte gate + independent review, then per-symbol land+commit (globals/testglob/routing, NO build) so a limit/disconnect wastes nothing; one build_test gate per home file at the end. One symbol at a time (serial within a file).',
  phases: [
    { title: 'Emit', detail: 'per symbol: emit (caller analysis + bytes) -> review -> iterate' },
    { title: 'Finalize', detail: 'per symbol: land+commit (globals/testglob/routing, no build); per home file: one build_test gate + mechanical fixes' },
  ],
}

// ---------------------------------------------------------------------------
// args (the main agent reads data_routing.json + a partition manifest and
// passes the work-list in, because workflow scripts have no filesystem access):
//   { root: "<ABS checkout path>",   // main repo or a worktree; all paths absolute
//     label: "pilot" | "data-p1" ...,
//     files: [ { home: "table/audtab.c",
//                symbols: [ { name, addr, segment, len, datatype, kind,
//                             emit_class, needs_bytes, writers[], note } ] } ],
//     maxRounds: 10 }
// Within a file, symbols are processed serially (same-file writes). globals.h /
// testglob.c / build are touched ONCE per file at finalize. Run one workflow per
// worktree for cross-partition parallelism.
// ---------------------------------------------------------------------------
const A = (typeof args === 'string' && args.length) ? JSON.parse(args) : (args || {})
const ROOT = A.root
const FILES = (A && A.files) || []
const MAX_ROUNDS = (A && A.maxRounds) || 10
const MIN_BUDGET_PER_SYM = (A && A.minBudgetPerSym) || 250000
if (!ROOT) { log('args.root missing (absolute checkout path required)'); return { error: 'no root' } }
if (!FILES.length) { log('args.files empty — nothing to do.'); return { error: 'empty worklist', argsType: typeof args } }

const ENV = [
  'environment: Ghidra 已開啟 FD2.LE（單一 program）。呼叫 Ghidra MCP 時 program 參數留空。',
  '先用 ToolSearch 一次載入所需 Ghidra 工具：',
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__read_memory,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_xrefs_from,mcp__ghidra__decompile_function,mcp__ghidra__disassemble_function,mcp__ghidra__get_function_by_address,mcp__ghidra__get_struct_layout,mcp__ghidra__get_function_signature,mcp__ghidra__set_plate_comment,mcp__ghidra__open_program,mcp__ghidra__get_current_program_info"',
  '用到不熟/沒把握的 Ghidra MCP function 前，先讀 `.claude/skills/ghidra-usage/` 確認正確參數格式，不要猜。',
  '本次 checkout 根目錄（所有檔案路徑都用這個絕對路徑前綴，不要用相對路徑、不要 cd 到別處）：',
  'ROOT = ' + ROOT,
].join('\n')

const SOP = [
  '# FD2 data-emit 鐵則（emitter / reviewer 共用）',
  '- 只處理被指派的這一個 data symbol；不順手、不批次其他符號。',
  '- **caller 分析是型別/形狀的最終權威**：實際反編譯/反組譯使用該符號的 caller，從它如何存取（base+idx*stride、.field、MOV 的 8/16/32 位元寬度、有號無號）反推真實型別/維度/struct。Ghidra 宣告的 datatype、以及 testglob.c 現有假定義、globals.h 現有 extern 都只是「強提示」，可能標錯，最終以 caller 用法為準。',
  '- **Layer-2 等價即可，不追 byte-exact layout**：emit 一般 C 定義讓 linker 擺放，不要 #pragma data_seg / 不要硬塞 segment（見 memory feedback_layer2_no_byte_exact_overengineering）。',
  '- **const-ness 由 Ghidra 遊戲端 write-xref 決定，禁為測試 demote**：遊戲程式碼從不寫該 data（get_xrefs_to 全 READ/DATA）＝真唯讀＝**必須 const**。**絕對禁止為了讓「會寫它的測試」能編譯，而把 const 改 non-const**（那是扭曲 emit code 迎合 test）。測試寫 const 表 → data 維持 const、把該測試 #if0 SKIP（鎖定決策①、Phase 3 重寫）。真懷疑 mutable 才去查遊戲端 write-xref，有遊戲 writer 才改 init-data/zero-bss（連 routing emit_class 一起修）。見 memory feedback_const_data_never_demote_for_tests。',
  '- 符號名與 Ghidra byte-identical；C89（變數/檔案範圍宣告在最前）；新檔名 8.3。',
  '- 程式碼內文字一律 ASCII（註解/字串）；唯中文角色/道具/法術專名可維持中文。',
  '- **發現原分類/型別錯就當場修，不留 TODO**：若 caller 證據推翻 emit_class（例如標 const 卻有 writer→改 init-data；標 zero-bss 卻有非零靜態初值→改 const/init-data 去抽 byte 驗；其實是 function-pointer 表→改 emit 函式名初始化列），就改正。若 Ghidra 的 datatype/plate 本身與 assembly 事實不符 → set_plate_comment / 修型別，並同步 globals.h / KB（見 memory feedback_misclassification_root_cause、feedback_modification_sync_mandatory）。',
  '- 絕不半成品（改名/static 充數/空殼/_impl）；絕不為遷就測試而扭曲定義。禁 workaround，只修 root cause。',
  '- Ghidra 連線失敗：某 MCP 呼叫失敗或回任何「無法連線/中斷/逾時」→ 先快速重試該呼叫一次；仍失敗才設 ghidra_unreachable=true（bool）+ ghidra_error_detail（string）給外層 watchdog，並停止本符號。',
].join('\n')

const NEED_KB = [
  '需要時才讀（不要全文預載）：src/include/types.h（struct 定義如 item_effect_entry / spell_learning_entry / runtime_char）、',
  'rebuild_info/emission/pipeline_spec.md、program_info/ 對應子系統（語意）。',
].join('\n')

function symHead(sym) {
  return [
    '目標 data symbol：',
    '- name: ' + sym.name,
    '- addr: 0x' + sym.addr + '   segment: ' + sym.segment,
    '- Ghidra len: ' + sym.len + '   Ghidra datatype(提示): ' + sym.datatype + '   kind: ' + sym.kind,
    '- 提議 emit_class: ' + sym.emit_class + '   needs_bytes: ' + sym.needs_bytes,
    '- 提議 home 檔: src/' + sym.home,
    '- writers(提示): ' + ((sym.writers && sym.writers.join(', ')) || '(none detected)'),
    '- note: ' + (sym.note || ''),
  ].join('\n')
}

function emitterPrompt(sym, mode, verdict) {
  const head = [
    ENV, '', SOP, '', NEED_KB, '',
    '# 角色：Emitter（' + (mode === 'fix' ? 'fix 回合' : 'emit 回合') + '）。以最高嚴謹度執行。',
    symHead(sym),
  ].join('\n')

  let body
  if (mode === 'fix') {
    body = [
      '',
      '# 本回合：修正 reviewer 提出的 blocking issue（不重做無關部分）',
      'reviewer verdict（逐條處理）：',
      JSON.stringify((verdict && verdict.blocking_issues) || [], null, 2),
      '對每條：要嘛已修（說明怎麼修 + evidence），要嘛不同意（附 assembly/decomp evidence 主張無需改）。',
    ].join('\n')
  } else {
    body = [
      '',
      '# 本回合：把這一個 symbol 的真實定義落地到 src/',
      'A. 三源證據：get_plate_comment(0x' + sym.addr + ')；read_memory(0x' + sym.addr + ', ' + (sym.len > 0 ? sym.len : '先讀足夠長度再判 extent') + ')；get_xrefs_to(0x' + sym.addr + ') 列出所有 reader/writer。',
      'B. caller 分析（最關鍵）：反編譯/反組譯 2-3 個代表性 caller（表→看 accessor 怎麼 index；state→看 writer/reader 怎麼存取），反推真實元素型別/stride/維度/struct/寬度/有號無號。對照 ROOT/src/include/globals.h 現有 extern 與 ROOT/tests/testglob.c 現有假定義（強提示但非權威）。判定最終 emit_class 與 C 型別。',
      'C. 依類別 emit C 定義。**落點路徑就是 ROOT/src/' + sym.home + ' 這個確切路徑**（home 是權威）：若父目錄不存在先 mkdir -p（例如 ROOT/src/table/），若檔不存在就新建（檔頭 #include "types.h" 與 "globals.h" + 一行檔案用途註解），存在就 append。**嚴禁**用 tests/where.py / genbuild / 任何 heuristic 改變落點，**嚴禁**寫到 src/undefined/、任何 writer 函式所屬的檔、或其他資料夾——只能寫這個 home 檔。',
      'C-append 鐵則（最高優先，違反會毀掉整檔，已發生過真實事故）：本 home 檔的多個符號是序列逐一 emit、共用同一個檔。若檔已存在，你**只能附加，不能覆寫**：先用 Read 讀出整檔現有內容，再用 Edit 在「最後一個既有定義之後」插入你這一個新定義（或用前景 shell `printf ... >> ROOT/src/' + sym.home + '`）。**絕對禁止用 Write 覆寫整檔**——覆寫會抹掉同檔其他符號已寫好的定義（曾發生整檔定義被覆寫到只剩最後 1 個）。寫完後**必跑前景** grep 自驗：`grep -c "\\b<你的符號名>\\b" ROOT/src/' + sym.home + '` 要 >=1（你的定義在檔內），且整檔行數不得比你動手前變短（代表沒把別人的定義弄不見）。**zero-bss 符號（needs_bytes=false、無 byte gate）尤其危險**：沒有 byte gate 把關，你更要親自 grep 確認定義真的寫進檔了，絕不能只做完分析就報 status=done。',
      '   - const flat 表：const <elem_type> ' + sym.name + '[N] = { read_memory 的真實 bytes，逐 byte 或逐元素 };（elem_type 用 caller 證實的型別，預設 uint8；uint16/uint32 視存取寬度；struct array 若 accessor 用 .field → 用該 struct 型別、依 get_struct_layout 逐欄位排出 byte-exact initializer）。',
      '   - init-data（有 writer + 非零）：<type> ' + sym.name + ' = { 真實 bytes };（可變，放 owner 檔）。',
      '   - zero-bss：<type> ' + sym.name + ';（或 [N]）；務必從 caller 確認真的零初始化（首次使用是寫入 / 執行期才填）。',
      '   - fn-ptr 表（kind=ptr_table 指向 functions）：read_memory 取每個 4-byte LE entry → get_function_by_address 解析成函式名 → <ret> (*' + sym.name + '[N])(<params>) = { fn0, fn1, ... };（函式原型應已在 protos.h）。',
      '   - data-ptr 表（kind=ptr_table 指向 data）：get_xrefs_from 解析每個目標 → 連同目標資料一起 emit（目標用具名符號，名稱與 Ghidra 一致），表內以 & 具名目標引用。',
      '   - sublabel（len=-1）：先用 Ghidra（下一個符號位址 / 使用範圍）判定 extent，再依上述類別處理。',
      'D. byte gate（needs_bytes 且為 flat 表/scalar）：跑前景指令',
      '     python ' + ROOT + '/tools/data_emit/verify_real.py --one ' + sym.name + ' <你 read_memory 讀到的連續 hex> ' + ROOT + '/src/' + sym.home + ' <element_width>',
      '   （element_width: uint8=1 / uint16=2 / uint32=4；struct array 此 flat 工具不適用，改逐欄位人工對 read_memory bytes 核對並在 notes 說明）。必須 PASS。',
      'E. 本回合**不要**動 globals.h / testglob.c / 不要跑 build（那是 finalize 的事）。但要在輸出的 extern_needed 寫明 finalize 該把 globals.h 的 extern 改成什麼（const 表要 `extern const <type> ' + sym.name + '[N];`、維度正確）。',
    ].join('\n')
  }

  const tail = [
    '',
    '# 收尾：若三源比對發現 Ghidra plate/datatype 與事實不符 → set_plate_comment / 修型別 並同步 KB（reviewer 會用 git diff 一併檢查）。',
    '# 輸出（最後一則訊息＝下列 JSON）：',
    'name, emit_class_final, type_decided, shape(flat_const|struct_const|init_data|zero_bss|fnptr_table|dataptr_table), caller_evidence(引 decomp/asm 證明型別/維度), ghidra_hex(needs_bytes 時填你讀到的連續 hex，否則 ""), verify_pass(bool), home_file, def_text(你寫進去的定義), extern_needed(string), testglob_fake_present(bool), reclassified(bool), reclassify_reason, ghidra_changes[], status(done|skip|blocked), ghidra_unreachable(bool), ghidra_error_detail(string), notes。',
  ].join('\n')

  return head + '\n' + body + '\n' + tail
}

function reviewerPrompt(sym, emitterOut) {
  return [
    ENV, '', SOP, '', NEED_KB, '',
    '# 角色：Reviewer（獨立驗證）。不信任 emitter 的結論，也不信任 Ghidra decompiled C 與現有 testglob/globals 型別——所有判定自己從 assembly + plate + read_memory + caller 重新確認。surface verify（只查名字存在）無效。',
    symHead(sym),
    emitterOut ? ('Emitter 本回合回報（僅供定位，不可當證據）：\n' + JSON.stringify(emitterOut, null, 2)) : '',
    '',
    '# 步驟',
    'A. 看 emitter 的精確改動：git -C ' + ROOT + ' --no-pager diff HEAD -- src/' + sym.home + '（序列下 HEAD 之後未 commit 的就是本符號的，含新建檔）。讀 ROOT/src/' + sym.home + ' 確認定義真的寫進去了。',
    'B. 自己抓證據：get_plate_comment / read_memory(0x' + sym.addr + ') / get_xrefs_to；反編譯 1-2 個 caller。',
    'C. 逐項 checklist（每項 通過/不通過 + evidence 引指令或行）：',
    '   1. 型別/寬度/有號：與 caller 實際存取一致（base+idx*stride、.field、MOV 寬度）。',
    '   2. 維度/長度：elem_size × N == Ghidra len（needs_bytes）；或形狀符合 caller。',
    '   3. emit_class 正確：真的無 writer 才 const；真的有 writer 才 init-data；真的零初值才 zero-bss。emitter 若 reclassify 過，確認理由成立。',
    '   4. zero-bss：零初始化確實正確（沒有被靜態初值漏掉）。',
    '   5. byte gate（needs_bytes flat 表/scalar）：**自己** read_memory 取 hex（不要用 emitter 的），自己跑 python ROOT/tools/data_emit/verify_real.py --one ' + sym.name + ' <你讀的 hex> ROOT/src/' + sym.home + ' <width> → 必須 PASS。struct 表則自己抽幾個欄位對 read_memory bytes 核對。',
    '   6. 符號名 byte-identical；extern_needed 計畫正確（const+維度）；C89；新檔 8.3。',
    '   7. Ghidra 事實：plate/datatype 與 assembly 一致；發現過時/錯誤 → blocking（fix_suggestion 寫明 set_plate_comment / 改型別 + 同步）。',
    '',
    '# 共識原則：只 block「型別/維度/byte/分類錯」「破壞 Layer-2 等價/編譯」「違反鐵則（半成品/跳步/符號名不符/未 root-cause）」；等價但寫法不同 → non_blocking_notes。不確定 → discussion_for_emitter 要 evidence，不直接 block。',
    '# 輸出（最後一則訊息＝下列 JSON）：approved(bool), ghidra_unreachable(bool), ghidra_error_detail(string), byte_gate_pass(bool|null), diff_reviewed(bool), blocking_issues[{severity,checklist_item,location,claim,evidence,fix_suggestion}], non_blocking_notes[], kb_plate_findings[], discussion_for_emitter。',
  ].join('\n')
}

function landerPrompt(sym, emitterOut) {
  return [
    ENV, '',
    '# 角色：Lander（per-symbol 落地 + commit；**不 build**）。本符號已 emit + reviewer-approved。只做機械收尾，**不改已 approved 的定義邏輯、不動別的符號、不改任何測試行為**。目的：每個符號一完成就 commit，撞 limit 時零浪費。',
    '目標符號：' + sym.name + '（home：ROOT/src/' + sym.home + '；emit_class：' + ((emitterOut && (emitterOut.emit_class_final || emitterOut.emit_class)) || sym.emit_class) + '）',
    'emitter 回報（定位用，非權威）：' + JSON.stringify({ extern_needed: emitterOut && emitterOut.extern_needed, testglob_fake_present: emitterOut && emitterOut.testglob_fake_present }, null, 2),
    '',
    '步驟（檔案路徑全用 ROOT 絕對前綴；git 一律用 git -C ' + ROOT + '）：',
    '1. 先驗定義在位：跑前景 `grep -c "\\b' + sym.name + '\\b" ROOT/src/' + sym.home + '` 必 >=1。若不在（emitter 沒寫成或被覆寫）→ status=blocked、committed=false、**不要自己補 emit、不要 commit**，回報讓外層處理。',
    '2. ROOT/src/include/globals.h：把 ' + sym.name + ' 的 extern 改成與定義「完全一致」（const 定義→ extern 也要 const、維度 [N] 正確；非 const 定義→ 非 const extern）。**Watcom 9.5a E1129：extern 與定義的 const 修飾或型別不一致是 HARD error，必須完全一致**。只用 whole-word 改這一行；extern 不存在就新增一行。',
    '3. ROOT/tests/testglob.c：若有 ' + sym.name + ' 的假 file-scope 定義（zero-fill/假初值）就移除那一段；只刪這一個符號，別誤刪相鄰符號或測試基建。沒有就跳過。',
    '4. 跑前景 `python ROOT/tests/genbuild.py --apply`（idempotent：掃 src/+tests/ 重產 build.bat/test.lnk/testmain.c；home 是新檔會接進去、已接過則無變動。嚴禁手改這三檔）。',
    '5. ROOT/src/data_routing.json：把 ' + sym.name + ' 這一個 entry 設 emitted=true、reviewed=true（json.load → 只改這個 entry → json.dump(indent=2, ensure_ascii=False) + 結尾換行）。',
    '6. **clobber 防線（必做）**：`git -C ' + ROOT + ' add -A` 後跑 `git -C ' + ROOT + ' diff --cached -- src/' + sym.home + ' | grep "^-" | grep "data_fd2_"`。若這個指令有輸出（代表本次 staged 把別的 data_fd2_ 定義行刪掉了）→ 立刻 `git -C ' + ROOT + ' checkout -- src/' + sym.home + '` 還原、status=blocked、committed=false、回報，**不要 commit**。沒有輸出才繼續。',
    '7. commit：`git -C ' + ROOT + ' commit`，訊息第一行：',
    '   data-emit: ' + sym.name + ' (src/' + sym.home + ', reviewed)',
    '   後接空行 + 一行摘要 + 空行 + Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>',
    '   （staged 範圍：src/' + sym.home + '、src/include/globals.h、tests/testglob.c、src/data_routing.json，genbuild 動到的 tests/build.bat tests/test.lnk tests/testmain.c，連帶這個符號的 KB 改動。）',
    '8. **不要 build**（整檔最後由 BuildGate 做一次）；**不要改任何測試邏輯/行為**（const-writer 衝突留給 BuildGate）。',
    '輸出（最後一則訊息＝此 JSON）：name, committed(bool), commit_hash, def_present(bool), status(done|blocked), notes。',
  ].join('\n')
}

function buildGatePrompt(file, names) {
  return [
    ENV, '',
    '# 角色：BuildGate（per-home-file build 驗證 + 純機械修正）。本 home 檔的符號已逐一 land+commit（都在 git 內了）。現在做整檔 build gate；**不回退任何已 land 的 commit、不改定義邏輯**。',
    'home 檔：ROOT/src/' + file.home + '；本批已 land 的符號（' + names.length + '）：' + JSON.stringify(names),
    '',
    '步驟（git 一律用 git -C ' + ROOT + '）：',
    '1. build gate（前景，嚴禁背景）：`python ROOT/tools/code_emit/build_test.py --changed "src/' + file.home + ',src/include/globals.h,tests/testglob.c"`。通過 = error_count==0 且 warning_count==0（run 階段 hang/fail 一律忽略；link 階段 W1027 redefinition 是預期 cascade、不計入 warning_count）。',
    '2. 若 error_count==0 且 warning_count==0：什麼都不用改，回報 build pass（已 land 的 commit 就是最終狀態）。',
    '3. 若 error/warning>0：只做**純機械修正**，改完另起一個 commit。允許的修正只有：',
    '   (a) 型別/const 對齊：測試檔內 redundant 區域 `extern <type> <name>...` 與 canonical globals.h 不符（如少 const）→ 對齊；globals.h 某 extern 與定義的 const/維度不符（E1129）→ 對齊成與定義一致。',
    '   (b) **const-writer 衝突一律 SKIP，絕禁 demote const**：測試「寫入已 const 化的表」造成 compile error → **嚴禁把該 data 改成 non-const 來過 build（那是扭曲 emit 迎合 test）、也不改測試邏輯**，直接用 `#if 0`/`#endif` 把該 static 測試 function 整段 + 其 RUN_TEST 行包住，上方加 ASCII 標記 `/* SKIP (Phase 3): writes now-const <table>; restore + rewrite to drive real data */`；記進 skipped_tests[]。',
    '   (c) 其它純型別 compile error（discard-const 取址等）→ 機械修正（沿用既有顯式 cast precedent）。',
    '   會碰到測試「行為」（重設 fixture / 換 driver / 改斷言）的 → 不要動，記 needs_user。修完 `git -C ' + ROOT + ' commit`（訊息：build-gate: src/' + file.home + ' (fixes)）。',
    '4. 回報（最後一則訊息＝此 JSON）：home, build{error_count,warning_count}, fixed(bool), commit_hash(若有修正), skipped_tests[], needs_user[], notes。',
  ].join('\n')
}

const EMITTER_SCHEMA = {
  type: 'object',
  required: ['name', 'status', 'verify_pass'],
  properties: {
    name: { type: 'string' }, emit_class_final: { type: 'string' }, type_decided: { type: 'string' },
    shape: { type: 'string' }, caller_evidence: { type: 'string' }, ghidra_hex: { type: 'string' },
    verify_pass: { type: 'boolean' }, home_file: { type: 'string' }, def_text: { type: 'string' },
    extern_needed: { type: 'string' }, testglob_fake_present: { type: 'boolean' },
    reclassified: { type: 'boolean' }, reclassify_reason: { type: 'string' },
    ghidra_changes: { type: 'array', items: { type: 'string' } },
    status: { type: 'string', enum: ['done', 'skip', 'blocked'] },
    ghidra_unreachable: { type: 'boolean' }, ghidra_error_detail: { type: 'string' }, notes: { type: 'string' },
  },
}
const REVIEWER_SCHEMA = {
  type: 'object',
  required: ['approved', 'blocking_issues'],
  properties: {
    approved: { type: 'boolean' }, ghidra_unreachable: { type: 'boolean' }, ghidra_error_detail: { type: 'string' },
    byte_gate_pass: { type: ['boolean', 'null'] }, diff_reviewed: { type: 'boolean' },
    blocking_issues: { type: 'array', items: { type: 'object', properties: { severity: { type: 'string' }, checklist_item: {}, location: { type: 'string' }, claim: { type: 'string' }, evidence: { type: 'string' }, fix_suggestion: { type: 'string' } } } },
    non_blocking_notes: { type: 'array', items: { type: 'string' } },
    kb_plate_findings: { type: 'array', items: { type: 'string' } },
    discussion_for_emitter: { type: 'string' },
  },
}
const LANDER_SCHEMA = {
  type: 'object',
  required: ['name', 'committed'],
  properties: {
    name: { type: 'string' }, committed: { type: 'boolean' }, commit_hash: { type: 'string' },
    def_present: { type: 'boolean' }, status: { type: 'string' }, notes: { type: 'string' },
  },
}
const BUILDGATE_SCHEMA = {
  type: 'object',
  required: ['home'],
  properties: {
    home: { type: 'string' },
    build: { type: 'object', properties: { error_count: { type: ['number', 'null'] }, warning_count: { type: ['number', 'null'] } } },
    fixed: { type: 'boolean' }, commit_hash: { type: 'string' },
    skipped_tests: { type: 'array' }, needs_user: { type: 'array' }, notes: { type: 'string' },
  },
}

function agentReportsGhidraDown(out) { return !!out && out.ghidra_unreachable === true }
async function runAgent(promptStr, opts) {
  const out = await agent(promptStr, opts)
  if (agentReportsGhidraDown(out)) {
    const detail = (out && out.ghidra_error_detail) ? String(out.ghidra_error_detail) : '(no detail)'
    const e = new Error('GHIDRA_DISCONNECT: ' + detail); e.ghidra = true; e.ghidraDetail = detail; throw e
  }
  return out
}

const results = []
let stopped = null
let batchStart = 0
try { batchStart = budget.spent() } catch (e) { batchStart = 0 }
const k = () => { try { return Math.round((budget.spent() - batchStart) / 1000) } catch (e) { return -1 } }
const kStr = (x) => (x < 0 ? 'n/a' : '~' + x + 'k')

let symTotal = 0
for (const f of FILES) symTotal += f.symbols.length
let symDone = 0

outer:
for (let fi = 0; fi < FILES.length; fi++) {
  const file = FILES[fi]
  const landed = []
  for (let si = 0; si < file.symbols.length; si++) {
    const sym = file.symbols[si]
    sym.home = file.home   // home lives on the file unit, not per-symbol; propagate so
                           // symHead/emitter/reviewer/lander prompts get the real path
    symDone++
    const tag = '[' + symDone + '/' + symTotal + '] ' + sym.name + ' (' + file.home + ')'
    if (budget.total && budget.remaining() < MIN_BUDGET_PER_SYM) {
      log('budget low: ' + Math.round(budget.remaining() / 1000) + 'k left; stopping before ' + tag + '. re-run skips committed (data_routing.reviewed).')
      stopped = 'budget'; break outer
    }
    log(tag + ' — emit start [batch ' + kStr(k()) + ']')
    try {
      let emitterOut = await runAgent(emitterPrompt(sym, 'emit', null), { schema: EMITTER_SCHEMA, label: 'emit:' + sym.name, phase: 'Emit' })
      let verdict = await runAgent(reviewerPrompt(sym, emitterOut), { schema: REVIEWER_SCHEMA, label: 'review:' + sym.name, phase: 'Emit' })
      let round = 0
      while (verdict && !verdict.approved && round < MAX_ROUNDS) {
        log(tag + ' — fix round ' + (round + 1) + ' (' + ((verdict.blocking_issues || []).length) + ' blocking)')
        emitterOut = await runAgent(emitterPrompt(sym, 'fix', verdict), { schema: EMITTER_SCHEMA, label: 'fix:' + sym.name + ':' + (round + 1), phase: 'Emit' })
        verdict = await runAgent(reviewerPrompt(sym, emitterOut), { schema: REVIEWER_SCHEMA, label: 'rereview:' + sym.name + ':' + (round + 1), phase: 'Emit' })
        round++
      }
      if (verdict && verdict.approved) {
        // per-symbol land + commit (NO build) -- a committed symbol survives a limit/disconnect
        const land = await agent(landerPrompt(sym, emitterOut), { schema: LANDER_SCHEMA, label: 'land:' + sym.name, phase: 'Finalize' })
        if (land && land.committed) {
          landed.push(sym.name)
          results.push({ name: sym.name, home: file.home, status: 'committed', rounds: round, commit: land.commit_hash, reclassified: !!(emitterOut && emitterOut.reclassified) })
          log(tag + ' — COMMITTED ' + (land.commit_hash || '') + ' (' + round + ' fix round(s))')
        } else {
          results.push({ name: sym.name, home: file.home, status: 'land_failed', rounds: round, land_notes: (land && land.notes) || '(no detail)' })
          log(tag + ' — LAND FAILED: ' + ((land && land.notes) || '(no detail)'))
        }
      } else {
        results.push({ name: sym.name, home: file.home, status: 'needs_user', rounds: round, blocking_issues: (verdict && verdict.blocking_issues) || [] })
        log(tag + ' — NOT approved after ' + round + ' rounds — left for user')
      }
    } catch (e) {
      const msg = String((e && e.message) || e); const isG = !!(e && e.ghidra)
      log(tag + (isG ? ' — GHIDRA DISCONNECT: ' : ' — INTERRUPTED: ') + msg)
      results.push({ name: sym.name, home: file.home, status: isG ? 'ghidra_disconnect' : 'interrupted', error: msg })
      stopped = isG ? 'ghidra_disconnect' : 'interrupt'; break outer
    }
  }
  if (landed.length) {
    log('BUILD GATE ' + file.home + ' — ' + landed.length + ' committed symbol(s) [batch ' + kStr(k()) + ']')
    try {
      const bg = await agent(buildGatePrompt(file, landed), { schema: BUILDGATE_SCHEMA, label: 'build:' + file.home, phase: 'Finalize' })
      const bok = !!(bg && bg.build && bg.build.error_count === 0 && bg.build.warning_count === 0)
      results.push({ home: file.home, status: 'build_gate', build_pass: bok, build: (bg && bg.build) || null, fixed: !!(bg && bg.fixed), skipped_tests: (bg && bg.skipped_tests) || [], needs_user: (bg && bg.needs_user) || [] })
      log('BUILD GATE ' + file.home + ' — ' + (bok ? 'PASS' : 'see report') + (bg && bg.fixed ? ' (mechanical fixes committed)' : ''))
    } catch (e) {
      // symbols are already committed (reviewed); a build-gate agent error does NOT waste
      // emit work. record and continue (final per-worktree build is authoritative).
      const msg = String((e && e.message) || e)
      results.push({ home: file.home, status: 'build_gate_error', error: msg })
      log('BUILD GATE ' + file.home + ' — agent error (symbols already committed): ' + msg)
    }
  }
}

const okSyms = results.filter(r => r.status === 'committed').length
const bgFiles = results.filter(r => r.status === 'build_gate').length
log('Batch ' + (stopped ? 'STOPPED (' + stopped + ')' : 'complete') + ': ' + okSyms + '/' + symTotal + ' symbols committed, ' + bgFiles + ' file(s) build-gated | out-tok ' + kStr(k()))
return { label: A.label || 'unnamed', root: ROOT, total_symbols: symTotal, committed: okSyms, build_gated_files: bgFiles, stopped, out_tok_k: (k() < 0 ? null : k()), results }
