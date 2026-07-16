#!/usr/bin/env python3
"""Insert 总装原理图 as section 4.5.3 in the HTML guide."""

diagram = r'''<h4>4.5.3 总装原理图 — 系统全貌 (中学物理书风格)</h4>
<p style="color:#64748b; margin:-8px 0 24px 0; font-size:15px">将全部模块按实际物理位置排列，标注每个引脚的连接对象。对照此图即可完成全部接线。</p>

<div class="card">
<div class="svg-container">
<svg class="wiring-svg" viewBox="0 0 1600 1200" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <marker id="pB" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#2563eb"/></marker>
    <marker id="pG" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#16a34a"/></marker>
    <marker id="pY" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#eab308"/></marker>
    <marker id="pO" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#f97316"/></marker>
    <marker id="pP" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#ec4899"/></marker>
    <marker id="pR" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#dc2626"/></marker>
    <marker id="pGy" markerWidth="10" markerHeight="8" refX="10" refY="4" orient="auto"><polygon points="0 0, 10 4, 0 8" fill="#475569"/></marker>
    <filter id="psh"><feDropShadow dx="1" dy="3" stdDeviation="4" flood-opacity="0.1"/></filter>
    <filter id="psh2"><feDropShadow dx="2" dy="5" stdDeviation="5" flood-opacity="0.15"/></filter>
  </defs>

  <rect width="1600" height="1200" fill="#f8fafc"/>

  <!-- ═══════════════════ TITLE ═══════════════════ -->
  <text x="800" y="42" text-anchor="middle" font-size="24" font-weight="700" fill="#0f172a">总装原理图 — AiRFLOW 智能空气净化器系统全貌</text>
  <text x="800" y="70" text-anchor="middle" font-size="13" fill="#64748b">对照此图即可完成全部接线 · 杜邦线直连 · 无源元件串联焊线上 · 所有GND共地汇接</text>

  <!-- ═══════════════════ DPC3512 POWER SUPPLY (top-center) ═══════════════════ -->
  <rect x="400" y="92" width="800" height="90" rx="14" fill="#1e293b" stroke="#475569" stroke-width="3" filter="url(#psh2)"/>
  <text x="800" y="122" text-anchor="middle" fill="white" font-size="16" font-weight="700">DPC3512 四路开关电源 (AC 220V输入 → 4路DC输出)</text>
  <text x="800" y="146" text-anchor="middle" fill="#94a3b8" font-size="12">L(棕) N(蓝) PE(黄绿) │ 内置过流·过压·短路保护 │ 所有输出GND端子需汇接共地</text>

  <!-- Four channel outputs -->
  <rect x="420" y="158" width="170" height="30" rx="6" fill="#334155"/>
  <text x="505" y="178" text-anchor="middle" fill="#60a5fa" font-size="14" font-weight="700">CH1: 5V/3A → ESP32</text>
  <text x="505" y="194" text-anchor="middle" fill="#94a3b8" font-size="10">红线 USB-C供电</text>

  <rect x="610" y="158" width="170" height="30" rx="6" fill="#334155"/>
  <text x="695" y="178" text-anchor="middle" fill="#facc15" font-size="14" font-weight="700">CH2: 12V/2A → 电机</text>
  <text x="695" y="194" text-anchor="middle" fill="#94a3b8" font-size="10">红线 电机V+ ⚠反对烧!</text>

  <rect x="800" y="158" width="170" height="30" rx="6" fill="#334155"/>
  <text x="885" y="178" text-anchor="middle" fill="#60a5fa" font-size="14" font-weight="700">CH3: 5V/2A</text>
  <text x="885" y="194" text-anchor="middle" fill="#94a3b8" font-size="10">→ LCD + Y01 + PAM8302</text>

  <rect x="990" y="158" width="190" height="30" rx="6" fill="#334155"/>
  <text x="1085" y="178" text-anchor="middle" fill="#4ade80" font-size="14" font-weight="700">CH4: 3.3V/1A</text>
  <text x="1085" y="194" text-anchor="middle" fill="#f87171" font-size="10">⚠ 仅供PT1000分压!</text>

  <!-- ═══════════════════ POWER DISTRIBUTION LINES ═══════════════════ -->
  <!-- CH1 → ESP32 -->
  <line x1="505" y1="188" x2="505" y2="220" stroke="#dc2626" stroke-width="4"/>
  <line x1="505" y1="220" x2="800" y2="220" stroke="#dc2626" stroke-width="4"/>
  <line x1="800" y1="220" x2="800" y2="270" stroke="#dc2626" stroke-width="4" marker-end="url(#pR)"/>
  <circle cx="505" cy="220" r="5" fill="#dc2626"/>
  <circle cx="800" cy="220" r="5" fill="#dc2626"/>

  <!-- CH2 → Motor -->
  <line x1="695" y1="188" x2="695" y2="234" stroke="#dc2626" stroke-width="4"/>
  <line x1="695" y1="234" x2="260" y2="234" stroke="#dc2626" stroke-width="4"/>
  <line x1="260" y1="234" x2="260" y2="408" stroke="#dc2626" stroke-width="4" marker-end="url(#pR)"/>
  <circle cx="695" cy="234" r="5" fill="#dc2626"/>
  <circle cx="260" cy="234" r="5" fill="#dc2626"/>

  <!-- CH3 → LCD + Y01 + PAM8302 (distribution node) -->
  <line x1="885" y1="188" x2="885" y2="248" stroke="#dc2626" stroke-width="3.5"/>
  <circle cx="885" cy="248" r="5" fill="#dc2626"/>
  <!-- CH3 → LCD -->
  <line x1="885" y1="248" x2="1280" y2="248" stroke="#dc2626" stroke-width="3"/>
  <line x1="1280" y1="248" x2="1280" y2="320" stroke="#dc2626" stroke-width="3" marker-end="url(#pR)"/>
  <!-- CH3 → Y01 -->
  <line x1="885" y1="248" x2="1430" y2="248" stroke="#dc2626" stroke-width="3"/>
  <line x1="1430" y1="248" x2="1430" y2="540" stroke="#dc2626" stroke-width="3" marker-end="url(#pR)"/>
  <!-- CH3 → PAM8302 -->
  <line x1="885" y1="248" x2="885" y2="266" stroke="#dc2626" stroke-width="2.5"/>
  <line x1="885" y1="266" x2="400" y2="266" stroke="#dc2626" stroke-width="2.5"/>
  <line x1="400" y1="266" x2="400" y2="740" stroke="#dc2626" stroke-width="2.5" marker-end="url(#pR)"/>

  <!-- CH4 → PT1000 -->
  <line x1="1085" y1="188" x2="1085" y2="600" stroke="#16a34a" stroke-width="3"/>
  <line x1="1085" y1="600" x2="1430" y2="600" stroke="#16a34a" stroke-width="3"/>
  <line x1="1430" y1="600" x2="1430" y2="680" stroke="#16a34a" stroke-width="3" marker-end="url(#pG)"/>

  <!-- ═══════════════════ ESP32-S3 (center) ═══════════════════ -->
  <rect x="480" y="270" width="640" height="280" rx="18" fill="white" stroke="#6366f1" stroke-width="4" filter="url(#psh2)"/>
  <rect x="480" y="270" width="640" height="50" rx="18" fill="#4f46e5"/>
  <rect x="480" y="302" width="640" height="18" fill="#4f46e5"/>
  <text x="800" y="302" text-anchor="middle" fill="white" font-size="17" font-weight="700">ESP32-S3 N16R8 · WROOM-1 · 扩展底座引出全部GPIO</text>

  <text x="800" y="344" text-anchor="middle" fill="#64748b" font-size="12">供电: CH1 5V → USB-C | 板载LDO 3.3V | GND 共地汇接</text>

  <!-- LEFT: LCD data + control GPIOs -->
  <rect x="500" y="358" width="290" height="24" rx="5" fill="#eff6ff"/>
  <text x="645" y="375" text-anchor="middle" fill="#2563eb" font-size="12" font-weight="700">LCD 并行数据 + 控制 (GPIO4-16)</text>

  <g font-size="11">
    <text x="510" y="400" fill="#2563eb" font-weight="700">GPIO4-11</text>  <text x="640" y="400" fill="#475569">→ LCD D0-D7 (8位数据总线)</text>
    <text x="510" y="420" fill="#2563eb" font-weight="700">GPIO12</text>    <text x="640" y="420" fill="#475569">→ LCD CS (片选)</text>
    <text x="510" y="440" fill="#2563eb" font-weight="700">GPIO13</text>    <text x="640" y="440" fill="#475569">→ LCD RS (D/C 数据/命令)</text>
    <text x="510" y="460" fill="#2563eb" font-weight="700">GPIO14</text>    <text x="640" y="460" fill="#475569">→ LCD WR (写使能)</text>
    <text x="510" y="480" fill="#2563eb" font-weight="700">GPIO15</text>    <text x="640" y="480" fill="#475569">→ LCD RST (复位)</text>
    <text x="510" y="500" fill="#2563eb" font-weight="700">GPIO16</text>    <text x="640" y="500" fill="#475569">→ LCD BL (背光 PWM 5kHz)</text>
  </g>

  <!-- RIGHT: Touch I2C -->
  <rect x="500" y="516" width="290" height="24" rx="5" fill="#fff7ed"/>
  <text x="645" y="533" text-anchor="middle" fill="#f97316" font-size="12" font-weight="700">Touch I2C + 中断 (GPIO3,17,18,21)</text>

  <g font-size="11">
    <text x="510" y="558" fill="#f97316" font-weight="700">GPIO17</text> <text x="640" y="558" fill="#475569">↔ Touch SDA [R1 4.7kΩ ↑3.3V]</text>
    <text x="510" y="578" fill="#f97316" font-weight="700">GPIO18</text> <text x="640" y="578" fill="#475569">→ Touch SCL [R2 4.7kΩ ↑3.3V]</text>
    <text x="510" y="598" fill="#f97316" font-weight="700">GPIO3</text>  <text x="640" y="598" fill="#475569">← Touch INT (中断输入)</text>
    <text x="510" y="618" fill="#f97316" font-weight="700">GPIO21</text> <text x="640" y="618" fill="#475569">→ Touch RST (复位)</text>
  </g>

  <!-- RIGHT SIDE: Motor, Sensors, Audio GPIOs -->
  <rect x="810" y="358" width="290" height="24" rx="5" fill="#fefce8"/>
  <text x="955" y="375" text-anchor="middle" fill="#eab308" font-size="12" font-weight="700">电机控制 (GPIO38,39,40)</text>

  <g font-size="11">
    <text x="820" y="400" fill="#eab308" font-weight="700">GPIO38</text> <text x="950" y="400" fill="#475569">→ Motor START [R3 4.7kΩ↓GND]</text>
    <text x="820" y="420" fill="#eab308" font-weight="700">GPIO39</text> <text x="950" y="420" fill="#475569">→ Motor PWM (25kHz 调速)</text>
    <text x="820" y="440" fill="#eab308" font-weight="700">GPIO40</text> <text x="950" y="440" fill="#475569">← Motor FG (6脉冲/转)</text>
  </g>

  <rect x="810" y="458" width="290" height="24" rx="5" fill="#f0fdf4"/>
  <text x="955" y="475" text-anchor="middle" fill="#16a34a" font-size="12" font-weight="700">传感器 UART + ADC (GPIO2,41,42)</text>

  <g font-size="11">
    <text x="820" y="500" fill="#16a34a" font-weight="700">GPIO41</text> <text x="950" y="500" fill="#475569">→ Y01 RX ⚠UART交叉!</text>
    <text x="820" y="520" fill="#16a34a" font-weight="700">GPIO42</text> <text x="950" y="520" fill="#475569">← Y01 TX ⚠UART交叉!</text>
    <text x="820" y="540" fill="#16a34a" font-weight="700">GPIO2</text>  <text x="950" y="540" fill="#475569">← PT1000分压中点 [R4 1kΩ±0.1%]</text>
  </g>

  <rect x="810" y="556" width="290" height="24" rx="5" fill="#fdf2f8"/>
  <text x="955" y="573" text-anchor="middle" fill="#ec4899" font-size="12" font-weight="700">音频 PWM (GPIO48)</text>

  <g font-size="11">
    <text x="820" y="598" fill="#ec4899" font-weight="700">GPIO48</text> <text x="950" y="598" fill="#475569">→ [R5 100Ω + C5 10nF↓GND] → PAM8302 IN+</text>
  </g>

  <!-- GPIO color bars on left/right edges of chip -->
  <line x1="492" y1="396" x2="492" y2="506" stroke="#2563eb" stroke-width="5" stroke-linecap="round"/>
  <line x1="492" y1="554" x2="492" y2="624" stroke="#f97316" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="396" x2="1108" y2="446" stroke="#eab308" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="496" x2="1108" y2="546" stroke="#16a34a" stroke-width="5" stroke-linecap="round"/>
  <line x1="1108" y1="594" x2="1108" y2="604" stroke="#ec4899" stroke-width="5" stroke-linecap="round"/>

  <!-- ESP32 pinout info box -->
  <rect x="1128" y="270" width="72" height="42" rx="4" fill="#eff6ff" stroke="#93c5fd" stroke-width="1"/>
  <text x="1164" y="288" text-anchor="middle" fill="#2563eb" font-size="9" font-weight="700">USB-S/JTAG</text>
  <text x="1164" y="302" text-anchor="middle" fill="#475569" font-size="9">GPIO43=D-</text>
  <text x="1164" y="314" text-anchor="middle" fill="#475569" font-size="9">GPIO44=D+</text>
  <text x="1164" y="326" text-anchor="middle" fill="#dc2626" font-size="8">⚠勿占用</text>

  <!-- ═══════════════════ PERIPHERALS ═══════════════════ -->

  <!-- LCD Display (top-right) -->
  <rect x="1180" y="310" width="220" height="170" rx="14" fill="white" stroke="#2563eb" stroke-width="3" filter="url(#psh2)"/>
  <rect x="1180" y="310" width="220" height="36" rx="14" fill="#2563eb"/>
  <rect x="1180" y="332" width="220" height="14" fill="#2563eb"/>
  <text x="1290" y="334" text-anchor="middle" fill="white" font-size="13" font-weight="700">TK043F1509 显示屏</text>
  <g font-size="11">
    <text x="1196" y="372" fill="#2563eb" font-weight="700">D0-D7</text>  <text x="1320" y="372" fill="#475569">← GPIO4-11 (8线)</text>
    <text x="1196" y="394" fill="#2563eb" font-weight="700">CS</text>     <text x="1320" y="394" fill="#475569">← GPIO12</text>
    <text x="1196" y="416" fill="#2563eb" font-weight="700">RS</text>     <text x="1320" y="416" fill="#475569">← GPIO13</text>
    <text x="1196" y="438" fill="#2563eb" font-weight="700">WR</text>     <text x="1320" y="438" fill="#475569">← GPIO14</text>
    <text x="1196" y="460" fill="#2563eb" font-weight="700">RST</text>    <text x="1320" y="460" fill="#475569">← GPIO15</text>
    <text x="1196" y="482" fill="#2563eb" font-weight="700">BL</text>     <text x="1320" y="482" fill="#475569">← GPIO16 (PWM)</text>
  </g>
  <!-- C3 -->
  <rect x="1200" y="492" width="90" height="16" rx="3" fill="#faf5ff" stroke="#a855f7" stroke-width="1"/>
  <text x="1245" y="504" text-anchor="middle" fill="#7c3aed" font-size="9" font-weight="700">C3 100nF退耦</text>

  <!-- Touch controller (right of LCD) -->
  <rect x="1418" y="310" width="160" height="110" rx="10" fill="white" stroke="#f97316" stroke-width="2.5" filter="url(#psh)"/>
  <text x="1498" y="338" text-anchor="middle" fill="#f97316" font-size="11" font-weight="700">FT5x06 触摸</text>
  <g font-size="10">
    <text x="1430" y="362" fill="#f97316" font-weight="700">SDA</text> <text x="1510" y="362" fill="#475569">↔ GPIO17</text>
    <text x="1430" y="382" fill="#f97316" font-weight="700">SCL</text> <text x="1510" y="382" fill="#475569">← GPIO18</text>
    <text x="1430" y="402" fill="#f97316" font-weight="700">INT</text> <text x="1510" y="402" fill="#475569">→ GPIO3</text>
    <text x="1430" y="422" fill="#f97316" font-weight="700">RST</text> <text x="1510" y="422" fill="#475569">← GPIO21</text>
  </g>

  <!-- Motor (bottom-left) -->
  <rect x="60" y="400" width="280" height="200" rx="14" fill="white" stroke="#eab308" stroke-width="3" filter="url(#psh2)"/>
  <rect x="60" y="400" width="280" height="36" rx="14" fill="#eab308"/>
  <rect x="60" y="422" width="280" height="14" fill="#eab308"/>
  <text x="200" y="425" text-anchor="middle" fill="white" font-size="13" font-weight="700">48F704P840 直流无刷电机</text>
  <g font-size="11">
    <circle cx="80" cy="462" r="5" fill="#dc2626"/><text x="96" y="466" fill="#dc2626" font-weight="700">红线: +12V ← CH2</text>
    <circle cx="80" cy="486" r="5" fill="#475569"/><text x="96" y="490" fill="#475569" font-weight="700">黑线: GND (共地)</text>
    <circle cx="80" cy="510" r="5" fill="#eab308"/><text x="96" y="514" fill="#eab308" font-weight="700">黄线: START ← GPIO38</text>
    <circle cx="80" cy="534" r="5" fill="#16a34a"/><text x="96" y="538" fill="#16a34a" font-weight="700">绿线: PWM ← GPIO39</text>
    <circle cx="80" cy="558" r="5" fill="#2563eb"/><text x="96" y="562" fill="#2563eb" font-weight="700">蓝线: FG → GPIO40</text>
  </g>
  <!-- C6 + C2 on motor power -->
  <text x="80" y="590" fill="#a855f7" font-size="10" font-weight="700">⚠ 12V端子并联:</text>
  <rect x="72" y="596" width="256" height="18" rx="4" fill="#faf5ff" stroke="#a855f7" stroke-width="1.5"/>
  <text x="200" y="609" text-anchor="middle" fill="#7c3aed" font-size="10" font-weight="700">C6 470µF/25V 电解 (长脚=+) + C2 100nF 陶瓷</text>

  <!-- Y01 Air Quality Sensor (mid-right) -->
  <rect x="1180" y="520" width="220" height="130" rx="14" fill="white" stroke="#16a34a" stroke-width="3" filter="url(#psh)"/>
  <rect x="1180" y="520" width="220" height="32" rx="14" fill="#16a34a"/>
  <rect x="1180" y="538" width="220" height="14" fill="#16a34a"/>
  <text x="1290" y="542" text-anchor="middle" fill="white" font-size="12" font-weight="700">Y01 空气质量传感器</text>
  <g font-size="11">
    <text x="1196" y="574" fill="#dc2626" font-weight="700">VCC</text> <text x="1310" y="574" fill="#dc2626">+5V ← CH3</text>
    <text x="1196" y="596" fill="#475569" font-weight="700">GND</text> <text x="1310" y="596" fill="#475569">共地</text>
    <text x="1196" y="618" fill="#16a34a" font-weight="700">TX</text>  <text x="1310" y="618" fill="#16a34a">→ GPIO42 ⚠交叉</text>
    <text x="1196" y="640" fill="#16a34a" font-weight="700">RX</text>  <text x="1310" y="640" fill="#16a34a">← GPIO41 ⚠交叉</text>
  </g>
  <!-- C4 -->
  <rect x="1220" y="646" width="90" height="14" rx="3" fill="#faf5ff" stroke="#a855f7" stroke-width="1"/>
  <text x="1265" y="656" text-anchor="middle" fill="#7c3aed" font-size="9" font-weight="700">C4 100nF退耦</text>

  <!-- PT1000 Temperature Sensor (bottom-right) -->
  <rect x="1180" y="660" width="220" height="110" rx="14" fill="white" stroke="#16a34a" stroke-width="2.5" filter="url(#psh)"/>
  <rect x="1180" y="660" width="220" height="30" rx="14" fill="#16a34a"/>
  <rect x="1180" y="678" width="220" height="12" fill="#16a34a"/>
  <text x="1290" y="682" text-anchor="middle" fill="white" font-size="12" font-weight="700">PT1000 铂电阻 (A级 ±0.15°C)</text>
  <g font-size="10">
    <text x="1196" y="712" fill="#475569">分压电路:</text>
    <text x="1196" y="732" fill="#16a34a">CH4 3.3V → [R4 1kΩ±0.1%] → GPIO2(ADC) → PT1000 → GND</text>
    <text x="1196" y="754" fill="#f97316" font-size="9">R4用±0.1%金属膜电阻 · 不容替代!</text>
    <text x="1196" y="770" fill="#dc2626" font-size="9">⚠ CH4 3.3V 仅供PT1000 · 不可接ESP32!</text>
  </g>

  <!-- PAM8302 + Speaker (bottom-center-left) -->
  <rect x="320" y="730" width="360" height="160" rx="14" fill="white" stroke="#ec4899" stroke-width="3" filter="url(#psh2)"/>
  <rect x="320" y="730" width="360" height="36" rx="14" fill="#ec4899"/>
  <rect x="320" y="752" width="360" height="14" fill="#ec4899"/>
  <text x="500" y="754" text-anchor="middle" fill="white" font-size="13" font-weight="700">PAM8302 D类功放 + 8Ω/2W 扬声器</text>
  <g font-size="11">
    <text x="340" y="790" fill="#dc2626" font-weight="700">VDD</text>     <text x="420" y="790" fill="#dc2626">← +5V (CH3) 勿接12V!</text>
    <text x="340" y="814" fill="#475569" font-weight="700">GND</text>     <text x="420" y="814" fill="#475569">← 共地</text>
    <text x="340" y="838" fill="#ec4899" font-weight="700">IN+</text>     <text x="420" y="838" fill="#ec4899">← [R5 100Ω + C5 10nF↓GND] ← GPIO48</text>
    <text x="340" y="862" fill="#a855f7" font-weight="700">SD</text>      <text x="420" y="862" fill="#a855f7">← 3.3V 使能</text>
    <text x="340" y="886" fill="#475569" font-weight="700">OUT±</text>    <text x="420" y="886" fill="#475569">→ 8Ω/2W 喇叭 (BTL直驱)</text>
  </g>

  <!-- ═══════════════════ SIGNAL CONNECTION WIRES ═══════════════════ -->

  <!-- LCD Data Bus: GPIO4-11 → LCD D0-D7 -->
  <line x1="790" y1="406" x2="1190" y2="380" stroke="#2563eb" stroke-width="7" stroke-opacity="0.3"/>
  <line x1="790" y1="406" x2="1190" y2="380" stroke="#2563eb" stroke-width="2.5" marker-end="url(#pB)"/>
  <rect x="880" y="376" width="240" height="18" rx="3" fill="#eff6ff"/>
  <text x="1000" y="389" text-anchor="middle" fill="#2563eb" font-size="11" font-weight="700">蓝线×8: GPIO4↔D0 ... GPIO11↔D7</text>

  <!-- LCD Control: GPIO12-16 → LCD CS,RS,WR,RST,BL -->
  <line x1="790" y1="430" x2="1190" y2="430" stroke="#2563eb" stroke-width="4" stroke-opacity="0.25"/>
  <line x1="790" y1="430" x2="1190" y2="430" stroke="#2563eb" stroke-width="2" marker-end="url(#pB)"/>
  <rect x="900" y="422" width="230" height="16" rx="3" fill="#eff6ff"/>
  <text x="1015" y="434" text-anchor="middle" fill="#2563eb" font-size="10" font-weight="700">GPIO12-16 → CS·RS·WR·RST·BL</text>

  <!-- Touch I2C: GPIO17,18 ↔ Touch SDA,SCL -->
  <line x1="790" y1="565" x2="1428" y2="375" stroke="#f97316" stroke-width="3" stroke-opacity="0.3"/>
  <line x1="790" y1="565" x2="1428" y2="375" stroke="#f97316" stroke-width="2" marker-end="url(#pO)"/>
  <rect x="980" y="452" width="210" height="16" rx="3" fill="#fff7ed"/>
  <text x="1085" y="464" text-anchor="middle" fill="#f97316" font-size="10" font-weight="700">GPIO17,18 ↔ Touch SDA,SCL [R1,R2]</text>

  <!-- Touch INT, RST -->
  <line x1="790" y1="595" x2="1428" y2="415" stroke="#f97316" stroke-width="1.5" marker-end="url(#pO)"/>
  <line x1="790" y1="605" x2="1428" y2="407" stroke="#f97316" stroke-width="1.5"/>

  <!-- Motor START: GPIO38 → Motor START -->
  <line x1="1100" y1="406" x2="340" y2="476" stroke="#eab308" stroke-width="3" stroke-opacity="0.35"/>
  <line x1="1100" y1="406" x2="340" y2="476" stroke="#eab308" stroke-width="2" marker-end="url(#pY)"/>
  <rect x="600" y="430" width="220" height="16" rx="3" fill="#fefce8"/>
  <text x="710" y="442" text-anchor="middle" fill="#eab308" font-size="10" font-weight="700">GPIO38 → Motor START [R3 4.7kΩ↓GND]</text>

  <!-- Motor PWM: GPIO39 → Motor PWM -->
  <line x1="1100" y1="426" x2="340" y2="510" stroke="#16a34a" stroke-width="3" stroke-opacity="0.35"/>
  <line x1="1100" y1="426" x2="340" y2="510" stroke="#16a34a" stroke-width="2" marker-end="url(#pG)"/>
  <rect x="620" y="456" width="200" height="16" rx="3" fill="#f0fdf4"/>
  <text x="720" y="468" text-anchor="middle" fill="#16a34a" font-size="10" font-weight="700">GPIO39 → Motor PWM (25kHz)</text>

  <!-- Motor FG: GPIO40 ← Motor FG -->
  <line x1="340" y1="542" x2="1100" y2="446" stroke="#2563eb" stroke-width="2.5" stroke-opacity="0.35"/>
  <line x1="340" y1="542" x2="1100" y2="446" stroke="#2563eb" stroke-width="1.5" marker-end="url(#pB)"/>
  <rect x="640" y="478" width="210" height="16" rx="3" fill="#eff6ff"/>
  <text x="745" y="490" text-anchor="middle" fill="#2563eb" font-size="10" font-weight="700">Motor FG (6脉冲/转) → GPIO40</text>

  <!-- Y01 UART cross: GPIO41→Y01 RX, Y01 TX→GPIO42 -->
  <line x1="1100" y1="506" x2="1180" y2="620" stroke="#16a34a" stroke-width="2.5" marker-end="url(#pG)"/>
  <rect x="1080" y="558" width="170" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1165" y="570" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO41 → Y01 RX ⚠交叉</text>

  <line x1="1400" y1="636" x2="1100" y2="526" stroke="#16a34a" stroke-width="2.5" marker-end="url(#pG)"/>
  <rect x="1130" y="572" width="170" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1215" y="584" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">Y01 TX → GPIO42 ⚠交叉</text>

  <!-- PT1000 ADC: GPIO2 ← PT1000 divider midpoint -->
  <line x1="1100" y1="546" x2="1400" y2="730" stroke="#16a34a" stroke-width="2.5" marker-end="url(#pG)"/>
  <rect x="1240" y="636" width="200" height="16" rx="3" fill="#f0fdf4"/>
  <text x="1340" y="648" text-anchor="middle" fill="#16a34a" font-size="9" font-weight="700">GPIO2(ADC) ← PT1000分压中点</text>

  <!-- Audio: GPIO48 → RC filter → PAM8302 IN+ -->
  <line x1="1100" y1="604" x2="370" y2="830" stroke="#ec4899" stroke-width="3" stroke-opacity="0.35"/>
  <line x1="1100" y1="604" x2="370" y2="830" stroke="#ec4899" stroke-width="2" marker-end="url(#pP)"/>
  <rect x="600" y="708" width="380" height="16" rx="3" fill="#fdf2f8"/>
  <text x="790" y="720" text-anchor="middle" fill="#ec4899" font-size="10" font-weight="700">GPIO48 → [R5 100Ω 串联] → [C5 10nF↓GND 并联] → PAM8302 IN+</text>

  <!-- ═══════════════════ GND BUS (bottom) ═══════════════════ -->
  <rect x="30" y="920" width="1540" height="36" rx="8" fill="#f1f5f9" stroke="#94a3b8" stroke-width="2"/>
  <text x="800" y="944" text-anchor="middle" fill="#475569" font-size="14" font-weight="700">GND 共地汇接: DPC3512所有GND端子 · ESP32 GND · 电机GND · LCD GND · Y01 GND · PAM8302 GND · 喇叭GND</text>

  <!-- GND drop lines from components -->
  <line x1="260" y1="600" x2="260" y2="920" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="500" y1="890" x2="500" y2="920" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="800" y1="550" x2="800" y2="920" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="1290" y1="480" x2="1290" y2="920" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>
  <line x1="1430" y1="770" x2="1430" y2="920" stroke="#475569" stroke-width="2.5" stroke-dasharray="6 4"/>

  <!-- ═══════════════════ LEGEND (bottom-left) ═══════════════════ -->
  <rect x="30" y="970" width="780" height="110" rx="12" fill="white" stroke="#e2e8f0" stroke-width="2" filter="url(#psh)"/>
  <text x="420" y="998" text-anchor="middle" fill="#0f172a" font-size="14" font-weight="700">线色约定</text>
  <g font-size="11">
    <!-- Row 1 -->
    <line x1="50" y1="1020" x2="110" y2="1020" stroke="#dc2626" stroke-width="3"/><text x="120" y="1024" fill="#475569">红 — 正电源 (5V/12V/3.3V)</text>
    <line x1="300" y1="1020" x2="360" y2="1020" stroke="#475569" stroke-width="3"/><text x="370" y="1024" fill="#475569">黑/灰 — GND 共地</text>
    <line x1="530" y1="1020" x2="590" y2="1020" stroke="#2563eb" stroke-width="3"/><text x="600" y="1024" fill="#475569">蓝 — LCD数据/控制</text>
    <!-- Row 2 -->
    <line x1="50" y1="1044" x2="110" y2="1044" stroke="#f97316" stroke-width="3"/><text x="120" y="1048" fill="#475569">橙 — I2C触摸</text>
    <line x1="300" y1="1044" x2="360" y2="1044" stroke="#eab308" stroke-width="3"/><text x="370" y="1048" fill="#475569">黄 — 电机控制</text>
    <line x1="530" y1="1044" x2="590" y2="1044" stroke="#16a34a" stroke-width="3"/><text x="600" y="1048" fill="#475569">绿 — 传感器/UART</text>
    <line x1="720" y1="1044" x2="780" y2="1044" stroke="#ec4899" stroke-width="3"/><text x="790" y="1048" fill="#475569">粉 — 音频PWM</text>
  </g>

  <!-- ═══════════════════ SAFETY CHECKLIST (bottom-right) ═══════════════════ -->
  <rect x="830" y="970" width="740" height="110" rx="12" fill="#fef2f2" stroke="#dc2626" stroke-width="2.5" filter="url(#psh)"/>
  <text x="1200" y="998" text-anchor="middle" fill="#dc2626" font-size="14" font-weight="700">⚠ 上电前必查 (接错=烧模块!)</text>
  <g font-size="11">
    <text x="850" y="1022" fill="#dc2626">① 万用表蜂鸣档确认所有GND连通 (电源GND = ESP32 GND = 电机GND = LCD GND = Y01 GND = PAM8302 GND)</text>
    <text x="850" y="1042" fill="#dc2626">② 电机12V正负极确认 (红线→CH2+  黑线→GND)  470µF电解极性 (长脚+→12V  短脚-→GND)</text>
    <text x="850" y="1062" fill="#dc2626">③ CH4 3.3V仅供PT1000分压参考! 万用表确认未接入ESP32 3.3V引脚! 否则烧毁!</text>
    <text x="850" y="1082" fill="#dc2626">④ Y01 TX/RX 必须交叉 (GPIO41→Y01_RX  GPIO42←Y01_TX)  所有焊点套热缩管绝缘防短路</text>
  </g>

  <!-- Bottom note -->
  <text x="800" y="1185" text-anchor="middle" font-size="12" fill="#64748b">详细分步接线步骤见 4.1-4.4 │ 无源元件 (R1-R5, C1-C6) 串联焊在杜邦线上并套热缩管绝缘 │ 上电前用万用表逐项核对</text>

</svg>
</div>
</div>
'''

html_path = r'd:\c\esp32\jhq\docs\AirPurifier_Complete_Guide.html'

with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Insert before <h2>五、无源元件接线细节</h2>
marker = '<h2>五、无源元件接线细节</h2>'
pos = content.find(marker)
if pos == -1:
    print("ERROR: Could not find insertion point!")
    exit(1)

content = content[:pos] + diagram + '\n\n' + content[pos:]

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"Successfully inserted 总装原理图 as section 4.5.3 at position {pos}")
