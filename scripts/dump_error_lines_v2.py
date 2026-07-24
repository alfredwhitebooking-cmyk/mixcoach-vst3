"""
dump_error_lines_v2.py — Write lines 240-350.
"""

src = "Source/MixCoach/UI/NavigationShell.cpp"
out = "scripts/dump_error_lines_v2.txt"

with open(src, 'rb') as f:
    data = f.read()

lines = data.split(b'\r\n')
with open(out, 'w', encoding='utf-8') as f:
    for i in range(239, min(350, len(lines))):
        line = lines[i].decode('utf-8', errors='replace')
        f.write(f"Line {i+1}: {line}\n")

print(f"Dumped {min(350, len(lines)) - 239} lines to {out}")
