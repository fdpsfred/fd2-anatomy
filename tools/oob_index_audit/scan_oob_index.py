#!/usr/bin/env python3
"""
scan_oob_index.py -- find array subscripts in src/ that can read past the
declared bound of a data_fd2_* array.

Motivation
----------
In the original image the compiler often folds a constant index adjustment
into the address displacement, e.g.

    price = level * cost_table[job_id - 1]        (cost_table @ 0x5266B)
  ->  MOVSX ESI, word ptr [job_id*2 + 0x52669]     (0x52669 == cost_table - 2)

Ghidra attributes the folded base 0x52669 to whatever symbol contains it --
here the *preceding* int16[6] table @ 0x5265F -- and renders the access as
`prev_table[job_id + 5]`. Transcribing that literally into C is only correct
while the two symbols stay adjacent; on a rebuild the linker separates them
and the read silently returns unrelated data.

Folded-base arithmetic: if the real array starts where the mis-attributed
symbol ends, a source index of `i - k` folds to a constant term of
(sym_count - k). So the signature is a positive constant term sitting at or
just below the declared element count, added to an otherwise unbounded index.

Buckets emitted:
  OOB_CONST      constant index >= declared element count
  OOB_OFFSET     `[x + N]` with N >= count-1     (folded-base signature, k<=1)
  NEAR_END       `[x + N]` with count-1 > N >= count-NEAR_SLACK (k = 2..slack)
  UNKNOWN_SIZE   subscript on a data_fd2_* array whose size we could not parse
  POINTER        subscript on a data_fd2_* pointer (no static bound; listed
                 for completeness, not a finding)
  UNPARSED       a `data_fd2_x[` whose bracket never closes -- must be
                 eyeballed so nothing is dropped silently

Every hit needs manual adjudication against the disassembly: a positive
constant term is legitimate when the index variable's own range keeps the
access inside the array.

Usage:  python tools/oob_index_audit/scan_oob_index.py [--src SRC] [--out OUT]
        python tools/oob_index_audit/scan_oob_index.py --selftest
"""

import argparse
import json
import os
import re
import sys

NEAR_SLACK = 4  # how far below `count` a constant term still looks folded

IDENT = r"[A-Za-z_]\w*"
QUALS = r"(?:(?:extern|static|const|volatile|register)\s+)*"
# multi-word types: `unsigned char`, `long long`, `struct foo`, `uint8`
TYPE = r"(?:" + IDENT + r"\s+)*" + IDENT

DECL_RE = re.compile(
    r"^[ \t]*" + QUALS + r"(?P<type>" + TYPE + r")[ \t]+(?P<ptr>\*[ \t]*)?"
    r"(?P<name>data_fd2_\w+)[ \t]*(?P<dims>(?:\[[^\];]*\])+)[ \t]*(?==|;)",
    re.MULTILINE,
)
PTR_DECL_RE = re.compile(
    r"^[ \t]*" + QUALS + r"(?P<type>" + TYPE + r")[ \t]+\*[ \t]*"
    r"(?P<name>data_fd2_\w+)[ \t]*(?==|;)",
    re.MULTILINE,
)
# function-pointer arrays: `void (*const data_fd2_x[30])(void)`
FNPTR_DECL_RE = re.compile(
    r"^[ \t]*" + QUALS + r"(?P<type>" + TYPE + r")[ \t]*\([ \t]*\*[ \t]*"
    r"(?:const[ \t]+)?(?P<name>data_fd2_\w+)[ \t]*"
    r"(?P<dims>(?:\[[^\];]*\])+)[ \t]*\)[ \t]*\(",
    re.MULTILINE,
)

DIM_RE = re.compile(r"\[([^\[\]]*)\]")
# whitespace (incl. newlines) may separate the name from its `[`
NAME_LBRACK_RE = re.compile(r"\b(?P<name>data_fd2_\w+)\s*\[")
INT_RE = re.compile(r"^\s*(?:0[xX][0-9a-fA-F]+|\d+)\s*$")
CONST_TAIL_RE = re.compile(r"^(?:0[xX][0-9a-fA-F]+|\d+)$")
OPS = set("+-*/%<>&|^!~=?:,")


