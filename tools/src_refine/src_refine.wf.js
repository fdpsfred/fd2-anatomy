export const meta = {
  name: 'fd2-src-refine',
  description: 'FD2 src/ per-symbol refine (Stage 1): for each function/global, analyze purpose+logic from Ghidra+src, refine the src/ comment and keep the Ghidra plate consistent (src authoritative), JUDGE the name but only RECORD a proposed rename (never apply -- renames are serial Stage 2 on main), note any logic issue, then write a per-symbol shard + per-symbol commit so a limit/disconnect wastes nothing. ONLY names+comments change; game logic is untouched. One symbol at a time, serial within a file. Run one workflow per worktree.',
  phases: [
    { title: 'Refine', detail: 'per symbol: analyze -> refine comment (src + Ghidra plate) -> record name verdict -> shard + commit' },
  ],
}

// ---------------------------------------------------------------------------
// args (built by tools/src_refine/scout.py; passed inline since workflow scripts
// have no filesystem access):
//   { root: "<ABS worktree path>", label: "refine-rp1", partition: "rp1",
//     files: [ { home: "battle/battle.c",
//                symbols: [ { address, name, kind:'function'|'global', home,
//                             routing_name?, worklist_name?, ghidra_name?, is_thunk?,
//                             category?, emit_action?, tsv_kind?, datatype?, len?,
//                             xrefs?, segment? } ] } ] }
// Symbols within a file are processed serially (same-file comment edits). One refiner
// agent per symbol does the full cycle incl. its own per-symbol commit.
// ---------------------------------------------------------------------------
const A = (typeof args === 'string' && args.length) ? JSON.parse(args) : (args || {})
const ROOT = A.root
const PART = A.partition || 'rp?'
const MAINROOT = A.mainRoot || 'C:/Users/fdpsf/Documents/fd2-anatomy'
let FILES = (A && A.files) || []
const MIN_BUDGET_PER_SYM = (A && A.minBudgetPerSym) || 120000
if (!ROOT) { log('args.root missing (absolute worktree path required)'); return { error: 'no root' } }

// Bootstrap: if no inline worklist, derive it via scout.py. Workflow scripts have no
// filesystem access, but an agent does -- it runs scout (which reads the partition
// manifest + this worktree's committed shards) and returns the not-yet-refined files.
// Keeps the launch args tiny ({root,partition}) and makes resume a re-launch.
if (!FILES.length && PART !== 'rp?') {
  log('bootstrap: scouting ' + PART + ' worklist via scout.py')
  const scoutCmd = 'python ' + MAINROOT + '/tools/src_refine/scout.py --partition ' + PART +
    ' --root ' + ROOT + ' --shards-dir ' + ROOT + '/tools/src_refine/data/shards/' + PART + ' --limit 99999'
  const argsPath = MAINROOT + '/workspace/src_refine/args/' + PART + '.args.json'
  const sc = await agent(
    ['Run EXACTLY this command in the foreground (it is read-only bookkeeping):',
     '  ' + scoutCmd,
     'Then Read the file it wrote: ' + argsPath,
     'Return that file\'s top-level "files" array VERBATIM as {"files": [...]} (a list of',
     '{home, symbols:[{address,name,kind,...}]} objects). Do nothing else -- no analysis, no edits.'].join('\n'),
    { schema: { type: 'object', required: ['files'], properties: { files: { type: 'array' } } },
      label: 'scout:' + PART, phase: 'Refine' })
  FILES = (sc && sc.files) || []
  log('bootstrap: ' + FILES.length + ' file(s) to process')
}
if (!FILES.length) { log('worklist empty -- nothing to do.'); return { error: 'empty worklist', argsType: typeof args } }

