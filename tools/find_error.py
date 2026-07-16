import re

with open('components/wifi_prov/wifi_prov.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

start_idx = None
for i, l in enumerate(lines):
    if 'HTML_NORMAL[]' in l and 'static const char' in l:
        start_idx = i
        break

parts = []
for i in range(start_idx + 1, len(lines)):
    stripped = lines[i].strip()
    if stripped == ';':
        break
    if stripped == '' or stripped.startswith('/*') or stripped.startswith('//'):
        continue
    m = re.match(r'^"((?:[^"\\]|\\.)*)"', stripped)
    if m:
        raw = m.group(1)
        s = raw.replace('\\"', '"').replace('\\n', '\n').replace('\\t', '\t').replace('\\\\', '\\')
        parts.append(s)
    else:
        break

html = ''.join(parts)
print(f'HTML length: {len(html)}')

pos = 52413
s1 = html.find('<script>')
s2 = html.find('</script>', s1 + 8)
print(f'Script at {s1} to {s2+9}')
print(f'Position {pos} is in script: {s1 < pos < s2}')

js_pos = pos - (s1 + 8)
print(f'JS position: {js_pos}')

ctx_start = max(0, pos - 200)
ctx_end = min(len(html), pos + 100)
ctx = html[ctx_start:ctx_end]
print(f'\nContext around position {pos}:')
print(ctx)

print(f'\nChar at {pos}: {repr(html[pos])}')
print(f'Chars around: {repr(html[pos-15:pos+15])}')

# Now look at what JS line contains this position
if s1 < pos < s2:
    js = html[s1+8:s2]
    # Find the line in the JS
    line_start = js.rfind('\n', 0, js_pos) + 1
    line_end = js.find('\n', js_pos)
    if line_end == -1:
        line_end = len(js)
    print(f'\nJS line containing error:')
    print(js[line_start:line_end])
