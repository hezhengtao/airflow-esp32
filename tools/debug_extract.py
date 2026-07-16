"""Debug: find which lines don't match the extraction regex."""
import re

with open('components/wifi_prov/wifi_prov.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

start = None
for i, l in enumerate(lines):
    if 'HTML_NORMAL[]' in l and 'static const char' in l:
        start = i
        break

bad = []
for i in range(start + 1, len(lines)):
    s = lines[i].strip()
    if s == ';':
        break  # end of array
    if s == '' or s.startswith('/*') or s.startswith('//'):
        continue

    # Match C string literal: "content"
    # The content can contain any chars that are valid in C strings
    # including escaped sequences like \", \\, etc.
    pat = r'^"((?:[^"\\]|\\.)*)"\s*;?\s*$'
    m = re.match(pat, s)
    if not m:
        bad.append((i + 1, s[:120]))
        if len(bad) >= 20:
            break

print(f'Found {len(bad)} non-matching lines:')
for b in bad:
    print(f'  Line {b[0]}: [{b[1]}]')
