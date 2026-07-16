const fs = require('fs');
const js = fs.readFileSync('d:/c/esp32/jhq/build/esp_js_gen2.js', 'utf8');

try {
    new Function(js);
    console.log('JS is valid');
} catch (e) {
    console.log('Error:', e.message);
    // Try to find the position
    if (e.stack) {
        const match = e.stack.match(/at position (\d+)/);
        if (match) {
            const pos = parseInt(match[1]);
            console.log('Position:', pos);
            console.log('Context before:', JSON.stringify(js.substring(Math.max(0, pos - 50), pos)));
            console.log('Context at:', JSON.stringify(js.substring(pos, pos + 20)));
            console.log('Context after:', JSON.stringify(js.substring(pos + 1, pos + 51)));
        }
    }
}
