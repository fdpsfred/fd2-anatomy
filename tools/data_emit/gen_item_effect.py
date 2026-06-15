#!/usr/bin/env python3
"""gen_item_effect.py -- generate the byte-exact C initializer for
data_fd2_battle_item_effect_table (item_effect[215]) from the real Ghidra
bytes, AND verify it round-trips byte-for-byte.

The table is a 23-byte-stride struct array (struct item_effect in types.h). The
generic flat verify_real.py packer assumes a single element width, which does
not fit a mixed-width struct (uint8 + uint16 fields). So this script does the
struct-aware job: it lays each 23-byte entry out across the struct's fields at
their true little-endian widths, emits one braced initializer per entry, then
re-packs the emitted integers back to bytes and asserts equality with the
source hex.

Field layout (struct item_effect, pack(1), 23 bytes):
  u8 unknown_00; u8 type; u16 ap; u16 ht; u16 dp; u16 ev;
  u8 special_type; u8 special_chance; u8 range_min; u8 range_max;
  u8 use_effect; u8 use_param_lo; u8 use_param_hi; u8 cast_range_flags;
  u8 target_side; u8 area; u16 price; u8 trailing_22;

Usage:
  python gen_item_effect.py emit   > entry_initializers.txt   (prints C body)
  python gen_item_effect.py verify <c_file>                   (struct-aware byte gate)
"""
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HEX = os.path.join(ROOT, "workspace", "data_emit", "item_effect_ghidra.hex")
NAME = "data_fd2_battle_item_effect_table"
STRIDE = 23
COUNT = 215

# (field_name, width_bytes) in declaration order; widths must sum to STRIDE
FIELDS = [
    ("unknown_00", 1), ("type", 1), ("ap", 2), ("ht", 2), ("dp", 2), ("ev", 2),
    ("special_type", 1), ("special_chance", 1), ("range_min", 1), ("range_max", 1),
    ("use_effect", 1), ("use_param_lo", 1), ("use_param_hi", 1),
    ("cast_range_flags", 1), ("target_side", 1), ("area", 1),
    ("price", 2), ("trailing_22", 1),
]
assert sum(w for _, w in FIELDS) == STRIDE


def load_bytes():
    h = open(HEX, encoding="utf-8").read().strip()
    b = bytes(int(h[i:i+2], 16) for i in range(0, len(h), 2))
    if len(b) != STRIDE * COUNT:
        sys.exit("hex length %d != %d" % (len(b), STRIDE * COUNT))
    return b


def entry_fields(entry):
    """Return list[int] of the 18 field values for one 23-byte entry (LE)."""
    vals = []
    off = 0
    for _, w in FIELDS:
        vals.append(int.from_bytes(entry[off:off+w], "little"))
        off += w
    return vals


def emit_body(b):
    lines = []
    for i in range(COUNT):
        entry = b[i*STRIDE:(i+1)*STRIDE]
        v = entry_fields(entry)
        # one entry per line: all 18 field initializers, decimal
        inner = ",".join(str(x) for x in v)
        lines.append("    { %s }," % inner)
    return "\n".join(lines)


def repack(vals_flat_per_entry):
    """Given list of 215 lists-of-18-ints, repack to bytes and return."""
    out = bytearray()
    for v in vals_flat_per_entry:
        if len(v) != len(FIELDS):
            return None, "entry has %d fields (want %d)" % (len(v), len(FIELDS))
        for (name, w), x in zip(FIELDS, v):
            if x < 0 or x >= (1 << (8*w)):
                return None, "field %s value %d out of range for width %d" % (name, x, w)
            out += int(x).to_bytes(w, "little")
    return bytes(out), None


def parse_c(text):
    """Extract the 215 brace-groups of the table initializer from C source,
    return list[list[int]] (per-entry field ints) or exits on error."""
    m = re.search(r"(?m)^(?:const\s+)?item_effect\s+" + re.escape(NAME) +
                  r"\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        sys.exit("table def not found in C file")
    # walk from the outer '{' collecting inner {...} groups at depth 1
    i = m.end() - 1  # at outer '{'
    depth = 0
    entries = []
    cur = None
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "{":
            depth += 1
            if depth == 2:
                cur = []  # start an entry; collect chars
                start = i + 1
        elif ch == "}":
            if depth == 2:
                body = text[start:i]
                toks = [t for t in re.split(r"[,\s]+", body) if t]
                for t in toks:
                    if t.startswith(("0x", "0X")):
                        cur.append(int(t, 16))
                    elif re.fullmatch(r"-?\d+", t):
                        cur.append(int(t))
                    else:
                        sys.exit("unparsed token in entry %d: %r" % (len(entries), t))
                entries.append(cur)
                cur = None
            depth -= 1
            if depth == 0:
                break
        i += 1
    return entries


def main():
    b = load_bytes()
    if len(sys.argv) >= 2 and sys.argv[1] == "emit":
        # self-check: emit -> reparse our own text -> repack -> compare
        body = emit_body(b)
        wrapped = ("const item_effect %s[%d] = {\n%s\n};\n" % (NAME, COUNT, body))
        entries = parse_c(wrapped)
        rb, err = repack(entries)
        if err:
            sys.exit("SELF-CHECK repack error: " + err)
        if rb != b:
            di = next(i for i in range(len(b)) if rb[i] != b[i])
            sys.exit("SELF-CHECK byte[%d] gen=0x%02x ghidra=0x%02x" % (di, rb[di], b[di]))
        sys.stderr.write("SELF-CHECK PASS: %d entries, %d bytes round-trip identical\n"
                         % (COUNT, len(b)))
        sys.stdout.write(body + "\n")
        return
    if len(sys.argv) >= 3 and sys.argv[1] == "verify":
        text = open(sys.argv[2], encoding="utf-8").read()
        entries = parse_c(text)
        if len(entries) != COUNT:
            sys.exit("FAIL: parsed %d entries, want %d" % (len(entries), COUNT))
        rb, err = repack(entries)
        if err:
            sys.exit("FAIL: " + err)
        if len(rb) != len(b):
            sys.exit("FAIL: C packs to %d bytes, Ghidra %d" % (len(rb), len(b)))
        if rb != b:
            di = next(i for i in range(len(b)) if rb[i] != b[i])
            sys.exit("FAIL byte[%d] C=0x%02x Ghidra=0x%02x (entry %d field-region)"
                     % (di, rb[di], b[di], di // STRIDE))
        print("  PASS  %-50s %d bytes, %d entries (struct-aware)" % (NAME, len(b), COUNT))
        return
    sys.exit(__doc__)


if __name__ == "__main__":
    main()
