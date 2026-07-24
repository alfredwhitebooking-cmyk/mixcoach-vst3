#!/usr/bin/env python3
"""Audit .cpp/.h files for Unicode escape sequences outside Latin-1 range."""

import os, re, sys
from pathlib import Path

ROOT = Path(r'C:\Proyectos\MixCoach')
SRC_DIRS = ['Source']
EXCLUDE = ['.freebuff', 'build', 'libs', 'tests', 'scripts', '.git']

ESC_RE = re.compile(b'\\\\x[0-9A-Fa-f]{2}')

# Categories for sorting
CAT_EMOJI = {}   # cp -> name (emoji)
CAT_GEOM = {}    # cp -> name (geometric shapes)
CAT_DINGBAT = {} # cp -> name (dingbats)
CAT_MISC = {}    # cp -> name (other symbols)
CAT_UTF8 = {}    # cp -> name (Latin-1 supplement that should work)

def categorize(cp, name):
    if cp >= 0x1F300:
        CAT_EMOJI[cp] = f'EMOJI: {name}'
    elif 0x25A0 <= cp <= 0x25FF:
        CAT_GEOM[cp] = f'GEOMETRIC: {name}'
    elif 0x2700 <= cp <= 0x27BF:
        CAT_DINGBAT[cp] = f'DINGBAT: {name}'
    elif 0x2600 <= cp <= 0x26FF:
        CAT_MISC[cp] = f'MISC: {name}'
    elif 0x2000 <= cp <= 0x206F:
        CAT_DINGBAT[cp] = f'PUNCTUATION: {name}'  # general punctuation (some work in Inter)
    elif 0x0080 <= cp <= 0x00FF:
        CAT_UTF8[cp] = f'LATIN-1 SUPP: {name}'  # These SHOULD work in Inter
    else:
        CAT_MISC[cp] = f'OTHER: {name}'

found = {}  # filepath -> [(line_num, codepoint, context)]

def safe(text):
    """Return ASCII-safe version of text."""
    result = []
    for ch in text:
        if ord(ch) < 128:
            result.append(ch)
        else:
            result.append(f'[U+{ord(ch):04X}]')
    return ''.join(result)

def scan_file(fp):
    try:
        with open(fp, 'rb') as f:
            content = f.read()
    except:
        return
    
    lines = content.split(b'\n')
    for ln, line in enumerate(lines, 1):
        matches = list(ESC_RE.finditer(line))
        if not matches:
            continue
        
        groups = []
        cur = []
        for m in matches:
            if not cur:
                cur.append(m)
            elif m.start() == cur[-1].end():
                cur.append(m)
            else:
                if cur: groups.append(cur)
                cur = [m]
        if cur: groups.append(cur)
        
        for g in groups:
            hex_bytes = b''
            for m in g:
                h = line[m.start()+2:m.end()]
                try:
                    hex_bytes += bytes.fromhex(h.decode('ascii'))
                except:
                    continue
            try:
                text = hex_bytes.decode('utf-8')
            except:
                continue
            
            for ch in text:
                cp = ord(ch)
                if cp > 0x00FF:
                    ctx_s = max(0, g[0].start() - 10)
                    ctx_e = min(len(line), g[-1].end() + 30)
                    ctx = line[ctx_s:ctx_e].decode('utf-8', errors='replace').strip()[:60]
                    
                    try:
                        import unicodedata
                        name = unicodedata.name(chr(cp), f'UNKNOWN U+{cp:04X}')
                    except:
                        name = f'UNKNOWN U+{cp:04X}'
                    
                    categorize(cp, name)
                    
                    key = str(fp.relative_to(ROOT))
                    if key not in found:
                        found[key] = []
                    found[key].append((ln, cp, safe(ctx)))
                    break

files = []
for sd in SRC_DIRS:
    p = ROOT / sd
    if p.exists():
        for ext in ('*.cpp', '*.h'):
            files.extend(p.rglob(ext))
files = [f for f in files if not any(e in f.parts for e in EXCLUDE)]

print(f'Scanning {len(files)} source files...')
for f in files:
    scan_file(f)

# Print summary by category
print(f'\n{"="*80}')
print(f'AUDIT: Unicode outside Latin-1 in Source/ files')
print(f'Found in {len(found)} of {len(files)} files')
total_occ = sum(len(v) for v in found.values())
print(f'Total occurrences: {total_occ}')
print(f'{"="*80}')

# Category summaries
all_cats = [
    ('EMOJI (U+1F300+) - WILL NOT RENDER', CAT_EMOJI),
    ('GEOMETRIC SHAPES (U+25A0-U+25FF) - WILL NOT RENDER', CAT_GEOM),
    ('DINGBATS / PUNCTUATION (U+2700-U+27BF, U+2000-U+206F) - check each', CAT_DINGBAT),
    ('MISC SYMBOLS (U+2600-U+26FF) - WILL NOT RENDER', CAT_MISC),
    ('LATIN-1 SUPPLEMENT (U+0080-U+00FF) - SHOULD RENDER in Inter', CAT_UTF8),
]

for cat_name, cat_dict in all_cats:
    if not cat_dict:
        continue
    print(f'\n  [{cat_name}]')
    for cp in sorted(cat_dict.keys()):
        desc = cat_dict[cp]
        # Count occurrences
        count = sum(1 for issues in found.values() for _, c, _ in issues if c == cp)
        print(f'    U+{cp:04X} x{count}  {desc}')

# Per-file report
print(f'\n{"="*80}')
print(f'PER-FILE REPORT (files with >0 occurrences):')
print(f'{"="*80}')

for fname, issues in sorted(found.items()):
    unique_cps = sorted(set(cp for _, cp, _ in issues))
    print(f'\n  {fname} ({len(issues)} occ, {len(unique_cps)} unique)')
    for ln, cp, ctx in issues[:8]:
        print(f'    L{ln:5d} U+{cp:04X}  {ctx[:60]}')
    if len(issues) > 8:
        print(f'    (+ {len(issues)-8} more)')

# Files with MOST occurrences (top 10)
print(f'\n{"="*80}')
print(f'TOP 10 FILES BY OCCURRENCES:')
print(f'{"="*80}')
ranked = sorted(found.items(), key=lambda x: -len(x[1]))
for fname, issues in ranked[:10]:
    print(f'  {fname} : {len(issues)} occurrences')

print(f'\n{"="*80}')
print(f'TOTAL: {len(found)} files, {total_occ} occurrences')