def parse_int(tok):
    try:
        return int(tok.strip(), 0)
    except (ValueError, AttributeError):
        return None


def strip_comments(text):
    """Blank out /* */, //, string and char literals, preserving offsets."""
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
        elif text[i] in "\"'":
            q, j = text[i], i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
        else:
            out.append(text[i])
            i += 1
            continue
        out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
        i = j
    return "".join(out)


def iter_subscripts(code):
    """Yield (pos, name, index_text|None). index_text is None when the
    bracket never closes. Handles nested [] and newlines inside the index."""
    for m in NAME_LBRACK_RE.finditer(code):
        start = m.end()
        depth, i, n = 1, m.end(), len(code)
        while i < n and depth:
            c = code[i]
            if c == "[":
                depth += 1
            elif c == "]":
                depth -= 1
            i += 1
        if depth:
            yield m.start(), m.group("name"), None
        else:
            yield m.start(), m.group("name"), " ".join(code[start:i - 1].split())


def split_trailing_const(idx):
    """Split `expr + N` / `expr - N` at the *top level* (outside any bracket
    or paren). Returns (op, N) or (None, None)."""
    depth = 0
    for i in range(len(idx) - 1, -1, -1):
        c = idx[i]
        if c in "])":
            depth += 1
        elif c in "[(":
            depth -= 1
        elif depth == 0 and c in "+-":
            # must be binary, not a unary sign: something non-operator before it
            before = idx[:i].rstrip()
            if not before or before[-1] in OPS:
                continue
            tail = idx[i + 1:].strip()
            if CONST_TAIL_RE.match(tail):
                return c, parse_int(tail)
            return None, None
    return None, None


def collect_sources(src_root):
    files = []
    for dirpath, _dirs, filenames in os.walk(src_root):
        for fn in filenames:
            if fn.endswith((".c", ".h")):
                files.append(os.path.join(dirpath, fn))
    return sorted(files)


def collect_decls(files, src_root="."):
    """name -> {'count': int|None, 'is_ptr': bool, 'where': str, 'dims': str}"""
    decls = {}

    def record(name, entry):
        prev = decls.get(name)
        if prev is None or (prev["count"] is None and entry["count"] is not None):
            decls[name] = entry

    for path in files:
        code = strip_comments(open(path, encoding="utf-8", errors="replace").read())
        rel = os.path.relpath(path, src_root)
        for m in DECL_RE.finditer(code):
            dims = DIM_RE.findall(m.group("dims"))
            count = parse_int(dims[0]) if dims and dims[0].strip() else None
            record(m.group("name"), dict(
                count=count, is_ptr=bool(m.group("ptr")),
                where="%s:%d" % (rel, code[: m.start()].count("\n") + 1),
                dims=m.group("dims").strip()))
        for m in FNPTR_DECL_RE.finditer(code):
            dims = DIM_RE.findall(m.group("dims"))
            count = parse_int(dims[0]) if dims and dims[0].strip() else None
            record(m.group("name"), dict(
                count=count, is_ptr=False,
                where="%s:%d" % (rel, code[: m.start()].count("\n") + 1),
                dims=m.group("dims").strip()))
        for m in PTR_DECL_RE.finditer(code):
            record(m.group("name"), dict(
                count=None, is_ptr=True,
                where="%s:%d" % (rel, code[: m.start()].count("\n") + 1),
                dims="*"))
    return decls


def decl_spans(code):
    """Ranges covered by declarations -- their `[N]` is a size, not an access."""
    return ([(m.start(), m.end()) for m in DECL_RE.finditer(code)] +
            [(m.start(), m.end()) for m in FNPTR_DECL_RE.finditer(code)])


def in_span(pos, spans):
    return any(lo <= pos < hi for lo, hi in spans)


