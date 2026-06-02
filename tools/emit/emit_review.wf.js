export const meta = {
  name: 'fd2-emit-review',
  description: 'FD2 per-function emit/review: 3-source verify, build gate, git diff, per-function commit. One function at a time (serial).',
  phases: [
    { title: 'Process', detail: 'per function: (emit ->) review -> iterate <=4 -> commit' },
  ],
}

// ---------------------------------------------------------------------------
// work-list comes from args.functions = [{addr, name, target, mode}]
//   mode: 'review' (done=true, already emitted)  |  'emit' (done=false, from scratch)
// The main agent scouts this from routing.json + Ghidra and passes it in,
// because workflow scripts have no filesystem access.
// ---------------------------------------------------------------------------
const A = (typeof args === 'string' && args.length) ? JSON.parse(args) : (args || {})
const fns = (A && A.functions) || []
if (!fns.length) { log('args.functions is empty — nothing to do. (args type=' + (typeof args) + ')'); return { error: 'empty worklist', argsType: typeof args } }
const MAX_ROUNDS = (A && A.maxRounds) || 10
// graceful budget guard: don't start a function we likely can't finish (~300k/fn observed)
const MIN_BUDGET_PER_FN = (A && A.minBudgetPerFn) || 400000

// Shared, byte-identical prefix across every agent call (prompt-cache friendly).
const ENV = [
  'environment: Ghidra 已開啟 FD2.LE（單一 program）。呼叫 Ghidra MCP 時 program 參數留空。',
  '先用 ToolSearch 一次載入所需 Ghidra 工具：',
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__disassemble_function,mcp__ghidra__decompile_function,mcp__ghidra__get_function_callers,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_function_signature,mcp__ghidra__emulate_function"',
].join('\n')

const SOP = [
  '# FD2 emit pipeline 鐵則（emitter / reviewer 共用）',
  '- 只處理被指派的這一個 function；不順手、不預載、不批次其他 function。',
  '- 三源不省略（plate / disasm / decomp），即使極簡 thunk。',
  '- 符號名與 Ghidra byte-identical；C89（變數宣告在 block 開頭）；檔名 8.3。',
  '- 語意完全保留。Ghidra 有系統性 EAX-tracking bug（CALL 後 EAX return value 常被誤標）；每個「CALL 後使用 EAX」的點都要對 assembly 核對，不可信 decompiled C。',
  '- cc / param 從 caller 推導，少報比多報危險（少報→callee 讀 stack 垃圾→crash）。',
  '- 絕不半成品（改名 / static / 空殼 / _impl）；絕不為遷就 test 而扭曲 emit code。',
  '- 禁 workaround，只修 root cause。',
  '- Test 覆蓋政策＝風險導向：對「數值計算 / 複雜控制流分支 / RNG / EAX-bug 風險 / 狀態轉移」的 state/path 強制測；純 blit/display 副作用的 state 可延到 Phase 9 integration（但須在輸出註明延後與理由）。',
  '- build gate：前景執行  python tools/emit/build_test.py --changed "<改動檔,逗號分隔>"  ，它內部自己輪詢 DONE.TXT（約 20-30 秒）並回傳 JSON。',
  '  嚴禁用背景 / run_in_background 跑它——subagent 一旦交出最終訊息就結束，收不到背景通知、不會閉環。必須前景阻塞等它回 JSON。',
  '- 目前 3 個既存 warning 在 spell/spellwk.c 與 life/main.c，與本批 summon 無關：你不需修、但絕不可新增任何 warning。',
].join('\n')

const NEED_KB = [
  '需要時才讀（不要全文預載）：rebuild_info/emission/calling_convention.md（cc 判定）、',
  'rebuild_info/emission/pipeline_spec.md（fall-through pattern A–F、EAX-bug 慣例）、',
  'src/include/types.h（runtime_char 等 struct）、program_info/overview.md（語意）。',
].join('\n')

