#!/usr/bin/env python3
"""publish_public.py -- sync the PUBLIC fd2-anatomy repo from this dev repo.

The public repo (github.com/fdpsfred/fd2-anatomy) is a clean subset MIRROR with
a fresh single-commit history -- it carries NONE of the dev repo's git history.
This script rebuilds that mirror: export the current HEAD tracked tree, drop the
dev-only paths (.claude/, CLAUDE.md, tools/publish/), scan for personal info,
build one commit, and (only with --push) force-push it to the public remote.

It publishes HEAD (the committed state), not the working tree -- commit dev
changes first. Output/staging goes under workspace/publish/ (gitignored).

Usage:
  python tools/publish/publish_public.py          # build + scan + manifest (NO push)
  python tools/publish/publish_public.py --push    # also force-push to the public repo

Exit: 0 on success (or clean dry-run); 1 if the PII scan fails (never pushes then).
"""
import argparse
import io
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "workspace" / "publish" / "staging"
PUBLIC_REMOTE = os.environ.get("FD2_PUBLIC_REMOTE",
                               "https://github.com/fdpsfred/fd2-anatomy.git")

# dev-only paths kept OUT of the public mirror. tools/publish is the publish
# tooling itself (dev-only; and its PII-scan source literally contains the
# 'fdpsf' name, which would otherwise trip the scan below).
EXCLUDES = [".claude", "CLAUDE.md", "tools/publish"]

# Patterns that must NOT reach the public tree. The public account handle
# "fdpsfred" (e.g. the fdpsfred.github.io Pages URLs in README) is allowed; the
# Windows username "fdpsf" in host paths and the real name are not.
PII = [
    (re.compile(r"fdpsf(?!red)"), "Windows username 'fdpsf' (host-path leak)"),
    (re.compile(r"REDACTED", re.I), "real name"),
]

AUTHOR = ("fdpsfred", "fdpsfred@gmail.com")

COMMIT_MSG = """炎龍騎士團 2 逆向工程專案

從遊戲執行檔還原 C 原始碼並重建 100% 行為一致的 FD2.EXE,
附完整逆向工程知識庫(程式 / 資源 / 章節 / 數值解析)。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
"""


def git(args, **kw):
    return subprocess.run(["git"] + args, check=True, **kw)


def _rmtree_force(path):
    """rmtree that survives Windows read-only git objects (.idx/.pack, WinError 5):
    clear the read-only bit on the offending file and retry the delete."""
    def onerr(func, p, _exc):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    try:
        shutil.rmtree(path, onexc=onerr)      # py>=3.12
    except TypeError:
        shutil.rmtree(path, onerror=onerr)    # py<3.12


def export_tree():
    """Extract HEAD's tracked files (no .git, no gitignored) into STAGE, then
    delete the dev-only excludes."""
    if STAGE.exists():
        _rmtree_force(STAGE)
    STAGE.mkdir(parents=True)
    tar_bytes = subprocess.run(["git", "-C", str(ROOT), "archive", "HEAD"],
                               check=True, stdout=subprocess.PIPE).stdout
    with tarfile.open(fileobj=io.BytesIO(tar_bytes)) as tf:
        try:
            tf.extractall(STAGE, filter="data")   # py>=3.12 safe extraction
        except TypeError:
            tf.extractall(STAGE)
    for ex in EXCLUDES:
        p = STAGE / ex
        if p.is_dir():
            shutil.rmtree(p)
        elif p.exists():
            p.unlink()


def scan_pii():
    """Return [(relpath, lineno, why)] for every PII hit in the staged text
    files. Binary/undecodable files are skipped (patterns are ASCII text)."""
    hits = []
    for path in sorted(STAGE.rglob("*")):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for i, line in enumerate(text.splitlines(), 1):
            for rx, why in PII:
                if rx.search(line):
                    hits.append((path.relative_to(STAGE).as_posix(), i, why))
    return hits


def manifest():
    files = [p for p in STAGE.rglob("*") if p.is_file()]
    top = {}
    for p in files:
        rel = p.relative_to(STAGE).as_posix()
        top_key = rel.split("/", 1)[0]
        top[top_key] = top.get(top_key, 0) + 1
    return len(files), sorted(top.items(), key=lambda kv: -kv[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--push", action="store_true",
                    help="force-push the fresh single commit to the public repo")
    ap.add_argument("--message", default=COMMIT_MSG, help="commit message override")
    args = ap.parse_args()

    dirty = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"],
                           check=True, capture_output=True, text=True).stdout.strip()
    if dirty:
        print("WARN: dev working tree has uncommitted changes; publishing HEAD "
              "(committed state) only.\n")

    for ex in EXCLUDES:
        if not (ROOT / ex).exists():
            print("WARN: exclude '%s' not present in dev repo (spec drift?)" % ex)

    export_tree()
    n, top = manifest()
    print("staging : %s" % STAGE)
    print("files   : %d" % n)
    for k, c in top:
        print("  %5d  %s" % (c, k))
    for ex in EXCLUDES:
        print("exclude gone? %-12s %s" % (ex, "NO -- STILL PRESENT" if (STAGE / ex).exists() else "yes"))

    hits = scan_pii()
    if hits:
        print("\nPII SCAN: FAIL -- %d hit(s); NOT publishing:" % len(hits))
        for rel, ln, why in hits[:40]:
            print("  %s:%d  (%s)" % (rel, ln, why))
        return 1
    print("\nPII SCAN: clean")

    at = str(STAGE)
    git(["-C", at, "init", "-b", "main", "-q"])
    git(["-C", at, "add", "-A"])
    git(["-C", at, "-c", "user.name=%s" % AUTHOR[0], "-c", "user.email=%s" % AUTHOR[1],
         "commit", "-q", "-m", args.message])
    sha = subprocess.run(["git", "-C", at, "rev-parse", "--short", "HEAD"],
                         check=True, capture_output=True, text=True).stdout.strip()
    print("\nbuilt single commit: %s (1 commit, no dev history)" % sha)

    if not args.push:
        print("\n(dry run) not pushed. Re-run with --push to force-push to:\n  %s"
              % PUBLIC_REMOTE)
        return 0

    git(["-C", at, "remote", "add", "origin", PUBLIC_REMOTE])
    git(["-C", at, "push", "--force", "-u", "origin", "main"])
    print("\nPUSHED (force) to public repo: %s" % PUBLIC_REMOTE)
    return 0


if __name__ == "__main__":
    sys.exit(main())
