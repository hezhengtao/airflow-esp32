"""Extract HTML_NORMAL and validate JS syntax."""
import re
import subprocess

with open('components/wifi_prov/wifi_prov.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find start
start_idx = None
for i, l in enumerate(lines):
    if 'HTML_NORMAL[]' in l and 'static const char' in l:
        start_idx = i
        break

# Collect all parts of the HTML string
parts = []
for i in range(start_idx + 1, len(lines)):
    stripped = lines[i].strip()

    # End of array: standalone ;
    if stripped == ';':
        break

    # Skip empty / standalone comments
    if stripped == '':
        continue
    if (stripped.startswith('/*') and stripped.endswith('*/')):
        continue
    if stripped.startswith('//'):
        continue

    # Match C string literal, possibly with trailing C comment
    # Pattern: "content" then optional whitespace, optional /* comment */, optional ;
    pat = r'^"((?:[^"\\]|\\.)*)"\s*(?:/\*.*\*/\s*)?;?\s*$'
    m = re.match(pat, stripped)
    if m:
        raw = m.group(1)
        # Process C escape sequences
        result = []
        j = 0
        while j < len(raw):
            if raw[j] == '\\' and j + 1 < len(raw):
                nxt = raw[j + 1]
                if nxt == '"':
                    result.append('"')
                elif nxt == '\\':
                    result.append('\\')
                elif nxt == 'n':
                    result.append('\n')
                elif nxt == 't':
                    result.append('\t')
                else:
                    result.append(raw[j:j+2])
                j += 2
            else:
                result.append(raw[j])
                j += 1
        parts.append(''.join(result))
    else:
        print(f'STOP at line {i+1}: [{stripped[:100]}]')
        break

html = ''.join(parts)
print(f'HTML length: {len(html)} chars')
print(f'UTF-8 bytes: {len(html.encode("utf-8"))}')

# Find script
s1 = html.find('<script>')
s2 = html.find('</script>', s1 + 8)
print(f'\n<script> at {s1}, </script> at {s2+9}')
js = html[s1+8:s2]
print(f'JS length: {len(js)} chars')

# Write JS
with open('tools/extracted.js', 'w', encoding='utf-8') as f:
    f.write(js)

# Node check
r = subprocess.run(['node', '--check', 'tools/extracted.js'], capture_output=True, text=True)
print(f'\nnode --check: rc={r.returncode}')
if r.stderr:
    err = r.stderr.strip()
    print(f'Error: {err[:500]}')

    # Parse error location
    m = re.search(r'at .*:(\d+):(\d+)', err)
    if m:
        err_line = int(m.group(1))
        err_col = int(m.group(2))
        print(f'\nError at JS line {err_line}, col {err_col}')
        js_lines = js.split('\n')
        if err_line <= len(js_lines):
            line = js_lines[err_line - 1]
            print(f'Line: {line[:300]}')
            if err_col <= len(line):
                ctx = line[max(0,err_col-30):err_col+30]
                print(f'Around col {err_col}: ...{ctx}...')

    # Also look for position in the full HTML
    # The browser error was at HTML line 1, col 52413
    # In the HTML (with script tags), the position is offset by s1+8
    html_pos_52413 = 52413
    if html_pos_52413 < len(html):
        print(f'\nWhat is at HTML position {html_pos_52413}?')
        ctx_html = html[html_pos_52413-50:html_pos_52413+50]
        print(f'  Context: [{ctx_html}]')
        print(f'  Char: {repr(html[html_pos_52413])}')
elif r.stdout:
    print(f'stdout: {r.stdout[:200]}')
else:
    print('JS syntax is VALID!')