def classify(idx, count, is_ptr):
    """-> (kind, offset) or (None, None) when the access is in bounds."""
    if is_ptr:
        return "POINTER", None
    if count is None:
        return "UNKNOWN_SIZE", None

    if INT_RE.match(idx):
        const = parse_int(idx)
        return ("OOB_CONST", const) if const >= count else (None, None)

    op, off = split_trailing_const(idx)
    if op != "+" or off is None:
        return None, None
    if off >= count - 1:
        return "OOB_OFFSET", off
    if off >= count - NEAR_SLACK:
        return "NEAR_END", off
    return None, None


def scan(files, decls, src_root="."):
    findings = []
    for path in files:
        raw = open(path, encoding="utf-8", errors="replace").read()
        code = strip_comments(raw)
        lines = raw.splitlines()
        rel = os.path.relpath(path, src_root)
        spans = decl_spans(code)

        for pos, name, idx in iter_subscripts(code):
            if in_span(pos, spans):
                continue
            line_no = code[:pos].count("\n") + 1
            src_line = lines[line_no - 1].strip() if line_no <= len(lines) else ""
            decl = decls.get(name)

            if idx is None:
                findings.append(dict(kind="UNPARSED", name=name, index=None,
                                     count=None, offset=None,
                                     where="%s:%d" % (rel, line_no),
                                     decl=decl["where"] if decl else None,
                                     line=src_line))
                continue
            if not idx:
                continue  # `name[]`

            kind, off = classify(idx, decl["count"] if decl else None,
                                 decl["is_ptr"] if decl else False)
            if kind is None:
                continue
            findings.append(dict(
                kind=kind, name=name, index=idx,
                count=decl["count"] if decl else None, offset=off,
                where="%s:%d" % (rel, line_no),
                decl=decl["where"] if decl else None, line=src_line))
    return findings


KIND_ORDER = ["OOB_CONST", "OOB_OFFSET", "NEAR_END", "UNPARSED",
              "UNKNOWN_SIZE", "POINTER"]


def report(files, decls, findings, out_dir, quiet_kinds=("POINTER",)):
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "findings.json"), "w", encoding="utf-8") as fh:
        json.dump({"files": len(files), "decls": len(decls),
                   "findings": findings}, fh, indent=2, ensure_ascii=False)

    print("scanned %d files, parsed %d data_fd2_* declarations" %
          (len(files), len(decls)))
    for kind in KIND_ORDER:
        hits = [f for f in findings if f["kind"] == kind]
        if kind in quiet_kinds:
            print("\n=== %s: %d (suppressed) ===" % (kind, len(hits)))
            continue
        print("\n=== %s: %d ===" % (kind, len(hits)))
        for f in sorted(hits, key=lambda x: x["where"]):
            bound = f["count"] if f["count"] is not None else "?"
            idx = f["index"] if f["index"] is not None else "<unbalanced>"
            print("  %s  %s[%s]  declared [%s]%s" %
                  (f["where"], f["name"], idx, bound,
                   "  @ " + f["decl"] if f["decl"] else ""))
            print("      %s" % f["line"])


