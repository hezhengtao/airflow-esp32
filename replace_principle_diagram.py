#!/usr/bin/env python3
"""Rewrite 4.5.3 总装原理图 with orthogonal (横平竖直) routing."""

diagram = r'''<h4>4.5.3 总装原理图 — 系统全貌 (中学物理书风格 · 横平竖直走线)</h4>
<p style="color:#64748b; margin:-8px 0 24px 0; font-size:15px">全部模块按实际物理位置排列，标注每个引脚的连接对象，走线横平竖直。对照此图即可完成全部接线。</p>

<div class="card">
<div class="svg-container">
<svg class="wiring-svg" viewBox="0 0 1600 1250" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <marker id="qB" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#2563eb"/></marker>
    <marker id="qG" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#16a34a"/></marker>
    <marker id="qY" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#eab308"/></marker>
    <marker id="qO" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#f97316"/></marker>
    <marker id="qP" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#ec4899"/></marker>
    <marker id="qR" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#dc2626"/></marker>
    <marker id="qGy" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#475569"/></marker>
    <filter id="qsh"><feDropShadow dx="1" dy="3" stdDeviation="4" flood-opacity="0.1"/></filter>
    <filter id="qsh2"><feDropShadow dx="2" dy="5" stdDeviation="5" flood-opacity="0.15"/></filter>
  </defs>

  <rect width="1600" height="1250" fill="#f8fafc"/>

  <!-- ═══════ TITLE ═══════ -->
  <text x="800" y="42" text-anchor="middle" font-size="24" font-weight="700" fill="#0f172a">总装原理图 — AiRFLOW 智能空气净化器系统全貌</text>
  <text x="800" y="70" text-anchor="middle" font-size="13" fill="#64748b">走线横平竖直 · 杜邦线直连 · 无源元件串联焊线上 · 所有GND共地汇接</text>

  <!-- ═══════ DPC3512 POWER SUPPLY (top) ═══════ -->
  <rect x="350" y="90" width="900" height="100" rx="14" fill="#1e293b" stroke="#475569" stroke-width="3" filter="url(#qsh2)"/>
  <text x="800" y="120" text-anchor="middle" fill="white" font-size="17" font-weight="700">DPC3512 四路开关电源 (AC 220V → 4路DC · 过流/过压/短路保护)</text>
  <text x="800" y="146" text-anchor="middle" fill="#94a3b8" font-size="12">输入: L火线(棕) N零线(蓝) PE地线(黄绿) │ 所有输出GND端子汇接共地</text>

  <!-- CH1 -->
  <rect x="375" y="160" width="190" height="34" rx="6" fill="#334155"/>
  <text x="470" y="182" text-anchor="middle" fill="#60a5fa" font-size="14" font-weight="700">CH1: 5V/3A → ESP32</text>
  <!-- CH2 -->
  <rect x="587" y="160" width="190" height="34" rx="6" fill="#334155"/>
  <text x="682" y="182" text-anchor="middle" fill="#facc15" font-size="14" font-weight="700">CH2: 12V/2A → 电机</text>
  <!-- CH3 -->
  <rect x="798" y="160" width="190" height="34" rx="6" fill="#334155"/>
  <text x="893" y="182" text-anchor="middle" fill="#60a5fa" font-size="14" font-weight="700">CH3: 5V/2A</text>
  <!-- CH4 -->
  <rect x="1010" y="160" width="215" height="34" rx="6" fill="#334155"/>
  <text x="1117" y="182" text-anchor="middle" fill="#4ade80" font-size="14" font-weight="700">CH4: 3.3V/1A ⚠ PT1000</text>

  <!-- Power labels below channels -->
  <text x="470" y="210" text-anchor="middle" fill="#94a3b8" font-size="10">→ ESP32 USB-C 红线</text>
  <text x="682" y="210" text-anchor="middle" fill="#f87171" font-size="10">⚠ 反接烧驱动板!</text>
  <text x="893" y="210" text-anchor="middle" fill="#94a3b8" font-size="10">→ LCD + Y01 + PAM8302</text>
  <text x="1117" y="210" text-anchor="middle" fill="#f87171" font-size="10">⚠ 仅供PT1000分压!</text>

  <!-- ═══════ POWER WIRES (orthogonal) ═══════ -->

  <!-- CH1 → ESP32: down, right to ESP32 center -->
  <polyline points="470,194 470,230 800,230 800,270" fill="none" stroke="#dc2626" stroke-width="4" marker-end="url(#qR)"/>
  <circle cx="470" cy="230" r="4" fill="#dc2626"/>
  <circle cx="800" cy="230" r="4" fill="#dc2626"/>

  <!-- CH2 → Motor: down, left to motor -->
  <polyline points="682,194 682,242 260,242 260,408" fill="none" stroke="#dc2626" stroke-width="4" marker-end="url(#qR)"/>
  <circle cx="682" cy="242" r="4" fill="#dc2626"/>
  <circle cx="260" cy="242" r="4" fill="#dc2626"/>

  <!-- CH3 → distribution node, then 3 branches -->
  <polyline points="893,194 893,258" fill="none" stroke="#dc2626" stroke-width="3.5"/>
  <circle cx="893" cy="258" r="4" fill="#dc2626"/>
  <!-- CH3 → LCD (right then up) -->
  <polyline points="893,258 1300,258 1300,310" fill="none" stroke="#dc2626" stroke-width="3" marker-end="url(#qR)"/>
  <circle cx="1300" cy="258" r="3" fill="#dc2626"/>
  <!-- CH3 → Y01 (right branch from node) -->
  <polyline points="893,258 1435,258 1435,520" fill="none" stroke="#dc2626" stroke-width="3" marker-end="url(#qR)"/>
  <!-- CH3 → PAM8302 (left, then down) -->
  <polyline points="893,258 893,274 400,274 400,738" fill="none" stroke="#dc2626" stroke-width="2.5" marker-end="url(#qR)"/>
  <circle cx="400" cy="274" r="3" fill="#dc2626"/>

  <!-- CH4 → PT1000 -->
  <polyline points="1117,194 1117,610 1435,610 1435,680" fill="none" stroke="#16a34a" stroke-width="3.5" marker-end="url(#qG)"/>
  <circle cx="1117" cy="610" r="4" fill="#16a34a"/>
  <circle cx="1435" cy="610" r="4" fill="#16a34a"/>

  <!-- Power wire labels -->
  <rect x="540" y="220" width="130" height="18" rx="4" fill="#fef2f2"/>
  <text x="605" y="233" text-anchor="middle" fill="#dc2626" font-size="12" font-weight="700">+5V → ESP32</text>

  <rect x="330" y="232" width="130" height="18" rx="4" fill="#fef2f2"/>
  <text x="395" y="245" text-anchor="middle" fill="#dc2626" font-size="12" font-weight="700">+12V → 电机</text>

  <rect x="960" y="248" width="170" height="16" rx="4" fill="#fef2f2"/>
  <text x="1045" y="260" text-anchor="middle" fill="#dc2626" font-size="11" font-weight="700">+5V → LCD + Y01 + 功放</text>

  <rect x="1050" y="600" width="150" height="16" rx="4" fill="#f0fdf4"/>
  <text x="1125" y="612" text-anchor="middle" fill="#16a34a" font-size="11" font-weight="700">3.3V → PT1000分压</text>

  <!-- ═══════ ESP32-S3 (center) ═══════ -->
  <rect x="480" y="270" width="640" height="310" rx="18" fill="white" stroke="#6366f1" stroke-width="4" filter="url(#qsh2)"/>
  <rect x="480" y="270" width="640" height="50" rx="18" fill="#4f46e5"/>
  <rect x="480" y="302" width="640" height="18" fill="#4f46e5"/>
  <text x="800" y="302" text-anchor="middle" fill="white" font-size="17" font-weight="700">ESP32-S3 N16R8 · WROOM-1 · 扩展底座引出全部GPIO</text>

  <text x="800" y="344" text-anchor="middle" fill="#64748b" font-size="12">CH1 5V → USB-C供电 │ 板载LDO 3.3V │ GND共地汇接</text>

  <!-- LEFT SIDE: LCD data + control GPIOs -->
  <rect x="500" y="358" width="290" height="22" rx="4" fill="#eff6ff"/>
  <text x="645" y="374" text-anchor="middle" fill="#2563eb" font-size="12" font-weight="700">LCD 并行数据 + 控制 (GPIO4-16)</text>

  <g font-size="11">
    <text x="510" y="400" fill="#2563eb" font-weight="700">GPIO4-11</text>  <text x="640" y="400" fill="#475569">→ LCD D0-D7 (8位数据总线)</text>
    <text x="510" y="420" fill="#2563eb" font-weight="700">GPIO12</text>    <text x="640" y="420" fill="#475569">→ LCD CS (片选)</text>
    <text x="510" y="440" fill="#2563eb" font-weight="700">GPIO13</text>    <text x="640" y="440" fill="#475569">→ LCD RS (D/C 数据/命令)</text>
    <text x="510" y="460" fill="#2563eb" font-weight="700">GPIO14</text>    <text x="640" y="460" fill="#475569">→ LCD WR (写使能)</text>
    <text x="510" y="480" fill="#2563eb" font-weight="700">GPIO15</text>    <text x="640" y="480" fill="#475569">→ LCD RST (复位)</text>
    <text x="510" y="500" fill="#2563eb" font-weight="700">GPIO16</text>    <text x="640" y="500" fill="#475569">→ LCD BL (背光 PWM 5kHz)</text>
  </g>

  <!-- LEFT SIDE: Touch -->
  <rect x="500" y="516" width="290" height="22" rx="4" fill="#fff7ed"/>
  <text x="645" y="532" text-anchor="middle" fill="#f97316" font-size="12" font-weight="700">Touch I2C + 中断 (GPIO3,17,18,21)</text>

  <g font-size="11">
    <text x="510" y="556" fill="#f97316" font-weight="700">GPIO17</text> <text x="640" y="556" fill="#475569">↔ Touch SDA [R1 4.7kΩ↑3.3V]</text>
    <text x="510" y="576" fill="#f97316" font-weight="700">GPIO18</text> <text x="640" y="576" fill="#475569">→ Touch SCL [R2 4.7kΩ↑3.3V]</text>
    <text x="510" y="596" fill="#f97316" font-weight="700">GPIO3</text>  <text x="640" y="596" fill="#475569">← Touch INT (中断)</text>
    <text x="510" y="616" fill="#f97316" font-weight="700">GPIO21</text> <text x="640" y="616" fill="#475569">→ Touch RST (复位)</text>
  </g>

  <!-- RIGHT SIDE: Motor GPIOs -->
  <rect x="810" y="358" width="290" height="22" rx="4" fill="#fefce8"/>
  <text x="955" y="374" text-anchor="middle" fill="#eab308" font-size="12" font-weight="700">电机控制 (GPIO38,39,40)</text>

  <g font-size="11">
    <text x="820" y="400" fill="#eab308" font-weight="700">GPIO38</text> <text x="950" y="400" fill="#475569">→ Motor START [R3 4.7kΩ↓GND]</text>
    <text x="820" y="420" fill="#eab308" font-weight="700">GPIO39</text> <text x="950" y="420" fill="#475569">→ Motor PWM (25kHz 调速)</text>
    <text x="820" y="440" fill="#eab308" font-weight="700">GPIO40</text> <text x="950" y="440" fill="#475569">← Motor FG (6脉冲/转)</text>
  </g>

  <!-- RIGHT SIDE: Sensor GPIOs -->
  <rect x="810" y="458" width="290" height="22" rx="4" fill="#f0fdf4"/>
  <text x="955" y="474" text-anchor="middle" fill="#16a34a" font-size="12" font-weight="700">传感器 UART + ADC (GPIO2,41,42)</text>

  <g font-size="11">
    <text x="820" y="500" fill="#16a34a" font-weight="700">GPIO41</text> <text x="950" y="500" fill="#475569">→ Y01 RX ⚠UART交叉!</text>
    <text x="820" y="520" fill="#16a34a" font-weight="700">GPIO42</text> <text x="950" y="520" fill="#475569">← Y01 TX ⚠UART交叉!</text>
    <text x="820" y="540" fill="#16a34a" font-weight="700">GPIO2</text>  <text x="950" y="540" fill="#475569">← PT1000分压中点 [R4 1kΩ±0.1%]</text>
  </g>

  <!-- RIGHT SIDE: Audio -->
  <rect x="810" y="556" width="290" height="22" rx="4" fill="#fdf2f8"/>
  <text x="955" y="572" text-anchor="middle" fill="#ec4899" font-size="12" font-weight="700">音频 PWM (GPIO48)</text>

  <g font-size="11">
    <text x="820" y="598" fill="#ec4899" font-weight="700">GPIO48</text> <text x="950" y="598" fill="#475569">→ [R5 100Ω + C5 10nF↓GND] → PAM8302 IN+</text>
  </g>

  <!-- GPIO color indicator bars -->
  <line x1="492" y1="398" x2="492" y2="506" stroke="#2563eb" stroke-width="5" stroke-linecap="round"/>
  <line x1="492" y1="554" x2="492" y2="622" stroke="#f97316" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="398" x2="1108" y2="446" stroke="#eab308" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="498" x2="1108" y2="546" stroke="#16a34a" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="596" x2="1108" y2="604" stroke="#ec4899" stroke-width="5" stroke-linecap="round"/>

  <!-- USB-S/JTAG warning -->
  <rect x="1128" y="270" width="72" height="44" rx="4" fill="#eff6ff" stroke="#93c5fd" stroke-width="1"/>
  <text x="1164" y="288" text-anchor="middle" fill="#2563eb" font-size="9" font-weight="700">USB-S/JTAG</text>
  <text x="1164" y="302" text-anchor="middle" fill="#475569" font-size="9">GPIO43=D-</text>
  <text x="1164" y="314" text-anchor="middle" fill="#475569" font-size="9">GPIO44=D+</text>
  <text x="1164" y="326" text-anchor="middle" fill="#dc2626" font-size="8">⚠勿占用</text>

  <!-- ═══════ PERIPHERALS ═══════ -->

  <!-- LCD Display (top-right) -->
  <rect x="1200" y="310" width="210" height="180" rx="14" fill="white" stroke="#2563eb" stroke-width="3" filter="url(#qsh2)"/>
  <rect x="1200" y="310" width="210" height="36" rx="14" fill="#2563eb"/>
  <rect x="1200" y="332" width="210" height="14" fill="#2563eb"/>
  <text x="1305" y="334" text-anchor="middle" fill="white" font-size="13" font-weight="700">TK043F1509 显示屏</text>
  <g font-size="11">
    <text x="1216" y="372" fill="#2563eb" font-weight="700">D0-D7</text>  <text x="1330" y="372" fill="#475569">← GPIO4-11 (8线)</text>
    <text x="1216" y="394" fill="#2563eb" font-weight="700">CS</text>     <text x="1330" y="394" fill="#475569">← GPIO12</text>
    <text x="1216" y="416" fill="#2563eb" font-weight="700">RS</text>     <text x="1330" y="416" fill="#475569">← GPIO13</text>
    <text x="1216" y="438" fill="#2563eb" font-weight="700">WR</text>     <text x="1330" y="438" fill="#475569">← GPIO14</text>
    <text x="1216" y="460" fill="#2563eb" font-weight="700">RST</text>    <text x="1330" y="460" fill="#475569">← GPIO15</text>
    <text x="1216" y="482" fill="#2563eb" font-weight="700">BL</text>     <text x="1330" y="482" fill="#475569">← GPIO16 (PWM)</text>
  </g>
  <rect x="1220" y="494" width="90" height="16" rx="3" fill="#faf5ff" stroke="#a855f7" stroke-width="1"/>
  <text x="1265" y="506" text-anchor="middle" fill="#7c3aed" font-size="9" font-weight="700">C3 100nF退耦</text>

  <!-- Touch controller (right of LCD, narrower) -->
  <rect x="1430" y="310" width="150" height="120" rx="10" fill="white" stroke="#f97316" stroke-width="2.5" filter="url(#qsh)"/>
  <text x="1505" y="340" text-anchor="middle" fill="#f97316" font-size="11" font-weight="700">FT5x06 触摸</text>
  <g font-size="10">
    <text x="1442" y="364" fill="#f97316" font-weight="700">SDA</text> <text x="1520" y="364" fill="#475569">↔ GPIO17</text>
    <text x="1442" y="384" fill="#f97316" font-weight="700">SCL</text> <text x="1520" y="384" fill="#475569">← GPIO18</text>
    <text x="1442" y="404" fill="#f97316" font-weight="700">INT</text> <text x="1520" y="404" fill="#475569">→ GPIO3</text>
    <text x="1442" y="424" fill="#f97316" font-weight="700">RST</text> <text x="1520" y="424" fill="#475569">← GPIO21</text>
  </g>

  <!-- Motor (bottom-left) -->
  <rect x="70" y="400" width="290" height="240" rx="14" fill="white" stroke="#eab308" stroke-width="3" filter="url(#qsh2)"/>
  <rect x="70" y="400" width="290" height="36" rx="14" fill="#eab308"/>
  <rect x="70" y="422" width="290" height="14" fill="#eab308"/>
  <text x="215" y="425" text-anchor="middle" fill="white" font-size="13" font-weight="700">48F704P840 直流无刷电机</text>
  <g font-size="11">
    <circle cx="90" cy="468" r="5" fill="#dc2626"/><text x="106" y="472" fill="#dc2626" font-weight="700">红线: +12V ← CH2</text>
    <circle cx="90" cy="494" r="5" fill="#475569"/><text x="106" y="498" fill="#475569" font-weight="700">黑线: GND (共地)</text>
    <circle cx="90" cy="520" r="5" fill="#eab308"/><text x="106" y="524" fill="#eab308" font-weight="700">黄线: START ← GPIO38</text>
    <circle cx="90" cy="546" r="5" fill="#16a34a"/><text x="106" y="550" fill="#16a34a" font-weight="700">绿线: PWM ← GPIO39</text>
    <circle cx="90" cy="572" r="5" fill="#2563eb"/><text x="106" y="576" fill="#2563eb" font-weight="700">蓝线: FG → GPIO40</text>
  </g>
  <!-- C6 + C2 -->
  <text x="90" y="608" fill="#a855f7" font-size="10" font-weight="700">⚠ 12V端子并联:</text>
  <rect x="85" y="616" width="260" height="18" rx="4" fill="#faf5ff" stroke="#a855f7" stroke-width="1.5"/>
  <text x="215" y="629" text-anchor="middle" fill="#7c3aed" font-size="10" font-weight="700">C6 470µF/25V 电解 (长脚=+) + C2 100nF</text>

  <!-- Y01 Air Quality Sensor (mid-right) -->
  <rect x="1200" y="520" width="210" height="140" rx="14" fill="white" stroke="#16a34a" stroke-width="3" filter="url(#qsh)"/>
  <rect x="1200" y="520" width="210" height="32" rx="14" fill="#16a34a"/>
  <rect x="1200" y="538" width="210" height="14" fill="#16a34a"/>
  <text x="1305" y="542" text-anchor="middle" fill="white" font-size="12" font-weight="700">Y01 空气质量传感器</text>
  <g font-size="11">
    <text x="1216" y="576" fill="#dc2626" font-weight="700">VCC</text> <text x="1320" y="576" fill="#dc2626">+5V ← CH3</text>
    <text x="1216" y="598" fill="#475569" font-weight="700">GND</text> <text x="1320" y="598" fill="#475569">共地</text>
    <text x="1216" y="620" fill="#16a34a" font-weight="700">TX</text>  <text x="1320" y="620" fill="#16a34a">→ GPIO42 ⚠交叉</text>
    <text x="1216" y="642" fill="#16a34a" font-weight="700">RX</text>  <text x="1320" y="642" fill="#16a34a">← GPIO41 ⚠交叉</text>
  </g>
  <rect x="1220" y="648" width="90" height="14" rx="3" fill="#faf5ff" stroke="#a855f7" stroke-width="1"/>
  <text x="1265" y="658" text-anchor="middle" fill="#7c3aed" font-size="9" font-weight="700">C4 100nF退耦</text>

  <!-- PT1000 Temperature Sensor (bottom-right) -->
  <rect x="1200" y="680" width="210" height="110" rx="14" fill="white" stroke="#16a34a" stroke-width="2.5" filter="url(#qsh)"/>
  <rect x="1200" y="680" width="210" height="30" rx="14" fill="#16a34a"/>
  <rect x="1200" y="698" width="210" height="12" fill="#16a34a"/>
  <text x="1305" y="700" text-anchor="middle" fill="white" font-size="12" font-weight="700">PT1000 铂电阻 (A级 ±0.15°C)</text>
  <g font-size="10">
    <text x="1216" y="734" fill="#475569">分压电路:</text>
    <text x="1216" y="754" fill="#16a34a">CH4 3.3V → [R4 1kΩ±0.1%]</text>
    <text x="1216" y="770" fill="#16a34a">         → GPIO2(ADC) → PT1000 → GND</text>
    <text x="1216" y="786" fill="#dc2626" font-size="9">⚠ CH4 3.3V 仅供PT1000 · 不可接ESP32!</text>
  </g>

  <!-- PAM8302 + Speaker (bottom-center) -->
  <rect x="300" y="738" width="380" height="170" rx="14" fill="white" stroke="#ec4899" stroke-width="3" filter="url(#qsh2)"/>
  <rect x="300" y="738" width="380" height="36" rx="14" fill="#ec4899"/>
  <rect x="300" y="760" width="380" height="14" fill="#ec4899"/>
  <text x="490" y="762" text-anchor="middle" fill="white" font-size="13" font-weight="700">PAM8302 D类功放 + 8Ω/2W 扬声器</text>
  <g font-size="11">
    <text x="320" y="800" fill="#dc2626" font-weight="700">VDD</text>   <text x="400" y="800" fill="#dc2626">← +5V (CH3) 勿接12V!</text>
    <text x="320" y="824" fill="#475569" font-weight="700">GND</text>   <text x="400" y="824" fill="#475569">← 共地</text>
    <text x="320" y="848" fill="#ec4899" font-weight="700">IN+</text>   <text x="400" y="848" fill="#ec4899">← [R5 100Ω + C5 10nF↓GND] ← GPIO48</text>
    <text x="320" y="872" fill="#a855f7" font-weight="700">SD</text>    <text x="400" y="872" fill="#a855f7">← 3.3V 使能</text>
    <text x="320" y="896" fill="#475569" font-weight="700">OUT±</text>  <text x="400" y="896" fill="#475569">→ 8Ω/2W 喇叭 (BTL直驱)</text>
  </g>

  <!-- ═══════ SIGNAL WIRES (orthogonal routing) ═══════ -->

  <!-- Routing channels:
       Channel R1: x=1175 (right of ESP32, left of LCD/Y01)
       Channel L1: x=460  (left of ESP32)
       Channel L2: x=380  (left side, between motor & PAM8302)
       Channel BW: x=1165 (bottom wire corridor)
  -->

  <!-- LCD Data Bus (GPIO4-11 → LCD D0-D7): straight horizontal right -->
  <polyline points="1120,395 1200,395" fill="none" stroke="#2563eb" stroke-width="7" stroke-opacity="0.3"/>
  <polyline points="1120,395 1200,395" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#qB)"/>
  <rect x="1085" y="388" width="125" height="16" rx="3" fill="#eff6ff"/>
  <text x="1147" y="400" text-anchor="middle" fill="#2563eb" font-size="10" font-weight="700">GPIO4-11↔D0-D7</text>

  <!-- LCD CS (GPIO12 → LCD CS): horizontal -->
  <polyline points="1120,415 1200,415" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#qB)"/>

  <!-- LCD RS (GPIO13 → LCD RS): horizontal -->
  <polyline points="1120,435 1200,435" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#qB)"/>

  <!-- LCD WR,RST,BL: horizontal lines cluster -->
  <polyline points="1120,455 1200,455" fill="none" stroke="#2563eb" stroke-width="2" marker-end="url(#qB)"/>
  <polyline points="1120,475 1200,475" fill="none" stroke="#2563eb" stroke-width="2" marker-end="url(#qB)"/>
  <polyline points="1120,495 1200,495" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#qB)"/>

  <!-- LCD control bundle label -->
  <rect x="1090" y="448" width="105" height="16" rx="3" fill="#eff6ff"/>
  <text x="1142" y="460" text-anchor="middle" fill="#2563eb" font-size="9" font-weight="700">CS·RS·WR·RST·BL</text>

  <!-- Touch SDA (GPIO17 ↔ Touch SDA): right, up to touch -->
  <polyline points="1120,555 1175,555 1175,370 1430,370" fill="none" stroke="#f97316" stroke-width="2.5" marker-end="url(#qO)"/>
  <rect x="1125" y="548" width="130" height="14" rx="3" fill="#fff7ed"/>
  <text x="1190" y="558" text-anchor="middle" fill="#f97316" font-size="9" font-weight="700">GPIO17↔SDA</text>

  <!-- Touch SCL (GPIO18 → Touch SCL): right, up -->
  <polyline points="1120,575 1190,575 1190,387 1430,387" fill="none" stroke="#f97316" stroke-width="2.5" marker-end="url(#qO)"/>

  <!-- Touch INT (GPIO3 ← Touch INT): right, up -->
  <polyline points="1430,407 1205,407 1205,605 1120,605" fill="none" stroke="#f97316" stroke-width="2" marker-end="url(#qO)"/>

  <!-- Touch RST (GPIO21 → Touch RST): right, up -->
  <polyline points="1120,615 1210,615 1210,420 1430,420" fill="none" stroke="#f97316" stroke-width="2" marker-end="url(#qO)"/>

  <!-- Motor START (GPIO38 → Motor START): down, left to motor -->
  <polyline points="955,400 955,390 380,390 380,456 360,456" fill="none" stroke="#eab308" stroke-width="2.5" marker-end="url(#qY)"/>
  <rect x="860" y="384" width="100" height="14" rx="3" fill="#fefce8"/>
  <text x="910" y="394" text-anchor="middle" fill="#eab308" font-size="9" font-weight="700">GPIO38→START</text>

  <!-- Motor PWM (GPIO39 → Motor PWM): down, left -->
  <polyline points="955,420 955,410 460,410 460,534 360,534" fill="none" stroke="#16a34a" stroke-width="2.5" marker-end="url(#qG)"/>
  <rect x="865" y="404" width="95" height="14" rx="3" fill="#f0fdf4"/>
  <text x="912" y="414" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO39→PWM</text>

  <!-- Motor FG (GPIO40 ← Motor FG): left, up from motor -->
  <polyline points="360,560 460,560 460,444 955,444" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#qB)"/>
  <rect x="865" y="438" width="95" height="14" rx="3" fill="#eff6ff"/>
  <text x="912" y="448" text-anchor="middle" fill="#2563eb" font-size="9" font-weight="700">FG→GPIO40</text>

  <!-- Y01 UART: GPIO41 → Y01 RX (ESP32 right, down, to Y01) -->
  <polyline points="1120,505 1170,505 1170,625 1200,625" fill="none" stroke="#16a34a" stroke-width="2.5" marker-end="url(#qG)"/>
  <rect x="1085" y="498" width="100" height="14" rx="3" fill="#f0fdf4"/>
  <text x="1135" y="508" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO41→Y01RX</text>

  <!-- Y01 UART: Y01 TX → GPIO42 (from Y01, left into ESP32) -->
  <polyline points="1200,640 1160,640 1160,522 1120,522" fill="none" stroke="#16a34a" stroke-width="2.5" marker-end="url(#qG)"/>
  <rect x="1085" y="515" width="100" height="14" rx="3" fill="#f0fdf4"/>
  <text x="1135" y="525" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">Y01TX→GPIO42</text>

  <!-- PT1000 ADC (GPIO2): from ESP32 bottom-rt, down to PT1000 -->
  <polyline points="1120,545 1175,545 1175,750 1200,750" fill="none" stroke="#16a34a" stroke-width="2" marker-end="url(#qG)"/>
  <rect x="1130" y="538" width="130" height="14" rx="3" fill="#f0fdf4"/>
  <text x="1195" y="548" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO2←ADC</text>

  <!-- Audio GPIO48: from ESP32 right, down long channel, left to PAM8302 -->
  <polyline points="1120,598 1165,598 1165,852 680,852" fill="none" stroke="#ec4899" stroke-width="2.5" marker-end="url(#qP)"/>
  <rect x="820" y="842" width="260" height="16" rx="3" fill="#fdf2f8"/>
  <text x="950" y="854" text-anchor="middle" fill="#ec4899" font-size="10" font-weight="700">GPIO48 → RC滤波 → PAM8302 IN+</text>

  <!-- ═══════ GND BUS (bottom) ═══════ -->
  <rect x="30" y="940" width="1540" height="36" rx="8" fill="#f1f5f9" stroke="#94a3b8" stroke-width="2"/>
  <text x="800" y="964" text-anchor="middle" fill="#475569" font-size="14" font-weight="700">GND 共地汇接: DPC3512所有GND · ESP32 GND · 电机GND · LCD GND · Y01 GND · PAM8302 GND · 喇叭GND</text>

  <!-- GND vertical drops (dashed) -->
  <line x1="260" y1="640" x2="260" y2="940" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="490" y1="908" x2="490" y2="940" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="800" y1="580" x2="800" y2="940" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="1305" y1="490" x2="1305" y2="940" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="1435" y1="790" x2="1435" y2="940" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>

  <!-- ═══════ LEGEND ═══════ -->
  <rect x="30" y="990" width="820" height="120" rx="12" fill="white" stroke="#e2e8f0" stroke-width="2" filter="url(#qsh)"/>
  <text x="440" y="1022" text-anchor="middle" fill="#0f172a" font-size="14" font-weight="700">线色约定</text>
  <g font-size="11">
    <line x1="46" y1="1046" x2="106" y2="1046" stroke="#dc2626" stroke-width="3"/>
    <text x="114" y="1050" fill="#475569">红 — 正电源 (5V/12V/3.3V)</text>
    <line x1="296" y1="1046" x2="356" y2="1046" stroke="#475569" stroke-width="3"/>
    <text x="364" y="1050" fill="#475569">灰 — GND 共地</text>
    <line x1="526" y1="1046" x2="586" y2="1046" stroke="#2563eb" stroke-width="3"/>
    <text x="594" y="1050" fill="#475569">蓝 — LCD数据/控制</text>
    <line x1="46" y1="1072" x2="106" y2="1072" stroke="#f97316" stroke-width="3"/>
    <text x="114" y="1076" fill="#475569">橙 — I2C 触摸</text>
    <line x1="296" y1="1072" x2="356" y2="1072" stroke="#eab308" stroke-width="3"/>
    <text x="364" y="1076" fill="#475569">黄 — 电机控制</text>
    <line x1="526" y1="1072" x2="586" y2="1072" stroke="#16a34a" stroke-width="3"/>
    <text x="594" y="1076" fill="#475569">绿 — 传感器/UART</text>
    <line x1="46" y1="1098" x2="106" y2="1098" stroke="#ec4899" stroke-width="3"/>
    <text x="114" y="1102" fill="#475569">粉 — 音频 PWM</text>
  </g>

  <!-- ═══════ SAFETY CHECKLIST ═══════ -->
  <rect x="870" y="990" width="700" height="120" rx="12" fill="#fef2f2" stroke="#dc2626" stroke-width="2.5" filter="url(#qsh)"/>
  <text x="1220" y="1022" text-anchor="middle" fill="#dc2626" font-size="14" font-weight="700">⚠ 上电前必查 (接错=烧模块!)</text>
  <g font-size="11">
    <text x="890" y="1048" fill="#dc2626">① 万用表蜂鸣档确认所有GND连通</text>
    <text x="890" y="1068" fill="#dc2626">② 电机12V极性确认 (红线CH2+)  470µF电解极性 (长脚+→12V)</text>
    <text x="890" y="1088" fill="#dc2626">③ CH4 3.3V仅供PT1000! 未接入ESP32 3.3V引脚!</text>
    <text x="890" y="1108" fill="#dc2626">④ Y01 TX/RX必须交叉!  所有焊点套热缩管绝缘防短路!</text>
  </g>

  <!-- Bottom note -->
  <text x="800" y="1235" text-anchor="middle" font-size="12" fill="#64748b">详细分步接线步骤见 4.1-4.4 │ 无源元件 R1-R5 C1-C6 串联焊在杜邦线上并套热缩管绝缘 │ 上电前用万用表逐项核对</text>

</svg>
</div>
</div>
'''

html_path = r'd:\c\esp32\jhq\docs\AirPurifier_Complete_Guide.html'

with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Find the existing 4.5.3 section and replace it
old_start = content.find('<h4>4.5.3 总装原理图')
if old_start == -1:
    print("ERROR: Could not find existing 4.5.3!")
    exit(1)

# Find end of the old 4.5.3 card section: find </div>\n</div> after the old section
# Then find <h2>五、
next_h2 = content.find('<h2>五、', old_start)
if next_h2 == -1:
    print("ERROR: Could not find section 五!")
    exit(1)

# Replace from old 4.5.3 start to section 五
content = content[:old_start] + diagram + '\n\n' + content[next_h2:]

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"Successfully replaced 4.5.3 with orthogonal routing diagram")
