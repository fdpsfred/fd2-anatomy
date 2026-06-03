"""
genbuild.py - regenerate the whole test build config from the source trees.

State-driven: scans src/ and tests/ and rewrites the generated regions, leaving
the static parts (mounts, env, CF, link/run lines) untouched:

  tests/build.bat   : both the `=== compile src ===` and `=== compile tests ===`
                      blocks (one WCC386 line per src/ and per tests/ .c file)
  tests/test.lnk    : every `file E:\\out\\*.obj` line (src objs + test objs)
  tests/testmain.c  : the extern decls + run_*_tests() calls (between markers)

So adding a new function's src .c and/or test .c just needs `genbuild.py --apply`
-- no hand-editing of build.bat / test.lnk / testmain. Existing src obj names and
their order are preserved (parsed from build.bat); new src files are appended
with obj = stem-without-underscores (<=8, unique). Object stems are validated
unique across {src objs} U {test objs}.

Usage:  python tests/genbuild.py [--apply]
"""
import re
import sys
from pathlib import Path

import naming

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
SRC = ROOT / 'src'
# The long compile/link/run command list lives in build.bat (a real file with no
# line limit); dosbox.conf's [autoexec] just mounts + sets env + calls it. This
# keeps the autoexec under DOSBox-X's buffer cap as the file count grows.
RUN_RE = re.compile(r'^void (run_\w+_tests)\s*\(', re.M)
# obj output lives in E:\out\obj\ (the `obj\` group is optional so a pre-reorg
# build.bat still parses for existing obj-name/order preservation).
SRC_LINE_RE = re.compile(r'WCC386\.EXE\s+(\S+\.c)\s+%CF%.*-fo=E:\\out\\(?:obj\\)?(\w+)\.obj')
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
        out.append({'rel': p.relative_to(TESTS).as_posix(), 'run': m.group(1)})
    return out


def _src_obj(stem, reserved):
    base = stem.replace('_', '')[:8]
    cand, i = base, 0
    while cand in reserved or not cand:
        i += 1
        cand = base[:8 - len(str(i))] + str(i)
    reserved.add(cand)
    return cand


def src_compile_list():
    """Ordered [(rel, obj)] for every src/ .c file. Existing entries keep their
    build.bat order + (possibly curated) obj name; new files are appended."""
    parsed, seen = [], set()
    inside = False
    for ln in BAT.read_text(encoding='utf-8').split('\n'):
        if '=== compile src ===' in ln:
            inside = True
            continue
        if '=== compile tests ===' in ln:
            break
        if inside:
            m = SRC_LINE_RE.search(ln)
            if m:
                rel = m.group(1).replace('\\', '/')
                parsed.append((rel, m.group(2)))
                seen.add(rel)
    disk = {p.relative_to(SRC).as_posix() for p in SRC.rglob('*.c')}
    kept = [(rel, obj) for (rel, obj) in parsed if rel in disk]
    reserved = {obj for _, obj in kept}
    new = []
    for rel in sorted(disk - seen):
        _, stem = naming.domain_stem(rel)
        new.append((rel, _src_obj(stem, reserved)))
    return kept + new


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


def gen_bat(test_files, test_objmap, src_list):
    """Fully generated. Compile (cwd = C:\\ = src) writes every .obj to
    E:\\out\\obj\\; link emits E:\\out\\TEST.EXE; the run then `cd \\out` on E:
    so TEST.EXE's cwd is tests/OUT — the resource loaders' bare-name fopen()
    (FDICON.B24 / *.DAT / FD2.SAV, staged into tests/OUT by build_test.py) and
    the FD2.TMP output resolve there, keeping src/ clean."""
    L = [r'echo === compile src === > E:\out\build.out']
    for rel, obj in src_list:
        L.append(r'D:\BIN\WCC386.EXE %s %%CF%% -fo=E:\out\obj\%s.obj '
                 r'>> E:\out\build.out' % (rel.replace('/', '\\'), obj))
    L.append('')
    L.append(r'echo === compile tests === >> E:\out\build.out')
    L.append(r'D:\BIN\WCC386.EXE E:\testmain.c %CF% '
             r'-fo=E:\out\obj\testmain.obj >> E:\out\build.out')
    L.append(r'D:\BIN\WCC386.EXE E:\testglob.c %CF% '
             r'-fo=E:\out\obj\testglob.obj >> E:\out\build.out')
    for f in sorted(test_files, key=lambda x: x['rel']):
        L.append(r'D:\BIN\WCC386.EXE E:\%s %%CF%% -fo=E:\out\obj\%s.obj '
                 r'>> E:\out\build.out'
                 % (f['rel'].replace('/', '\\'), test_objmap[f['rel']]))
    L.append('')
    L.append(r'echo === link === >> E:\out\build.out')
    L.append(r'D:\BIN\WLINK.EXE @E:\test.lnk >> E:\out\build.out')
    L.append(r'echo === run === >> E:\out\build.out')
    L.append('E:')
    L.append(r'cd \out')
    L.append('TEST.EXE > test.out')
    L.append('echo done > done.txt')
    L.append('exit')
    return '\n'.join(L) + '\n'


