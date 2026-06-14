#!/usr/bin/env python3
"""verify_real.py -- prove (or disprove) byte-equality between each
`real_in_src` data symbol's C initializer in src/ and the real bytes in
FD2.LE at that symbol's address.

A name-only reconcile (reconcile.py) only proves a same-named def exists; it
does NOT prove the values match. This script closes that gap: it parses the
actual C initializer and compares it byte-for-byte against the Ghidra dump.

Inputs:
  workspace/data_emit/real_in_src_ghidra_bytes.tsv  (name,addr,len,hex from Ghidra read_memory)
  workspace/data_emit/worklist.tsv                  (for src_home basename)
  src/**/*.c                                        (the C initializers)

Per-symbol verdict: PASS (len + every byte identical) / FAIL (with first diff).
All 28 must PASS for `real_in_src` to be a trustworthy status. UTF-8 reads.
"""
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GBYTES = os.path.join(ROOT, "workspace", "data_emit", "real_in_src_ghidra_bytes.tsv")
WORK   = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")

COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
LINECOM_RE = re.compile(r"//[^\n]*")

def load_ghidra():
    out = {}
    with io.open(GBYTES, encoding="utf-8") as f:
        f.readline()
        for line in f:
            name, addr, ln, hx = line.rstrip("\n").split("\t")
            b = bytes(int(hx[i:i+2], 16) for i in range(0, len(hx), 2))
            out[name] = (addr, int(ln), b)
    return out

def load_homes():
    out = {}
    with io.open(WORK, encoding="utf-8") as f:
        f.readline()
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 9 and p[7] == "real_in_src":
                out[p[2]] = p[8]
    return out

def find_src_file(basename):
    for dp, _, fns in os.walk(os.path.join(ROOT, "src")):
        if basename in fns:
            return os.path.join(dp, basename)
    return None

def parse_c_initializer(text, name):
    """Return list[int] of byte values from the file-scope def of `name`,
    or None if the def is not found. Assumes 1-byte (uint8/byte) elements."""
    # locate: optional const + type + name + optional [..] + '='
    m = re.search(r"(?m)^(?:const\s+)?[A-Za-z_][\w ]*\*?\s*" +
                  re.escape(name) + r"\s*(?:\[[^\]]*\])?\s*=\s*", text)
    if not m:
        return None
    # capture from '=' end up to the terminating ';' at top level
    rest = text[m.end():]
    depth = 0; end = None
    for i, ch in enumerate(rest):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        elif ch == ";" and depth == 0:
            end = i; break
    if end is None:
        return None
    body = rest[:end]
    body = COMMENT_RE.sub(" ", body)
    body = LINECOM_RE.sub(" ", body)
    body = body.strip().lstrip("{").rstrip("}")
    vals = []
    for tok in re.split(r"[,\s]+", body):
        tok = tok.strip()
        if not tok:
            continue
        if tok.startswith(("0x", "0X")):
            vals.append(int(tok, 16) & 0xFF)
        elif re.fullmatch(r"-?\d+", tok):
            vals.append(int(tok) & 0xFF)
        else:
            # unexpected token (char literal / macro) -> signal
            return ("UNPARSED", tok)
    return vals

def main():
    ghidra = load_ghidra()
    homes = load_homes()
    npass = nfail = 0
    fails = []
    for name in sorted(ghidra):
        addr, ln, gbytes = ghidra[name]
        home = homes.get(name)
        path = find_src_file(home) if home else None
        if not path:
            nfail += 1; fails.append((name, "src file not found: %s" % home)); continue
        with io.open(path, encoding="utf-8") as f:
            text = f.read()
        parsed = parse_c_initializer(text, name)
        if parsed is None:
            nfail += 1; fails.append((name, "C def not found in %s" % home)); continue
        if isinstance(parsed, tuple) and parsed[0] == "UNPARSED":
            nfail += 1; fails.append((name, "unparsed token %r" % parsed[1])); continue
        cbytes = bytes(parsed)
        if len(cbytes) != ln:
            nfail += 1
            fails.append((name, "length C=%d Ghidra=%d" % (len(cbytes), ln)))
            continue
        if cbytes != gbytes:
            # first differing index
            di = next(i for i in range(ln) if cbytes[i] != gbytes[i])
            nfail += 1
            fails.append((name, "byte[%d] C=0x%02x Ghidra=0x%02x" %
                          (di, cbytes[di], gbytes[di])))
            continue
        npass += 1
        print("  PASS  %-58s %3d bytes @%s" % (name, ln, addr))
    print("\n=== verify_real: %d PASS / %d FAIL (of %d real_in_src) ===" %
          (npass, nfail, len(ghidra)))
    for name, why in fails:
        print("  FAIL  %-58s %s" % (name, why))
    sys.exit(1 if nfail else 0)

if __name__ == "__main__":
    main()
