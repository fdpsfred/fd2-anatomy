#!/usr/bin/env python3
"""dedup_protos.py -- de-duplicate repeated function prototypes in src/include/protos.h.

The emit pipeline added each function's prototype to every section it was
associated with, so ~47 fd2_* functions are declared 2-3x. All copies are
type-compatible (the file compiles -> C forbids conflicting redecl), so removing
the redundant copies is codegen-neutral. This tool keeps, for each duplicated
function, the single declaration whose PARAM NAMES best match the authoritative
src_info.json signature (the refined definition), and removes the others.

Modes:
  --plan   : print every duplicate function, all its declaration occurrences
             (line span + collapsed text + score), and KEEP/REMOVE decision.
             Does NOT modify protos.h.
  --apply  : remove the non-canonical declaration spans in place. Backs up the
             original to workspace/src_refine/protos.h.bak first; prints the
             removed spans. Re-run --plan after to confirm zero duplicates.

Selection score per occurrence (higher wins; tie -> earliest line kept):
  (match_count, named_count)
    match_count = #params whose name == authoritative name at same position
    named_count = #params that have a name at all (named > unnamed decls)
Sanity: all occurrences of a name MUST share the same param COUNT (else the
tool refuses to touch that function and flags it).
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROTOS = ROOT / "src" / "include" / "protos.h"
SRCINFO = ROOT / "tools" / "src_refine" / "data" / "src_info.json"
BAK = ROOT / "workspace" / "src_refine" / "protos.h.bak"

TYPE_WORDS = {"void", "char", "short", "int", "long", "unsigned", "signed",
              "uint8", "uint16", "uint32", "int8", "int16", "int32", "ushort",
              "uint", "float", "double", "const", "struct", "FILE", "size_t"}


def split_top_commas(s):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    return parts


def param_names(sig):
    """Return list of param names (''=unnamed) from a signature/decl string."""
    i = sig.find("(")
    if i < 0:
        return None
    depth, j = 0, i
    while j < len(sig):
        if sig[j] == "(":
            depth += 1
        elif sig[j] == ")":
            depth -= 1
            if depth == 0:
                break
        j += 1
    inner = sig[i + 1:j].strip()
    if inner in ("", "void"):
        return []
    names = []
    for p in split_top_commas(inner):
        words = re.findall(r"[A-Za-z_]\w*", p)
        nm = ""
        for w in reversed(words):
            if w not in TYPE_WORDS:
                nm = w
                break
        names.append(nm)
    return names


def load_auth():
    """name -> [authoritative param names] from src_info.json signatures."""
    data = json.loads(SRCINFO.read_text(encoding="utf-8"))
    auth = {}
    for rec in data.values():
        nm = rec.get("name_final") or rec.get("name_current")
        sig = rec.get("signature") or ""
        if nm and "(" in sig:
            pn = param_names(sig)
            if pn is not None:
                auth[nm] = pn
    return auth


DECL_START = re.compile(r"^\s*(?:extern\s+)?[A-Za-z_][\w\s\*]*\b(fd2_[A-Za-z0-9_]+)\s*\(")


def parse_decls(lines):
    """Return list of (name, start_idx, end_idx, text) for every fd2_* prototype.
    Tracks block-comment state so comment bodies are never parsed as decls."""
    decls = []
    in_block = False
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        stripped = line.strip()
        if in_block:
            if "*/" in line:
                in_block = False
            i += 1
            continue
        # single-line block comment(s) only -> skip; opening of a multi-line block
        if stripped.startswith("/*") and "*/" not in line:
            in_block = True
            i += 1
            continue
        if (stripped.startswith("//") or stripped.startswith("*")
                or stripped.startswith("/*")):   # comment line (incl. 1-line block)
            i += 1
            continue
        m = DECL_START.match(line)
        if m and "*/" not in line.split("(")[0]:
            name = m.group(1)
            start = i
            # consume until a line that contains ';'
            buf = line
            while ";" not in lines[i]:
                i += 1
                if i >= n:
                    break
                buf += "\n" + lines[i]
            end = i
            decls.append((name, start, end, buf))
        i += 1
    return decls


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--plan"
    lines = PROTOS.read_text(encoding="utf-8").split("\n")
    auth = load_auth()
    decls = parse_decls(lines)

    by_name = {}
    for d in decls:
        by_name.setdefault(d[0], []).append(d)
    dups = {k: v for k, v in by_name.items() if len(v) > 1}

    remove_spans = []   # (start_idx, end_idx)
    flagged = []
    print("=== protos.h dedup plan: %d duplicated functions ===" % len(dups))
    for name in sorted(dups, key=lambda k: dups[k][0][1]):
        occ = dups[name]
        counts = {len(param_names(o[3]) or []) for o in occ}
        a = auth.get(name)
        scored = []
        for (nm, s, e, txt) in occ:
            pn = param_names(txt) or []
            named = sum(1 for x in pn if x)
            match = sum(1 for k, x in enumerate(pn)
                        if a and k < len(a) and x and x == a[k])
            scored.append((match, named, -s, (nm, s, e, txt), pn))
        scored.sort(reverse=True)
        keep = scored[0]
        conflict = len(counts) > 1
        tag = "  !!PARAM-COUNT-CONFLICT" if conflict else ""
        if conflict:
            flagged.append((name, "param count differs across occurrences: %s" % counts))
        print("\n%s%s" % (name, tag))
        print("  auth params: %s" % (a if a is not None else "<not in src_info>"))
        for (match, named, negs, (nm, s, e, txt), pn) in scored:
            mark = "KEEP " if (match, named, negs) == (keep[0], keep[1], keep[2]) else "DROP "
            one = " ".join(l.strip() for l in txt.split("\n"))
            print("  %s L%d-%d  match=%d named=%d  %s" % (mark, s + 1, e + 1, match, named, pn))
            print("        %s" % one)
            if mark == "DROP ":
                remove_spans.append((s, e))
        # warn if a dropped occurrence had names the kept one lacks
        keep_pn = keep[4]
        for (match, named, negs, (nm, s, e, txt), pn) in scored[1:]:
            extra = [x for k, x in enumerate(pn) if x and (k >= len(keep_pn) or not keep_pn[k])]
            if extra:
                flagged.append((name, "DROP L%d had names KEEP lacks: %s" % (s + 1, extra)))

    print("\n=== %d declaration spans to remove ===" % len(remove_spans))
    if flagged:
        print("\n!!! %d FLAGS for manual review:" % len(flagged))
        for nm, msg in flagged:
            print("  %-50s %s" % (nm, msg))

    if mode == "--apply":
        if flagged:
            print("\nREFUSING --apply: resolve flags first (or override).")
            return 1
        BAK.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PROTOS, BAK)
        drop = set()
        for (s, e) in remove_spans:
            for k in range(s, e + 1):
                drop.add(k)
        out = [ln for idx, ln in enumerate(lines) if idx not in drop]
        PROTOS.write_text("\n".join(out), encoding="utf-8")
        print("\nAPPLIED: removed %d lines; backup at %s" % (len(drop), BAK))
    return 0


if __name__ == "__main__":
    sys.exit(main())
