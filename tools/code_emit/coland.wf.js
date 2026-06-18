export const meta = {
  name: 'fd2-coland',
  description: 'FD2 Phase-2 coordinated-landing helper: emit OR review a list of functions (per-function 3-source). NO build/test/commit and NO testglob/routing edits -- the orchestrator (main agent) owns spy deletion, the build gate, and the per-unit commit. mode=emit runs SERIAL (agents append to one shared src file); mode=review runs PARALLEL (read-only).',
  phases: [
    { title: 'Process', detail: 'emit (serial) or review (parallel) each assigned function' },
  ],
}

// args = { mode:'emit'|'review', functions:[{addr,name,target}], batchLabel? }
const A = (typeof args === 'string' && args.length) ? JSON.parse(args) : (args || {})
const fns = (A && A.functions) || []
const mode = (A && A.mode) || 'emit'
if (!fns.length) { log('args.functions empty -- nothing to do (args type=' + (typeof args) + ')'); return { error: 'empty worklist', argsType: typeof args } }

const ENV = [
  'environment: Ghidra 已開啟 FD2.LE（單一 program）。呼叫 Ghidra MCP 時 program 參數留空。',
  '先用 ToolSearch 一次載入所需 Ghidra 工具：',
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__disassemble_function,mcp__ghidra__decompile_function,mcp__ghidra__get_function_callers,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_function_signature,mcp__ghidra__get_function_callees,mcp__ghidra__set_plate_comment,mcp__ghidra__get_current_program_info"',
].join('\n')

// Reviewer ENV: strictly READ-ONLY Ghidra toolset (no set_plate_comment) so the
// parallel review fan-out can never race on Ghidra writes; all plate/name drift
// is reported to the orchestrator, which applies fixes + saves FD2.LE serially.
const REVIEW_ENV = [
  'environment: Ghidra 已開啟 FD2.LE（單一 program）。呼叫 Ghidra MCP 時 program 參數留空。',
  '先用 ToolSearch 一次載入所需 Ghidra 工具（全部唯讀；本角色嚴禁寫入 Ghidra）：',
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__disassemble_function,mcp__ghidra__decompile_function,mcp__ghidra__get_function_callers,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_function_signature,mcp__ghidra__get_function_callees,mcp__ghidra__get_current_program_info"',
].join('\n')

const SOP = [
  '# FD2 emit pipeline 鐵則',
  '- 只處理被指派的這一個 function；不順手、不預載、不批次其他 function。',
  '- 三源不省略（plate / disasm / decomp），即使極簡 thunk。',
  '- 符號名與 Ghidra byte-identical；C89（變數宣告在 block 開頭）；檔名 8.3。',
  '- 語意完全保留。Ghidra 有系統性 EAX-tracking bug（CALL 後 EAX return value 常被誤標）；每個「CALL 後使用 EAX」的點都要對 assembly 核對，不可信 decompiled C。',
  '- cc / param 從 caller 推導，少報比多報危險（少報→callee 讀 stack 垃圾→crash）。',
  '- 程式碼內文字一律 ASCII（.c/.h 註解與字串）；唯中文角色/道具/法術專名維持中文；em-dash 用 --、箭頭用 ->、中文標點換 ASCII。',
  '- 絕不半成品（改名 / static / 空殼 / _impl）；絕不為遷就 test 而扭曲 emit code；禁 workaround，只修 root cause。',
  '- Ghidra 連線失敗：某個 Ghidra MCP 呼叫失敗 / 逾時 / 回任何「無法連線 / 連線中斷 / instance 不可用」錯誤時，先快速重試該呼叫一次（僅一次，勿反覆 hammer）。重試成功照常繼續。若仍失敗 → 立刻停止本 function、設 ghidra_unreachable=true（bool）並把實際錯誤寫進 ghidra_error_detail（string）。',
].join('\n')

