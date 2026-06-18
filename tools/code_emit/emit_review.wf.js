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
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__disassemble_function,mcp__ghidra__decompile_function,mcp__ghidra__get_function_callers,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_function_signature,mcp__ghidra__emulate_function,mcp__ghidra__open_program,mcp__ghidra__get_current_program_info"',
  '用到不熟/沒把握的 Ghidra MCP function（emulate_function、analyze_dataflow、apply_data_type、create_struct…）前，先讀 `.claude/skills/ghidra-usage/` 的說明確認正確參數格式與 best practice，不要猜格式硬試（猜錯會讀到垃圾引數、甚至卡死 Ghidra）。',
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
  '- Ghidra 連線失敗：某個 Ghidra MCP 呼叫失敗、或回任何形式的「instance 不可用 / 無法連線 / 連線中斷 / 逾時」錯誤（不限特定字串）時，先「快速重試該呼叫一次」（僅一次，不可反覆重試以免 hammer/wedge Ghidra）排除瞬間 blip；重試成功就照常繼續、不要設旗標。若重試仍失敗（持續無法連線）→ 立刻停止本 function、不臆測不硬湊，務必設 ghidra_unreachable=true（bool），並把你實際看到的錯誤訊息/原因寫進 ghidra_error_detail（string）。這是給外層 watchdog 的唯一停批訊號。',
  '  例外：若 timeout 緊接在你呼叫 `emulate_function` 之後（emulate 格式錯造成 runaway 會連帶卡死 Ghidra、後續所有呼叫也 timeout）→ 這不是真連線中斷，**不要設 ghidra_unreachable**，改走下方「emulate_function 使用規範」的自助重啟+重試流程（最多 5 次）。',
  '- Test 覆蓋政策＝風險導向：對「數值計算 / 複雜控制流分支 / RNG / EAX-bug 風險 / 狀態轉移」的 state/path 強制測；純 blit/display 副作用的 state 可延到 Phase 9 integration（但須在輸出註明延後與理由）。',
  '- build gate：前景執行  python tools/code_emit/build_test.py --changed "<改動檔,逗號分隔>"  ，它內部自己輪詢 DONE.TXT（約 20-30 秒）並回傳 JSON。',
  '  嚴禁用背景 / run_in_background 跑它——subagent 一旦交出最終訊息就結束，收不到背景通知、不會閉環。必須前景阻塞等它回 JSON。',
  '- build 目前 0 warning；gate（gate_pass）要求 0 warning，絕不可新增任何 warning。',
].join('\n')