const ENV = [
  'environment: Ghidra 已開啟 FD2.LE（單一 program）。呼叫 Ghidra MCP 時 program 參數留空。',
  '先用 ToolSearch 一次載入所需 Ghidra 工具：',
  'ToolSearch query="select:mcp__ghidra__get_plate_comment,mcp__ghidra__decompile_function,mcp__ghidra__disassemble_function,mcp__ghidra__get_function_signature,mcp__ghidra__get_function_callers,mcp__ghidra__get_xrefs_to,mcp__ghidra__get_xrefs_from,mcp__ghidra__read_memory,mcp__ghidra__get_function_by_address,mcp__ghidra__set_plate_comment,mcp__ghidra__run_script_inline,mcp__ghidra__get_current_program_info"',
  '用到不熟/沒把握的 Ghidra MCP function 前，先讀 .claude/skills/ghidra-usage/ 確認正確參數格式，不要猜。',
  '判斷名稱尾碼數字是「領域 ID（要保留）」還是「Ghidra 位址殘留（要清）」時，用遊戲領域知識：',
  '  章節 01-30、道具 00-D6、法術 00-23、職業 00-1A、頭像 00-41 是有意義 ID；陣列維度/倍率/半徑等常數也是語意。',
  '  可查 .claude/skills/fd2-knowledge/（若存在）或 assets/ KB 佐證。**嚴禁用 regex 機械剝除尾碼**。',
  '本次 checkout 根目錄（所有檔案路徑都用這個絕對前綴，不要用相對路徑、不要 cd 到別處）：',
  'ROOT = ' + ROOT,
].join('\n')

const SOP = [
  '# FD2 src-refine 鐵則（refiner 遵守）',
  '- **只處理被指派的這一個 symbol**；不順手改別的符號、不批次。',
  '- **只能改「名稱(僅記錄proposed,不套用)」與「註解」。嚴禁改任何遊戲邏輯**：不動任何 statement/運算式/型別/控制流/常數值/參數型別/函式簽章語意。遊戲邏輯已驗證正確。',
  '- **comment 同步 = 一致即可、src 為準（非 byte-identical）**：src/ 註解保精煉正確；Ghidra plate 保留其詳盡內容；只要兩者語意不矛盾即可。若 Ghidra plate 與你 refine 後的理解矛盾/過時/空白，才更新 plate（以 src 的結論為準），否則不要動 plate。',
  '- **名稱判定只記錄不套用**：rename 會在 Stage 2 於 main 序列套用（牽動 protos.h/globals.h + 所有 call site）。本階段**絕不** rename_function/rename_global、**絕不**改 src 內的符號名、**絕不**改宣告。只在 shard 記 name_verdict/proposed/reason。',
  '- 程式碼內文字一律 ASCII（註解/字串）；唯中文角色/道具/法術專名可維持中文。',
  '- **改 src 註解一律用 Edit，嚴禁 Write 覆寫整檔**（會抹掉同檔其他符號的內容，已發生過事故）。只動「你這個符號的註解區塊」。',
  '- 發現的是「邏輯疑慮」（非命名/註解，例如疑似等價性 bug）→ 寫進 shard 的 issues[]（含 evidence），**本階段不修**（workflow 結束後處理）。',
  '- Ghidra 連線失敗：某 MCP 呼叫失敗或回「無法連線/中斷/逾時」→ 先快速重試該呼叫一次；仍失敗才設 ghidra_unreachable=true(bool)+ghidra_error_detail(string) 給外層 watchdog，並停止本符號（不要寫 shard、不要 commit）。',
].join('\n')

function symHead(sym) {
  const meta = sym.kind === 'function'
    ? ['- is_thunk: ' + !!sym.is_thunk, '- category: ' + sym.category, '- emit_action: ' + sym.emit_action]
    : ['- tsv_kind: ' + sym.tsv_kind, '- datatype(hint): ' + sym.datatype, '- len: ' + sym.len,
       '- xrefs: ' + sym.xrefs, '- segment: ' + sym.segment]
  return [
    '目標 symbol（' + sym.kind + '）：',
    '- name (current, = live Ghidra): ' + sym.name,
    '- address: 0x' + sym.address,
    '- home 檔: ROOT/src/' + sym.home,
  ].concat(meta).join('\n')
}