def gen_lnk(test_files, test_objmap, src_list):
    header = []
    for ln in LNK.read_text(encoding='utf-8').split('\n'):
        if ln.strip().startswith(('system', 'name', 'option')):
            header.append(ln)
    out = list(header)
    for _, obj in src_list:
        out.append(r'file E:\out\obj\%s.obj' % obj)
    out.append(r'file E:\out\obj\testmain.obj')
    out.append(r'file E:\out\obj\testglob.obj')
    for f in sorted(test_files, key=lambda x: x['rel']):
        out.append(r'file E:\out\obj\%s.obj' % test_objmap[f['rel']])
    return '\n'.join(out) + '\n'


def _replace_between(text, start, end, body):
    if start not in text or end not in text:
        raise SystemExit('testmain.c missing GENBUILD markers (%s / %s)'
                         % (start, end))
    a = text.index(start) + len(start)
    b = text.index(end)
    return text[:a] + '\n' + body + '\n' + text[b:]


def gen_testmain(test_files):
    text = TESTMAIN.read_text(encoding='utf-8')
    runs = [f['run'] for f in sorted(test_files, key=lambda x: x['rel'])]
    ext = '\n'.join('extern void %s(void);' % r for r in runs)
    calls = '\n'.join('    %s();' % r for r in runs)
    text = _replace_between(text, EXT_START, EXT_END, ext)
    text = _replace_between(text, CALL_START, CALL_END, calls)
    return text


def gen_all():
    src_list = src_compile_list()
    src_objs = [obj for _, obj in src_list]
    test_files = suite_files()
    for f in test_files:
        if not (TESTS / f['rel']).is_file():
            raise SystemExit('missing source: %s' % f['rel'])
    reserved = set(src_objs) | {'testmain', 'testglob'}
    test_objmap = assign_objs(test_files, reserved)
    all_objs = src_objs + list(test_objmap.values()) + ['testmain', 'testglob']
    assert len(all_objs) == len(set(all_objs)), 'duplicate obj stems: %s' % all_objs
    assert all(len(o) <= 8 for o in all_objs), 'obj stem >8 chars'
    return {
        'test_files': test_files, 'test_objmap': test_objmap, 'src_list': src_list,
        str(BAT): gen_bat(test_files, test_objmap, src_list),
        str(LNK): gen_lnk(test_files, test_objmap, src_list),
        str(TESTMAIN): gen_testmain(test_files),
    }


def apply(verbose=True):
    g = gen_all()
    for path in (BAT, LNK, TESTMAIN):
        Path(path).write_text(g[str(path)], encoding='utf-8')
    if verbose:
        print('genbuild: %d src + %d test source files'
              % (len(g['src_list']), len(g['test_files'])))
    return g


def main():
    do_apply = '--apply' in sys.argv
    g = gen_all()
    print('genbuild: %d src files, %d test source files (objs unique <=8: OK)'
          % (len(g['src_list']), len(g['test_files'])))
    for f in sorted(g['test_files'], key=lambda x: x['rel']):
        print('   %-24s obj=%-8s run=%s'
              % (f['rel'], g['test_objmap'][f['rel']], f['run']))
    if not do_apply:
        print('\n[dry-run] no writes. Re-run with --apply.')
        return 0
    apply(verbose=False)
    print('\napplied: build.bat, test.lnk, testmain.c')
    return 0


if __name__ == '__main__':
    sys.exit(main())