function emitterPrompt(fn, mode, verdict) {
  const head = [
    ENV, '', SOP, '', NEED_KB, '',
    '# 角色：Emitter（' + (mode === 'fix' ? 'fix 回合' : 'emit 回合') + '）。以最高嚴謹度執行。',
    '目標 function：',
    '- address: ' + fn.addr,
    '- name: ' + fn.name,
    '- target .c: src/' + fn.target,
    '- 模式: ' + fn.mode + '（review=既有 C 已存在；emit=從零產出）',
  ].join('\n')

  let body
  if (mode === 'fix') {
    body = [
      '',
      '# 本回合：修正 reviewer 提出的 blocking issue（不要重做無關部分）',
      'reviewer verdict（逐條處理）：',
      JSON.stringify(verdict && verdict.blocking_issues || [], null, 2),
      '',
      '對每條 blocking issue：要嘛「已修（說明怎麼修）」，要嘛「不同意（附 assembly/decomp evidence 主張無需改）」。',
      '若 issue 是 emitted C 真有等價性 bug → 修 src/' + fn.target + '（維持語意）。',
      '若 issue 是 test 覆蓋不足（風險導向 path 未測）→ 補 test（deterministic input + 真實斷言；',
      '  純計算可用 emulate_function 取 ground-truth；SFX/分流用 counting stub）。不要為過 test 而改 C。',
    ].join('\n')
  } else {
    body = [
      '',
      '# 本回合：從零 emit 這個 function',
      '步驟：',
      'A. 三源：get_plate_comment / disassemble_function / decompile_function(' + fn.addr + ')。',
      '   判 cc（末指令 RET 0 / RET N、caller 是否 ADD ESP K、entry 是否未 PUSH EBX 就讀 EBX…）；標記每個 CALL 後 EAX 用法。',
      'B. 需要時 get_function_callers / get_xrefs_to 推 param 數與 cc。若 plate 標 DECOMPILER FRAGMENT → 不獨立 emit，回報 skip。',
      'C. 寫 C 進 src/' + fn.target + '（檔頭註解標 name @ addr (N callers)）；同步 src/include/protos.h（原型）、globals.h（新 global，用 Ghidra 真名）。',
      'D. 寫 unit test（風險導向覆蓋）：跑 python tests/where.py ' + fn.target + ' 取得確切落點檔與 runner（自動處理已切分的檔，回報 append 或 create）；action=create 就新建該檔、寫完跑 python tests/genbuild.py --apply 接上 build.bat/test.lnk/testmain。加 static test + 在回報的 run_*_tests() 用 RUN_TEST 註冊。新 stub/fake global/未初始化 fnptr table → tests/testglob.c（fnptr table 必須初始化 noop）。跨多個測試檔共用的 fixture → tests/include/<domain>fix.h（如 battlfix.h）。',
    ].join('\n')
  }

  const tail = [
    '',
    '# 共同收尾',
    '1. KB / Ghidra 同步：若三源比對發現 plate 描述錯誤 → set_plate_comment 修正；發現命名前綴不符 → rename + 同步 globals.h/testglob.c；KB doc 與事實不符 → 修對應 .md。所有改動都會被 reviewer 經 git diff 一併檢查。',
    '2. emit_issues：需實際編譯才能確認的等價性疑慮（FPU rounding / word width / table-copy）→ append src/emit_issues.json（key 用 routing.json 同款 8-hex address，例如 00010b43，不要寫成 0x10b43；utf-8），寫後讀回檢查編碼。',
    '3. build gate：前景跑 python tools/emit/build_test.py --changed "<改動檔>" ，必須 0 error、0 新增 warning、全部 test PASS（含新增）。紅燈就修到綠或回報 blocked + 真因。',
    '4. 用 git --no-pager diff 看自己這次的所有改動，確認無越界（沒動到別的 function）。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）：addr,name,three_source_done{plate,disasm,decomp},c_changed,files_touched[],test_cases_added[],',
    'build{gate_pass,error_count,warning_count,tests_passed,tests_failed},emit_issues[],ghidra_changes[],status(done|skip|blocked),notes。',
  ].join('\n')

  return head + '\n' + body + '\n' + tail
}