function refinerPrompt(sym) {
  const shardPath = ROOT + '/tools/src_refine/data/shards/' + PART + '/' + sym.address + '.json'
  const isFn = sym.kind === 'function'
  const analyze = isFn ? [
    'A. 分析（理解用途以正確命名/註解，**不是**重做等價驗證）：',
    '   1. get_plate_comment(0x' + sym.address + ') 讀現有 plate。',
    '   2. decompile_function(0x' + sym.address + ') 讀反編譯（理解邏輯的主依據）。',
    '   3. 讀 ROOT/src/' + sym.home + '，定位本 function 的定義與其上方註解區塊。',
    '   4. get_function_callers / get_xrefs_to(0x' + sym.address + ') 看呼叫情境（命名與參數語義的關鍵依據）。',
    '   5. 只有反編譯不清楚時才 disassemble_function 佐證（折疊分支/EAX 風險）；不預設逐行 asm。',
  ].join('\n') : [
    'A. 分析（理解用途以正確命名/註解）：',
    '   1. get_plate_comment(0x' + sym.address + ') 讀現有 plate。',
    '   2. get_xrefs_to(0x' + sym.address + ') 列出所有 reader/writer。',
    '   3. read_memory(0x' + sym.address + ', <足夠長度>) 看內容（const 表/初值）。',
    '   4. 反編譯 1-2 個 user 看存取方式（型別/維度/用途；**本階段不改型別/維度**，只據此驗證名稱與註解正確）。',
    '   5. 讀 ROOT/src/' + sym.home + ' 定位本 global 的定義與其上方註解區塊。',
  ].join('\n')

  return [
    ENV, '', SOP, '',
    '# 角色：Refiner（單一 agent，最高嚴謹度）。對這一個 symbol 完整解析並 refine 名稱判定與註解。',
    symHead(sym), '',
    analyze, '',
    'B. 結論：用途(purpose 一句) + 內部邏輯摘要(logic_summary)。' +
      (isFn ? ' function 另列 signature、回傳語義(returns)、讀/寫的 global(reads_globals/writes_globals)，以及每個參數(params，見 C2)。'
            : ' global 另列真實 datatype 與每元素語義(element_semantics)、是否 const(is_const，由遊戲端有無 write-xref 判定)。'),
    '',
    'C. 名稱判定（**只記錄，不套用**；rename 一律在 Stage 2 序列套用，因牽動 protos.h/globals.h/call site/簽章）：',
    '  C1. 符號名(' + (isFn ? 'function' : 'global') + ')：依命名規範檢查 current name 是否正確反映' + (isFn ? '用途與內部邏輯' : '用途與資料型態') + '：',
    '   - 規範：遊戲 function=fd2_<domain>_<descriptor>（main 唯一豁免）；CRT 包裝=crt_equivalent_*；DPMI=fd2_dpmi_*；',
    '     global=data_fd2_<domain>_<descriptor>（battle 核心表 data_fd2_battle_*；CRT=data_crt_*）。' + (isFn ? '' : ' global 名應同時反映資料型態（表/陣列/旗標/指標/計數器等）。'),
    '   - **名稱不得帶 Ghidra 位址殘留**（尾碼 hex 等於自身或某 data 的位址，例 _b43=0x10b43、_52758=0x52758；fd2_noop_stub_4e915 的 _4e915 亦是）。',
    '     但**領域 ID 必須保留**（章節/道具/法術/職業/頭像 ID、陣列維度、倍率），用領域知識判定，不可 regex 機械剝除。',
    '   - 判 name_verdict: keep（已正確）/ rename（需改，給 name_proposed 合規新名 + name_reason + evidence）/ uncertain（給 name_reason）。',
    (isFn ?
     '  C2. **每個參數名逐一判定**(params)：依每個參數的實際用途（caller 傳入值 + body 使用方式判定）檢查名稱是否貼切，且**不得帶 address 殘留**（param_1 泛稱、與用途不符、帶位址都算需改）。每筆 params 記 {name(現名), type, semantics(用途), verdict(keep|rename), proposed(改名時新名，否則 null), reason}。void 則 params=[]。'
     : '  C2. （global 無參數，略。）'),
    '   - **本階段絕不 rename 任何符號名或參數名、絕不改 src 宣告/簽章**；全部 rename（符號名+參數名）一律 Stage 2 套用。',
    '',
    'D. 註解 refine（**一致即可、src 為準**）：',
    '   1. 用 Edit 改 ROOT/src/' + sym.home + ' 內本 symbol 上方的註解區塊，使其精煉正確反映用途+邏輯（ASCII；保留 `@ 0x' + sym.address + '` 之類位址標註是允許的，那是註解不是名稱）。原註解已正確精煉就維持(comment_verdict=keep)。**嚴禁 Write 整檔**。',
    '   2. 看 Ghidra plate 是否與你 refine 後的理解一致：一致就不要動 plate；若 plate 矛盾/過時/空白才更新（' +
      (isFn ? 'set_plate_comment(0x' + sym.address + ', ...)'
            : '用 run_script_inline 寫 PLATE_COMMENT，data plate 機制見 .claude/skills/ghidra-usage/ 與 memory project_ghidra_data_plate_mechanism') +
      '，內容以 src 結論為準、可精煉，不必複製 src 全文）。記錄 ghidra_plate_action。',
    '',
    'E. logic 疑慮（若有）：發現疑似邏輯/等價性問題（非命名/註解）→ 放進 issues[]（category/severity/title/description/evidence），**本階段不修**。',
    '',
    'F. 寫 shard + commit（durability，務必做）：',
    '   1. 用 Write 建立 shard 檔：' + shardPath,
    '      內容 = 下方 JSON（單一物件）。父目錄不存在先用前景 shell `mkdir -p` 建。',
    '   2. clobber 防線：前景跑 `git -C ' + ROOT + ' add -A` 後，',
    '      `git -C ' + ROOT + ' diff --cached -- src/' + sym.home + ' | grep "^-" | grep -v "^---"`。',
    '      若輸出顯示刪掉了「不是你要改的註解行」（例如別的 function 的程式碼/註解被刪）→ `git -C ' + ROOT + ' checkout -- src/' + sym.home + '` 還原、status=blocked、committed=false、不要 commit、回報。',
    '   3. commit：`git -C ' + ROOT + ' commit`，第一行：',
    '      src-refine: ' + sym.name + ' @ ' + sym.address + ' (comment ' + '{verdict}' + (')'),
    '      後接空行 + Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>',
    '      （staged 範圍只該有：src/' + sym.home + ' 與 tools/src_refine/data/shards/' + PART + '/' + sym.address + '.json）。',
    '',
    '# 輸出（最後一則訊息＝下列 JSON，且與 shard 檔內容一致）：',
    'address, name_current, name_final(=current 若 keep；rename 時=proposed), name_verdict(keep|rename|uncertain), name_proposed, name_reason,',
    'kind, home, purpose, logic_summary, ' +
      (isFn ? 'signature, params[{name,type,semantics,verdict(keep|rename),proposed,reason}], returns{type,semantics}, reads_globals[], writes_globals[],'
            : 'datatype, element_semantics, is_const,') +
    ' comment_original_issue, comment_verdict(keep|rewrite|augment), ghidra_plate_action(none|updated|created),',
    'issues[{category,severity,title,description,evidence}], processed(true), committed(bool), commit_hash,',
    'status(done|blocked), ghidra_unreachable(bool), ghidra_error_detail(string), notes.',
  ].join('\n')
}

