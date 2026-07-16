import re
import subprocess

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
        # convert C escape sequences
        s = raw.replace('\\"', '"').replace('\\n', '\n').replace('\\t', '\t').replace('\\\\', '\\')
        parts.append(s)
    else:
        print(f'STOP at line {i+1}: [{stripped[:80]}]')
        break

html = ''.join(parts)
print(f'HTML length: {len(html)}')

s1 = html.find('<script>')
s2 = html.find('</script>', s1 + 8)
if s1 >= 0 and s2 >= 0:
    js = html[s1+8:s2]
    print(f'JS starts at HTML pos {s1+8}, ends at {s2}, length {len(js)}')
    with open('/tmp/extracted.js', 'w', encoding='utf-8') as f:
        f.write(js)

    r = subprocess.run(['node', '--check', '/tmp/extracted.js'], capture_output=True, text=True)
    print(f'node --check rc={r.returncode}')
    if r.stderr:
        print(f'stderr: {r.stderr[:500]}')
    if r.stdout:
        print(f'stdout: {r.stdout[:500]}')
else:
    print(f'Script tag not found')
    print(f'HTML[:200]: {html[:200]}')
