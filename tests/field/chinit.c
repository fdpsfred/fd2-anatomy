/*
 * unit tests for src/field/chinit.c
 *
 * fd2_chapter_01_init is a pure chapter-prologue ORCHESTRATOR: its entire
 * body is a fixed, straight-line script of void side-effect calls
 * (cutscene/dialog/portrait/bgm/camera) plus unconditional constant writes
 * to engine globals (current_chapter_id, battle_anim_phase,
 * cutscene_event_state, party_total_gold). It contains NO numeric
 * computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (so no Ghidra EAX-tracking-bug exposure) — the only control
 * flow is two constant-bound counters (15x and 13x fd2_walk_step_up(2)).
 *
 * A behavioral unit test is DEFERRED to Phase 9 integration (reason proven,
 * not convenience):
 *
 *   - The body issues ~22 fd2_display_dialog_scene() calls. That function is
 *     real-linked (src/dialog/dialog.c) and reaches
 *     fd2_wait_for_input_dialog_with_blink(), a real-linked keyboard busy-wait
 *     (src/input/input.c) that hangs forever in the silent automated harness
 *     (the same hang already proven by bisection for the 0x10010 save-load
 *     checksum-mismatch arm). It also dereferences current_chapter_text as a
 *     parsed opcode stream.
 *   - It drives fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24 +
 *     dump FD2.TMP) and 30+ fd2_cutscene_event_trigger() calls that each parse
 *     an in-memory per-event byte-script (event ids 0x5A..0x69 and 0,1,2,5),
 *     i.e. a full chapter-1 prologue scene environment.
 *   - All callees are real-linked from src/, so they cannot be replaced by
 *     capture stubs to observe the call ordering, and the emit code must not
 *     be distorted to make it host-testable (forbidden).
 *
 * There is no isolable host-safe computational slice to assert at unit level;
 * the ordered global-state contract is reachable only after the blocking/
 * display pipeline runs. Equivalence of the translation was instead verified
 * statically, line-by-line, against the disassembly @0x3231B (call sequence,
 * constants, the two loop bounds, and the four phase current_chapter_id
 * transitions 0x20 -> 0x1F -> 0). See src/emit_issues.json (0003231b).
 *
 * This suite is intentionally empty pending the Phase 9 integration harness
 * (scripted input + staged chapter-1 cutscene scripts), mirroring the
 * documented-empty pattern used for other pure display/blocking orchestrators
 * (e.g. fd2_render_status_screen_static_layout @0x17EEF).
 *
 * fd2_chapter_02_init @0x32D18 is the same shape (and simpler: no loops, no
 * current_chapter_id transitions, four dialog pages chained with cutscenes
 * 0x9/0xA/0xB/0xC, portrait sets 1/2, one composited frame, two camera pans).
 * It likewise has NO numeric computation, NO RNG, NO data-dependent branch,
 * and NO CALL-result consumption (no EAX-bug exposure), and every callee is
 * real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24) /
 * fd2_cutscene_event_trigger byte-script parsing apply. Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x32D18 (call sequence,
 * constants, the init_phase_flag 1/0 bracket around portrait set 2, and the
 * three battle_anim_phase resets after pages 0/1/2 but not page 3). Note the
 * page-3 dialog call and the final fd2_pan_cursor_to_char(0) are physically a
 * tail-JMP into the shared epilogues of fd2_chapter_07_init (@0x33206) and
 * fd2_chapter_05_init (@0x33140); the emit reconstructs the equivalent
 * straight-line form. See src/emit_issues.json (00032d18).
 *
 * fd2_chapter_03_init @0x32E8C is the same shape as chapter 02: a flat
 * orchestrator playing four dialog pages chained with three cutscenes
 * (0x12/0x11/0x13), portrait set 1 loaded after the first cutscene, two camera
 * pans, NO char init. It likewise has NO numeric computation, NO RNG, NO
 * data-dependent branch, and NO CALL-result consumption (no EAX-bug exposure),
 * and every callee is real-linked from src/ — the same fd2_display_dialog_scene
 * -> fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24) /
 * fd2_cutscene_event_trigger byte-script parsing apply. Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x32E8C (call sequence,
 * constants, and the three battle_anim_phase resets after pages 0/1/2 but not
 * page 3). Note the page-3 dialog call plus the trailing
 * fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are physically a
 * tail-JMP into the shared epilogue at 0x3312D; the emit reconstructs the
 * equivalent straight-line form. See src/emit_issues.json (00032e8c).
 *
 * fd2_chapter_04_init @0x32FB2 is the same shape as chapter 03 (and simpler:
 * two dialog pages bracketing a single cutscene 0x14, portrait set 1 loaded
 * after page 0, two camera pans, NO char init). It likewise has NO numeric
 * computation, NO RNG, NO data-dependent branch, and NO CALL-result consumption
 * (no EAX-bug exposure), and every callee is real-linked from src/ — the same
 * fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang plus the fd2_load_chapter_portraits_and_dump_tmp (fopen
 * FDICON.B24) / fd2_cutscene_event_trigger byte-script parsing apply. Its
 * behavioral test is therefore DEFERRED to Phase 9 on identical grounds;
 * equivalence was verified statically, line-by-line, against the disassembly
 * @0x32FB2 (call sequence, constants, and the single battle_anim_phase reset
 * after page 0 only — page 1 has no reset). Note the page-1 dialog call plus
 * the trailing fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are
 * physically a tail-JMP into the same shared epilogue at 0x3312D used by
 * chapter 03; the emit reconstructs the equivalent straight-line form. See
 * src/emit_issues.json (00032fb2).
 *
 * fd2_chapter_05_init @0x33049 is the same shape as chapter 03: a flat
 * orchestrator playing three dialog pages chained with two cutscenes
 * (0x16/0x15), portrait set 1 loaded mid-run (after one composited frame),
 * two camera pans, NO char init. It likewise has NO numeric computation, NO
 * RNG, NO data-dependent branch, and NO CALL-result consumption (no EAX-bug
 * exposure), and every callee is real-linked from src/ — the same
 * fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang plus the fd2_load_chapter_portraits_and_dump_tmp (fopen
 * FDICON.B24) / fd2_cutscene_event_trigger byte-script parsing apply. Its
 * behavioral test is therefore DEFERRED to Phase 9 on identical grounds;
 * equivalence was verified statically, line-by-line, against the disassembly
 * @0x33049 (call sequence, constants, and the two battle_anim_phase resets
 * after pages 0/1 but not page 2). Note this function is itself the OWNER of
 * four shared alt-entry points (0x3310C/0x3312D/0x3313B/0x33140) that
 * chapters 11/18, 03/04/26, 30, and 07 tail-JMP into; the straight-line body
 * here is the canonical full code path. See src/emit_issues.json (00033049).
 *
 * fd2_chapter_06_init @0x3314B is the MINIMAL chapter init and the same
 * shape as the others, but reduced to the bare skeleton: re-init battle
 * state, reset battle_anim_phase, play a single dialog page (page 0), pan
 * the camera to char 0 — NO cutscene, NO portrait load, NO char init, NO
 * camera-pan-and-window prelude. It likewise has NO numeric computation,
 * NO RNG, NO data-dependent branch, and NO CALL-result consumption (no
 * EAX-bug exposure), and every callee is real-linked from src/ — the same
 * fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang applies to its single dialog page (here there is no
 * portrait load or cutscene at all). Its behavioral test is therefore
 * DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x3314B (the entry
 * block fd2_init_battle_state_for_chapter + battle_anim_phase=0, the single
 * page-0 dialog call, and the final fd2_pan_cursor_to_char(0)). Note the
 * dialog call and the final pan are physically a tail-JMP through three
 * shared alt-entries — 0x3344D (page-0 dialog-arg push, owned by
 * fd2_chapter_12_init), 0x33206 (the dialog call, owned by
 * fd2_chapter_07_init), and 0x33140 (the pan + RET, owned by
 * fd2_chapter_05_init); the emit reconstructs the equivalent straight-line
 * form. See src/emit_issues.json (0003314b).
 *
 * fd2_chapter_07_init @0x33169 is the same shape as chapter 04: a flat
 * orchestrator playing two dialog pages (pages 0/1), portrait set 1 loaded
 * between them, two cutscenes (0x1C/0x1D) each preceded by a camera pan, NO
 * char init. It likewise has NO numeric computation, NO RNG, NO data-dependent
 * branch, and NO CALL-result consumption (no EAX-bug exposure), and every
 * callee is real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24) /
 * fd2_cutscene_event_trigger byte-script parsing apply. Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x33169 (call sequence,
 * constants, the single battle_anim_phase reset after page 0 only, and the
 * init_phase_flag 1/0 bracket around the portrait load). Note the page-1
 * dialog call plus the final fd2_pan_cursor_to_char(0) are physically a
 * tail-JMP into the shared epilogue at 0x33140 (owned by fd2_chapter_05_init,
 * entered directly without a clear-facing); the emit reconstructs the
 * equivalent straight-line form. This function is itself the OWNER of two
 * shared alt-entry points (0x331EA, entered by chapter_24_init; 0x33206,
 * entered by chapter_02_init / chapter_12_init). See src/emit_issues.json
 * (00033169).
 *
 * fd2_chapter_08_init @0x33219 is the SIMPLEST chapter init: a flat
 * orchestrator playing two dialog pages (pages 0/1) bracketing two cutscenes
 * (0x1F/0x20) each preceded by a camera pan, NO char init, NO portrait load,
 * and — uniquely — NO global-state writes at all (it never resets
 * battle_anim_phase, not even between the two pages). It likewise has NO
 * numeric computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (no EAX-bug exposure), and every callee is real-linked from
 * src/ — the same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink
 * keyboard busy-wait hang plus the fd2_cutscene_event_trigger byte-script
 * parsing apply (here there is no portrait load). Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x33219 (call sequence
 * and constants; there are no state writes and no battle_anim_phase resets to
 * check). Note this handler is a two-hop tail consumer: it tail-JMPs (0x33278
 * -> 0x33028) into the alt-entry physically owned by fd2_chapter_04_init, which
 * in turn JMPs (0x33044 -> 0x3312D) into the shared epilogue owned by
 * fd2_chapter_05_init; the emit reconstructs the equivalent straight-line form
 * (page-1 dialog + clear_facing + pan_cursor_to_char(0)). See
 * src/emit_issues.json (00033219).
 *
 * fd2_chapter_09_init @0x3327D is the same shape as chapter 04: a flat
 * orchestrator playing two dialog pages (pages 0/1) bracketing one cutscene
 * (0x23), one camera pan, NO char init, NO portrait load. The one structural
 * addition over chapters 02..08 is a leading constant-bound loop that turns
 * the 11 on-field units to face north — it writes
 * data_fd2_battle_runtime_char_array_ptr[i].sprite_state[1] = 2 for i in
 * 0..10 (struct offset +0x03; disasm element address base + i*0x50 + 3). That
 * loop is a pure constant-strided in-memory fill: NO numeric computation, NO
 * RNG, NO data-dependent branch, and NO CALL-result consumption (no EAX-bug
 * exposure). It is NOT independently host-testable, however, because there is
 * no entry point that runs only the loop — fd2_chapter_09_init falls straight
 * from the loop into the blocking fd2_pan_cursor_and_window / dialog pipeline,
 * so the post-loop facing state is observable only after the function returns,
 * which it cannot do in the silent harness. As with chapters 02..08 every
 * callee is real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_cutscene_event_trigger byte-script parsing apply (here there is no
 * portrait load). Its behavioral test is therefore DEFERRED to Phase 9 on
 * identical grounds; equivalence was verified statically, line-by-line,
 * against the disassembly @0x3327D (the 11-iteration facing loop with its
 * i*0x50+3 element address and value 2, the call sequence and constants, and
 * the single battle_anim_phase reset after page 0 only — page 1 has no reset).
 * Note this handler is fully self-contained (no tail-JMP into another
 * chapter's epilogue and no alt-entry of its own). See src/emit_issues.json
 * (0003327d).
 *
 * fd2_chapter_10_init @0x3332B is a flat orchestrator of the same family as
 * chapters 02..09, and is the first chapter init to seed per-unit status: it
 * re-inits battle state, pans the camera (10,0), puts two NPC units to sleep,
 * plays one dialog page (page 0), and pans the camera to char 0. NO cutscene,
 * NO portrait load, NO char init. The one structural addition over the minimal
 * chapter 06 is two unconditional constant writes that set
 * data_fd2_battle_runtime_char_array_ptr[0x32].status_sleep_flag = 100
 * (索菲亞/Sophia) and [0x33].status_sleep_flag = 100 (卡納恩三世/Kanaan III)
 * — both NPCs start the battle asleep (struct offset +0x26; disasm element
 * address base[0x53A45] + idx*0x50 + 0x26, 0xFA0 = 0x32*0x50, 0xFF0 =
 * 0x33*0x50). Those writes are a pure constant in-memory store: NO numeric
 * computation, NO RNG, NO data-dependent branch, and NO CALL-result consumption
 * (no EAX-bug exposure). They are NOT independently host-testable, however,
 * because there is no entry point that runs only the writes — fd2_chapter_10_init
 * falls straight from them (which sit after fd2_pan_cursor_and_window(10,0)) into
 * the blocking dialog pipeline, so the post-write sleep state is observable only
 * after the function returns, which it cannot do in the silent harness. As with
 * the siblings every callee is real-linked from src/ — the same
 * fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang applies to its single dialog page (here there is no portrait
 * load or cutscene at all). Its behavioral test is therefore DEFERRED to Phase 9
 * on identical grounds; equivalence was verified statically, line-by-line,
 * against the disassembly @0x3332B (the two status_sleep_flag=100 writes with
 * their idx*0x50+0x26 element addresses, the call sequence and constants, and
 * the absence of any battle_anim_phase reset or clear-facing). Note the page-0
 * dialog call plus the final fd2_pan_cursor_to_char(0) are physically a tail-JMP
 * into the same shared chain as chapter 06 — 0x3344D (page-0 dialog-arg push,
 * owned by fd2_chapter_12_init) -> 0x33206 (the dialog call, in
 * fd2_chapter_07_init) -> 0x33140 (the pan + RET, owned by fd2_chapter_05_init,
 * entered directly without a clear-facing); the emit reconstructs the equivalent
 * straight-line form. See src/emit_issues.json (0003332b).
 *
 * fd2_chapter_11_init @0x33367 is the same shape as chapters 02..08: a flat
 * orchestrator playing three dialog pages (pages 0/1/2), portrait set 1 loaded
 * after page 0, two cutscenes (0x26/0x27) chained between the pages, NO char
 * init. It likewise has NO numeric computation, NO RNG, NO data-dependent
 * branch, and NO CALL-result consumption (no EAX-bug exposure), and every
 * callee is real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24) /
 * fd2_cutscene_event_trigger byte-script parsing apply. Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x33367 (call sequence,
 * constants, and the single battle_anim_phase reset after page 0 only — pages 1
 * and 2 have no reset). Note the page-2 dialog call plus the trailing
 * fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are physically a
 * tail-JMP (0x333f0 -> 0x3310c) into the shared epilogue owned by
 * fd2_chapter_05_init (which fd2_chapter_11_init enters at the cutscene-0x27
 * trigger CALL); the emit reconstructs the equivalent straight-line form. See
 * src/emit_issues.json (00033367).
 *
 * fd2_chapter_12_init @0x333F5 is a flat orchestrator of the same family as
 * chapters 02..11 but is CUTSCENE-FIRST (it leads with the portrait load + two
 * cutscenes 0x28/0x29 rather than a dialog page): portrait set 1, two cutscenes
 * each preceded by a camera pan, then clear-facing, then a single dialog page
 * (page 0) before the final camera-to-char pan, NO char init. It likewise has
 * NO numeric computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (no EAX-bug exposure), and every callee is real-linked from src/
 * — the same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink
 * keyboard busy-wait hang plus the fd2_load_chapter_portraits_and_dump_tmp
 * (fopen FDICON.B24) / fd2_cutscene_event_trigger byte-script parsing apply.
 * Its behavioral test is therefore DEFERRED to Phase 9 on identical grounds;
 * equivalence was verified statically, line-by-line, against the disassembly
 * @0x333F5 (call sequence, constants, the init_phase_flag 1/0 bracket around
 * the portrait load, the absence of any battle_anim_phase reset, and the
 * clear-facing sitting BEFORE the single page-0 dialog rather than after it).
 * Note the page-0 dialog call plus the final fd2_pan_cursor_to_char(0) are
 * physically a tail-JMP from this handler's own dialog-arg push (0x3344D) into
 * the shared epilogue at 0x33206 (owned by fd2_chapter_07_init) -> 0x33140
 * (owned by fd2_chapter_05_init, entered directly without a clear-facing); the
 * emit reconstructs the equivalent straight-line form. This handler itself OWNS
 * two shared alt-entry points: 0x33440 (cutscene-0x29 CALL onward), entered by
 * chapter_22_init; and 0x3344D (page-0 dialog-arg push onward), the shared
 * page-0 dialog tail entered by chapters 06/10/13/14/17. See
 * src/emit_issues.json (000333f5).
 *
 * fd2_chapter_13_init @0x3346B is the SMALLEST chapter init in the game (17
 * bytes) — even more reduced than the minimal chapter 06: it only re-inits
 * battle state, plays a single dialog page (page 0), and pans the camera to
 * char 0. NO cutscene, NO portrait load, NO char init, NO camera-pan-and-window
 * prelude, NO battle_anim_phase reset (chapter 06 has one; chapter 13 does
 * not), and NO clear-facing. It likewise has NO numeric computation, NO RNG,
 * NO data-dependent branch, and NO CALL-result consumption (no EAX-bug
 * exposure), and every callee is real-linked from src/ — the same
 * fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang applies to its single dialog page (here there is no portrait
 * load or cutscene at all). Its behavioral test is therefore DEFERRED to Phase
 * 9 on identical grounds; equivalence was verified statically, line-by-line,
 * against the disassembly @0x3346B (the entry block
 * fd2_init_battle_state_for_chapter only, the single page-0 dialog call, and
 * the final fd2_pan_cursor_to_char(0); the absence of any battle_anim_phase
 * reset and of any clear-facing). Note the dialog call and the final pan are
 * physically a tail-JMP through the same shared chain as chapters 06/10 —
 * 0x3344D (page-0 dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206
 * (the dialog call, in fd2_chapter_07_init) -> 0x33140 (the pan + RET, owned
 * by fd2_chapter_05_init, entered directly without a clear-facing); the emit
 * reconstructs the equivalent straight-line form. The handler's own entry
 * (0x33470, the fd2_init_battle_state_for_chapter CALL onward) is itself a
 * shared alt-entry tail-JMPed into by fd2_chapter_16_init and
 * fd2_chapter_19_20_21_init_shared. See src/emit_issues.json (0003346b).
 *
 * fd2_chapter_14_init @0x3347C is a minimal chapter init of the same family
 * as chapter 06 / 13: it re-inits battle state, pans the camera-and-window
 * once (0x14,0x14), plays a single dialog page (page 0), and pans the camera
 * to char 0. NO cutscene, NO portrait load, NO char init, NO clear-facing,
 * and — unlike chapter 06 — NO battle_anim_phase reset (there is no MOV
 * [0x51A83],0 on its code path). The lone camera-pan-and-window prelude is
 * the only thing distinguishing it from the bare chapter 13. It likewise has
 * NO numeric computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (no EAX-bug exposure), and every callee is real-linked from src/
 * — the same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink
 * keyboard busy-wait hang applies to its single dialog page (here there is no
 * portrait load or cutscene at all). Its behavioral test is therefore DEFERRED
 * to Phase 9 on identical grounds; equivalence was verified statically,
 * line-by-line, against the disassembly @0x3347C (the entry block
 * fd2_init_battle_state_for_chapter + the single fd2_pan_cursor_and_window
 * (0x14,0x14), the single page-0 dialog call, and the final
 * fd2_pan_cursor_to_char(0); the absence of any battle_anim_phase reset and of
 * any clear-facing). Note the dialog call and the final pan are physically a
 * tail-JMP through the same shared chain as chapters 06/10/13 — 0x3344D
 * (page-0 dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206 (the
 * dialog call, in fd2_chapter_07_init) -> 0x33140 (the pan + RET, owned by
 * fd2_chapter_05_init, entered directly without a clear-facing); the emit
 * reconstructs the equivalent straight-line form. See src/emit_issues.json
 * (0003347c).
 *
 * fd2_chapter_15_init @0x334D9 is the FIRST chapter init in this file with a
 * DATA-DEPENDENT dialog page selection and a consumed CALL return value (so,
 * unlike chapters 01..14, it is NOT a pure straight-line orchestrator). It
 * re-inits battle state, then computes a three-page narrative base from party
 * composition — page_base = (fd2_check_party_has_char_id(0xC) ^ 1) * 3, i.e.
 * base 0 (pages 0/1/2) when 凱麗 is present and base 3 (pages 3/4/5) when 凱麗
 * is absent — and plays pages page_base / page_base+1 / page_base+2 chained
 * with one camera pan (0x18,0x11) and one cutscene (0x30), with a single
 * battle_anim_phase reset after the page_base+1 dialog. The computation is the
 * EAX-bug-risk point: in the disassembly @0x334D9 it is XOR AL,1; MOV AH,3;
 * MUL AH (8-bit AX = AL*AH) with MOVZX EBX,AL taking the low byte, and the
 * XOR consumes the genuine byte return of the CALL — the emit encodes it as the
 * real return of fd2_check_party_has_char_id (verified against the assembly, not
 * trusted from the decompiler).
 *
 * Despite the added computation, its behavioral test is STILL DEFERRED to Phase
 * 9 (reason proven, not convenience). page_base is consumed by the very first
 * fd2_display_dialog_scene call, and there is no entry point that runs only the
 * computation before that blocking call (the handler's own alt-entry 0x33594 is
 * the final fd2_pan_cursor_to_char(0)+RET tail, after all three dialogs). The
 * computation cannot be observed in isolation because:
 *   - fd2_init_battle_state_for_chapter runs FIRST and RELOADS current_chapter_text
 *     from the real FDTXT.DAT (it is the linked chapter-battle-data loader), so the
 *     immediate-END / single-glyph fixture-page trick used by the army-overview
 *     tests (tests/gfx/rndstat.c) cannot substitute observable pages here — the
 *     dialog VM would parse the real FDTXT chapter stream.
 *   - Real FDTXT pages contain -3 PAGE BREAK opcodes, which drive
 *     fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink(1), the
 *     real-linked keyboard busy-wait that hangs forever in the silent harness.
 *   - fd2_check_party_has_char_id is itself still the testglob.c fake (slated for
 *     src/util/misc.c); its captured arg/return are only reachable after the
 *     function survives past the first blocking dialog, which it cannot.
 * The emit must not be distorted to make it host-testable (forbidden). Equivalence
 * was therefore verified statically, line-by-line, against the disassembly @0x334D9
 * (the page_base XOR/MUL computation and its consumption as the real CALL return,
 * the three page_base+{0,1,2} dialog page indices, the (0x18,0x11) pan, the 0x30
 * cutscene, the single battle_anim_phase reset after page_base+1 only, and the
 * self-contained 0x33594 pan_cursor_to_char(0)+RET tail shared into by chapters
 * 23/28). See src/emit_issues.json (000334d9).
 *
 * fd2_chapter_16_init @0x335A0 is, in the binary, a PURE THUNK (just PUSH 0x28;
 * JMP 0x33470) that tail-jumps into the shared body owned by fd2_chapter_13_init
 * (0x33470 = fd2_chapter_13_init + 5, its CALL __CHK site). It therefore runs the
 * IDENTICAL body to chapter 13 — the SMALLEST/minimal init family: re-init battle
 * state, a single dialog page (page 0), pan the camera to char 0; NO cutscene, NO
 * portrait load, NO char init, NO camera-pan-and-window prelude, NO battle_anim_phase
 * reset, NO clear-facing, and (like chapter 13) zero global writes at all. It
 * likewise has NO numeric computation, NO RNG, NO data-dependent branch, and NO
 * CALL-result consumption (no EAX-bug exposure), and every callee
 * (fd2_init_battle_state_for_chapter, fd2_display_dialog_scene, fd2_pan_cursor_to_char)
 * is real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang applies to its single
 * dialog page (here there is no portrait load or cutscene at all). Its behavioral
 * test is therefore DEFERRED to Phase 9 on identical grounds (and there is no state
 * contract to assert — zero global writes); equivalence was verified statically,
 * line-by-line, against the disassembly @0x335A0 + the shared tail at 0x33470 (PUSH
 * 0x28 -> the __CHK frame check; CALL fd2_init_battle_state_for_chapter; then JMP
 * 0x3344D into the same shared page-0 dialog chain used by chapters 06/10/13/14 —
 * 0x3344D page-0 dialog-arg push owned by fd2_chapter_12_init -> 0x33206 the dialog
 * call in fd2_chapter_07_init -> 0x33140 the pan + RET owned by fd2_chapter_05_init,
 * entered directly without a clear-facing). The straight-line form is identical to
 * the fd2_chapter_13_init body. The leading __CHK(0x28) is the Watcom frame-size
 * stack-probe and is not part of the source body. See src/emit_issues.json
 * (000335a0).
 *
 * fd2_chapter_17_init @0x335AA is a near-minimal chapter init of the same family
 * as chapters 06/13/14/16, with one structural addition: a DATA-DEPENDENT,
 * CALL-return-consuming branch that gates the portrait load (so, like chapter
 * 15, it is NOT a pure straight-line orchestrator). It re-inits battle state,
 * loads portrait set 1 ONLY when 蜜蒂 (char id 0x12) is NOT in the party
 * (if fd2_check_party_has_char_id(0x12) == 0), plays a single dialog page
 * (page 0), and pans the camera to char 0. NO cutscene, NO char init, NO
 * camera-pan-and-window prelude, NO battle_anim_phase reset, NO clear-facing.
 * The gate is the EAX-bug-risk point: in the disassembly @0x335AA it is TEST
 * EAX,EAX; JNZ (skip the load), and the TEST consumes the genuine byte return
 * of the CALL — the emit encodes it as the real return of
 * fd2_check_party_has_char_id (verified against the assembly @0x335C3, not
 * trusted from the decompiler), as `if (... == 0)`.
 *
 * Despite the added branch, its behavioral test is DEFERRED to Phase 9 (reason
 * proven, not convenience), on identical grounds to chapter 15. The branch
 * outcome cannot be observed in isolation because:
 *   - fd2_init_battle_state_for_chapter runs FIRST and is the real-linked
 *     chapter-battle-data loader (it calls fd2_load_chapter_battle_data +
 *     fd2_composite_battle_frame(1) + fd2_play_palette_fade_in), so there is no
 *     host-safe slice before the branch.
 *   - both the load-taken (蜜蒂 absent) and load-skipped (蜜蒂 present) paths
 *     converge on the SAME single fd2_display_dialog_scene(page 0) call, which
 *     is real-linked (src/dialog/dialog.c) and reaches
 *     fd2_wait_for_input_dialog_with_blink(1) on a -3 PAGE BREAK opcode — the
 *     real-linked keyboard busy-wait that hangs forever in the silent harness —
 *     so the function never returns and neither the portrait-load side effect
 *     (fopen FDICON.B24 + dump FD2.TMP) nor any post-branch state is observable.
 *   - fd2_check_party_has_char_id is itself still the testglob.c fake (slated
 *     for src/util/misc.c); its captured arg/return are only reachable after the
 *     function survives past the blocking dialog, which it cannot.
 * The emit must not be distorted to make it host-testable (forbidden).
 * Equivalence was therefore verified statically, line-by-line, against the
 * disassembly @0x335AA (the gated portrait load with its TEST EAX,EAX; JNZ
 * consuming the real CALL return, the single page-0 dialog call, and the final
 * fd2_pan_cursor_to_char(0); the absence of any battle_anim_phase reset and of
 * any clear-facing). Note both branch paths converge on a tail-JMP through the
 * same shared page-0 dialog chain used by chapters 06/10/13/14/16 — 0x3344D
 * (page-0 dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206 (the dialog
 * call, in fd2_chapter_07_init) -> 0x33140 (the pan + RET, owned by
 * fd2_chapter_05_init, entered directly without a clear-facing); the emit
 * reconstructs the equivalent straight-line form. See src/emit_issues.json
 * (000335aa).
 *
 * fd2_chapter_18_init @0x335DA is the same shape as chapters 02..08/11: a
 * flat orchestrator playing three dialog pages (pages 0/1/2) chained with
 * two cutscenes (0x36/0x37), each cutscene preceded by a camera pan to the
 * same (0x10,4) target, NO portrait load, NO char init. It is back to a pure
 * straight-line orchestrator (unlike the data-dependent chapters 15/17): NO
 * numeric computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (no EAX-bug exposure), and every callee is real-linked from
 * src/ — the same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink
 * keyboard busy-wait hang plus the fd2_cutscene_event_trigger byte-script
 * parsing apply (here there is no portrait load). Its behavioral test is
 * therefore DEFERRED to Phase 9 on identical grounds; equivalence was verified
 * statically, line-by-line, against the disassembly @0x335DA (call sequence,
 * constants, the two (0x10,4) camera pans, and the two battle_anim_phase
 * resets after pages 0 and 1 but not page 2). Note the page-2 dialog call plus
 * the trailing fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are
 * physically a tail-JMP (0x3366f -> 0x3310c) into the shared epilogue owned by
 * fd2_chapter_05_init (the same 0x3310C alt-entry fd2_chapter_11_init enters,
 * at the cutscene-trigger CALL); the emit reconstructs the equivalent
 * straight-line form. See src/emit_issues.json (000335da).
 *
 * fd2_chapter_19_20_21_init_shared @0x33674 is the game's ONLY three-chapter
 * shared init (chapters 19/20/21) and, in the binary, a 10-byte PURE THUNK
 * (PUSH 0x28; JMP 0x33470) that tail-jumps into the shared body owned by
 * fd2_chapter_13_init (0x33470 = fd2_chapter_13_init + 5, its CALL __CHK site) —
 * the SAME 0x33470 alt-entry that fd2_chapter_16_init also enters. Its executed
 * path is therefore byte-identical to chapters 13 and 16, so it runs the
 * IDENTICAL minimal/smallest-init body: re-init battle state, a single dialog
 * page (page 0), pan the camera to char 0; NO cutscene, NO portrait load, NO
 * char init, NO camera-pan-and-window prelude, NO battle_anim_phase reset, NO
 * clear-facing, and zero global writes at all. It likewise has NO numeric
 * computation, NO RNG, NO data-dependent branch, and NO CALL-result consumption
 * (no EAX-bug exposure), and every callee (fd2_init_battle_state_for_chapter,
 * fd2_display_dialog_scene, fd2_pan_cursor_to_char) is real-linked from src/ —
 * the same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink
 * keyboard busy-wait hang applies to its single dialog page (here there is no
 * portrait load or cutscene at all). Its behavioral test is therefore DEFERRED
 * to Phase 9 on identical grounds to chapters 13/16 (and there is no state
 * contract to assert — zero global writes); equivalence was verified statically,
 * line-by-line, against the disassembly @0x33674 + the shared tail at 0x33470
 * (PUSH 0x28 -> the __CHK frame check; CALL fd2_init_battle_state_for_chapter;
 * then JMP 0x3344D into the same shared page-0 dialog chain used by chapters
 * 06/10/13/14/16 — 0x3344D page-0 dialog-arg push owned by fd2_chapter_12_init
 * -> 0x33206 the dialog call in fd2_chapter_07_init -> 0x33140 the pan + RET
 * owned by fd2_chapter_05_init, entered directly without a clear-facing). The
 * straight-line form is identical to the fd2_chapter_13_init body. Unlike the
 * other chapter inits (one dispatch-table slot each), this single function is
 * dispatched from THREE consecutive table slots (xrefs @0x51DB9 / 0x51DBD /
 * 0x51DC1 = chapters 19/20/21). See src/emit_issues.json (00033674).
 *
 * fd2_chapter_22_init @0x3367E is a minimal flat orchestrator of the same
 * family as chapter 06/14: re-init battle state, one camera-pan-and-window
 * (0x10,0x1C), a single cutscene (0x43), clear-facing, a single dialog page
 * (page 0), pan the camera to char 0. NO portrait load, NO char init, and
 * (like chapter 14) NO battle_anim_phase reset on its code path. It is back to
 * a pure straight-line orchestrator (unlike the data-dependent chapters
 * 15/17): NO numeric computation, NO RNG, NO data-dependent branch, and NO
 * CALL-result consumption (no EAX-bug exposure), and every callee is
 * real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_cutscene_event_trigger byte-script parsing apply (here there is no
 * portrait load). Its behavioral test is therefore DEFERRED to Phase 9 on
 * identical grounds; equivalence was verified statically, line-by-line,
 * against the disassembly @0x3367E (call sequence, constants, the single
 * (0x10,0x1C) camera pan, the 0x43 cutscene, the clear-facing sitting BEFORE
 * the single page-0 dialog rather than after it, and the absence of any
 * battle_anim_phase reset). Note this handler physically contains only its
 * entry block (init battle state + the one fd2_pan_cursor_and_window) and then
 * pushes the cutscene-0x43 arg and tail-JMPs (0x3369B -> 0x33440) into the
 * shared body owned by fd2_chapter_12_init: the cutscene-trigger CALL, then
 * clear-facing, then the shared page-0 dialog chain 0x3344D (page-0 dialog-arg
 * push, owned by fd2_chapter_12_init) -> 0x33206 (the dialog call, in
 * fd2_chapter_07_init) -> 0x33140 (the pan + RET, owned by
 * fd2_chapter_05_init, entered directly without a clear-facing); the emit
 * reconstructs the equivalent straight-line form. See src/emit_issues.json
 * (0003367e).
 *
 * fd2_chapter_23_init @0x336A0 is the LARGEST chapter init in the game (548
 * bytes): a cinematic "reassemble the party" prologue built around a
 * screen-wide spell. Structurally it adds three things over the flat
 * orchestrators (chapters 02..14/18/22) — (1) a constant-bound mark-dead loop
 * over the 16 active party slots (fd2_mark_char_as_dead(i), i in 0..0xF);
 * (2) a constant-strided post-spell HP-survivor revive filter (for i in
 * 0..0xF, if runtime_char[i].hp_current != 0 then .flags = 0 and
 * .sprite_state[1] = 2; disasm element address base + i*0x50, survivor test
 * reads word [EAX+0x40], writes byte [EAX+5] then byte [EAX+3]); and (3) the
 * spell cast fd2_cast_screen_wide_spell_with_fade(cursor_screen_x + 6,
 * cursor_screen_y + 5, 10, 8) plus a portrait-load palette-flash sequence and
 * two fixed slot-0x10/0x11 sprite_state[1]=2 writes. It is NOT a pure
 * straight-line orchestrator (it has the two loops and the HP-filter branch),
 * BUT it still has NO numeric computation, NO RNG, and — critically — NO
 * CALL-result consumption: the HP-filter branch tests an IN-MEMORY value
 * (runtime_char[i].hp_current), not a CALL return, so there is NO Ghidra
 * EAX-tracking-bug exposure (verified against the disassembly @0x336A0: the
 * only CALL whose result could be read is fd2_mark_char_as_dead, and its EAX
 * is discarded — ADD ESP,0x4; INC EBX).
 *
 * Its behavioral test is DEFERRED to Phase 9 (reason proven, not convenience),
 * on identical grounds to chapters 09/10 (constant in-memory loop writes) and
 * the rest of this file. The loops and writes are NOT independently
 * host-testable because there is no entry point that runs only them:
 *   - fd2_init_battle_state_for_chapter (real-linked, src/battle/btl_init.c)
 *     runs FIRST and RELOADS current_chapter_text from the real FDTXT.DAT
 *     (it is the chapter-battle-data loader: fd2_load_chapter_battle_data +
 *     fd2_composite_battle_frame(1) + fd2_play_palette_fade_in), so there is
 *     no host-safe slice before the body.
 *   - the mark-dead loop falls straight into
 *     fd2_cast_screen_wide_spell_with_fade and then five
 *     fd2_display_dialog_scene(page 0..4) calls; fd2_display_dialog_scene is
 *     real-linked (src/dialog/dialog.c) and reaches
 *     fd2_wait_for_input_dialog_with_blink(1) (real-linked, src/input/input.c)
 *     on a -3 PAGE BREAK opcode in the real FDTXT chapter stream — the
 *     keyboard busy-wait that hangs forever in the silent automated harness —
 *     so the function never returns and neither the post-loop revive state nor
 *     the slot-0x10/0x11 facing writes are observable at unit level.
 *   - fd2_mark_char_as_dead is itself real-linked (src/battle/btl_turn.c), so
 *     the loop cannot be observed via a capture stub, and the emit must not be
 *     distorted to make it host-testable (forbidden).
 * Equivalence was therefore verified statically, line-by-line, against the
 * disassembly @0x336A0 (the two 16-iteration loops with their i*0x50 element
 * addresses and the hp_current/flags/sprite_state[1] field offsets, the
 * fd2_cast_screen_wide_spell_with_fade(+6,+5,10,8) spell call, the five
 * page-0..4 dialog calls chained with cutscenes 0x44/0x45/0x46, the
 * portrait-load palette-flash bracket (add 0xFF saturate -> add 0 restore),
 * the two slot-0x10/0x11 sprite_state[1]=2 writes, and the single
 * battle_anim_phase reset after page 3 only). Note the final
 * fd2_pan_cursor_to_char(0) is physically a tail-JMP (0x336BF -> 0x33594) into
 * the shared epilogue owned by fd2_chapter_15_init; the emit reconstructs the
 * equivalent straight-line form. See src/emit_issues.json (000336a0).
 *
 * fd2_chapter_24_init @0x338C4 is back to a pure straight-line orchestrator of
 * the same family as chapters 02..14/18/22: re-init battle state, play a dialog
 * page (page 0), load portrait set 1, sweep the camera-and-window to the four
 * map corners — fd2_pan_cursor_and_window to (0,4)/(0,0x16)/(0x1A,0x18)/(0x1A,2)
 * each held 400ms via __delay_thunk_375b2(400) — play a second dialog page
 * (page 1), and pan the camera to char 0. NO cutscene, NO char init, NO
 * battle_anim_phase reset, NO clear-facing. It has NO numeric computation, NO
 * RNG, NO data-dependent branch, NO loops, and NO CALL-result consumption (no
 * EAX-bug exposure — the dialog-scene CALL returns are discarded), and every
 * callee is real-linked from src/ — the same fd2_display_dialog_scene ->
 * fd2_wait_for_input_dialog_with_blink keyboard busy-wait hang plus the
 * fd2_load_chapter_portraits_and_dump_tmp (fopen FDICON.B24) apply. Its
 * behavioral test is therefore DEFERRED to Phase 9 on identical grounds;
 * equivalence was verified statically, line-by-line, against the disassembly
 * @0x338C4 (call sequence, constants, the four corner pans each followed by a
 * 400ms hold, and the absence of any battle_anim_phase reset / clear-facing).
 * Note the page-1 dialog call plus the final fd2_pan_cursor_to_char(0) are
 * physically a tail-JMP (0x33965 -> 0x331EA) into the alt-entry owned by
 * fd2_chapter_07_init (its page-1 dialog-arg push), which in turn JMPs (0x33214
 * -> 0x33140) into the shared epilogue owned by fd2_chapter_05_init (entered
 * directly without a clear-facing); the emit reconstructs the equivalent
 * straight-line form. See src/emit_issues.json (000338c4).
 *
 * fd2_chapter_25_init @0x3396A is the ONLY chapter init that stages an
 * earthquake set-piece, but structurally it remains a flat orchestrator of
 * the same family as chapters 02..14/18/22/24: re-init battle state, load the
 * earthquake SFX wave from FDOTHER.DAT (fd2_load_dat_resource(0x51A4D, 0,
 * 0x58)) into the shared status-effect SFX handle, pan the camera-and-window
 * (5,0), play dialog page 1, memset the 0x25680-byte large game-state buffer,
 * then four SFX-prefixed screen-shake cycles — three normal-magnitude
 * (fd2_animate_screen_shake(0x14)) shakes separated by __delay_thunk_375b2(600)
 * holds and a final 3x-magnitude (0x3C) shake with no trailing hold — then
 * dialog page 2, pan to char 0, and fd2_play_and_free_status_effect_sfx(). NO
 * char init, NO portrait load. It has NO numeric computation, NO RNG, NO
 * data-dependent branch, and NO loops; the ONLY CALL-result consumed is the
 * fd2_load_dat_resource return, which is stored verbatim to
 * data_fd2_audio_status_effect_sfx_handle_ptr (disasm MOV [0x53B13],EAX — a
 * direct pointer store, verified against the assembly, NOT a branch/compute,
 * so no Ghidra EAX-tracking-bug exposure). Every other CALL return (the two
 * fd2_display_dialog_scene calls) is discarded.
 *
 * Its behavioral test is DEFERRED to Phase 9 (reason proven, not convenience),
 * on identical grounds to chapters 01..24:
 *   - fd2_init_battle_state_for_chapter (real-linked) runs FIRST and RELOADS
 *     current_chapter_text from the real FDTXT.DAT (it is the chapter-battle-
 *     data loader), so there is no host-safe slice before the body, and the
 *     loaded SFX handle / memset state cannot be set up independently.
 *   - the body falls straight into fd2_display_dialog_scene(page 1), which is
 *     real-linked (src/dialog/dialog.c) and reaches
 *     fd2_wait_for_input_dialog_with_blink(1) on a -3 PAGE BREAK opcode in the
 *     real FDTXT chapter stream — the real-linked keyboard busy-wait that
 *     hangs forever in the silent automated harness — so the function never
 *     returns and neither the loaded SFX handle, the memset, the shake
 *     sequence, nor the final SFX free is observable at unit level. (The
 *     fd2_load_dat_resource fopen of the REAL FDOTHER.DAT entry 0x58 is
 *     likewise only reached on a path that immediately hangs on the next
 *     dialog call, so it cannot be driven to a host-observable assertion.)
 *   - fd2_load_dat_resource / fd2_play_sfx_with_handle / fd2_animate_screen_shake
 *     / fd2_play_and_free_status_effect_sfx are all real-linked from src/, so
 *     the call ordering cannot be observed via capture stubs, and the emit must
 *     not be distorted to make it host-testable (forbidden).
 * Equivalence was therefore verified statically, line-by-line, against the
 * disassembly @0x3396A (the handle clear-to-0 then fd2_load_dat_resource(
 * 0x51A4D,0,0x58) store to [0x53B13], the (5,0) camera pan, the two page-1/
 * page-2 dialog calls, the memset(0x25680), the four SFX+shake cycles with
 * shake magnitudes 0x14/0x14/0x14/0x3C and the three 600ms holds after the
 * first three only, and the final fd2_pan_cursor_to_char(0)). Note the final
 * fd2_play_and_free_status_effect_sfx() is physically a tail-JMP (0x33AA9 ->
 * 0x1D4F6) to that self-contained handler; the emit reconstructs the
 * equivalent straight-line call form. See src/emit_issues.json (0003396a).
 *
 * fd2_chapter_26_init @0x33AAE is back to a minimal flat orchestrator of the
 * same family as chapter 22: re-init battle state, one camera-pan-and-window
 * (9,0x27), a single cutscene (0x4C), a single dialog page (page 0),
 * clear-facing, pan the camera to char 0. NO portrait load, NO char init, and
 * NO battle_anim_phase reset on its code path. It is a pure straight-line
 * orchestrator: NO numeric computation, NO RNG, NO data-dependent branch, NO
 * loops, and NO CALL-result consumption (no EAX-bug exposure — the dialog-scene
 * CALL return is discarded), and every callee is real-linked from src/ — the
 * same fd2_display_dialog_scene -> fd2_wait_for_input_dialog_with_blink keyboard
 * busy-wait hang plus the fd2_cutscene_event_trigger byte-script parsing apply
 * (here there is no portrait load). Its behavioral test is therefore DEFERRED to
 * Phase 9 on identical grounds; equivalence was verified statically,
 * line-by-line, against the disassembly @0x33AAE (call sequence, constants, the
 * single (9,0x27) camera pan, the 0x4C cutscene, the single page-0 dialog, the
 * clear-facing AFTER it, and the absence of any battle_anim_phase reset). Note
 * this handler physically contains only its entry block (init battle state, the
 * one fd2_pan_cursor_and_window(9,0x27), and the cutscene-0x4C trigger) and then
 * pushes the 9 page-0 dialog-scene args and tail-JMPs (0x33AEC -> 0x3312D) into
 * the shared epilogue owned by fd2_chapter_05_init (PUSH current_chapter_text;
 * CALL fd2_display_dialog_scene; clear-facing; pan_cursor_to_char(0); RET — the
 * same 0x3312D alt-entry chapters 03/04 reach); the emit reconstructs the
 * equivalent straight-line form. See src/emit_issues.json (00033aae).
 *
 * fd2_chapter_27_init @0x33AF1 is the GOOD/BAD ENDING fork chapter — a
 * cinematic prologue built around three screen-wide spell visual effects:
 * re-init battle state, one camera-pan-and-window (9,0x31), a cutscene
 * (0x4C) and dialog page 0, then — ONLY if any party member carries item
 * 100 (天空之鑰 / Sky Key) — a bonus dialog page 3, then dialog page 4,
 * a re-pan, and three spell-effect beats each followed by a full VGA
 * palette reset (add 0) and a dialog page (5/6/7), with cutscene 0x51
 * before page 6; clear-facing, pan the camera to char 0. NO portrait load,
 * NO char init, and NO battle_anim_phase reset on its code path. The three
 * spell beats are fd2_cast_screen_wide_spell_with_fade(.,.,2,2) epicentered
 * at the live battle cursor with per-beat tile offsets (+0/+3, +0/+0,
 * +2/+0). Unlike the pure straight-line orchestrators (chapters 22/24/26),
 * it has a DATA-DEPENDENT, CALL-return-consuming branch (so, like chapters
 * 15/17, it is NOT a pure orchestrator): the Sky-Key gate is the
 * EAX-bug-risk point — in the disassembly @0x33AF1 it is CALL
 * fd2_any_char_has_item; CMP EAX,-1; JZ (skip page 3), and the CMP consumes
 * the genuine return of the CALL — the emit encodes it as the real return
 * of fd2_any_char_has_item, `if (... != -1)` (verified against the assembly
 * @0x33B47, NOT trusted from the decompiler). It is the sole CALL-result
 * consumed; the dialog-scene / spell / palette CALL returns are discarded.
 *
 * Despite the added branch, its behavioral test is DEFERRED to Phase 9
 * (reason proven, not convenience), on identical grounds to chapters 15/17.
 * The branch outcome cannot be observed in isolation because:
 *   - fd2_init_battle_state_for_chapter (real-linked) runs FIRST and RELOADS
 *     current_chapter_text from the real FDTXT.DAT (it is the chapter-battle-
 *     data loader), so there is no host-safe slice before the body.
 *   - the body reaches fd2_display_dialog_scene(page 0) BEFORE the Sky-Key
 *     gate; that call is real-linked (src/dialog/dialog.c) and reaches
 *     fd2_wait_for_input_dialog_with_blink(1) on a -3 PAGE BREAK opcode in
 *     the real FDTXT chapter stream — the real-linked keyboard busy-wait
 *     that hangs forever in the silent automated harness — so the function
 *     never reaches the fd2_any_char_has_item branch, and neither the
 *     bonus-page-3 outcome nor any later spell/palette/dialog state is
 *     observable at unit level.
 *   - fd2_any_char_has_item is itself not yet emitted (slated for
 *     src/util/misc.c) and fd2_cast_screen_wide_spell_with_fade /
 *     fd2_set_vga_palette_range_with_add / fd2_cutscene_event_trigger are
 *     real-linked from src/, so the call ordering cannot be observed via
 *     capture stubs, and the emit must not be distorted to make it
 *     host-testable (forbidden).
 * Equivalence was therefore verified statically, line-by-line, against the
 * disassembly @0x33AF1 (the (9,0x31) pan, the 0x4C cutscene, the page-0
 * dialog, the Sky-Key gate consuming the real CALL return with its CMP
 * EAX,-1; JZ over the page-3 dialog, the page-4 dialog, the second (9,0x31)
 * pan, the three fd2_cast_screen_wide_spell_with_fade beats with their
 * cursor +0/+3, +0/+0, +2/+0 epicenter offsets and (2,2) radius/increment,
 * the three fd2_set_vga_palette_range_with_add(0,0xFF,0) palette resets
 * after each beat, the 0x51 cutscene before page 6, the page-5/6/7 dialogs,
 * and the absence of any battle_anim_phase reset). Note the page-7 dialog
 * call plus the trailing fd2_clear_all_chars_facing() and
 * fd2_pan_cursor_to_char(0) are physically a tail-JMP (0x33C98 -> 0x3312D)
 * into the shared epilogue owned by fd2_chapter_05_init (the same 0x3312D
 * alt-entry chapters 03/04/26 reach); the emit reconstructs the equivalent
 * straight-line form. See src/emit_issues.json (00033af1).
 */

