"""Extract HTML_NORMAL and find position of JS syntax error."""
import re

with open('components/wifi_prov/wifi_prov.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find start
start_idx = None
for i, l in enumerate(lines):
    if 'HTML_NORMAL[]' in l and 'static const char' in l:
        start_idx = i
        break

# Build C string literal content, line by line
parts = []
skipped = []
for i in range(start_idx + 1, len(lines)):
    stripped = lines[i].strip()

    # End of array
    if stripped == ';':
        break

    # Skip empty / comments
    if stripped == '' or stripped.startswith('/*') or stripped.startswith('//'):
        skipped.append((i+1, 'comment/empty'))
        continue

    # Must be a C string literal (possibly with trailing ;
    # Handle cases like: "content"; or just "content"
    # Match: optional whitespace, ", content, ", then optional ; and whitespace
    m = re.match(r'^"((?:[^"\\]|\\.)*)"\s*;?\s*$', stripped)
    if m:
        raw = m.group(1)
        # Process C escape sequences to get actual JS content
        # The key escapes in C strings are:
        # \" -> " (literal double quote in string)
        # \\ -> \ (literal backslash)
        # \n -> newline (not expected in these strings)
        # \t -> tab (not expected)

        # We need to be careful: in C source, \\ represents a single backslash
        # and \" represents a double quote.
        # But in the raw content from regex, we get what's between the outer quotes.
        # For example, C source: "ab\"cd" -> raw: ab\"cd -> actual string: ab"cd

        # Process backslash escapes
        result = []
        j = 0
        while j < len(raw):
            if raw[j] == '\\' and j + 1 < len(raw):
                next_c = raw[j + 1]
                if next_c == '"':
                    result.append('"')
                elif next_c == '\\':
                    result.append('\\')
                elif next_c == 'n':
                    result.append('\n')
                elif next_c == 't':
                    result.append('\t')
                else:
                    # Unknown escape, keep as-is
                    result.append(raw[j:j+2])
                j += 2
            else:
                result.append(raw[j])
                j += 1
        parts.append(''.join(result))
    else:
        skipped.append((i+1, f'NO MATCH: [{stripped[:80]}]'))
        break

html = ''.join(parts)
print(f'HTML length (chars): {len(html)}')
print(f'HTML length (UTF-8 bytes): {len(html.encode("utf-8"))}')
print(f'Skipped {len(skipped)} lines')
for s in skipped[:10]:
    print(f'  Line {s[0]}: {s[1]}')

# Find script
s1 = html.find('<script>')
s2 = html.find('</script>', s1 + 8)
if s1 >= 0 and s2 >= 0:
    print(f'\n<script> at HTML pos {s1}')
    print(f'</script> at HTML pos {s2+9}')
    print(f'Script content length: {s2 - (s1+8)} chars')

    # Extract JS (not including script tags)
    # The error position 52413 is in the FULL HTML served
    # Full HTML = html + possibly </script></body></html>
    # Let me check if html already contains these...
    if '</script>' in html:
        print('HTML already contains </script>')
    if '</body>' in html:
        print('HTML already contains </body>')
    if '</html>' in html:
        print('HTML already contains </html>')

    # What is at position 52413?
    pos = 52413
    if pos < len(html):
        print(f'\nCharacter at HTML[{pos}]: {repr(html[pos])}')
        print(f'Context HTML[{pos-100}:{pos+50}]:')
        print(html[pos-100:pos+50])
    else:
        print(f'\nPosition {pos} is beyond HTML length {len(html)}')
        print(f'Difference: {pos - len(html)} chars')

    # Write JS to temp file
    js = html[s1+8:s2]
    out_path = 'tools/extracted.js'
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(js)
    print(f'\nJS written to {out_path} ({len(js)} chars)')

    # Try node --check
    import subprocess
    r = subprocess.run(['node', '--check', out_path], capture_output=True, text=True)
    print(f'node --check: rc={r.returncode}')
    if r.stderr:
        # Parse line:col from error
        err = r.stderr.strip()
        print(f'Error: {err[:300]}')
        # Try to extract position info
        m = re.search(r'at .*:(\d+):(\d+)', err)
        if m:
            err_line = int(m.group(1))
            err_col = int(m.group(2))
            print(f'Error at line {err_line}, col {err_col}')
            js_lines = js.split('\n')
            if err_line <= len(js_lines):
                problem_line = js_lines[err_line - 1]
                print(f'Line content: {problem_line[:200]}')
                if err_col > 0 and err_col <= len(problem_line):
                    print(f'Cursor at col {err_col}: ...{problem_line[max(0,err_col-20):err_col]}<<HERE>>{problem_line[err_col:err_col+20]}...')
    if r.stdout:
        print(f'stdout: {r.stdout[:200]}')
else:
    print('Script tag not found in extracted HTML')