function reviewerPrompt(fn, emitterOut) {
  return [
    ENV, '', SOP, '', NEED_KB, '',
    '# 角色：Reviewer（獨立驗證）。你不信任既有 emitted C，也不信任 Ghidra decompiled C——所有結論自己從 assembly + plate + 原始碼重新確認。surface verify（只查名稱存在）無效。',
    '目標 function：address ' + fn.addr + '  name ' + fn.name + '  target src/' + fn.target + '  模式 ' + fn.mode + '。',
    emitterOut ? ('Emitter 本回合回報（僅供定位，不可當證據）：\n' + JSON.stringify(emitterOut, null, 2)) : '（review 模式初次：尚無 emitter 回合，直接驗證既有 baseline C。）',
    '',
    '# 步驟',
    'A. 先看 emitter 的精確改動：執行  git --no-pager diff HEAD -- src/' + fn.target + ' tests/  （序列+per-function commit 下，HEAD 之後的未 commit 改動即本 function 的）。',
    '   這份 diff 涵蓋 code 與任何 KB/doc(.md) 改動——都要一併檢查。若 diff 為空（review 模式初次），review 既有 baseline C。',
    'B. 自己抓三源：get_plate_comment / disassemble_function / decompile_function(' + fn.addr + ')。',
    'C. 讀 src/' + fn.target + ' 內該 function 的 C、對應 tests/' + fn.target + ' 的 test（鏡像 src 子檔，可能依大小切成 <stem>1/<stem>2）、testglob.c 相關 stub。',
    'D. 逐項 checklist（每項給 通過/不通過 + evidence 引 assembly 指令或行）：',
    '   1. 控制流：assembly 分支/迴圈/呼叫順序 ↔ emitted C 一致。',
    '   2. EAX-tracking bug：每個 CALL 後用 EAX 處，C 是否取正確 return（對 assembly，不對 decompiled C）。',
    '   3. cc/param：signature cc 與 param 數與 caller 一致（少報=crash，重點查）。',
    '   4. 數值/型別：width(8/16/32)、signed/unsigned、FPU rounding 是否等價。',
    '   5. fall-through/特殊 pattern：是否依 pipeline_spec §A–F 正確處理。',
    '   6. 符號名 byte-identical；C89 宣告位置；8.3。',
    '   7. test 品質（風險導向政策）：高風險 state/path（數值/分支/RNG/EAX/狀態轉移）是否都有真實斷言（非永真/trivially-pass）；純計算期望值是否來自 emulate_function 而非臆測；純 blit/display 延後是否合理註明。未覆蓋高風險 path → block。',
    '   8. 半成品/遷就 test：有無改名/static/空殼/_impl；有無為 test 扭曲 C。',
    '   9. Ghidra 事實正確性（主動驗證，不只查 emitter 改的）：plate 描述的行為/caller/callee、function name 的語意、以及本 function 引用到的 global data symbol 之名稱與 plate，是否與 assembly 事實相符。發現過時命名 / placeholder / 描述錯 / 分類錯 → 列為 blocking_issue（fix_suggestion 寫明應 set_plate_comment 或 rename 成什麼）。專案硬規範：Ghidra 與事實不符必當場追根修正並同步 KB / globals.h / testglob.c，不留待後續。emitter 本回合若已改 plate/KB(.md)/rename，也一併確認改得正確。',
    'E. emit_issues：emitter 該記而未記的等價性疑慮，列出。',
    '',
    '# 共識原則：只 block「破壞 Layer 2 等價/編譯/執行」或「違反鐵則（半成品/批次/跳步/符號名不符/未覆蓋高風險 path）」；等價但寫法不同、純風格 → non_blocking_notes。不確定 → discussion_for_emitter 要求 evidence，而非直接 block。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）：',
    'approved(bool), three_source_done{plate,disasm,decomp}, diff_reviewed(bool),',
    'blocking_issues[{severity,checklist_item,location,claim,evidence,fix_suggestion}], non_blocking_notes[], kb_plate_findings[], emit_issues_to_log[], discussion_for_emitter。',
  ].join('\n')
}

