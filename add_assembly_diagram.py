import re

svg_diagram = r'''<!-- ═══════════════════════════════════════════════════════════ -->
<h2>总装接线图 — 一目了然</h2>

<div class="svg-container">
<svg class="wiring-svg" viewBox="0 0 1500 1150" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <marker id="ab" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#2563eb"/></marker>
    <marker id="ag" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#16a34a"/></marker>
    <marker id="ay" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#eab308"/></marker>
    <marker id="ao" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#f97316"/></marker>
    <marker id="ap" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#ec4899"/></marker>
    <marker id="av" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#7c3aed"/></marker>
    <filter id="sh"><feDropShadow dx="1" dy="2" stdDeviation="3" flood-opacity="0.08"/></filter>
    <filter id="sh2"><feDropShadow dx="1" dy="4" stdDeviation="4" flood-opacity="0.12"/></filter>
  </defs>

  <rect width="1500" height="1150" fill="#f8fafc"/>

  <!-- Title -->
  <text x="750" y="36" text-anchor="middle" font-size="22" font-weight="700" fill="#0f172a">总装接线图 — AiRFLOW 智能空气净化器</text>
  <text x="750" y="62" text-anchor="middle" font-size="13" fill="#64748b">全部杜邦线直连 · 无源元件串联焊接在线上 · 对照线色说明 │ ESP32-S3 + 扩展底座 · 所有GND共地</text>

  <!-- ═══════════════════ DPC3512 POWER SUPPLY (top) ═══════════════════ -->
  <rect x="350" y="84" width="800" height="78" rx="12" fill="white" stroke="#7c3aed" stroke-width="2.5" filter="url(#sh2)"/>
  <text x="750" y="112" text-anchor="middle" font-size="15" font-weight="700" fill="#1e293b">DPC3512 四路开关电源 (AC 220V → 4路DC输出 · 内置过载/短路保护)</text>

  <rect x="380" y="128" width="155" height="28" rx="5" fill="#faf5ff" stroke="#a855f7" stroke-width="1.5"/>
  <text x="457" y="147" text-anchor="middle" font-size="13" font-weight="700" fill="#7c3aed">CH1: 5V / 3A</text>

  <rect x="555" y="128" width="155" height="28" rx="5" fill="#fefce8" stroke="#eab308" stroke-width="2"/>
  <text x="632" y="147" text-anchor="middle" font-size="13" font-weight="700" fill="#ca8a04">CH2: 12V / 2A</text>

  <rect x="730" y="128" width="155" height="28" rx="5" fill="#faf5ff" stroke="#a855f7" stroke-width="1.5"/>
  <text x="807" y="147" text-anchor="middle" font-size="13" font-weight="700" fill="#7c3aed">CH3: 5V / 2A</text>

  <rect x="905" y="128" width="155" height="28" rx="5" fill="#f0fdf4" stroke="#16a34a" stroke-width="2"/>
  <text x="982" y="147" text-anchor="middle" font-size="13" font-weight="700" fill="#166534">CH4: 3.3V / 1A</text>

  <!-- Power labels below channels -->
  <text x="457" y="172" text-anchor="middle" font-size="10" fill="#64748b">→ ESP32 USB-C 供电</text>
  <text x="632" y="172" text-anchor="middle" font-size="10" fill="#ca8a04">→ 48F704P840 电机专用</text>
  <text x="807" y="172" text-anchor="middle" font-size="10" fill="#64748b">→ LCD屏幕 + Y01传感器 + PAM8302</text>
  <text x="982" y="172" text-anchor="middle" font-size="10" fill="#dc2626">⚠ 仅PT1000分压 · 勿接ESP32!</text>

  <!-- Power supply output lines down -->
  <line x1="457" y1="180" x2="457" y2="230" stroke="#7c3aed" stroke-width="1.5" stroke-dasharray="5 3"/>
  <line x1="632" y1="180" x2="632" y2="230" stroke="#eab308" stroke-width="1.5" stroke-dasharray="5 3"/>
  <line x1="807" y1="180" x2="807" y2="230" stroke="#7c3aed" stroke-width="1.5" stroke-dasharray="5 3"/>
  <line x1="982" y1="180" x2="982" y2="230" stroke="#16a34a" stroke-width="1.5" stroke-dasharray="5 3"/>

  <!-- ═══════════════════ ESP32-S3 (center) ═══════════════════ -->
  <rect x="220" y="230" width="1060" height="500" rx="18" fill="white" stroke="#6366f1" stroke-width="3" filter="url(#sh2)"/>

  <!-- ESP32 header -->
  <rect x="220" y="230" width="1060" height="46" rx="18" fill="#4f46e5"/>
  <rect x="220" y="258" width="1060" height="18" fill="#4f46e5"/>
  <text x="750" y="259" text-anchor="middle" font-size="16" font-weight="700" fill="white">ESP32-S3 N16R8 · WROOM-1 · 16MB Flash · 8MB PSRAM · 扩展底座引出全部GPIO</text>

  <!-- LEFT GPIO column: LCD data + control -->
  <rect x="240" y="290" width="440" height="22" rx="4" fill="#eff6ff"/>
  <text x="460" y="306" text-anchor="middle" font-size="12" font-weight="700" fill="#2563eb">LCD 并行数据 + 控制 (GPIO4-16)</text>

  <!-- LCD GPIOs - left side -->
  <g font-size="12">
    <text x="255" y="332" font-weight="700" fill="#2563eb">GPIO4</text><text x="420" y="332" fill="#475569">→ LCD D0 (bit0)</text>
    <text x="255" y="354" font-weight="700" fill="#2563eb">GPIO5</text><text x="420" y="354" fill="#475569">→ LCD D1 (bit1)</text>
    <text x="255" y="376" font-weight="700" fill="#2563eb">GPIO6</text><text x="420" y="376" fill="#475569">→ LCD D2 (bit2)</text>
    <text x="255" y="398" font-weight="700" fill="#2563eb">GPIO7</text><text x="420" y="398" fill="#475569">→ LCD D3 (bit3)</text>
    <text x="255" y="420" font-weight="700" fill="#2563eb">GPIO8</text><text x="420" y="420" fill="#475569">→ LCD D4 (bit4)</text>

    <text x="475" y="332" font-weight="700" fill="#2563eb">GPIO9</text> <text x="635" y="332" fill="#475569">→ LCD D5 (bit5)</text>
    <text x="475" y="354" font-weight="700" fill="#2563eb">GPIO10</text><text x="635" y="354" fill="#475569">→ LCD D6 (bit6)</text>
    <text x="475" y="376" font-weight="700" fill="#2563eb">GPIO11</text><text x="635" y="376" fill="#475569">→ LCD D7 (bit7) ⚠顺序!</text>

    <text x="255" y="450" font-weight="700" fill="#2563eb">GPIO12</text><text x="420" y="450" fill="#475569">→ LCD CS (片选)</text>
    <text x="475" y="450" font-weight="700" fill="#2563eb">GPIO13</text><text x="635" y="450" fill="#475569">→ LCD RS (D/C 数据/命令)</text>
    <text x="255" y="474" font-weight="700" fill="#2563eb">GPIO14</text><text x="420" y="474" fill="#475569">→ LCD WR (写使能)</text>
    <text x="475" y="474" font-weight="700" fill="#2563eb">GPIO15</text><text x="635" y="474" fill="#475569">→ LCD RST (复位)</text>
    <text x="255" y="498" font-weight="700" fill="#2563eb">GPIO16</text><text x="420" y="498" fill="#475569">→ LCD BL (背光 PWM 5kHz)</text>
  </g>

  <!-- Touch I2C section -->
  <rect x="240" y="518" width="440" height="22" rx="4" fill="#fff7ed"/>
  <text x="460" y="534" text-anchor="middle" font-size="12" font-weight="700" fill="#f97316">Touch I2C + 中断 (GPIO3,17,18,21)</text>

  <g font-size="12">
    <text x="255" y="558" font-weight="700" fill="#f97316">GPIO17</text><text x="420" y="558" fill="#475569">↔ Touch SDA [R1 4.7kΩ → 3.3V]</text>
    <text x="255" y="580" font-weight="700" fill="#f97316">GPIO18</text><text x="420" y="580" fill="#475569">→ Touch SCL [R2 4.7kΩ → 3.3V]</text>
    <text x="475" y="558" font-weight="700" fill="#f97316">GPIO3</text> <text x="635" y="558" fill="#475569">← Touch INT (中断信号)</text>
    <text x="475" y="580" font-weight="700" fill="#f97316">GPIO21</text><text x="635" y="580" fill="#475569">→ Touch RST (复位)</text>
  </g>

  <!-- RIGHT GPIO column: Motor, Sensors, Audio -->
  <rect x="720" y="290" width="540" height="22" rx="4" fill="#fefce8"/>
  <text x="990" y="306" text-anchor="middle" font-size="12" font-weight="700" fill="#eab308">电机控制 (GPIO38,39,40)</text>

  <rect x="720" y="354" width="540" height="22" rx="4" fill="#f0fdf4"/>
  <text x="990" y="370" text-anchor="middle" font-size="12" font-weight="700" fill="#16a34a">传感器 UART + ADC (GPIO2,41,42)</text>

  <rect x="720" y="440" width="540" height="22" rx="4" fill="#fdf2f8"/>
  <text x="990" y="456" text-anchor="middle" font-size="12" font-weight="700" fill="#ec4899">音频 PWM (GPIO48)</text>

  <g font-size="12">
    <!-- Motor -->
    <text x="735" y="332" font-weight="700" fill="#eab308">GPIO38</text><text x="920" y="332" fill="#475569">→ Motor START (启停) [R3 4.7kΩ↓GND]</text>
    <text x="735" y="398" font-weight="700" fill="#eab308">GPIO39</text><text x="920" y="398" fill="#475569">→ Motor PWM (25kHz 调速)</text>
    <text x="735" y="422" font-weight="700" fill="#eab308">GPIO40</text><text x="920" y="422" fill="#475569">← Motor FG (转速反馈 · 6脉冲/转)</text>

    <!-- Sensors -->
    <text x="735" y="482" font-weight="700" fill="#16a34a">GPIO41</text><text x="920" y="482" fill="#475569">→ Y01 RX ⚠UART交叉!</text>
    <text x="735" y="504" font-weight="700" fill="#16a34a">GPIO42</text><text x="920" y="504" fill="#475569">← Y01 TX ⚠UART交叉!</text>
    <text x="735" y="526" font-weight="700" fill="#16a34a">GPIO2</text> <text x="920" y="526" fill="#475569">← PT1000 分压中点 [R4 1kΩ±0.1%→3.3V]</text>

    <!-- Audio -->
    <text x="735" y="556" font-weight="700" fill="#ec4899">GPIO48</text><text x="920" y="556" fill="#475569">→ 音频 [R5 100Ω+C5 10nF↓GND] → PAM8302 IN+</text>
  </g>

  <!-- GPIO group indicator bars (left edge) -->
  <line x1="230" y1="328" x2="230" y2="504" stroke="#2563eb" stroke-width="4" stroke-linecap="round"/>
  <line x1="230" y1="522" x2="230" y2="586" stroke="#f97316" stroke-width="4" stroke-linecap="round"/>

  <!-- GPIO group indicator bars (right edge) -->
  <line x1="1270" y1="328" x2="1270" y2="338" stroke="#eab308" stroke-width="4" stroke-linecap="round"/>
  <line x1="1270" y1="394" x2="1270" y2="428" stroke="#eab308" stroke-width="4" stroke-linecap="round"/>
  <line x1="1270" y1="478" x2="1270" y2="532" stroke="#16a34a" stroke-width="4" stroke-linecap="round"/>
  <line x1="1270" y1="550" x2="1270" y2="562" stroke="#ec4899" stroke-width="4" stroke-linecap="round"/>

  <!-- ═══════════════════ PERIPHERALS ═══════════════════ -->

  <!-- LCD Display (right-top) -->
  <rect x="1310" y="240" width="170" height="290" rx="12" fill="white" stroke="#2563eb" stroke-width="2.5" filter="url(#sh2)"/>
  <rect x="1310" y="240" width="170" height="32" rx="12" fill="#2563eb"/>
  <rect x="1310" y="258" width="170" height="14" fill="#2563eb"/>
  <text x="1395" y="262" text-anchor="middle" font-size="12" font-weight="700" fill="white">LCD 4.3"</text>
  <g font-size="11">
    <text x="1324" y="296" fill="#2563eb" font-weight="700">D0-D7</text><text x="1410" y="296" fill="#475569">8线数据</text>
    <text x="1324" y="318" fill="#2563eb" font-weight="700">CS</text>   <text x="1410" y="318" fill="#475569">片选</text>
    <text x="1324" y="340" fill="#2563eb" font-weight="700">RS</text>   <text x="1410" y="340" fill="#475569">D/C</text>
    <text x="1324" y="362" fill="#2563eb" font-weight="700">WR</text>   <text x="1410" y="362" fill="#475569">写使能</text>
    <text x="1324" y="384" fill="#2563eb" font-weight="700">RST</text>  <text x="1410" y="384" fill="#475569">复位</text>
    <text x="1324" y="406" fill="#2563eb" font-weight="700">BL</text>   <text x="1410" y="406" fill="#475569">背光</text>
    <line x1="1324" y1="420" x2="1466" y2="420" stroke="#e2e8f0" stroke-width="1"/>
    <text x="1324" y="440" fill="#f97316" font-weight="700">SDA</text>  <text x="1410" y="440" fill="#f97316">I2C数据</text>
    <text x="1324" y="462" fill="#f97316" font-weight="700">SCL</text>  <text x="1410" y="462" fill="#f97316">I2C时钟</text>
    <text x="1324" y="484" fill="#475569" font-weight="700">INT</text>  <text x="1410" y="484" fill="#475569">触摸中断</text>
    <text x="1324" y="506" fill="#475569" font-weight="700">RST</text>  <text x="1410" y="506" fill="#475569">触摸复位</text>
  </g>
  <text x="1395" y="528" text-anchor="middle" font-size="9" fill="#64748b">VCC=5V · GND共地</text>

  <!-- Motor (left-bottom) -->
  <rect x="20" y="590" width="280" height="230" rx="12" fill="white" stroke="#eab308" stroke-width="2.5" filter="url(#sh2)"/>
  <rect x="20" y="590" width="280" height="32" rx="12" fill="#eab308"/>
  <rect x="20" y="608" width="280" height="14" fill="#eab308"/>
  <text x="160" y="612" text-anchor="middle" font-size="12" font-weight="700" fill="white">48F704P840 无刷电机</text>
  <g font-size="11">
    <circle cx="40" cy="650" r="5" fill="#dc2626"/><text x="54" y="654" fill="#dc2626" font-weight="700">红线: +12V (CH2)</text>
    <circle cx="40" cy="674" r="5" fill="#475569"/><text x="54" y="678" fill="#475569" font-weight="700">黑线: GND (共地)</text>
    <circle cx="40" cy="698" r="5" fill="#eab308"/><text x="54" y="702" fill="#eab308" font-weight="700">黄线: START → GPIO38</text>
    <circle cx="40" cy="722" r="5" fill="#16a34a"/><text x="54" y="726" fill="#16a34a" font-weight="700">绿线: PWM → GPIO39</text>
    <circle cx="40" cy="746" r="5" fill="#2563eb"/><text x="54" y="750" fill="#2563eb" font-weight="700">蓝线: FG ← GPIO40</text>
  </g>
  <!-- 470uF cap on motor power -->
  <text x="40" y="780" fill="#a855f7" font-weight="700" font-size="10">⚠ 12V端子并联:</text>
  <rect x="40" y="790" width="220" height="22" rx="4" fill="#faf5ff" stroke="#a855f7" stroke-width="1.5"/>
  <text x="150" y="806" text-anchor="middle" fill="#7c3aed" font-size="10" font-weight="700">C6 470µF/25V 电解 (长脚=+) + C2 100nF 陶瓷</text>

  <!-- Y01 Sensor (right-mid) -->
  <rect x="1310" y="560" width="170" height="140" rx="12" fill="white" stroke="#16a34a" stroke-width="2.5" filter="url(#sh)"/>
  <rect x="1310" y="560" width="170" height="28" rx="12" fill="#16a34a"/>
  <rect x="1310" y="576" width="170" height="12" fill="#16a34a"/>
  <text x="1395" y="580" text-anchor="middle" font-size="11" font-weight="700" fill="white">Y01 空气传感器</text>
  <g font-size="11">
    <text x="1324" y="610" fill="#dc2626" font-weight="700">VCC</text><text x="1410" y="610" fill="#dc2626">+5V (CH3)</text>
    <text x="1324" y="634" fill="#475569" font-weight="700">GND</text><text x="1410" y="634" fill="#475569">共地</text>
    <text x="1324" y="658" fill="#16a34a" font-weight="700">TX</text> <text x="1410" y="658" fill="#16a34a">→ GPIO42 ⚠交叉</text>
    <text x="1324" y="682" fill="#16a34a" font-weight="700">RX</text> <text x="1410" y="682" fill="#16a34a">← GPIO41 ⚠交叉</text>
  </g>

  <!-- PT1000 (right-bottom) -->
  <rect x="1310" y="720" width="170" height="100" rx="12" fill="white" stroke="#16a34a" stroke-width="2" filter="url(#sh)"/>
  <text x="1395" y="748" text-anchor="middle" font-size="11" font-weight="700" fill="#1e293b">PT1000 温度传感器</text>
  <text x="1395" y="768" text-anchor="middle" font-size="9" fill="#64748b">A级 ±0.15°C · 无极性</text>
  <text x="1324" y="792" fill="#16a34a" font-size="10">3.3V→[R4 1kΩ]→GPIO2→PT1000→GND</text>
  <text x="1324" y="812" fill="#f97316" font-size="9">R4用±0.1%金属膜 不容替代!</text>

  <!-- PAM8302 + Speaker (left-bottom below motor) -->
  <rect x="20" y="850" width="280" height="130" rx="12" fill="white" stroke="#ec4899" stroke-width="2.5" filter="url(#sh2)"/>
  <rect x="20" y="850" width="280" height="28" rx="12" fill="#ec4899"/>
  <rect x="20" y="866" width="280" height="12" fill="#ec4899"/>
  <text x="160" y="870" text-anchor="middle" font-size="11" font-weight="700" fill="white">PAM8302 功放 + 8Ω扬声器</text>
  <g font-size="10">
    <text x="34" y="902" fill="#dc2626" font-weight="700">VCC</text><text x="100" y="902" fill="#dc2626">+5V (CH3)</text>
    <text x="170" y="902" fill="#ec4899" font-weight="700">IN+</text><text x="240" y="902" fill="#ec4899">← RC滤波</text>
    <text x="34" y="926" fill="#475569" font-weight="700">GND</text><text x="100" y="926" fill="#475569">共地</text>
    <text x="170" y="926" fill="#a855f7" font-weight="700">SD</text><text x="240" y="926" fill="#a855f7">← 3.3V使能</text>
    <text x="34" y="950" fill="#475569" font-weight="700">OUT±</text><text x="100" y="950" fill="#475569">→ 8Ω/2W喇叭</text>
    <text x="170" y="950" fill="#64748b" font-size="9">BTL直驱 · 无需隔直电容</text>
  </g>

  <!-- ═══════════════════ CONNECTION WIRES ═══════════════════ -->

  <!-- LCD data bus: 8 blue lines from GPIO4-11 to LCD D0-D7 -->
  <line x1="700" y1="335" x2="1310" y2="300" stroke="#2563eb" stroke-width="6" stroke-opacity="0.35"/>
  <line x1="700" y1="335" x2="1310" y2="300" stroke="#2563eb" stroke-width="1.5" marker-end="url(#ab)"/>
  <rect x="900" y="302" width="310" height="18" rx="3" fill="#eff6ff"/>
  <text x="1055" y="315" text-anchor="middle" fill="#2563eb" font-size="11" font-weight="700">蓝线×8: GPIO4-11 ↔ LCD D0-D7</text>

  <!-- LCD control -->
  <line x1="700" y1="476" x2="1310" y2="340" stroke="#2563eb" stroke-width="5" stroke-opacity="0.3"/>
  <line x1="700" y1="476" x2="1310" y2="340" stroke="#2563eb" stroke-width="1.5" marker-end="url(#ab)"/>
  <rect x="900" y="358" width="340" height="16" rx="3" fill="#eff6ff"/>
  <text x="1070" y="370" text-anchor="middle" fill="#2563eb" font-size="10" font-weight="700">蓝线: CS·RS·WR·RST·BL → GPIO12-16</text>

  <!-- Touch I2C: orange -->
  <line x1="700" y1="565" x2="1310" y2="445" stroke="#f97316" stroke-width="4" stroke-opacity="0.35"/>
  <line x1="700" y1="565" x2="1310" y2="445" stroke="#f97316" stroke-width="1.5" marker-end="url(#ao)"/>
  <rect x="920" y="430" width="220" height="16" rx="3" fill="#fff7ed"/>
  <text x="1030" y="442" text-anchor="middle" fill="#f97316" font-size="10" font-weight="700">橙线: GPIO17,18 ↔ Touch SDA,SCL</text>

  <!-- Touch INT/RST -->
  <line x1="700" y1="585" x2="1310" y2="490" stroke="#f97316" stroke-width="1" marker-end="url(#ao)"/>

  <!-- Motor control: yellow -->
  <line x1="1300" y1="336" x2="300" y2="650" stroke="#eab308" stroke-width="3" stroke-opacity="0.4"/>
  <line x1="1300" y1="336" x2="300" y2="650" stroke="#eab308" stroke-width="1.5" marker-end="url(#ay)"/>
  <rect x="550" y="458" width="220" height="16" rx="3" fill="#fefce8"/>
  <text x="660" y="470" text-anchor="middle" fill="#eab308" font-size="10" font-weight="700">黄线: GPIO38 → Motor START</text>

  <line x1="1300" y1="402" x2="300" y2="720" stroke="#16a34a" stroke-width="3" stroke-opacity="0.4"/>
  <line x1="1300" y1="402" x2="300" y2="720" stroke="#16a34a" stroke-width="1.5" marker-end="url(#ag)"/>
  <rect x="580" y="492" width="220" height="16" rx="3" fill="#f0fdf4"/>
  <text x="690" y="504" text-anchor="middle" fill="#16a34a" font-size="10" font-weight="700">绿线: GPIO39 → Motor PWM</text>

  <line x1="300" y1="746" x2="1300" y2="426" stroke="#2563eb" stroke-width="2" stroke-opacity="0.4"/>
  <line x1="300" y1="746" x2="1300" y2="426" stroke="#2563eb" stroke-width="1.5" marker-end="url(#ab)"/>
  <rect x="610" y="528" width="220" height="16" rx="3" fill="#eff6ff"/>
  <text x="720" y="540" text-anchor="middle" fill="#2563eb" font-size="10" font-weight="700">蓝线: Motor FG → GPIO40</text>

  <!-- Y01 UART: green -->
  <line x1="1300" y1="486" x2="1480" y2="655" stroke="#16a34a" stroke-width="2" marker-end="url(#ag)"/>
  <rect x="1320" y="558" width="140" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1390" y="570" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO41→Y01 RX ⚠交叉</text>

  <line x1="1480" y1="680" x2="1300" y2="508" stroke="#16a34a" stroke-width="2" marker-end="url(#ag)"/>
  <rect x="1320" y="574" width="140" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1390" y="586" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">Y01 TX→GPIO42 ⚠交叉</text>

  <!-- PT1000 ADC -->
  <line x1="1300" y1="530" x2="1350" y2="760" stroke="#16a34a" stroke-width="2"/>
  <line x1="1350" y1="760" x2="1480" y2="760" stroke="#16a34a" stroke-width="2" marker-end="url(#ag)"/>
  <rect x="1370" y="740" width="120" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1430" y="752" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO2 ← PT1000</text>

  <!-- Audio PWM: pink -->
  <line x1="1300" y1="560" x2="300" y2="910" stroke="#ec4899" stroke-width="3" stroke-opacity="0.4"/>
  <line x1="1300" y1="560" x2="300" y2="910" stroke="#ec4899" stroke-width="1.5" marker-end="url(#ap)"/>
  <rect x="620" y="728" width="340" height="16" rx="3" fill="#fdf2f8"/>
  <text x="790" y="740" text-anchor="middle" fill="#ec4899" font-size="10" font-weight="700">粉线: GPIO48 → [R5 100Ω] → [C5 10nF↓GND] → PAM8302 IN+</text>

  <!-- ═══════════════════ LEGEND (bottom) ═══════════════════ -->
  <rect x="340" y="850" width="500" height="130" rx="10" fill="white" stroke="#e2e8f0" stroke-width="1.5" filter="url(#sh)"/>
  <text x="590" y="878" text-anchor="middle" fill="#0f172a" font-size="13" font-weight="700">线色约定</text>
  <g font-size="11">
    <line x1="360" y1="900" x2="430" y2="900" stroke="#2563eb" stroke-width="3"/><text x="442" y="904" fill="#475569">蓝 — LCD 数据/控制</text>
    <line x1="360" y1="922" x2="430" y2="922" stroke="#f97316" stroke-width="3"/><text x="442" y="926" fill="#475569">橙 — Touch I2C</text>
    <line x1="360" y1="944" x2="430" y2="944" stroke="#eab308" stroke-width="3"/><text x="442" y="948" fill="#475569">黄 — 电机控制</text>
    <line x1="570" y1="900" x2="640" y2="900" stroke="#16a34a" stroke-width="3"/><text x="652" y="904" fill="#475569">绿 — 传感器/UART</text>
    <line x1="570" y1="922" x2="640" y2="922" stroke="#ec4899" stroke-width="3"/><text x="652" y="926" fill="#475569">粉 — 音频 PWM</text>
    <line x1="570" y1="944" x2="640" y2="944" stroke="#a855f7" stroke-width="3"/><text x="652" y="948" fill="#475569">紫 — 控制/使能</text>
  </g>

  <!-- Quick reference box -->
  <rect x="860" y="850" width="340" height="130" rx="10" fill="#fef2f2" stroke="#dc2626" stroke-width="1.5" filter="url(#sh)"/>
  <text x="1030" y="878" text-anchor="middle" fill="#dc2626" font-size="13" font-weight="700">⚠ 必查清单</text>
  <g font-size="10">
    <text x="878" y="902" fill="#dc2626">① 所有GND共地: ESP32=电机=Y01=LCD=功放=电源GND</text>
    <text x="878" y="922" fill="#dc2626">② LCD D0-D7 数据线顺序不能错</text>
    <text x="878" y="942" fill="#dc2626">③ Y01 传感器UART TX/RX 必须交叉</text>
    <text x="878" y="962" fill="#dc2626">④ 电机红线→12V: 反接烧驱动板! GPIO48→RC滤波必须接!</text>
  </g>

  <!-- Bottom note -->
  <text x="750" y="1130" text-anchor="middle" font-size="12" fill="#64748b">各模块详细接线步骤见下文"四、完整接线步骤" │ 无源元件（R1-R5, C1-C6）串联焊在杜邦线上并套热缩管绝缘</text>

</svg>
</div>
'''

# Read the HTML file
with open(r'd:\c\esp32\jhq\docs\AirPurifier_Complete_Guide.html', 'r', encoding='utf-8') as f:
    content = f.read()

# Find insertion point: right before "三、系统供电架构"
insert_marker = '<h2>三、系统供电架构</h2>'
insert_pos = content.find(insert_marker)
if insert_pos == -1:
    print("ERROR: Could not find insertion point!")
    exit(1)

# Insert the diagram
content = content[:insert_pos] + svg_diagram + '\n\n' + content[insert_pos:]

with open(r'd:\c\esp32\jhq\docs\AirPurifier_Complete_Guide.html', 'w', encoding='utf-8') as f:
    f.write(content)

print("Successfully inserted assembly diagram!")
