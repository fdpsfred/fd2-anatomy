"""
where.py - resolve a routing target to the exact test leaf a new test goes in.

The emit workflow runs it to turn "which test file?" into a lookup. Given a
function's src target (routing.json `target`, e.g. "battle/btl_aisc.c"), tests
mirror that path, but a src sub-file whose tests passed 1000 lines was split into
<stem>1.c/<stem>2.c, so the mirror path may not exist as one file. This reports
the exact file + runner to use:

  - un-split leaf with room      -> append to it
  - split leaf                   -> append to the latest part that has room
  - all parts full / no leaf yet -> create the next part / the mirror file,
                                     then run tests/genbuild.py --apply

Usage:  python tests/where.py <domain>/<stem>.c
"""
import re
import sys
from pathlib import Path

import naming

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
ROOM = 920          # a leaf at/under this has room for another test and stays <1000

RUN_RE = re.compile(r'^void (run_\w+_tests)\s*\(', re.M)


def runner_of(path):
    m = RUN_RE.search(path.read_text(encoding='utf-8'))
    return m.group(1) if m else '?'


def nlines(path):
    return path.read_text(encoding='utf-8').count('\n')


def existing(domain, stem):
    """Files backing this src sub-file: (path, part_no|None), exact first."""
    d = TESTS / domain
    out = []
    exact = d / (stem + '.c')
    if exact.is_file():
        out.append((exact, None))
    n = 1
    while (d / (naming.part_fstem(stem, n, 2) + '.c')).is_file():
        out.append((d / (naming.part_fstem(stem, n, 2) + '.c'), n))
        n += 1
    return out


def resolve(target):
    domain, stem = naming.domain_stem(target)
    files = existing(domain, stem)

    if not files:                                   # leaf doesn't exist yet
        return dict(leaf=TESTS / domain / (stem + '.c'),
                    runner=naming.runner_name(domain, stem, 1, 1), action='create',
                    note='new mirror file; after writing it run '
                         'python tests/genbuild.py --apply')

    if len(files) == 1 and files[0][1] is None:     # single un-split leaf
        leaf = files[0][0]
        ln = nlines(leaf)
        if ln <= ROOM:
            return dict(leaf=leaf, runner=runner_of(leaf), action='append',
                        note='%d lines, ~%d free' % (ln, 1000 - ln))
        return dict(leaf=leaf, runner=runner_of(leaf), action='append-tight',
                    note='%d lines (near 1000); if it would cross 1000, split it '
                         'into a new part file first (see tests/_index.md)' % ln)

    parts = [(f, n, nlines(f)) for (f, n) in files if n is not None]
    summary = ', '.join('%s=%d' % (f.name, ln) for (f, _, ln) in parts)
    room = [p for p in parts if p[2] <= ROOM]
    if room:
        f, n, ln = max(room, key=lambda p: p[1])    # latest part with room
        return dict(leaf=f, runner=runner_of(f), action='append',
                    note='split leaf; part %d has %d lines (~%d free). parts: %s'
                         % (n, ln, 1000 - ln, summary))

    maxn = max(n for (_, n, _) in parts)            # all parts full -> new part
    return dict(leaf=TESTS / domain / (naming.part_fstem(stem, maxn + 1, 2) + '.c'),
                runner=naming.runner_name(domain, stem, maxn + 1, 2), action='create',
                note='all parts full (%s); add new part, then run '
                     'python tests/genbuild.py --apply' % summary)


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: python tests/where.py <domain>/<stem>.c')
    r = resolve(sys.argv[1])
    print('target src : %s' % sys.argv[1])
    print('test leaf  : %s' % r['leaf'].relative_to(ROOT).as_posix())
    print('runner     : %s' % r['runner'])
    print('action     : %s (%s)' % (r['action'], r['note']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
