"""
dump_error_lines.py — Write lines 590-660 of NavigationShell.cpp to a text file.
"""

src = "Source/MixCoach/UI/NavigationShell.cpp"
out = "scripts/dump_error_lines.txt"

with open(src, 'rb') as f:
    data = f.read()

lines = data.split(b'\r\n')
with open(out, 'w', encoding='utf-8') as f:
    for i in range(589, min(660, len(lines))):
        line = lines[i].decode('utf-8', errors='replace')
        f.write(f"Line {i+1}: {line}\n")

print(f"Dumped {min(660, len(lines)) - 589} lines to {out}")
print(f"Total lines in file: {len(lines)}")
