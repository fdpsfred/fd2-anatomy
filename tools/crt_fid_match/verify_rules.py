"""Behavior signatures for Watcom 9.5a CDECL CRT (CLIB3S.LIB) symbols.

Each rule encodes the minimum-set of static-analysis signals that a FD2.LE
function must exhibit to be confirmed as the corresponding Watcom CRT symbol.
The rule schema favors over-specification (multiple AND-ed conditions) so a
PASS is positive evidence of a real match, not just hash coincidence.

Schema (Rule):
    body_size_range:   (lo, hi)     - inclusive byte size bounds
    callees_required_any: [str,..]  - at least one callee name must be in this set
    callees_required_all: [str,..]  - all listed callees must appear
    callees_forbidden:    [str,..]  - none of these callees may appear
    instructions_any:     [str,..]  - at least one substring must appear in asm
    instructions_all:     [str,..]  - all substrings must appear in asm
    int21_ah_any:         [int,..]  - at least one DOS INT 21h AH value (e.g. 0x40)
    is_leaf:              bool|None - True = should have no callees; None = unspecified
    notes:                str       - one-line description of expected operation

apply_rule() returns a list of failure messages (empty = PASS). Caller supplies
asm text, callee names list, body_size, and optionally the decompiled C source
(currently unused, reserved for future rule extensions).

The matched_name -> rule mapping covers every symbol that appears in any of the
4 verify queues (sample / conflict / manual). When a symbol has no entry in
RULES, apply_rule() returns ["no_rule"] so callers can route to ad-hoc review.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import re


@dataclass
class Rule:
    notes: str = ""
    body_size_range: tuple[int, int] | None = None
    callees_required_any: list[str] = field(default_factory=list)
    callees_required_all: list[str] = field(default_factory=list)
    callees_forbidden: list[str] = field(default_factory=list)
    callers_required_any: list[str] = field(default_factory=list)
    callers_required_all: list[str] = field(default_factory=list)
    callers_forbidden: list[str] = field(default_factory=list)
    instructions_any: list[str] = field(default_factory=list)
    instructions_all: list[str] = field(default_factory=list)
    int21_ah_any: list[int] = field(default_factory=list)
    is_leaf: bool | None = None


# Note on body_size_range: Watcom CDECL stack-call prologue/epilogue alone is
# ~7 bytes (PUSH EBP / MOV EBP,ESP / ... / POP EBP / RET), so the lower bound
# is set generously. Upper bounds are based on typical Watcom 9.5a CLIB3S
# implementations; ±20% slack is permitted.
RULES: dict[str, Rule] = {
    # --- string / memory primitives ---
    "memset": Rule(
        notes="Fill n bytes at dest with byte value. In Watcom 9.5a CLIB3S, "
              "memset is a thin wrapper that broadcasts val to a 4-byte word "
              "and tail-calls the __STOSB helper.",
        body_size_range=(15, 60),
        callees_required_any=["__STOSB", "__STOSD"],
    ),
    "memcpy": Rule(
        notes="Copy n bytes src->dest. REP MOVSB/MOVSD; no overlap handling.",
        body_size_range=(30, 60),
        instructions_any=["MOVSB", "MOVSD"],
        is_leaf=True,
    ),
    "memmove": Rule(
        notes="Like memcpy but handles overlap (forward/backward direction).",
        body_size_range=(50, 120),
        instructions_any=["MOVSB", "MOVSD"],
    ),
    "strlen": Rule(
        notes="Scan for NUL byte; SCASB or manual loop.",
        body_size_range=(15, 50),
        instructions_any=["SCASB", "CMP", "TEST"],
        is_leaf=True,
    ),
    "strcpy": Rule(
        notes="Copy NUL-terminated string. MOV byte loop.",
        body_size_range=(30, 60),
        is_leaf=True,
    ),
    "strncpy": Rule(
        notes="Copy at most n bytes; pad with NUL.",
        body_size_range=(40, 80),
        is_leaf=True,
    ),
    "strncmp": Rule(
        notes="Compare at most n bytes.",
        body_size_range=(40, 80),
        is_leaf=True,
    ),
    "strnicmp": Rule(
        notes="Case-insensitive ncmp; calls tolower/toupper or inlines case fold.",
        body_size_range=(60, 120),
    ),

    # --- character classification ---
    "tolower": Rule(
        notes="ASCII fold A-Z to a-z.",
        body_size_range=(15, 35),
        is_leaf=True,
    ),
    "toupper": Rule(
        notes="ASCII fold a-z to A-Z.",
        body_size_range=(15, 35),
        is_leaf=True,
    ),

    # --- numeric ---
    "abs": Rule(
        notes="Return |x| for int. NEG if negative.",
        body_size_range=(8, 20),
        instructions_any=["NEG"],
        is_leaf=True,
    ),
    "labs": Rule(
        notes="Same as abs for long (32-bit on Watcom 386).",
        body_size_range=(8, 20),
        instructions_any=["NEG"],
        is_leaf=True,
    ),
    "rand": Rule(
        notes="LCG: seed = seed*0x41c64e6d + 0x3039; returns (seed>>16)&0x7FFF. "
              "FD2 calls a thunked seed-pointer getter (not strictly leaf).",
        body_size_range=(20, 60),
        instructions_any=["IMUL", "0x41c64e6d"],
    ),
    "srand": Rule(
        notes="Set global rand seed via seed-pointer getter.",
        body_size_range=(8, 30),
    ),
    "div": Rule(
        notes="Integer divmod returning div_t {quot, rem}.",
        body_size_range=(30, 80),
        instructions_any=["IDIV", "DIV"],
        is_leaf=True,
    ),
    "itoa": Rule(
        notes="Signed int to string in given base.",
        body_size_range=(35, 80),
    ),
    "utoa": Rule(
        notes="Unsigned int to string in given base.",
        body_size_range=(50, 100),
    ),
    "ltoa": Rule(
        notes="Signed long to string in given base.",
        body_size_range=(35, 80),
    ),
    "ultoa": Rule(
        notes="Unsigned long to string in given base.",
        body_size_range=(50, 100),
    ),

    # --- I/O port ---
    "outp": Rule(
        notes="Write byte to I/O port. OUT DX,AL.",
        body_size_range=(8, 25),
        instructions_any=["OUT"],
        is_leaf=True,
    ),

    # --- DOS console I/O (low level) ---
    "getch": Rule(
        notes="Read char from console. INT 21h AH=07h (no echo).",
        body_size_range=(15, 50),
        int21_ah_any=[0x07, 0x08],
    ),
    "getche": Rule(
        notes="Read char with echo. INT 21h AH=08h then echo.",
        body_size_range=(15, 50),
        int21_ah_any=[0x01, 0x08],
    ),

    # --- file I/O (low level) ---
    "open": Rule(
        notes="DOS-level open wrapper; calls sopen with default share mode.",
        body_size_range=(20, 60),
        callees_required_any=["sopen"],
    ),
    "sopen": Rule(
        notes="Share-mode open. INT 21h AH=3D/3C/6C; sets _iob entry.",
        body_size_range=(200, 600),
        int21_ah_any=[0x3C, 0x3D, 0x6C],
    ),
    "close": Rule(
        notes="Close fd. INT 21h AH=3Eh.",
        body_size_range=(30, 100),
        int21_ah_any=[0x3E],
    ),
    "read": Rule(
        notes="Buffered/text-translated read; calls __qread for raw read.",
        body_size_range=(150, 350),
        callees_required_any=["__qread"],
    ),
    "__qread": Rule(
        notes="Raw read: INT 21h AH=3Fh.",
        body_size_range=(30, 100),
        int21_ah_any=[0x3F],
    ),
    "write": Rule(
        notes="Buffered/text-translated write; calls __qwrite or __chktty.",
        body_size_range=(300, 600),
        callees_required_any=["__qwrite", "__chktty"],
    ),
    "__qwrite": Rule(
        notes="Raw write: INT 21h AH=40h.",
        body_size_range=(80, 200),
        int21_ah_any=[0x40],
    ),
    "lseek": Rule(
        notes="Seek fd. INT 21h AH=42h.",
        body_size_range=(50, 120),
        int21_ah_any=[0x42],
    ),
    "tell": Rule(
        notes="Tell fd position; calls lseek with whence=1, offset=0.",
        body_size_range=(15, 50),
        callees_required_any=["lseek"],
    ),
    "filelength": Rule(
        notes="Get file size; uses lseek end / current.",
        body_size_range=(40, 100),
    ),
    "unlink": Rule(
        notes="Delete file. INT 21h AH=41h.",
        body_size_range=(20, 80),
        int21_ah_any=[0x41],
    ),
    "remove": Rule(
        notes="Delete file; aliases unlink.",
        body_size_range=(8, 80),
        callees_required_any=["unlink"],
    ),
    "isatty": Rule(
        notes="Test if fd is a TTY; INT 21h AX=4400h IOCTL.",
        body_size_range=(20, 80),
        int21_ah_any=[0x44],
    ),

    # --- stream (FILE *) layer ---
    "fopen": Rule(
        notes="Calls _fsopen with default share mode.",
        body_size_range=(15, 40),
        callees_required_any=["_fsopen"],
    ),
    "_fsopen": Rule(
        notes="Share-mode fopen; calls __open_flags + sopen + __ioalloc.",
        body_size_range=(30, 100),
        callees_required_any=["__open_flags", "__ioalloc", "sopen"],
    ),
    "freopen": Rule(
        notes="Close fp, then reopen with new path/mode.",
        body_size_range=(30, 120),
    ),
    "fclose": Rule(
        notes="Close stream; calls __shutdown_stream then __doclose.",
        body_size_range=(30, 80),
        callees_required_any=["__shutdown_stream", "__doclose"],
    ),
    "__shutdown_stream": Rule(
        notes="Pre-close stream cleanup; calls __flush + __freefp.",
        body_size_range=(20, 80),
        callees_required_any=["__flush", "__freefp", "close"],
    ),
    "__doclose": Rule(
        notes="Final close: clear _iob flags, call close(fd).",
        body_size_range=(80, 250),
        callees_required_any=["close", "__freefp", "__flush"],
    ),
    "fcloseall": Rule(
        notes="Loop over _iob[] and fclose each open one.",
        body_size_range=(8, 80),
    ),
    "fread": Rule(
        notes="Read N elements; size*count, calls read/__filbuf in loop.",
        body_size_range=(200, 700),
        callees_required_any=["read", "__filbuf", "__qread"],
    ),
    "fwrite": Rule(
        notes="Write N elements; size*count, calls __flush or write.",
        body_size_range=(200, 600),
        callees_required_any=["write", "__flush", "__qwrite"],
    ),
    "fseek": Rule(
        notes="Seek FILE*; calls lseek + buffer flush.",
        body_size_range=(200, 600),
        callees_required_any=["lseek", "__flush"],
    ),
    "ftell": Rule(
        notes="Tell FILE*; calls lseek with current.",
        body_size_range=(30, 80),
        callees_required_any=["lseek"],
    ),
    "fgetc": Rule(
        notes="One char from stream; tests _cnt then __filbuf.",
        body_size_range=(80, 200),
        callees_required_any=["__filbuf", "__fill_buffer"],
    ),
    "fgetchar": Rule(
        notes="Thin wrapper: fgetc(stdin).",
        body_size_range=(8, 25),
        callees_required_any=["fgetc"],
    ),
    "fputc": Rule(
        notes="One char to stream; tests _cnt then __flush.",
        body_size_range=(100, 250),
        callees_required_any=["__flush", "write"],
    ),
    "fgets": Rule(
        notes="Read up to n-1 chars or until newline.",
        body_size_range=(60, 150),
        callees_required_any=["fgetc", "__filbuf"],
    ),
    "fputs": Rule(
        notes="Write NUL-terminated string to stream.",
        body_size_range=(60, 200),
        callees_required_any=["fputc", "write", "__flush"],
    ),
    "fprintf": Rule(
        notes="Format to stream; calls __fprtf or __prtf.",
        body_size_range=(20, 60),
        callees_required_any=["__fprtf", "__prtf"],
    ),
    "printf": Rule(
        notes="Format to stdout (_iob[1]); calls __fprtf or __prtf via wrapper.",
        body_size_range=(20, 60),
        callees_required_any=["__fprtf", "__prtf", "vfprintf"],
    ),
    "sprintf": Rule(
        notes="Format to buffer; calls __prtf with string output sink.",
        body_size_range=(30, 80),
        callees_required_any=["__prtf"],
    ),
    "vfprintf": Rule(
        notes="Va-list variant; calls __fprtf or __prtf.",
        body_size_range=(20, 200),
        callees_required_any=["__fprtf", "__prtf"],
    ),
    "__fprtf": Rule(
        notes="File-output prtf core; routes to __prtf with FILE* sink.",
        body_size_range=(80, 200),
        callees_required_any=["__prtf"],
    ),
    "__prtf": Rule(
        notes="Format engine: % spec parser + dispatch table.",
        body_size_range=(400, 800),
    ),
    "__filbuf": Rule(
        notes="Refill stream buffer; calls read/__qread.",
        body_size_range=(30, 250),
        callees_required_any=["read", "__qread", "__fill_buffer"],
    ),
    "__fill_buffer": Rule(
        notes="Internal buffer fill helper; calls read.",
        body_size_range=(80, 250),
        callees_required_any=["read", "__qread"],
    ),
    "__flush": Rule(
        notes="Flush stream buffer; calls write to drain _ptr/_cnt.",
        body_size_range=(80, 250),
        callees_required_any=["write", "__qwrite"],
    ),
    "__flushall": Rule(
        notes="Flush all open streams; loop over _iob[].",
        body_size_range=(30, 100),
        callees_required_any=["__flush"],
    ),
    "__ioalloc": Rule(
        notes="Allocate FILE struct + buffer; calls malloc/_nmalloc.",
        body_size_range=(80, 200),
        callees_required_any=["malloc", "_nmalloc"],
    ),
    "__allocfp": Rule(
        notes="Find free _iob slot; allocate buffer.",
        body_size_range=(80, 200),
        callees_required_any=["__ioalloc", "malloc"],
    ),
    "__freefp": Rule(
        notes="Free FILE struct buffer; calls free.",
        body_size_range=(30, 80),
        callees_required_any=["free", "_nfree"],
    ),
    "__purgefp": Rule(
        notes="Mark FILE slot as unused; small.",
        body_size_range=(15, 60),
    ),
    "__open_flags": Rule(
        notes="Parse fopen mode string into _iob flag bits.",
        body_size_range=(120, 300),
    ),
    "__chktty": Rule(
        notes="Test if fd is TTY; cache flag in _iob.",
        body_size_range=(30, 100),
    ),
    "__set_binary": Rule(
        notes="Switch fd to binary mode; INT 21h AX=4400h IOCTL.",
        body_size_range=(80, 200),
        int21_ah_any=[0x44],
    ),
    "__MkTmpFile": Rule(
        notes="Build temp filename from PID + counter.",
        body_size_range=(80, 200),
    ),
    "__IOMode": Rule(
        notes="Get/access fd's mode flags from _iob[].",
        body_size_range=(50, 130),
    ),
    "__SetIOMode": Rule(
        notes="Set fd's mode flags in _iob[].",
        body_size_range=(15, 60),
    ),
    "setbuf": Rule(
        notes="Pass through to setvbuf with default buffer size.",
        body_size_range=(20, 80),
        callees_required_any=["setvbuf"],
    ),
    "setvbuf": Rule(
        notes="Configure FILE* buffer mode and size.",
        body_size_range=(80, 200),
    ),

    # --- memory / heap ---
    "malloc": Rule(
        notes="Calls _nmalloc with default heap.",
        body_size_range=(8, 30),
        callees_required_any=["_nmalloc"],
    ),
    "_nmalloc": Rule(
        notes="Heap allocator; manages free list, may grow heap.",
        body_size_range=(80, 200),
        callees_required_any=["__MemAllocator", "__ExpandDGROUP"],
    ),
    "free": Rule(
        notes="Calls _nfree on default heap.",
        body_size_range=(8, 30),
        callees_required_any=["_nfree"],
    ),
    "_nfree": Rule(
        notes="Heap deallocator; coalesces with neighbors.",
        body_size_range=(20, 80),
        callees_required_any=["__MemFree"],
    ),
    "__MemAllocator": Rule(
        notes="Heap segment allocator; finds free block.",
        body_size_range=(120, 300),
    ),
    "__MemFree": Rule(
        notes="Heap segment free; merges adjacent free blocks.",
        body_size_range=(200, 400),
    ),
    "__LastFree": Rule(
        notes="Track last freed segment for fast realloc.",
        body_size_range=(20, 80),
    ),
    "__ExpandDGROUP": Rule(
        notes="Grow DGROUP heap segment; calls sbrk/__brk.",
        body_size_range=(400, 800),
        callees_required_any=["sbrk", "__brk"],
    ),
    "sbrk": Rule(
        notes="Adjust break; DPMI/DOS allocate-extend wrapper.",
        body_size_range=(80, 250),
    ),
    "__brk": Rule(
        notes="Set break to specific address; DPMI realloc.",
        body_size_range=(80, 250),
    ),
    "__nmemneed": Rule(
        notes="Default OOM callback; weak stub returning 0. CLIB3S has multiple "
              "byte-identical 7-byte XOR-RET stubs (signal default, etc.) that "
              "hash to the same family; require a heap-pathway caller to "
              "discriminate.",
        body_size_range=(3, 15),
        is_leaf=True,
        callers_required_any=["_nmalloc", "__MemAllocator"],
    ),
    "_heapenable": Rule(
        notes="Enable/disable heap; toggles flag.",
        body_size_range=(8, 40),
        is_leaf=True,
    ),

    # --- DOS / DPMI helpers ---
    "int386": Rule(
        notes="DPMI-style INT n with regs; calls _DoINTR_.",
        body_size_range=(30, 80),
        callees_required_any=["_DoINTR_"],
    ),
    "int386x": Rule(
        notes="Like int386 but with sregs.",
        body_size_range=(20, 80),
        callees_required_any=["_DoINTR_", "__int386x_"],
    ),
    "segread": Rule(
        notes="Save current segment regs into struct.",
        body_size_range=(30, 80),
    ),
    "_dos_setvect": Rule(
        notes="Set DOS interrupt vector. INT 21h AH=25h.",
        body_size_range=(30, 80),
        int21_ah_any=[0x25],
    ),
    "_DoINTR_": Rule(
        notes="DPMI INT XX dispatcher; uses INT 31h or raw.",
        body_size_range=(30, 120),
    ),
    "__int386x_": Rule(
        notes="int386x assembly trampoline.",
        body_size_range=(30, 80),
    ),
    "_dosret0": Rule(
        notes="DOS-call return helper: success path zeros eax.",
        body_size_range=(15, 35),
    ),
    "_dosretax": Rule(
        notes="DOS-call return helper: error path -> _set_errno.",
        body_size_range=(15, 35),
        callees_required_any=["_set_errno"],
    ),
    "__EINVAL": Rule(
        notes="Set errno=EINVAL, return -1.",
        body_size_range=(8, 30),
    ),
    "_set_errno": Rule(
        notes="Map DOS error to errno; lookup table.",
        body_size_range=(60, 200),
    ),

    # --- time ---
    "time": Rule(
        notes="Get unix time_t. Calls __getctime; INT 21h AH=2A/2C.",
        body_size_range=(30, 100),
        callees_required_any=["__getctime", "mktime"],
    ),
    "__getctime": Rule(
        notes="Read DOS date+time. INT 21h AH=2Ah and AH=2Ch.",
        body_size_range=(80, 250),
        int21_ah_any=[0x2A, 0x2C],
    ),
    "localtime": Rule(
        notes="Thin wrapper: _localtime(time, &static_buf).",
        body_size_range=(15, 35),
        callees_required_any=["_localtime"],
    ),
    "_localtime": Rule(
        notes="Convert time_t to local tm; calls __brktime + tzset.",
        body_size_range=(60, 150),
        callees_required_any=["__brktime", "tzset"],
    ),
    "gmtime": Rule(
        notes="Thin wrapper: _gmtime(time, &static_buf).",
        body_size_range=(15, 35),
        callees_required_any=["_gmtime"],
    ),
    "_gmtime": Rule(
        notes="Convert time_t to UTC tm; calls __brktime.",
        body_size_range=(20, 80),
        callees_required_any=["__brktime"],
    ),
    "asctime": Rule(
        notes="Thin wrapper: _asctime(tm, &static_buf).",
        body_size_range=(15, 35),
        callees_required_any=["_asctime"],
    ),
    "_asctime": Rule(
        notes="Format tm to string; sprintf.",
        body_size_range=(150, 350),
    ),
    "ctime": Rule(
        notes="Thin wrapper: localtime then asctime.",
        body_size_range=(15, 50),
        callees_required_any=["_ctime", "localtime"],
    ),
    "_ctime": Rule(
        notes="Internal: localtime + _asctime composition.",
        body_size_range=(30, 80),
        callees_required_any=["_localtime", "_asctime"],
    ),
    "mktime": Rule(
        notes="Convert tm to time_t; uses __leapyear, __isindst, tzset.",
        body_size_range=(200, 400),
        callees_required_any=["__leapyear", "__isindst", "tzset"],
    ),
    "__brktime": Rule(
        notes="Decompose seconds-since-epoch into tm fields.",
        body_size_range=(200, 500),
        callees_required_any=["__leapyear", "tzset"],
    ),
    "__leapyear": Rule(
        notes="Test if year is leap year. y%4==0 && (y%100!=0 || y%400==0).",
        body_size_range=(20, 100),
        is_leaf=True,
    ),
    "__isindst": Rule(
        notes="Test if time falls inside DST window.",
        body_size_range=(300, 800),
    ),
    "tzset": Rule(
        notes="Initialize timezone globals from TZ env.",
        body_size_range=(20, 60),
        callees_required_any=["__parse_tz", "getenv"],
    ),
    "__parse_tz": Rule(
        notes="Parse TZ string into timezone/daylight globals.",
        body_size_range=(120, 300),
    ),
    "getenv": Rule(
        notes="Look up name in __environ vector.",
        body_size_range=(60, 150),
    ),

    # --- runtime startup / shutdown ---
    "__CMain": Rule(
        notes="Watcom CRT entry: setup stack frame, init streams, push (argv, "
              "argc), call user main, push retval, JMP exit. Watcom 9.5a "
              "splits the __CMain logic across multiple .obj — the init-phase "
              "(__InitRtns/__Init_Argv/__init_8087) is in the bootstrap caller, "
              "this entry is the user-main invoker post-init segment.",
        body_size_range=(40, 200),
        callers_required_any=["crt_dos_main_bootstrap"],
    ),
    "__InitRtns": Rule(
        notes="Walk init list; call each ctor.",
        body_size_range=(30, 120),
    ),
    "__FiniRtns": Rule(
        notes="Walk fini list; call each dtor.",
        body_size_range=(30, 120),
    ),
    "__Init_Argv": Rule(
        notes="Parse PSP/cmdline into argv[].",
        body_size_range=(100, 350),
    ),
    "__InitFiles": Rule(
        notes="Init _iob[] for stdin/stdout/stderr.",
        body_size_range=(30, 150),
    ),
    "__init_8087": Rule(
        notes="Initialize x87 FPU; FNINIT or hook.",
        body_size_range=(20, 80),
        callees_required_any=["__chk8087", "__init_80x87"],
    ),
    "__chk8087": Rule(
        notes="Detect 8087/287/387 FPU.",
        body_size_range=(20, 100),
    ),
    "__init_80x87": Rule(
        notes="Set FPU control word.",
        body_size_range=(20, 80),
    ),
    "__hook387": Rule(
        notes="Install x87 emulator/notifier hook.",
        body_size_range=(80, 300),
    ),
    "__unhook387": Rule(
        notes="Remove x87 hook.",
        body_size_range=(30, 120),
    ),
    "__int7": Rule(
        notes="x87 emulator INT 7 dispatcher (FPU-not-present trap).",
        body_size_range=(15, 60),
    ),
    "__setenvp": Rule(
        notes="Build envp[] from PSP environment block.",
        body_size_range=(120, 300),
    ),
    "__full_io_exit": Rule(
        notes="Flush + close all open _iob entries on exit.",
        body_size_range=(8, 50),
    ),
    "__CHK": Rule(
        notes="Stack overflow check at function entry; XCHGs caller's saved EAX, "
              "tail-calls __STK to do the actual probe.",
        body_size_range=(8, 30),
        callees_required_any=["__STK"],
    ),
    "__STK": Rule(
        notes="Stack probe / allocator; touches each 4K page.",
        body_size_range=(15, 60),
        is_leaf=True,
    ),
    "__delay": Rule(
        notes="Busy-wait loop calibrated to ms.",
        body_size_range=(30, 120),
    ),

    # --- misc helpers ---
    "__STOSB": Rule(
        notes="Helper: REP STOSB block.",
        body_size_range=(30, 80),
        instructions_any=["STOSB"],
    ),
    "__STOSD": Rule(
        notes="Helper: REP STOSD block.",
        body_size_range=(60, 150),
        instructions_any=["STOSD"],
    ),
    "_EFG_Format": Rule(
        notes="Float printf %e/%f/%g formatter.",
        body_size_range=(150, 350),
    ),
    "__ZBuf2F": Rule(
        notes="BCD-buffer to float conversion helper.",
        body_size_range=(40, 120),
    ),
}


# Patterns to detect INT 21h with specific AH values in disassembly text.
# Looks for common patterns like "MOV AH,0x40" or "MOV AH,40h" near "INT 21".
_AH_PATTERNS = [
    re.compile(r"\bMOV\s+AH\s*,\s*(?:0x([0-9A-Fa-f]+)|([0-9A-Fa-f]+)h?)\b", re.IGNORECASE),
    re.compile(r"\bMOV\s+EAX\s*,\s*(?:0x([0-9A-Fa-f]+)|([0-9A-Fa-f]+))\b", re.IGNORECASE),
]
_INT21_PATTERN = re.compile(r"\bINT\s+(?:0x21|21h)\b", re.IGNORECASE)


def detect_int21_ah_values(asm_text: str) -> set[int]:
    """Heuristic: look for AH=XX preceding INT 21h, return set of AH values seen."""
    if not _INT21_PATTERN.search(asm_text):
        return set()
    found: set[int] = set()
    for pat in _AH_PATTERNS:
        for m in pat.finditer(asm_text):
            hex_str = m.group(1) or m.group(2) or ""
            try:
                val = int(hex_str, 16)
                # Heuristic: the upper byte of EAX or AH alone — keep low byte
                found.add(val & 0xFF)
                # Also try as plain decimal in case Ghidra renders that way
            except ValueError:
                continue
    return found


def apply_rule(matched_name: str, asm_text: str, callee_names: list[str],
               body_size: int, decomp: str = "",
               caller_names: list[str] | None = None) -> list[str]:
    """Apply RULES[matched_name] to the gathered evidence.

    Returns list of failure messages (empty list = PASS). Returns ["no_rule"] if
    no rule is defined for matched_name (caller should fall back to ad-hoc).
    """
    rule = RULES.get(matched_name)
    if rule is None:
        return ["no_rule"]

    failures: list[str] = []

    if rule.body_size_range:
        lo, hi = rule.body_size_range
        if not (lo <= body_size <= hi):
            failures.append(f"body_size {body_size} outside [{lo}, {hi}]")

    asm_upper = asm_text.upper()
    if rule.instructions_any:
        if not any(p.upper() in asm_upper for p in rule.instructions_any):
            failures.append(f"none of {rule.instructions_any} in asm")
    if rule.instructions_all:
        missing = [p for p in rule.instructions_all if p.upper() not in asm_upper]
        if missing:
            failures.append(f"required instructions missing: {missing}")

    callee_set = set(callee_names)
    if rule.callees_required_any:
        if not any(c in callee_set for c in rule.callees_required_any):
            failures.append(
                f"none of expected callees {rule.callees_required_any} found "
                f"(observed: {sorted(callee_set)[:8]})"
            )
    if rule.callees_required_all:
        missing = [c for c in rule.callees_required_all if c not in callee_set]
        if missing:
            failures.append(f"required callees missing: {missing}")
    if rule.callees_forbidden:
        bad = [c for c in rule.callees_forbidden if c in callee_set]
        if bad:
            failures.append(f"forbidden callees present: {bad}")

    if rule.is_leaf and callee_names:
        failures.append(f"expected leaf, but found callees: {callee_names[:5]}")

    if rule.int21_ah_any:
        seen = detect_int21_ah_values(asm_text)
        if not any(ah in seen for ah in rule.int21_ah_any):
            failures.append(
                f"none of INT 21h AH={rule.int21_ah_any} detected "
                f"(seen AH values: {sorted(seen)})"
            )

    # Caller checks (skip if caller_names not supplied)
    if caller_names is not None:
        caller_set = set(caller_names)
        if rule.callers_required_any:
            if not any(c in caller_set for c in rule.callers_required_any):
                failures.append(
                    f"none of expected callers {rule.callers_required_any} "
                    f"found (observed: {sorted(caller_set)[:8]})"
                )
        if rule.callers_required_all:
            missing = [c for c in rule.callers_required_all if c not in caller_set]
            if missing:
                failures.append(f"required callers missing: {missing}")
        if rule.callers_forbidden:
            bad = [c for c in rule.callers_forbidden if c in caller_set]
            if bad:
                failures.append(f"forbidden callers present: {bad}")

    return failures
