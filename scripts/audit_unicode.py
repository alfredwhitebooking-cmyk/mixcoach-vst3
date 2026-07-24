#!/usr/bin/env python3
"""Audit all .cpp/.h source files for Unicode outside Latin-1."""

import os, re, sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRS = ["Source"]
EXCLUDE_DIRS = [".freebuff", "build", "libs", "tests", "scripts", ".git"]
found_codepoints = {}

# Pattern for backslash-x hex sequences in C++ strings
ESC_PATTERN = re.compile(b'\\\\x[0-9A-Fa-f]{2}')

def parse_utf8_from_bytes(raw_bytes):
    """Try to decode raw bytes as UTF-8, return codepoints list."""
    try:
        text = raw_bytes.decode('utf-8')
        return [ord(c) for c in text]
    except:
        return []

def get_char_name(cp):
    try:
        import unicodedata
        return unicodedata.name(chr(cp), 'UNKNOWN')
    except:
        return 'UNKNOWN'

def scan_file(filepath):
    try:
        with open(filepath, 'rb') as f:
            content = f.read()
    except:
        return
    
    lines = content.split(b'\n')
    
    for line_num, line in enumerate(lines, 1):
        # Find positions of all \xNN sequences
        matches = list(ESC_PATTERN.finditer(line))
        if not matches:
            continue
        
        # Group consecutive \xNN sequences
        groups = []
        current_group = []
        for i, m in enumerate(matches):
            if not current_group:
                current_group.append(m)
            else:
                prev_end = current_group[-1].end()
                if m.start() == prev_end:
                    current_group.append(m)
                else:
                    groups.append(current_group)
                    current_group = [m]
        if current_group:
            groups.append(current_group)
        
        for group in groups:
            # Extract the hex bytes
            hex_bytes = b''
            for m in group:
                hex_str = line[m.start()+2:m.end()]  # skip \x
                hex_bytes += bytes.fromhex(hex_str.decode('ascii'))
            
            codepoints = parse_utf8_from_bytes(hex_bytes)
            for cp in codepoints:
                if cp > 0x00FF:
                    if cp not in found_codepoints:
                        found_codepoints[cp] = []
                    
                    ctx_start = max(0, group[0].start() - 30)
                    ctx_end = min(len(line), group[-1].end() + 50)
                    context = line[ctx_start:ctx_end].decode('utf-8', errors='replace').strip()
                    
                    found_codepoints[cp].append({
                        'file': str(filepath.relative_to(PROJECT_ROOT)),
                        'line': line_num,
                        'context': context,
                    })

def main():
    files_to_scan = []
    for src_dir in SOURCE_DIRS:
        src_path = PROJECT_ROOT / src_dir
        if not src_path.exists():
            print(f"  [WARN] Directory not found: {src_path}")
            continue
        for ext in ('*.cpp', '*.h'):
            files_to_scan.extend(src_path.rglob(ext))
    
    files_to_scan = [f for f in files_to_scan 
                     if not any(excl in f.parts for excl in EXCLUDE_DIRS)]
    
    print(f"Scanning {len(files_to_scan)} source files...\n")
    
    for filepath in files_to_scan:
        scan_file(filepath)
    
    if not found_codepoints:
        print("DONE - No Unicode outside Latin-1 found!")
        return
    
    print(f"\n{'='*70}")
    print(f"  FOUND {len(found_codepoints)} UNIQUE CODEPOINTS outside Latin-1")
    total_occ = sum(len(v) for v in found_codepoints.values())
    print(f"  TOTAL OCCURRENCES: {total_occ}")
    print(f"{'='*70}\n")
    
    # Group by category
    categories = {
        'Emoji (U+1F300+)': [],
        'Geometric (U+25A0-U+25FF)': [],
        'Dingbats (U+2700-U+27BF)': [],
        'Misc Symbols (U+2600-U+26FF)': [],
        'Other (U+2E00+, U+2000+)': [],
    }
    
    for cp, occurrences in sorted(found_codepoints.items()):
        name = get_char_name(cp)
        count = len(occurrences)
        
        if 0x1F300 <= cp <= 0x1FFFF:
            categories['Emoji (U+1F300+)'].append((cp, name, count, occurrences))
        elif 0x25A0 <= cp <= 0x25FF:
            categories['Geometric (U+25A0-U+25FF)'].append((cp, name, count, occurrences))
        elif 0x2700 <= cp <= 0x27BF:
            categories['Dingbats (U+2700-U+27BF)'].append((cp, name, count, occurrences))
        elif 0x2600 <= cp <= 0x26FF:
            categories['Misc Symbols (U+2600-U+26FF)'].append((cp, name, count, occurrences))
        else:
            categories['Other (U+2E00+, U+2000+)'].append((cp, name, count, occurrences))
    
    for cat_name, items in categories.items():
        if not items:
            continue
        print(f"\n  [{cat_name}] - {len(items)} codepoints")
        for cp, name, count, occurrences in sorted(items):
            char = chr(cp)
            print(f"\n    U+{cp:04X} {char}  <<{name}>>  x{count}")
            for occ in occurrences[:3]:
                print(f"      {occ['file']}:{occ['line']}  ...{occ['context']}...")
            if count > 3:
                print(f"      ... + {count-3} more")
    
    print(f"\n{'='*70}")
    print(f"  {len(found_codepoints)} codepoints to address")
    print(f"{'='*70}")

if __name__ == '__main__':
    main()