const REFINER_SCHEMA = {
  type: 'object',
  required: ['address', 'name_current', 'name_verdict', 'comment_verdict', 'processed', 'committed', 'status'],
  properties: {
    address: { type: 'string' }, name_current: { type: 'string' },
    name_final: { type: 'string' }, name_verdict: { type: 'string', enum: ['keep', 'rename', 'uncertain'] },
    name_proposed: { type: ['string', 'null'] }, name_reason: { type: 'string' },
    kind: { type: 'string' }, home: { type: 'string' },
    purpose: { type: 'string' }, logic_summary: { type: 'string' },
    signature: { type: 'string' },
    params: { type: 'array', items: { type: 'object', properties: { name: { type: 'string' }, type: { type: 'string' }, semantics: { type: 'string' }, verdict: { type: 'string' }, proposed: { type: ['string', 'null'] }, reason: { type: 'string' } } } },
    returns: { type: 'object' },
    reads_globals: { type: 'array' }, writes_globals: { type: 'array' },
    datatype: { type: 'string' }, element_semantics: { type: 'string' }, is_const: { type: ['boolean', 'null'] },
    comment_original_issue: { type: ['string', 'null'] },
    comment_verdict: { type: 'string', enum: ['keep', 'rewrite', 'augment'] },
    ghidra_plate_action: { type: 'string' },
    issues: { type: 'array' },
    processed: { type: 'boolean' }, committed: { type: 'boolean' }, commit_hash: { type: 'string' },
    status: { type: 'string', enum: ['done', 'blocked'] },
    ghidra_unreachable: { type: 'boolean' }, ghidra_error_detail: { type: 'string' }, notes: { type: 'string' },
  },
}

