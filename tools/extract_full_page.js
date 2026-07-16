const fs = require('fs');
const lines = fs.readFileSync('d:/c/esp32/jhq/components/wifi_prov/wifi_prov.c', 'utf8').split('\n');

// Extract full main SPA page: lines 293-1151 (0-indexed: 292-1150)
let html = '';
for (let i = 292; i < 1151 && i < lines.length; i++) {
    const line = lines[i].trim();
    const matches = line.match(/"((?:[^"\\]|\\.)*)"/g);
    if (matches) {
        for (const m of matches) {
            html += m.slice(1, -1);
        }
    }
}
fs.writeFileSync('d:/c/esp32/jhq/build/full_page.html', html);
console.log('Written', html.length, 'chars');
console.log('Position 27643 context:', JSON.stringify(html.substring(27643 - 50, 27643 + 50)));