function bookkeepPrompt(fn, verdict, emitterOut) {
  const files = (emitterOut && emitterOut.files_touched) || []
  return [
    '# 角色：Bookkeeper。function ' + fn.name + ' @ ' + fn.addr + ' 已經 reviewer approved。做 per-function 記帳，不做任何 code/test 邏輯修改。',
    'emitter 回報改動的檔：' + JSON.stringify(files),
    '步驟：',
    '1. 更新 src/routing.json：寫一段 python（json.load 讀檔 → 把 key "' + fn.addr + '" 的物件設 reviewed=True → 用 json.dumps(indent=2, ensure_ascii=False) 加結尾換行寫回）。只改這一個 entry，保持原格式不動其他 entry。',
    '2. 若 reviewer 的 emit_issues_to_log 非空，append 到 src/emit_issues.json（key 用與 routing.json 同款的 8-hex address，例如 00010b43，不要寫成 0x10b43；以 utf-8 讀寫、寫後讀回確認無亂碼）。reviewer issues：' + JSON.stringify((verdict && verdict.emit_issues_to_log) || []),
    '3. git add 本 function 相關改動：src/' + fn.target + ' tests/ src/routing.json src/emit_issues.json src/include/（只 add 真正變動的；用 git --no-pager diff --staged --stat 確認範圍只含本 function）。',
    '4. git commit，message：',
    '   emit-review: ' + fn.name + ' @ ' + fn.addr + ' (' + fn.mode + ', reviewed-approved)',
    '   空行後簡述 reviewer 結論一行，再空行後：Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>',
    '5. 回報：git --no-pager log --oneline -1 的結果（commit hash + title）。你的最終訊息只需這一行 commit 資訊。',
  ].join('\n')
}

const EMITTER_SCHEMA = {
  type: 'object',
  required: ['addr', 'name', 'three_source_done', 'build', 'status'],
  properties: {
    addr: { type: 'string' }, name: { type: 'string' },
    three_source_done: { type: 'object', properties: { plate: { type: 'boolean' }, disasm: { type: 'boolean' }, decomp: { type: 'boolean' } } },
    c_changed: { type: 'boolean' },
    files_touched: { type: 'array', items: { type: 'string' } },
    test_cases_added: { type: 'array', items: { type: 'string' } },
    build: { type: 'object', properties: { gate_pass: { type: 'boolean' }, error_count: { type: 'integer' }, warning_count: { type: 'integer' }, tests_passed: { type: 'integer' }, tests_failed: { type: 'integer' } } },
    emit_issues: { type: 'array', items: { type: 'string' } },
    ghidra_changes: { type: 'array', items: { type: 'string' } },
    status: { type: 'string', enum: ['done', 'skip', 'blocked'] },
    notes: { type: 'string' },
  },
}

const REVIEWER_SCHEMA = {
  type: 'object',
  required: ['approved', 'three_source_done', 'blocking_issues'],
  properties: {
    approved: { type: 'boolean' },
    three_source_done: { type: 'object', properties: { plate: { type: 'boolean' }, disasm: { type: 'boolean' }, decomp: { type: 'boolean' } } },
    diff_reviewed: { type: 'boolean' },
    blocking_issues: { type: 'array', items: { type: 'object', properties: { severity: { type: 'string' }, checklist_item: {}, location: { type: 'string' }, claim: { type: 'string' }, evidence: { type: 'string' }, fix_suggestion: { type: 'string' } } } },
    non_blocking_notes: { type: 'array', items: { type: 'string' } },
    kb_plate_findings: { type: 'array', items: { type: 'string' } },
    emit_issues_to_log: { type: 'array', items: { type: 'string' } },
    discussion_for_emitter: { type: 'string' },
  },
}

const results = []
let stopped = null
// output-token accounting (budget.spent() works even when budget.total is null)
let batchStart = 0
try { batchStart = budget.spent() } catch (e) { batchStart = 0 }
const batchK = () => { try { return Math.round((budget.spent() - batchStart) / 1000) } catch (e) { return -1 } }
const kStr = (k) => (k < 0 ? 'n/a' : '~' + k + 'k')

