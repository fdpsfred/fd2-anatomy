"""
genbuild.py - regenerate the test build config from the current tests/ tree.

State-driven: scans every test source file under tests/ (monoliths and/or
domain leaves), assigns each a unique <=8-char object stem, and rewrites three
generated regions while leaving everything else (mounts, env, CF, the src
compile block, link/run lines) untouched:

  tests/build.bat   : the `=== compile tests ===` .. `=== link ===` block
  tests/test.lnk    : the test-side `file E:\\out\\*.obj` lines (src objs kept)
  tests/testmain.c  : the extern decls + run_*_tests() calls (between markers)

Object stems are validated unique across {src objs} U {test objs}; src obj
stems are read live from build.bat so there is no hard-coded list.

Usage:  python tests/genbuild.py [--apply]
"""
import io
import re
import sys
from pathlib import Path

import naming

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
# The long compile/link/run command list lives in build.bat (a real file with no
# line limit); dosbox.conf's [autoexec] just mounts + sets env + calls it. This
# keeps the autoexec under DOSBox-X's buffer cap as the test-file count grows.
RUN_RE = re.compile(r'^void (run_\w+_tests)\s*\(', re.M)
BAT = TESTS / 'build.bat'
LNK = TESTS / 'test.lnk'
TESTMAIN = TESTS / 'testmain.c'

EXT_START = '/* >>> GENBUILD externs >>> */'
EXT_END = '/* <<< GENBUILD externs <<< */'
CALL_START = '/* >>> GENBUILD calls >>> */'
CALL_END = '/* <<< GENBUILD calls <<< */'


def suite_files():
    out = []
    for p in sorted(TESTS.rglob('*.c')):
        if p.name in ('testmain.c', 'testglob.c'):
            continue
        m = RUN_RE.search(p.read_text(encoding='utf-8'))
        if not m:                          # not a suite file (no dispatcher)
            continue
        out.append({'rel': p.relative_to(TESTS).as_posix(),
                    'run': m.group(1)})
    return out


def src_obj_stems():
    stems, inside = set(), False
    for ln in BAT.read_text(encoding='utf-8').split('\n'):
        if '=== compile src ===' in ln:
            inside = True
            continue
        if '=== compile tests ===' in ln:
            break
        if inside:
            m = re.search(r'-fo=E:\\out\\(\w+)\.obj', ln)
            if m:
                stems.add(m.group(1))
    return stems


def assign_objs(files, reserved):
    objmap = {}
    for f in sorted(files, key=lambda x: x['rel']):
        _, stem = naming.domain_stem(f['rel'])
        if '/' not in f['rel'] and stem.startswith('test') and len(stem) <= 8 \
                and stem not in reserved:
            objmap[f['rel']] = stem
            reserved.add(stem)
        else:
            objmap[f['rel']] = naming.obj_stem(stem, reserved)
    return objmap


def _compile_path(rel):
    return 'E:\\' + rel.replace('/', '\\')


def gen_bat(files, objmap):
    lines = BAT.read_text(encoding='utf-8').split('\n')
    out, i, n = [], 0, len(lines)
    while i < n:
        out.append(lines[i])
        if '=== compile tests ===' in lines[i]:
            out.append(r'D:\BIN\WCC386.EXE E:\testmain.c %CF% '
                       r'-fo=E:\out\testmain.obj >> E:\out\build.out')
            out.append(r'D:\BIN\WCC386.EXE E:\testglob.c %CF% '
                       r'-fo=E:\out\testglob.obj >> E:\out\build.out')
            for f in sorted(files, key=lambda x: x['rel']):
                out.append(r'D:\BIN\WCC386.EXE %s %%CF%% -fo=E:\out\%s.obj '
                           r'>> E:\out\build.out'
                           % (_compile_path(f['rel']), objmap[f['rel']]))
            out.append('')
            i += 1
            while i < n and '=== link ===' not in lines[i]:
                i += 1
            continue
        i += 1
    return '\n'.join(out)


def gen_lnk(files, objmap, reserved_src):
    header, src_files = [], []
    for ln in LNK.read_text(encoding='utf-8').split('\n'):
        s = ln.strip()
        if s.startswith('file '):
            m = re.search(r'\\out\\(\w+)\.obj', ln)
            if m and m.group(1) in reserved_src:
                src_files.append(ln)
        elif s.startswith(('system', 'name', 'option')):
            header.append(ln)
    out = header + src_files
    out.append(r'file E:\out\testmain.obj')
    out.append(r'file E:\out\testglob.obj')
    for f in sorted(files, key=lambda x: x['rel']):
        out.append(r'file E:\out\%s.obj' % objmap[f['rel']])
    return '\n'.join(out) + '\n'


def _replace_between(text, start, end, body):
    if start not in text or end not in text:
        raise SystemExit('testmain.c missing GENBUILD markers (%s / %s)'
                         % (start, end))
    a = text.index(start) + len(start)
    b = text.index(end)
    return text[:a] + '\n' + body + '\n' + text[b:]


def gen_testmain(files):
    text = TESTMAIN.read_text(encoding='utf-8')
    runs = [f['run'] for f in sorted(files, key=lambda x: x['rel'])]
    ext = '\n'.join('extern void %s(void);' % r for r in runs)
    calls = '\n'.join('    %s();' % r for r in runs)
    text = _replace_between(text, EXT_START, EXT_END, ext)
    text = _replace_between(text, CALL_START, CALL_END, calls)
    return text


def gen_all():
    files = suite_files()
    reserved_src = src_obj_stems()
    reserved = set(reserved_src) | {'testmain', 'testglob'}
    # validate one run-name per file + path exists
    for f in files:
        if not (TESTS / f['rel']).is_file():
            raise SystemExit('missing source: %s' % f['rel'])
    objmap = assign_objs(files, reserved)
    # uniqueness already guaranteed by allocator; assert anyway
    objs = list(objmap.values())
    assert len(objs) == len(set(objs)), 'duplicate test obj stems: %s' % objs
    assert all(len(o) <= 8 for o in objs), 'obj stem >8 chars'
    return {
        'files': files, 'objmap': objmap,
        str(BAT): gen_bat(files, objmap),
        str(LNK): gen_lnk(files, objmap, reserved_src),
        str(TESTMAIN): gen_testmain(files),
    }


def apply(verbose=True):
    g = gen_all()
    for path in (BAT, LNK, TESTMAIN):
        Path(path).write_text(g[str(path)], encoding='utf-8')
    if verbose:
        print('genbuild: %d test source files' % len(g['files']))
        for f in sorted(g['files'], key=lambda x: x['rel']):
            print('   %-24s obj=%-8s run=%s'
                  % (f['rel'], g['objmap'][f['rel']], f['run']))
    return g


def main():
    do_apply = '--apply' in sys.argv
    g = gen_all()
    print('genbuild: %d test source files (obj stems unique <=8: OK)'
          % len(g['files']))
    for f in sorted(g['files'], key=lambda x: x['rel']):
        print('   %-24s obj=%-8s run=%s'
              % (f['rel'], g['objmap'][f['rel']], f['run']))
    if not do_apply:
        print('\n[dry-run] no writes. Re-run with --apply.')
        return 0
    apply(verbose=False)
    print('\napplied: build.bat, test.lnk, testmain.c')
    return 0


if __name__ == '__main__':
    sys.exit(main())