const COLAND = [
  '# 這是 Phase-2 COORDINATED LANDING（特殊約束，務必遵守）',
  '- 不要跑 build_test.py。本單元在全部 body 落地 + orchestrator 刪除 testglob spy 之前 build 會故意是紅的；你跑只會浪費時間且看起來像失敗。',
  '- 不要新增或修改任何 unit test。本單元相關測試的重寫由 user 決定延到 Phase 3。',
  '- 不要碰 tests/testglob.c（spy 由 orchestrator 刪）。不要碰 src/routing.json（記帳由 orchestrator 做）。',
].join('\n')

function emitPrompt(fn) {
  return [
    ENV, '', SOP, '', COLAND, '',
    '# 角色：Emitter（coordinated landing，emit 回合）。以最高嚴謹度執行。',
    '目標 function（只處理這一個）：',
    '- address: ' + fn.addr,
    '- name: ' + fn.name,
    '- home src 檔: src/' + fn.target + '（已存在；用 append 把你的 function 加進去）',
    (fn.hint ? '\n# 重要提示（callee 契約 / 已知陷阱；仍須自行從三源確認，不可只信此提示）：\n' + fn.hint + '\n' : ''),
    '',
    '# 你唯一可動的檔：(1) append body 進 src/' + fn.target + '；(2) 在 src/include/protos.h 新增/校正本 function 的 prototype；(3) 若三源比對證明 Ghidra plate/name/global 名稱錯了才修（set_plate_comment / rename）並回報。',
    '',
    '# 步驟',
    'A. 三源：get_plate_comment / disassemble_function / decompile_function(' + fn.addr + ')。判 cc（末指令 RET 0 / RET N、caller 是否 ADD ESP K、entry push 慣例）；標記每個「CALL 後用 EAX」的點並對 assembly 核對。',
    'B. get_function_callees(' + fn.addr + ') 看它呼叫誰；對「已 emit 在同檔或他檔」的 callee，讀其既有 C signature 並照樣呼叫（引數順序/型別對齊 binary 的 register/stack 設定）。get_function_callers / get_xrefs_to 從 caller 實際傳值推真 param 數與型別。',
    'C. 讀 src/' + fn.target + '（既有 function + house style）與 src/include/protos.h 內本 function 既有的 prototype（若有）。從 Ghidra body 用法決定「真簽名」，寫真 C body 進 src/' + fn.target + '。',
    '   APPEND-ONLY anti-clobber：嚴禁用 Write 整檔覆寫；用 Edit 插在最後一個 function 之後。寫完 grep/讀回 VERIFY 檔案沒變短、所有既有 function + 你新增的都在。若既有 prototype 與真簽名不符（含「參數名誤導」如把 dst_buf 叫成 x）→ 校正 prototype 成真簽名 + 正確參數名。若本 function 還沒有 prototype → 新增。',
    'D. 若三源證明 Ghidra plate 文字 / function name / 引用到的 global 名稱錯 → 當場修（set_plate_comment / rename）並列入 ghidra_changes；否則不動 Ghidra。',
    'E. emit_issues：只有實際編譯才能確認的等價性疑慮（FPU rounding / word width / table-copy）→ 列成字串。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）',
    '{ "addr","name","signature","three_source_done":{"plate","disasm","decomp"},"callees":[],"param_evidence","files_touched":[],"proto_reconciled":bool,"ghidra_changes":[],"emit_issues":[],"ghidra_unreachable":false,"ghidra_error_detail":"","status":"done|skip|blocked","notes" }',
  ].join('\n')
}

