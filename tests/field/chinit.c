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
    printf("\n");
}
