"""
naming.py - shared name derivation for tools/test_split.

Filenames obey DOS 8.3 (Watcom 9.5a has no LFN). C identifiers (runner names)
do not. Object stems must be <=8 chars and unique across the whole link.
"""
import re


def domain_stem(path):
    """'battle/btl_aisc.c' -> ('battle', 'btl_aisc'). Root files -> ('', stem)."""
    p = path.replace('\\', '/')
    if '/' in p:
        d, s = p.split('/', 1)
    else:
        d, s = '', p
    if s.endswith('.c'):
        s = s[:-2]
    return d, s


def part_fstem(stem, part, nparts):
    """Filename stem for part `part` (1-based) of a split leaf. <=8 chars."""
    if nparts <= 1:
        return stem
    if part > 9:
        raise ValueError('more than 9 parts not supported: %s' % stem)
    return stem[:7] + str(part)


def runner_name(domain, stem, part, nparts):
    """C identifier for a leaf's dispatcher; globally unique (domain-qualified,
    full original stem + part suffix)."""
    suffix = '' if nparts <= 1 else str(part)
    return 'run_%s_%s%s_tests' % (domain, stem, suffix)


def fixhdr(domain):
    """8.3-safe basename stem (<=8 chars) for a domain's shared fixture header.
    e.g. 'battle' -> 'battlfix' (battlfix.h)."""
    return domain[:5] + 'fix'


def obj_stem(fstem, reserved):
    """A unique <=8-char object stem for a test leaf filename stem. Trailing
    digits (split parts) are preserved so anisumm1/anisumm2 stay distinct.
    `reserved` is mutated with the chosen stem."""
    m = re.match(r'^(.*?)(\d*)$', fstem)
    base, num = m.group(1), m.group(2)
    base = 't' + base.replace('_', '')
    keep = max(1, 8 - len(num))
    cand = (base[:keep] + num)[:8]
    if cand not in reserved:
        reserved.add(cand)
        return cand
    for d in range(100):
        suf = str(d)
        cand = (base[:8 - len(suf)] + suf)[:8]
        if cand not in reserved:
            reserved.add(cand)
            return cand
    raise RuntimeError('cannot allocate unique obj stem for %s' % fstem)
