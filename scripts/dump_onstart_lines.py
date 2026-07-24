"""
dump_onstart_lines.py — Write lines 205-240 of NavigationShell.cpp to a text file.
"""

src = "Source/MixCoach/UI/NavigationShell.cpp"
out = "scripts/dump_onstart_lines.txt"

with open(src, 'rb') as f:
    data = f.read()

lines = data.split(b'\r\n')
with open(out, 'w', encoding='utf-8') as f:
    for i in range(204, min(240, len(lines))):
        line = lines[i].decode('utf-8', errors='replace')
        f.write(f"Line {i+1}: {line}\n")

print(f"Dumped {min(240, len(lines)) - 204} lines to {out}")
