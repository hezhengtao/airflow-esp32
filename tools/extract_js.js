const fs = require('fs');
const lines = fs.readFileSync('d:/c/esp32/jhq/components/wifi_prov/wifi_prov.c', 'utf8').split('\n');

// Extract JS from lines 731-1149
let js = '';
for (let i = 730; i < 1149 && i < lines.length; i++) {
    const line = lines[i].trim();
    const matches = line.match(/"((?:[^"\\]|\\.)*)"/g);
    if (matches) {
        for (const m of matches) {
            let inner = m.slice(1, -1);
            // Process C escape sequences (as the C compiler would)
            inner = inner.replace(/\\"/g, '"');
            inner = inner.replace(/\\\\/g, '\\');
            inner = inner.replace(/\\n/g, '\n');
            inner = inner.replace(/\\t/g, '\t');
            js += inner;
        }
    }
}
fs.writeFileSync('d:/c/esp32/jhq/build/esp_js_gen2.js', js);
console.log('Written', js.length, 'chars');
