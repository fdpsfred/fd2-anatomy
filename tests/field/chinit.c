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
    printf("\n");
}