def selftest():
    ok = True

    def check(label, got, want):
        nonlocal ok
        good = got == want
        ok = ok and good
        print("  [%s] %s: got %r want %r" % ("PASS" if good else "FAIL",
                                             label, got, want))

    here = os.path.dirname(os.path.abspath(__file__))

    def run_on(src_text, fname):
        path = os.path.join(here, fname)
        open(path, "w", encoding="utf-8").write(src_text)
        try:
            d = collect_decls([path], here)
            return d, scan([path], d, here)
        finally:
            os.remove(path)

    # 1. declaration parsing across qualifier / type shapes
    d, _ = run_on(
        "const int16 data_fd2_a[6] = { 1 };\n"
        "static uint8 data_fd2_b[4][8];\n"
        "extern const int16 data_fd2_c[30];\n"
        "uint8 *data_fd2_p;\n"
        "runtime_char data_fd2_structs[8];\n"
        "static const unsigned char data_fd2_multi[3] = { 1, 2, 3 };\n"
        "uint8 data_fd2_unsized[] = { 1, 2 };\n", "_st1.c")
    check("decl a count", d["data_fd2_a"]["count"], 6)
    check("decl b count", d["data_fd2_b"]["count"], 4)
    check("decl c count", d["data_fd2_c"]["count"], 30)
    check("decl p is_ptr", d["data_fd2_p"]["is_ptr"], True)
    check("decl struct count", d["data_fd2_structs"]["count"], 8)
    check("decl 'unsigned char' count", d["data_fd2_multi"]["count"], 3)
    check("decl unsized count", d["data_fd2_unsized"]["count"], None)

    # 1b. function-pointer arrays must yield a bound, not UNKNOWN_SIZE
    d, found = run_on(
        "extern void (*const data_fd2_fn[30])(void);\n"
        "int (*data_fd2_fn2[10])(\n    uint32 a, uint32 b);\n"
        "void g(int i) { data_fd2_fn[i](); data_fd2_fn2[31](1, 2); }\n", "_st1b.c")
    check("fnptr count", d["data_fd2_fn"]["count"], 30)
    check("fnptr multiline count", d["data_fd2_fn2"]["count"], 10)
    check("fnptr call in bounds silent",
          [f["kind"] for f in found if f["name"] == "data_fd2_fn"], [])
    check("fnptr call oob flagged",
          [f["kind"] for f in found if f["name"] == "data_fd2_fn2"], ["OOB_CONST"])

    # 2. classification
    check("in bounds -1", classify("job_id - 1", 30, False)[0], None)
    check("in bounds +1", classify("i + 1", 30, False)[0], None)
    check("const oob", classify("7", 6, False)[0], "OOB_CONST")
    check("const ok", classify("5", 6, False)[0], None)
    check("near end", classify("i + 28", 30, False)[0], "NEAR_END")
    check("ptr", classify("i", None, True)[0], "POINTER")
    check("unknown", classify("i", None, False)[0], "UNKNOWN_SIZE")

    # 3. top-level split must ignore +N nested inside brackets/parens
    check("nested + ignored", split_trailing_const("a[i + 5]"), (None, None))
    check("paren + ignored", split_trailing_const("f(i + 5)"), (None, None))
    check("top-level +", split_trailing_const("rc[x].job_id + 5"), ("+", 5))
    check("unary + skipped", split_trailing_const("i * +5"), (None, None))

    # 4. END-TO-END on the real church-revive bug shape: the subscript sits on
    #    the line AFTER the symbol and the index itself contains brackets.
    _, found = run_on(
        "const int16 data_fd2_tbl[6] = { 1 };\n"
        "uint32 data_fd2_bss[3];\n"
        "void f(int i) {\n"
        "    v = (uint32)rc[i].level *\n"
        "        (int32)data_fd2_tbl\n"
        "            [rc[i].job_id + 5];\n"
        "    h(data_fd2_bss[7]);\n"
        "}\n", "_st2.c")
    kinds = sorted((f["kind"], f["name"]) for f in found)
    check("multiline+nested access flagged", kinds,
          [("OOB_CONST", "data_fd2_bss"), ("OOB_OFFSET", "data_fd2_tbl")])

    # 5. declarations must not flag themselves
    _, found = run_on("uint32 data_fd2_z[3];\nconst int16 data_fd2_w[6] = {0};\n",
                      "_st3.c")
    check("decls silent", found, [])

    # 6. comment / string stripping
    text = '/* data_fd2_x[99] */ a; "data_fd2_y[99]"; b;\n'
    code = strip_comments(text)
    check("comment stripped", "data_fd2_x" in code, False)
    check("string stripped", "data_fd2_y" in code, False)
    check("offsets preserved", len(code), len(text))

    print("\nselftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="src")
    ap.add_argument("--out", default=os.path.join("workspace", "oob_index_audit"))
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--show-pointers", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    files = collect_sources(args.src)
    decls = collect_decls(files, args.src)
    findings = scan(files, decls, args.src)
    report(files, decls, findings, args.out,
           quiet_kinds=() if args.show_pointers else ("POINTER",))
    return 0


if __name__ == "__main__":
    sys.exit(main())