const EMULATE_GUIDE = [
  '# emulate_function 使用規範（取純計算 ground-truth 時；checksum / CRC / hash / bit-packing 等「輸入已知、純計算」leaf）',
  '呼叫前先讀 `.claude/skills/ghidra-usage/` 確認最新用法。下面是已驗證的正確格式 —— 格式錯會讀到垃圾引數→暴衝迴圈→卡死 Ghidra，務必照做：',
  '- 先從 disasm 判 calling convention：引數是從 register 取（MOV ...,EAX/EDX/EBX/ECX）還是從 STACK 取（MOV ...,[EBP+0x8] / [EBP+0xc]，cdecl）。',
  '- `registers`：JSON string，如 {"ECX":"0x10"}。register-cc 的引數設這裡。',
  '- `memory`：必須用 regions wrapper → {"regions":[{"address":"0x7FFE0000","hex":"01020304"}]}（每 region 可用 hex / data(base64) / string）。',
  '- stack 由工具自動初始化在 0x7FFF0000、return sentinel 0xDEADBEEF —— **不要自己設 ESP/鋪 stack**。cdecl(stack-cc) 引數放成 memory region：arg1 在 0x7FFF0004、arg2 在 0x7FFF0008（小端 4 bytes）；引數若是指標，把它指向的 buffer 放資料區（如 0x7FFE0000）、該引數值＝該位址。',
  '- 回傳含 `hit_return:true` 代表正常跑到 RET；return_registers 指定的 register（如 EAX）即結果。先用「已知輸入、可手算的小案例」驗證回值正確，再用它取真正的 ground-truth。',
  '## emulate_function timeout → 自助重啟 + 修正參數重試，最多 5 次（超過才放棄）：',
  '1. 重啟 Ghidra（用 PowerShell 工具，或 Bash 呼叫 powershell.exe；需要時加 dangerouslyDisableSandbox）。只鎖定 Ghidra 那個 javaw，勿殺其他 java：',
  '   kill： Get-CimInstance Win32_Process -Filter "Name=\'javaw.exe\'" | Where-Object { $_.CommandLine -like \'*ghidra_12.1_PUBLIC*\' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }',
  '   relaunch： Start-Process -FilePath "C:\\Users\\fdpsf\\Documents\\ghidra_12.1_PUBLIC\\ghidraRun.bat"',
  '2. 等 MCP port 就緒（單一 Bash 指令，勿用前景長 sleep）： for i in $(seq 1 80); do (exec 3<>/dev/tcp/127.0.0.1/8089) 2>/dev/null && { exec 3>&-; echo UP; break; }; sleep 3; done',
  '3. ghidraRun **不會自動載入程式**（get_current_program_info 會回 "No program loaded"）→ 用 MCP open_program(path="/FD2.LE") 載回，再 get_current_program_info 確認 1375 functions。',
  '4. 依上面的正確格式 + 正確 cc/引數位置修正參數後重試 emulate_function。',
  '5. 累計 5 次仍失敗才放棄 emulate_function：改「手動對 disasm/decomp 推導期望值」並在輸出 notes 註明改用手算；不可因 emulate 失敗就 abort 整個 function，也不要設 ghidra_unreachable。',
].join('\n')

const NEED_KB = [
  '需要時才讀（不要全文預載）：rebuild_info/emission/calling_convention.md（cc 判定）、',
  'rebuild_info/emission/pipeline_spec.md（fall-through pattern A–F、EAX-bug 慣例）、',
  'src/include/types.h（runtime_char 等 struct）、program_info/overview.md（語意）。',
].join('\n')