#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

void run_field_chinit_tests(void)
{
    printf("Suite: field/chinit\n");
    printf("  (fd2_chapter_01_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003231b)\n");
    printf("  (fd2_chapter_02_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00032d18)\n");
    printf("  (fd2_chapter_03_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00032e8c)\n");
    printf("  (fd2_chapter_04_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00032fb2)\n");
    printf("  (fd2_chapter_05_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00033049)\n");
    printf("  (fd2_chapter_06_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003314b)\n");
    printf("  (fd2_chapter_07_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00033169)\n");
    printf("  (fd2_chapter_08_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00033219)\n");
    printf("  (fd2_chapter_09_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003327d)\n");
    printf("  (fd2_chapter_10_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003332b)\n");
    printf("  (fd2_chapter_11_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00033367)\n");
    printf("  (fd2_chapter_12_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 000333f5)\n");
    printf("  (fd2_chapter_13_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003346b)\n");
    printf("  (fd2_chapter_14_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003347c)\n");
    printf("  (fd2_chapter_15_init: data-dependent page_base swap; behavioral "
           "test deferred to Phase 9 integration; see src/emit_issues.json "
           "000334d9)\n");
    printf("  (fd2_chapter_16_init: pure thunk into chapter_13 shared body; "
           "behavioral test deferred to Phase 9 integration; see "
           "src/emit_issues.json 000335a0)\n");
    printf("  (fd2_chapter_17_init: data-dependent gated portrait load; "
           "behavioral test deferred to Phase 9 integration; see "
           "src/emit_issues.json 000335aa)\n");
    printf("  (fd2_chapter_18_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 000335da)\n");
    printf("  (fd2_chapter_19_20_21_init_shared: pure thunk into chapter_13 "
           "shared body (3-chapter shared init); behavioral test deferred to "
           "Phase 9 integration; see src/emit_issues.json 00033674)\n");
    printf("  (fd2_chapter_22_init: behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 0003367e)\n");
    printf("  (fd2_chapter_23_init: largest init (548B); mark-dead loop + "
           "screen-wide spell + HP-survivor revive filter (no CALL-result "
           "consumption); behavioral test deferred to Phase 9 integration; "
           "see src/emit_issues.json 000336a0)\n");
    printf("  (fd2_chapter_24_init: 4-corner camera scan orchestrator; "
           "behavioral test deferred to Phase 9 integration; see "
           "src/emit_issues.json 000338c4)\n");
    printf("  (fd2_chapter_25_init: earthquake set-piece (SFX load + 4x "
           "screen shake); behavioral test deferred to Phase 9 integration; "
           "see src/emit_issues.json 0003396a)\n");
    printf("  (fd2_chapter_26_init: minimal orchestrator (pan + cutscene 0x4C "
           "+ single dialog page); behavioral test deferred to Phase 9 "
           "integration; see src/emit_issues.json 00033aae)\n");
    printf("  (fd2_chapter_27_init: GOOD/BAD ending fork; data-dependent "
           "Sky-Key bonus page + 3x screen-wide spell; behavioral test "
           "deferred to Phase 9 integration; see src/emit_issues.json "
           "00033af1)\n");
    printf("\n");
}