function reviewPrompt(fn) {
  return [
    REVIEW_ENV, '', SOP, '', COLAND, '',
    '# 角色：Reviewer（獨立驗證 coordinated landing 的 emit）。你不信任既有 emitted C，也不信任 Ghidra decompiled C；所有結論自己從 assembly + plate 重新確認。surface verify（只查名稱存在）無效。',
    '目標 function：address ' + fn.addr + '  name ' + fn.name + '  src/' + fn.target + '。',
    '# coordinated-landing 復驗特例：orchestrator 已跑過 build gate（compile+link 0 error / 0 warning，PASS）。不要自己跑 build_test.py（run 階段會故意卡在 Phase-3-deferred 測試，那是預期、不是本 function 的缺陷）。本 function 的 unit-test 重寫延到 Phase 3，故 checklist 的 test-quality 項本階段 N/A、不要因「沒有 runtime test」而 block，只註明。只驗 body 等價性。不要改任何檔，也嚴禁寫入 Ghidra（不要 set_plate_comment / rename / 任何 write 類 MCP；本批 review 並行執行，寫入會互相干擾並搶走 orchestrator 對 Ghidra 變更的單一控制權）；所有 plate/name 漂移或修正建議寫進 kb_plate_findings，由 orchestrator 統一套用 + 存 FD2.LE。只回報、不動手。',
    '',
    '# 步驟',
    'A. 自己抓三源：get_plate_comment / disassemble_function / decompile_function(' + fn.addr + ')。',
    'B. 讀 emitted C：src/' + fn.target + ' 內本 function + src/include/protos.h 的 prototype。可跑 `git --no-pager diff -- src/' + fn.target + ' src/include/protos.h` 看未 commit 的 emit。只 review 這一個 function（其他 function 與 testglob 協調編輯不在你範圍）。',
    'C. 逐項 checklist（每項：通過/不通過 + evidence 引 assembly 指令/位址）：',
    '   1. 控制流：assembly 分支/迴圈/呼叫順序 ↔ emitted C 一致。',
    '   2. EAX-tracking：每個 CALL 後用 EAX 處 → C 取正確 return（對 assembly，不對 decompiled C）。',
    '   3. cc/param：signature cc 與 param 數與 caller 一致（少報=crash，重點查）。',
    '   4. 數值/型別：width(8/16/32)、signed/unsigned、cast 是否等價。',
    '   5. fall-through / 特殊 pattern（pipeline_spec §A-F）。',
    '   6. 符號名 byte-identical；C89 宣告位置；8.3。callee 引數對齊 binary register/stack 設定。',
    '   7. (test 品質) 本階段 N/A（Phase 3 deferred）-- 只註明。',
    '   9. Ghidra 事實：plate 描述 / function name / 引用 global 的名稱與 plate，是否與 assembly 事實相符。emitter 若改過 plate/rename，獨立確認改得正確。發現過時/錯誤 → kb_plate_findings 列出（非 block，除非影響等價判定）。',
    '',
    '# 共識原則：只 block「破壞 Layer-2 等價 / 編譯 / 連結」或「違反鐵則（半成品/符號名不符/cc 少報/未保留語意）」；等價但寫法不同、純風格、純 plate 散文 → non_blocking_notes / kb_plate_findings。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）',
    '{ "approved":bool,"addr","name","three_source_done":{"plate","disasm","decomp"},"blocking_issues":[{"severity","checklist_item","location","claim","evidence","fix_suggestion"}],"non_blocking_notes":[],"kb_plate_findings":[],"ghidra_unreachable":false,"ghidra_error_detail":"" }',
  ].join('\n')
}

const EMIT_SCHEMA = {
  type: 'object',
  required: ['addr', 'name', 'three_source_done', 'status'],
  properties: {
    addr: { type: 'string' }, name: { type: 'string' }, signature: { type: 'string' },
    three_source_done: { type: 'object', properties: { plate: { type: 'boolean' }, disasm: { type: 'boolean' }, decomp: { type: 'boolean' } } },
    callees: { type: 'array', items: { type: 'string' } },
    param_evidence: { type: 'string' },
    files_touched: { type: 'array', items: { type: 'string' } },
    proto_reconciled: { type: 'boolean' },
    ghidra_changes: { type: 'array', items: { type: 'string' } },
    emit_issues: { type: 'array', items: { type: 'string' } },
    ghidra_unreachable: { type: 'boolean' }, ghidra_error_detail: { type: 'string' },
    status: { type: 'string', enum: ['done', 'skip', 'blocked'] },
    notes: { type: 'string' },
  },
}