function emitterPrompt(fn, mode, verdict) {
  const head = [
    ENV, '', SOP, '', EMULATE_GUIDE, '', NEED_KB, '',
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
      'C. 寫 C 進 src/' + fn.target + '（檔頭註解標 name @ addr (N callers)）；同步 src/include/protos.h（原型）、globals.h（新 global，用 Ghidra 真名）。若這是該 src 子檔的第一個 function（src/' + fn.target + ' 還不存在），照常建新檔即可，build 接線由步驟 D 的 genbuild 自動處理，不要自己去改 build.bat / test.lnk。',
      'D. 寫 unit test（風險導向覆蓋）：跑 python tests/where.py ' + fn.target + ' 取得確切落點測試檔與 runner（自動處理已切分的檔，回報 append 或 create）；在該檔加 static test + 在回報的 run_*_tests() 用 RUN_TEST 註冊（create 就新建該測試檔）。新 stub/fake global/未初始化 fnptr table → tests/testglob.c（fnptr table 必須初始化 noop）。跨多個測試檔共用的 in-memory fixture（純記憶體 struct/buffer 的 helper）→ tests/include/<domain>fix.h（如 battlfix.h）。【真實檔案測試（鐵則）】若本 function（或其 test 所驅動的 callee）會 fopen/fread 真實遊戲檔（FDICON.B24 / FDFIELD/FDSHAP/FDOTHER/FDTXT/FDMUS.DAT / FD2.SAV 等），**嚴禁捏造該檔**（不可寫 write_fake_dat / write_fake_fdicon 之類製造結構假檔，也不可 remove() 這些檔）。build_test.py 會把這些真檔從 fd2_game_files/ stage 到 tests/OUT（= TEST.EXE 的 cwd），test 直接讓 real loader fopen 讀到真檔，並對**真檔實際解析值**斷言（期望值用 fd2-knowledge 或直接讀真檔求得；加密檔如 FD2.SAV 需經真 decrypt function 解出）。【build 接線】只要新建了任何 src 或 test 的 .c 檔，跑一次 python tests/genbuild.py --apply——它掃 src/ 與 tests/ 重新產生 build.bat 的 src/test 兩個 compile 區 + test.lnk + testmain.c。嚴禁手改 build.bat / test.lnk / testmain.c，一律用 genbuild。',
    ].join('\n')
  }

  const tail = [
    '',
    '# 共同收尾',
    '1. KB / Ghidra 同步：若三源比對發現 plate 描述錯誤 → set_plate_comment 修正；發現命名前綴不符 → rename + 同步 globals.h/testglob.c；KB doc 與事實不符 → 修對應 .md。所有改動都會被 reviewer 經 git diff 一併檢查。',
    '2. emit_issues：需實際編譯才能確認的等價性疑慮（FPU rounding / word width / table-copy）→ append src/emit_issues.json（key 用 routing.json 同款 8-hex address，例如 00010b43，不要寫成 0x10b43；utf-8），寫後讀回檢查編碼。',
    '3. build gate：前景跑 python tools/code_emit/build_test.py --changed "<改動檔>" ，必須 0 error、0 新增 warning、全部 test PASS（含新增）。紅燈就修到綠或回報 blocked + 真因。',
    '4. 用 git --no-pager diff 看自己這次的所有改動，確認無越界（沒動到別的 function）。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）：addr,name,three_source_done{plate,disasm,decomp},c_changed,files_touched[],test_cases_added[],',
    'build{gate_pass,error_count,warning_count,tests_passed,tests_failed},emit_issues[],ghidra_changes[],status(done|skip|blocked),ghidra_unreachable(bool),ghidra_error_detail(string；Ghidra 連線出問題時寫你看到的錯誤/原因),notes。',
  ].join('\n')

  return head + '\n' + body + '\n' + tail
}

function reviewerPrompt(fn, emitterOut) {
  return [
    ENV, '', SOP, '', EMULATE_GUIDE, '', NEED_KB, '',
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
    '   7b. 真實檔案測試（鐵則，讀檔 function 必驗）：若本 function（或其 test 所驅動的 callee）會 fopen/fread 真實遊戲檔（FDICON.B24 / FDFIELD/FDSHAP/FDOTHER/FDTXT/FDMUS.DAT / FD2.SAV 等），test 必須讀「從 fd2_game_files/ staged 到 tests/OUT（TEST.EXE 的 cwd）的真檔」並對真實解析值斷言。build_test.py 會 stage 這些真檔；test 不可自己寫假檔（write_fake_dat / write_fake_fdicon 之類捏造結構的 stand-in），也不可 remove() 這些 staged 真檔（會被同 run 其他 suite 影響）。發現假檔過關 → 一律 block（fix_suggestion：改讀 staged 真檔、斷言用真檔實際 byte/size/解析值，期望值可用 fd2-knowledge 或直接讀真檔求得）。',
    '   8. 半成品/遷就 test：有無改名/static/空殼/_impl；有無為 test 扭曲 C。',
    '   9. Ghidra 事實正確性（主動驗證，不只查 emitter 改的）：plate 描述的行為/caller/callee、function name 的語意、以及本 function 引用到的 global data symbol 之名稱與 plate，是否與 assembly 事實相符。發現過時命名 / placeholder / 描述錯 / 分類錯 → 列為 blocking_issue（fix_suggestion 寫明應 set_plate_comment 或 rename 成什麼）。專案硬規範：Ghidra 與事實不符必當場追根修正並同步 KB / globals.h / testglob.c，不留待後續。emitter 本回合若已改 plate/KB(.md)/rename，也一併確認改得正確。',
    'E. emit_issues：emitter 該記而未記的等價性疑慮，列出。',
    '',
    '# 共識原則：只 block「破壞 Layer 2 等價/編譯/執行」或「違反鐵則（半成品/批次/跳步/符號名不符/未覆蓋高風險 path）」；等價但寫法不同、純風格 → non_blocking_notes。不確定 → discussion_for_emitter 要求 evidence，而非直接 block。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON）：',
    'approved(bool), ghidra_unreachable(bool；Ghidra 連線出問題設 true), ghidra_error_detail(string；寫你看到的錯誤/原因), three_source_done{plate,disasm,decomp}, diff_reviewed(bool),',
    'blocking_issues[{severity,checklist_item,location,claim,evidence,fix_suggestion}], non_blocking_notes[], kb_plate_findings[], emit_issues_to_log[], discussion_for_emitter。',
  ].join('\n')
}