function ghidraDown(out) { return !!out && out.ghidra_unreachable === true }
async function runAgent(promptStr, opts) {
  const out = await agent(promptStr, opts)
  if (ghidraDown(out)) {
    const detail = (out && out.ghidra_error_detail) ? String(out.ghidra_error_detail) : '(no detail)'
    const e = new Error('GHIDRA_DISCONNECT: ' + detail); e.ghidra = true; throw e
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
let symDone = 0, renameProposed = 0, paramRenameProposed = 0, issuesFound = 0

outer:
for (let fi = 0; fi < FILES.length; fi++) {
  const file = FILES[fi]
  for (let si = 0; si < file.symbols.length; si++) {
    const sym = file.symbols[si]
    sym.home = file.home
    symDone++
    const tag = '[' + PART + ' ' + symDone + '/' + symTotal + '] ' + sym.name + ' (' + file.home + ')'
    if (budget.total && budget.remaining() < MIN_BUDGET_PER_SYM) {
      log('budget low: ' + Math.round(budget.remaining() / 1000) + 'k left; stop before ' + tag + '. re-run scout skips committed shards.')
      stopped = 'budget'; break outer
    }
    log(tag + ' -- refine start [batch ' + kStr(k()) + ']')
    try {
      const out = await runAgent(refinerPrompt(sym), { schema: REFINER_SCHEMA, label: 'refine:' + sym.name, phase: 'Refine' })
      if (out && out.committed && out.status === 'done') {
        const rp = out.name_verdict === 'rename'
        const nis = (out.issues || []).length
        const npr = ((out.params || []).filter(p => p && p.verdict === 'rename')).length
        if (rp) renameProposed++
        paramRenameProposed += npr
        issuesFound += nis
        results.push({ address: sym.address, name: sym.name, home: file.home, status: 'committed',
                       name_verdict: out.name_verdict, name_proposed: out.name_proposed || null,
                       param_renames: npr, comment_verdict: out.comment_verdict, issues: nis, commit: out.commit_hash })
        log(tag + ' -- COMMITTED ' + (out.commit_hash || '') + ' | name:' + out.name_verdict +
            (rp ? ('->' + out.name_proposed) : '') + (npr ? (' | ' + npr + ' param-rename') : '') +
            ' | comment:' + out.comment_verdict + (nis ? (' | ' + nis + ' issue(s)') : ''))
      } else {
        results.push({ address: sym.address, name: sym.name, home: file.home, status: out ? (out.status || 'not_committed') : 'no_output',
                       notes: (out && out.notes) || '(no detail)' })
        log(tag + ' -- NOT committed: ' + ((out && out.notes) || out && out.status || '(no output)'))
      }
    } catch (e) {
      const msg = String((e && e.message) || e); const isG = !!(e && e.ghidra)
      log(tag + (isG ? ' -- GHIDRA DISCONNECT: ' : ' -- INTERRUPTED: ') + msg)
      results.push({ address: sym.address, name: sym.name, home: file.home, status: isG ? 'ghidra_disconnect' : 'interrupted', error: msg })
      stopped = isG ? 'ghidra_disconnect' : 'interrupt'; break outer
    }
  }
}

const ok = results.filter(r => r.status === 'committed').length
log('Batch ' + (stopped ? 'STOPPED (' + stopped + ')' : 'complete') + ': ' + ok + '/' + symTotal +
    ' committed | ' + renameProposed + ' name-rename | ' + paramRenameProposed + ' param-rename | ' +
    issuesFound + ' issue(s) | out-tok ' + kStr(k()))
return { label: A.label || PART, partition: PART, root: ROOT, total: symTotal, committed: ok,
         rename_proposed: renameProposed, param_rename_proposed: paramRenameProposed, issues_found: issuesFound,
         stopped, out_tok_k: (k() < 0 ? null : k()), results }