const REVIEW_SCHEMA = {
  type: 'object',
  required: ['approved', 'addr', 'name', 'three_source_done', 'blocking_issues'],
  properties: {
    approved: { type: 'boolean' }, addr: { type: 'string' }, name: { type: 'string' },
    three_source_done: { type: 'object', properties: { plate: { type: 'boolean' }, disasm: { type: 'boolean' }, decomp: { type: 'boolean' } } },
    blocking_issues: { type: 'array', items: { type: 'object', properties: { severity: { type: 'string' }, checklist_item: {}, location: { type: 'string' }, claim: { type: 'string' }, evidence: { type: 'string' }, fix_suggestion: { type: 'string' } } } },
    non_blocking_notes: { type: 'array', items: { type: 'string' } },
    kb_plate_findings: { type: 'array', items: { type: 'string' } },
    ghidra_unreachable: { type: 'boolean' }, ghidra_error_detail: { type: 'string' },
  },
}

function ghidraDown(out) { return !!out && out.ghidra_unreachable === true }
async function runAgent(promptStr, opts) {
  const out = await agent(promptStr, opts)
  if (ghidraDown(out)) {
    const detail = (out && out.ghidra_error_detail) ? String(out.ghidra_error_detail) : '(no detail)'
    const e = new Error('GHIDRA_DISCONNECT: ' + detail); e.ghidra = true; e.ghidraDetail = detail; throw e
  }
  return out
}

if (mode === 'emit') {
  // SERIAL: every agent appends into the same shared src file.
  const results = []
  for (let i = 0; i < fns.length; i++) {
    const fn = fns[i]
    const tag = '[' + (i + 1) + '/' + fns.length + '] ' + fn.name
    log(tag + ' -- emit start')
    try {
      const out = await runAgent(emitPrompt(fn), { schema: EMIT_SCHEMA, label: 'emit:' + fn.name, phase: 'Process' })
      results.push({ addr: fn.addr, name: fn.name, status: out.status || 'emitted', signature: out.signature || '', emit_issues: out.emit_issues || [], ghidra_changes: out.ghidra_changes || [], notes: out.notes || '' })
      log(tag + ' -- ' + (out.status || 'emitted'))
    } catch (e) {
      const isG = !!(e && e.ghidra); const msg = String((e && e.message) || e)
      log(tag + ' -- ' + (isG ? 'GHIDRA DISCONNECT' : 'INTERRUPTED') + ': ' + msg)
      results.push({ addr: fn.addr, name: fn.name, status: isG ? 'ghidra_disconnect' : 'interrupted', error: msg })
      for (let j = i + 1; j < fns.length; j++) results.push({ addr: fns[j].addr, name: fns[j].name, status: 'deferred' })
      return { mode: mode, batch: (A && A.batchLabel) || 'blit', stopped: isG ? 'ghidra_disconnect' : 'interrupt', results: results }
    }
  }
  log('emit batch complete: ' + results.length + ' processed')
  return { mode: mode, batch: (A && A.batchLabel) || 'blit', stopped: null, results: results }
} else {
  // REVIEW: parallel (read-only). Each thunk resolves to its own object, never throws.
  log('review batch: ' + fns.length + ' functions (parallel)')
  const out = await parallel(fns.map((fn) => () =>
    runAgent(reviewPrompt(fn), { schema: REVIEW_SCHEMA, label: 'review:' + fn.name, phase: 'Process' })
      .then((v) => ({ addr: fn.addr, name: fn.name, approved: !!(v && v.approved), verdict: v }))
      .catch((e) => ({ addr: fn.addr, name: fn.name, approved: false, verdict: null, error: String((e && e.message) || e), ghidra: !!(e && e.ghidra) }))
  ))
  const ok = out.filter((r) => r && r.approved).length
  const ghDown = out.filter((r) => r && r.ghidra).length
  log('review batch done: ' + ok + '/' + fns.length + ' approved' + (ghDown ? (' | ' + ghDown + ' ghidra-disconnect') : ''))
  return { mode: mode, batch: (A && A.batchLabel) || 'blit', approved: ok, ghidra_disconnect: ghDown, results: out }
}