function bookkeepPrompt(fn, verdict, emitterOut) {
  const files = (emitterOut && emitterOut.files_touched) || []
  return [
    '# 角色：Bookkeeper。function ' + fn.name + ' @ ' + fn.addr + ' 已經 reviewer approved。做 per-function 記帳，不做任何 code/test 邏輯修改。',
    'emitter 回報改動的檔：' + JSON.stringify(files),
    '步驟：',
    '1. 更新 src/routing.json：寫一段 python（json.load 讀檔 → 把 key "' + fn.addr + '" 的物件**同時**設 done=True 與 reviewed=True（emit 模式起始 done=False，務必補上；review 模式 done 本就 True，重設無害）→ 用 json.dumps(indent=2, ensure_ascii=False) 加結尾換行寫回）。只改這一個 entry，保持原格式不動其他 entry。',
    '2. 若 reviewer 的 emit_issues_to_log 非空，append 到 src/emit_issues.json（key 用與 routing.json 同款的 8-hex address，例如 00010b43，不要寫成 0x10b43；以 utf-8 讀寫、寫後讀回確認無亂碼）。reviewer issues：' + JSON.stringify((verdict && verdict.emit_issues_to_log) || []),
    '3. git add 本 function 相關改動：src/' + fn.target + ' tests/ src/routing.json src/emit_issues.json src/include/ program_info/ resource_info/ rebuild_info/ assets/（最後四個是 KB 資料夾，本 function emit 連帶的 KB 同步務必一起 add——絕不可留在 working tree。只 add 真正變動的；用 git --no-pager diff --staged --stat 確認範圍只含本 function，再用 git status --porcelain 確認「已追蹤檔」無殘留未 staged 的改動）。',
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
    ghidra_unreachable: { type: 'boolean' },
    ghidra_error_detail: { type: 'string' },
    notes: { type: 'string' },
  },
}

const REVIEWER_SCHEMA = {
  type: 'object',
  required: ['approved', 'three_source_done', 'blocking_issues'],
  properties: {
    approved: { type: 'boolean' },
    ghidra_unreachable: { type: 'boolean' },
    ghidra_error_detail: { type: 'string' },
    three_source_done: { type: 'object', properties: { plate: { type: 'boolean' }, disasm: { type: 'boolean' }, decomp: { type: 'boolean' } } },
    diff_reviewed: { type: 'boolean' },
    blocking_issues: { type: 'array', items: { type: 'object', properties: { severity: { type: 'string' }, checklist_item: {}, location: { type: 'string' }, claim: { type: 'string' }, evidence: { type: 'string' }, fix_suggestion: { type: 'string' } } } },
    non_blocking_notes: { type: 'array', items: { type: 'string' } },
    kb_plate_findings: { type: 'array', items: { type: 'string' } },
    emit_issues_to_log: { type: 'array', items: { type: 'string' } },
    discussion_for_emitter: { type: 'string' },
  },
}

