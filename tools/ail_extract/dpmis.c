/* dpmi_stub.c - no-op stubs for fd2_dpmi_lock/unlock region/size.
 *
 * RATIONALE: ailv3.lib's AIL_internal_init_globals_once and
 * AIL_internal_register_mix_globals call fd2_dpmi_lock_region with two
 * hardcoded binary-time addresses (originally contiguous AIL ISR-touchable
 * code range, ~20 KB). After rebuild the linker arranges per-fn .obj in
 * its own order, so those two address arguments resolve to wlink-relocated
 * fn entries with a much larger (often MBs) gap between them. DPMI INT 31h
 * fn 0x600 (Lock Linear Region) then commits + locks every page in that
 * inflated range and runs the host out of memory.
 *
 * Replacing lock/unlock with no-op stubs is safe for test_audio.c because:
 *   - test_audio doesn't install its own ISRs that need page-locked code
 *   - AIL's own ISR is installed via DPMI int 21h ah=25h, which doesn't
 *     require the surrounding code to be page-locked under DOSBox-X (whose
 *     DPMI host doesn't swap)
 *   - alloc_dos / free_dos / get_eflags helpers are preserved (real driver
 *     setup still needs DOS memory)
 *
 * For a real FD2 rebuild where ISRs run while the EXE is paged out, a
 * different solution is required (e.g. compute the lock range dynamically
 * from the AIL fn-entry table, or order .obj contiguously via wlink).
 */

int fd2_dpmi_lock_region(unsigned int start, unsigned int end) {
    (void)start; (void)end;
    return 1;  /* DPMI success */
}

int fd2_dpmi_lock_size(unsigned int start, unsigned int size) {
    (void)start; (void)size;
    return 1;
}

int fd2_dpmi_unlock_region(unsigned int start, unsigned int end) {
    (void)start; (void)end;
    return 1;
}

int fd2_dpmi_unlock_size(unsigned int start, unsigned int size) {
    (void)start; (void)size;
    return 1;
}