for (let i = 0; i < fns.length; i++) {
  const fn = fns[i]
  const tag = '[' + (i + 1) + '/' + fns.length + '] ' + fn.name
  let fnStart = 0
  try { fnStart = budget.spent() } catch (e) { fnStart = 0 }
  const fnK = () => { try { return Math.round((budget.spent() - fnStart) / 1000) } catch (e) { return -1 } }

  // graceful budget guard: stop on a clean boundary (between functions) rather
  // than throwing mid-function and leaving a half-written test in the tree.
  if (budget.total && budget.remaining() < MIN_BUDGET_PER_FN) {
    log('budget low: ' + Math.round(budget.remaining() / 1000) + 'k left < ' + Math.round(MIN_BUDGET_PER_FN / 1000) + 'k/fn; stopping before ' + tag + '. ' + (fns.length - i) + ' fn(s) deferred — re-run after reset (routing.json reviewed flag skips done work).')
    for (let j = i; j < fns.length; j++) results.push({ addr: fns[j].addr, name: fns[j].name, status: 'deferred_budget' })
    stopped = 'budget'
    break
  }

  log(tag + ' — start (' + fn.mode + ') [batch out-tok ' + kStr(batchK()) + ']')
  try {
    let emitterOut = null
    if (fn.mode === 'emit') {
      emitterOut = await agent(emitterPrompt(fn, 'emit', null), { schema: EMITTER_SCHEMA, label: 'emit:' + fn.name, phase: 'Process' })
    }

    let verdict = await agent(reviewerPrompt(fn, emitterOut), { schema: REVIEWER_SCHEMA, label: 'review:' + fn.name, phase: 'Process' })
    let round = 0
    while (verdict && !verdict.approved && round < MAX_ROUNDS) {
      const nIssues = (verdict.blocking_issues || []).length
      log(tag + ' — fix round ' + (round + 1) + ' (' + nIssues + ' blocking) [fn out-tok ' + kStr(fnK()) + ']')
      emitterOut = await agent(emitterPrompt(fn, 'fix', verdict), { schema: EMITTER_SCHEMA, label: 'fix:' + fn.name + ':' + (round + 1), phase: 'Process' })
      verdict = await agent(reviewerPrompt(fn, emitterOut), { schema: REVIEWER_SCHEMA, label: 'rereview:' + fn.name + ':' + (round + 1), phase: 'Process' })
      round++
    }

    if (verdict && verdict.approved) {
      const commitInfo = await agent(bookkeepPrompt(fn, verdict, emitterOut), { label: 'commit:' + fn.name, phase: 'Process' })
      log(tag + ' — APPROVED & committed after ' + round + ' fix round(s) | fn ' + kStr(fnK()) + ' out-tok, batch ' + kStr(batchK()))
      results.push({ addr: fn.addr, name: fn.name, status: 'approved', rounds: round, commit: commitInfo, out_tok_k: fnK(), emit_issues: verdict.emit_issues_to_log || [] })
    } else {
      log(tag + ' — NOT approved after ' + round + ' rounds — left for user | fn ' + kStr(fnK()) + ' out-tok, batch ' + kStr(batchK()))
      results.push({ addr: fn.addr, name: fn.name, status: 'needs_user', rounds: round, out_tok_k: fnK(), blocking_issues: (verdict && verdict.blocking_issues) || [] })
    }
  } catch (e) {
    // agent() throws when the token/usage limit (or workflow budget) is reached.
    // Stop the batch to preserve already-committed progress; the in-flight fn was
    // NOT committed, so routing.json still has it as !reviewed and a re-run redoes it.
    const msg = String((e && e.message) || e)
    log(tag + ' — INTERRUPTED (likely token/usage limit or agent error): ' + msg + ' | batch out-tok ' + kStr(batchK()))
    results.push({ addr: fn.addr, name: fn.name, status: 'interrupted', error: msg, out_tok_k: fnK() })
    for (let j = i + 1; j < fns.length; j++) results.push({ addr: fns[j].addr, name: fns[j].name, status: 'deferred_after_interrupt' })
    stopped = 'interrupt'
    break
  }
}

const ok = results.filter(r => r.status === 'approved').length
const totalK = batchK()
log('Batch ' + (stopped ? 'STOPPED EARLY (' + stopped + ')' : 'complete') + ': ' + ok + '/' + fns.length + ' approved | total out-tok ' + kStr(totalK))
return { batch: (A && A.batchLabel) || 'unnamed', total: fns.length, approved: ok, stopped: stopped, out_tok_k: (totalK < 0 ? null : totalK), results }