// --- Ghidra MCP disconnect detection (schema-only) -----------------------
// The single Ghidra instance can drop mid-batch. The agent is the only party
// that sees the actual MCP error (in whatever form — there is no single fixed
// error string, so NO prose/grep matching anywhere). Detection is purely the
// structured ghidra_unreachable bool the agent sets (SOP-instructed) plus a
// free-text ghidra_error_detail cause. On that bool we fast-stop the batch
// (stopped:'ghidra_disconnect') instead of churning MAX_ROUNDS of doomed
// reviews; the main-loop watchdog keys only on that signal in the result.
function agentReportsGhidraDown(out) {
  // SOLE signal = the structured schema bool `ghidra_unreachable`, set by the agent
  // — the only party that actually sees the Ghidra MCP error, in whatever form it
  // takes (there is NO single fixed error string, so no prose/grep matching anywhere).
  // The agent also returns `ghidra_error_detail` (free-text cause) for the human/log.
  return !!out && out.ghidra_unreachable === true
}
async function runAgent(promptStr, opts) {
  const out = await agent(promptStr, opts)
  if (agentReportsGhidraDown(out)) {
    const detail = (out && out.ghidra_error_detail) ? String(out.ghidra_error_detail) : '(no detail given)'
    const e = new Error('GHIDRA_DISCONNECT: agent set ghidra_unreachable=true. Reported cause: ' + detail)
    e.ghidra = true
    e.ghidraDetail = detail
    throw e
  }
  return out
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
      emitterOut = await runAgent(emitterPrompt(fn, 'emit', null), { schema: EMITTER_SCHEMA, label: 'emit:' + fn.name, phase: 'Process' })
    }

    let verdict = await runAgent(reviewerPrompt(fn, emitterOut), { schema: REVIEWER_SCHEMA, label: 'review:' + fn.name, phase: 'Process' })
    let round = 0
    while (verdict && !verdict.approved && round < MAX_ROUNDS) {
      const nIssues = (verdict.blocking_issues || []).length
      log(tag + ' — fix round ' + (round + 1) + ' (' + nIssues + ' blocking) [fn out-tok ' + kStr(fnK()) + ']')
      emitterOut = await runAgent(emitterPrompt(fn, 'fix', verdict), { schema: EMITTER_SCHEMA, label: 'fix:' + fn.name + ':' + (round + 1), phase: 'Process' })
      verdict = await runAgent(reviewerPrompt(fn, emitterOut), { schema: REVIEWER_SCHEMA, label: 'rereview:' + fn.name + ':' + (round + 1), phase: 'Process' })
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
    // agent()/runAgent() throws on token/usage limit, workflow budget, agent error,
    // OR a detected Ghidra MCP disconnect (runAgent tags e.ghidra). Stop the batch to
    // preserve already-committed progress; the in-flight fn was NOT committed, so
    // routing.json still has it !reviewed and a re-run redoes it.
    const msg = String((e && e.message) || e)
    const isGhidra = !!(e && e.ghidra)
    log(tag + (isGhidra ? ' — GHIDRA DISCONNECT (agent set ghidra_unreachable): ' : ' — INTERRUPTED (likely token/usage limit or agent error): ') + msg + ' | batch out-tok ' + kStr(batchK()))
    results.push({ addr: fn.addr, name: fn.name, status: isGhidra ? 'ghidra_disconnect' : 'interrupted', error: msg, ghidra_error_detail: (e && e.ghidraDetail) || null, out_tok_k: fnK() })
    for (let j = i + 1; j < fns.length; j++) results.push({ addr: fns[j].addr, name: fns[j].name, status: 'deferred_after_interrupt' })
    stopped = isGhidra ? 'ghidra_disconnect' : 'interrupt'
    break
  }
}

const ok = results.filter(r => r.status === 'approved').length
const totalK = batchK()
log('Batch ' + (stopped ? 'STOPPED EARLY (' + stopped + ')' : 'complete') + ': ' + ok + '/' + fns.length + ' approved | total out-tok ' + kStr(totalK))
return { batch: (A && A.batchLabel) || 'unnamed', total: fns.length, approved: ok, stopped: stopped, out_tok_k: (totalK < 0 ? null : totalK), results }
