#include "wifi_prov.h"
#include "board.h"
#include "settings.h"
#include "mqtt_ha.h"
#include "app_controller.h"
#include "ui/ui_screen_power.h"
#include "holiday/holiday_client.h"
#include "sensor_history.h"
#include "sensor_ds18b20.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/param.h>
#include <netdb.h>

static const char *TAG = "wifi_prov";

#define PROV_SOFTAP_SSID    "AiRFLOW"

static wifi_prov_state_t g_state = WIFI_PROV_STATE_IDLE;
static wifi_prov_state_cb_t g_state_cb = NULL;
static void *g_cb_user_data = NULL;
static char g_scan_cache[4096] = "[]";
static wifi_prov_sensor_t g_sensor = {0};
static bool g_http_normal_mode = false;

static void set_state(wifi_prov_state_t state)
{
    g_state = state;
    if (g_state_cb) g_state_cb(state, g_cb_user_data);
}

/* ═══════════════════════════════════════════════════════════════════
   DNS hijack — all queries resolve to 192.168.4.1
   ═══════════════════════════════════════════════════════════════════ */

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    uint8_t buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&from, &fromlen);
        if (len < 12) continue;

        buf[2]  = 0x81;   /* QR=1, RA=1 */
        buf[3]  = 0x80;
        buf[7]  = 0x01;   /* ANCOUNT = 1 */

        uint8_t *ans = buf + len;
        ans[0] = 0xC0; ans[1] = 0x0C;   /* NAME = pointer to query */
        ans[2] = 0x00; ans[3] = 0x01;   /* TYPE = A */
        ans[4] = 0x00; ans[5] = 0x01;   /* CLASS = IN */
        ans[6] = 0x00; ans[7] = 0x00;   /* TTL */
        ans[8] = 0x00; ans[9] = 0x3C;   /* TTL = 60s */
        ans[10]= 0x00; ans[11]= 0x04;    /* RDLENGTH = 4 */
        ans[12]= 192;  ans[13]= 168;     /* 192.168.4.1 */
        ans[14]= 4;    ans[15]= 1;

        sendto(sock, buf, len + 16, 0,
               (struct sockaddr *)&from, fromlen);
    }
}

/* ═══════════════════════════════════════════════════════════════════
   WiFi scan — cached once on startup
   ═══════════════════════════════════════════════════════════════════ */

static void scan_cache_refresh(void)
{
    /* Clear stale AP list, use default scan config for best compatibility */
    esp_wifi_clear_ap_list();

    /* Active scan with longer dwell: sends probe requests, better for phone hotspots */
    wifi_scan_config_t scan_cfg = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 120, .max = 360 } },
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) return;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) { esp_wifi_clear_ap_list(); return; }

    wifi_ap_record_t *aps = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (!aps) { esp_wifi_clear_ap_list(); return; }
    esp_wifi_scan_get_ap_records(&ap_count, aps);
    esp_wifi_clear_ap_list();  /* free driver buffer */

    /* Sort by RSSI */
    for (int i = 0; i < (int)ap_count - 1; i++) {
        for (int j = i + 1; j < (int)ap_count; j++) {
            if (aps[j].rssi > aps[i].rssi) {
                wifi_ap_record_t tmp = aps[i]; aps[i] = aps[j]; aps[j] = tmp;
            }
        }
    }

    int off = snprintf(g_scan_cache, sizeof(g_scan_cache), "[");
    for (int i = 0; i < (int)ap_count; i++) {
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (strcmp((char *)aps[i].ssid, (char *)aps[j].ssid) == 0) {
                dup = true; break;
            }
        }
        if (dup) continue;

        int pct = (aps[i].rssi + 100) * 2;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        char esc[37] = {0};
        const char *src = (const char *)aps[i].ssid;
        char *dst = esc;
        while (*src && (dst - esc) < 35) {
            if (*src == '\\') { *dst++ = '\\'; *dst++ = '\\'; }
            else if (*src == '"') { *dst++ = '\\'; *dst++ = '"'; }
            else *dst++ = *src;
            src++;
        }

        if (off < (int)sizeof(g_scan_cache) - 80) {
            off += snprintf(g_scan_cache + off, sizeof(g_scan_cache) - off,
                "%s{\"s\":\"%s\",\"r\":%d}", i > 0 ? "," : "", esc, pct);
        }
    }
    snprintf(g_scan_cache + off, sizeof(g_scan_cache) - off, "]");
    free(aps);
    ESP_LOGI(TAG, "WiFi scan cached: %d APs", ap_count);
}

/* ═══════════════════════════════════════════════════════════════════
   HTML page — WiFi config form
   ═══════════════════════════════════════════════════════════════════ */

/* WiFi provisioning page — Chinese, light theme, WiFi only */
static const char HTML_PAGE[] =
"<!DOCTYPE html><html><head>"
"<meta charset='utf-8' name='viewport' content='width=device-width,initial-scale=1'>"
"<title>AiRFLOW 配网</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC','Microsoft YaHei',sans-serif;"
"background:linear-gradient(180deg,#f0f7f4 0%,#e8f4f8 50%,#f5f9fc 100%);"
"color:#333;display:flex;flex-direction:column;align-items:center;gap:16px;"
"padding:24px 16px;min-height:100vh}"
".logo{width:64px;height:64px;margin-top:8px;animation:breathe 3s ease-in-out infinite}"
"@keyframes breathe{0%,100%{transform:scale(1);opacity:.85}50%{transform:scale(1.08);opacity:1}}"
".card{background:#fff;padding:28px 24px;border-radius:16px;width:100%;max-width:420px;"
"box-shadow:0 2px 16px rgba(0,0,0,.06)}"
"h2{text-align:center;margin-bottom:6px;color:#388e3c;font-weight:600;font-size:20px}"
".sub{text-align:center;color:#81a089;font-size:13px;margin-bottom:18px}"
/* ── Guide steps ── */
".guide{display:flex;gap:0;justify-content:center;margin-bottom:20px}"
".step{flex:1;text-align:center;position:relative;padding:0 4px}"
".step .n{width:28px;height:28px;border-radius:50%;background:#e8f5e9;color:#388e3c;"
"display:flex;align-items:center;justify-content:center;margin:0 auto 6px;"
"font-size:12px;font-weight:700}"
".step .t{font-size:11px;color:#81a089;line-height:1.3}"
".step-line{position:absolute;top:14px;left:60%;width:80%;height:1px;border-top:1.5px dashed #c8e6c9}"
".step:last-child .step-line{display:none}"
/* ── Form ── */
"label{display:block;margin-top:16px;font-size:13px;color:#5a7a60;font-weight:500}"
"input{width:100%;padding:11px 12px;margin-top:4px;border:1.5px solid #dce8df;"
"border-radius:10px;background:#f9fbf9;color:#333;font-size:15px;"
"transition:border-color .2s;outline:none}"
"input:focus{border-color:#66bb6a}"
".row{display:flex;gap:8px;margin-top:4px;align-items:center}"
".row input{flex:1}"
".row span{color:#66bb6a;font-size:22px;cursor:pointer;user-select:none;padding:4px}"
".row span:active{color:#388e3c}"
"#list{list-style:none;margin-top:6px;max-height:38vh;overflow-y:auto;"
"border:1.5px solid #dce8df;border-radius:10px;background:#f9fbf9}"
"#list li{padding:11px 14px;cursor:pointer;border-bottom:1px solid #eef5ef;"
"display:flex;justify-content:space-between;font-size:14px}"
"#list li:last-child{border-bottom:none}"
"#list li:hover{background:#e8f5e9}"
"#list li span.rssi{color:#81a089;font-size:12px;font-weight:500}"
"#list li.msg{color:#a0b8a5;cursor:default;justify-content:center;font-size:13px}"
"#list li.msg:hover{background:0}"
".tip{font-size:11px;color:#a0b8a5;text-align:center;margin-top:16px;line-height:1.6}"
"button[type=submit]{width:100%;margin-top:22px;padding:13px;"
"background:linear-gradient(135deg,#43a047,#66bb6a);color:#fff;"
"border:none;border-radius:10px;font-size:16px;font-weight:600;cursor:pointer;"
"box-shadow:0 2px 8px rgba(67,160,71,.25);transition:all .15s}"
"button[type=submit]:active{transform:scale(.97);box-shadow:0 1px 4px rgba(67,160,71,.2)}"
"#lang_btn{font-family:inherit}"
"</style></head><body>"
"<div style='position:fixed;top:12px;right:16px;z-index:99'>"
"<button id=lang_btn onclick='toggleLang()' style='padding:6px 12px;border:1.5px solid #dce8df;"
"border-radius:14px;background:#fff;color:#555;font-size:12px;cursor:pointer;box-shadow:0 1px 4px rgba(0,0,0,.06);font-weight:500'>EN</button>"
"</div>"
"<svg class=logo viewBox='0 0 64 64'><defs><radialGradient id='g'><stop offset='0%' stop-color='#81c784'/><stop offset='100%' stop-color='#66bb6a'/></radialGradient></defs>"
"<circle cx='32' cy='32' r='26' fill='url(#g)' opacity='.15'/>"
"<path d='M18 36 Q24 22 32 24 Q40 22 46 36 M24 36 Q28 30 32 31 Q36 30 40 36' stroke='#43a047' stroke-width='2.2' fill='none' stroke-linecap='round'/>"
"<path d='M22 40 Q28 34 32 35 Q36 34 42 40' stroke='#66bb6a' stroke-width='1.6' fill='none' stroke-linecap='round' opacity='.7'/>"
"</svg>"
/* ═══ Guide card: 3 steps ═══ */
"<div class=card style='padding:20px 16px'>"
"<h2 style='font-size:18px'>AiRFLOW 配网</h2><div class=sub>清新空气 智慧生活</div>"
"<div class=guide>"
"<div class=step><div class=step-line></div><div class=n>1</div><div class=t>手机连接<br>AiRFLOW 热点</div></div>"
"<div class=step><div class=step-line></div><div class=n>2</div><div class=t>选择你的<br>WiFi 网络</div></div>"
"<div class=step><div class=n>3</div><div class=t>输入密码<br>完成连接</div></div>"
"</div>"
"<div class=tip>&#9432; 手机通常会自动弹出此页面。<br>如未弹出，请在浏览器地址栏输入 <b>192.168.4.1</b></div>"
"</div>"
/* ═══ Form card ═══ */
"<form class=card action='/save' method='POST'>"
"<label>WiFi 名称</label><div class=row><input name='s' id='s' required placeholder='扫描中...' autocomplete='off'><span onclick='scan(true)'>&#x21bb;</span></div>"
"<ul id='list'><li class=msg>正在扫描...</li></ul>"
"<label>WiFi 密码</label><input name='p' type='password' placeholder='请输入WiFi密码'>"
"<button type='submit'>保存并连接</button>"
"<div class=tip>配置完成后，AiRFLOW 将自动连接你的 WiFi。设备屏幕会显示空气质量数据，通过滑动屏幕可切换不同页面。</div>"
"</form>"
"<script>"
"async function scan(force){"
"var uri=force?'/rescan':'/scan';"
"var l=document.getElementById('list');l.innerHTML='<li class=msg>扫描中...</li>';"
"try{"
"var r=await fetch(uri);var j=await r.json();"
"if(!j.length){l.innerHTML='<li class=msg>未发现 WiFi 网络</li>';return}"
"l.innerHTML=j.map(function(w){return '<li onclick=fill(this) data-s=\"'+w.s.replace(/\"/g,'&quot;')+'\">'+w.s+'<span class=rssi>'+w.r+'%</span></li>'}).join('');"
"}catch(e){l.innerHTML='<li class=msg>扫描失败，请手动输入</li>'}"
"}"
"function fill(li){document.getElementById('s').value=li.getAttribute('data-s');document.getElementById('list').style.display='none';}"
"scan();"
"</script>"
"</body></html>";

/* Post-save page — Chinese, simple, auto-restart */
static const char HTML_DONE[] =
"<!DOCTYPE html><html><head>"
"<meta charset='utf-8' name='viewport' content='width=device-width,initial-scale=1'>"
"<title>已保存 - AiRFLOW</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:linear-gradient(180deg,#f0f7f4 0%,#e8f4f8 50%,#f5f9fc 100%);"
"color:#333;display:flex;flex-direction:column;align-items:center;justify-content:center;"
"gap:20px;padding:24px 16px;min-height:100vh}"
".logo{width:56px;height:56px;animation:breathe 3s ease-in-out infinite}"
"@keyframes breathe{0%,100%{transform:scale(1);opacity:.85}50%{transform:scale(1.08);opacity:1}}"
".card{background:#fff;padding:32px 24px;border-radius:16px;width:100%;max-width:420px;"
"box-shadow:0 2px 16px rgba(0,0,0,.06);text-align:center}"
".check{width:48px;height:48px;margin:0 auto 16px;background:#e8f5e9;border-radius:50%;display:flex;align-items:center;justify-content:center}"
".check svg{width:24px;height:24px}"
"h2{color:#388e3c;font-size:20px;margin-bottom:8px}"
"p{color:#81a089;font-size:14px;line-height:1.8}"
".spinner{width:36px;height:36px;margin:20px auto 0;border:3px solid #dce8df;"
"border-top:3px solid #66bb6a;border-radius:50%;animation:spin .8s linear infinite}"
"@keyframes spin{to{transform:rotate(360deg)}}"
".tip{margin-top:16px;font-size:12px;color:#a0b8a5;line-height:1.6}"
"</style></head><body>"
"<svg class=logo viewBox='0 0 64 64'><defs><radialGradient id='g'><stop offset='0%' stop-color='#81c784'/><stop offset='100%' stop-color='#66bb6a'/></radialGradient></defs>"
"<circle cx='32' cy='32' r='26' fill='url(#g)' opacity='.15'/>"
"<path d='M18 36 Q24 22 32 24 Q40 22 46 36 M24 36 Q28 30 32 31 Q36 30 40 36' stroke='#43a047' stroke-width='2.2' fill='none' stroke-linecap='round'/>"
"</svg>"
"<div class=card>"
"<div class=check><svg viewBox='0 0 24 24' fill='none' stroke='#43a047' stroke-width='2.5'><polyline points='4 12 10 18 20 6'/></svg></div>"
"<h2>保存成功</h2>"
"<p>AiRFLOW 正在重启并连接 WiFi...</p>"
"<div class=spinner></div>"
"<div class=tip>若 AiRFLOW 热点重新出现，说明连接失败，<br>请重新连接热点配置</div>"
"</div>"
"</body></html>";

/* ── Normal-mode device dashboard ─────────────────────────────────
   Responsive · Animated · Card-based · Chinese
   Mobile / Tablet / Desktop  adaptive via CSS Grid
   ─────────────────────────────────────────────────────────────────── */
static const char HTML_NORMAL[] =
"<!DOCTYPE html><html lang='zh-CN'><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>"
"<title>AiRFLOW</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
":root{--green:#43a047;--green-lt:#66bb6a;--green-bg:#e8f5e9;"
"--bg1:#f0f7f4;--bg2:#e8f4f8;--bg3:#f5f9fc;--card:#fff;"
"--text:#1e2930;--text2:#5a7a60;--text3:#81a089;--border:#dce8df;"
"--shadow:0 2px 16px rgba(0,0,0,.06);"
"--radius:14px;--radius-sm:10px;--radius-xs:8px}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC','Microsoft YaHei',sans-serif;"
"background:linear-gradient(175deg,var(--bg1),var(--bg2) 40%,var(--bg3));"
"color:var(--text);min-height:100vh;padding:16px;"
"display:flex;flex-direction:column;align-items:center}"
".container{width:100%;max-width:1100px;display:flex;flex-direction:column;gap:14px}"

/* ── Header ────────────────────────────── */
".header{display:flex;align-items:center;gap:12px;padding:8px 0;animation:fadeDown .5s ease-out}"
".header svg{width:40px;height:40px;flex-shrink:0;animation:breathe 3s ease-in-out infinite}"
".header h1{font-size:22px;font-weight:700;color:#388e3c;letter-spacing:-.3px}"
".header .ver{font-size:11px;color:var(--text3);font-weight:400;margin-left:4px}"
"@keyframes fadeDown{from{opacity:0;transform:translateY(-12px)}to{opacity:1;transform:translateY(0)}}"
"@keyframes breathe{0%,100%{transform:scale(1);opacity:.85}50%{transform:scale(1.06);opacity:1}}"

/* ── Card base ────────────────────────── */
".card{background:var(--card);border-radius:var(--radius);"
"padding:20px 18px;box-shadow:var(--shadow);"
"animation:fadeUp .45s ease-out both;transition:box-shadow .3s}"
".card:hover{box-shadow:0 4px 24px rgba(0,0,0,.09)}"
"@keyframes fadeUp{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}"
".card-title{font-size:15px;font-weight:600;color:#388e3c;margin-bottom:14px;"
"display:flex;align-items:center;gap:8px}"
".card-title svg{width:20px;height:20px;flex-shrink:0}"
".holiday-badge{margin-left:10px;padding:2px 10px;border-radius:12px;font-size:12px;"
"background:linear-gradient(135deg,#ff6b6b,#feca57);color:#fff;font-weight:600;animation:pulse 2s infinite}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:.7}}"
".card-desc{font-size:11px;color:var(--text3);margin-top:-10px;margin-bottom:14px;line-height:1.5}"

/* ── Sensor grid (responsive) ─────────── */
".sensor-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}"
".s-cell{text-align:center;padding:14px 8px;border-radius:var(--radius-sm);"
"background:linear-gradient(180deg,#f9fcfa,#f2f8f3);"
"transition:transform .2s,box-shadow .2s;animation:fadeUp .45s ease-out both}"
".s-cell:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(67,160,71,.1)}"
".s-icon{width:28px;height:28px;margin:0 auto 6px;display:block}"
".s-value{font-size:26px;font-weight:700;color:var(--text);line-height:1.1;transition:color .3s}"
".s-unit{font-size:11px;color:var(--text3);margin-top:1px}"
".s-label{font-size:10px;color:#a0b8a5;margin-top:5px;letter-spacing:.5px}"
".s-value.warn{color:#f59e0b}.s-value.bad{color:#e53935}"

/* ── Fan control card ─────────────────── */
".fan-row{display:flex;align-items:center;gap:12px;flex-wrap:wrap}"
".fan-icon{width:44px;height:44px;flex-shrink:0;transition:transform .4s}"
".fan-icon.on{animation:fanSpin 1.5s linear infinite}"
"@keyframes fanSpin{to{transform:rotate(360deg)}}"
".fan-info{flex:1;min-width:160px}"
".fan-rpm{font-size:22px;font-weight:700;color:var(--text)}"
".fan-label{font-size:11px;color:var(--text3)}"
".fan-ctl{display:flex;align-items:center;gap:10px;width:100%;margin-top:6px}"
"input[type=range]{width:100%;height:5px;-webkit-appearance:none;appearance:none;"
"background:linear-gradient(90deg,var(--green-bg),var(--green));border-radius:3px;"
"outline:none;cursor:pointer;margin:4px 0}"
"input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;"
"width:22px;height:22px;background:#fff;border:2.5px solid var(--green);"
"border-radius:50%;cursor:pointer;box-shadow:0 2px 8px rgba(67,160,71,.25);transition:transform .15s}"
"input[type=range]::-webkit-slider-thumb:active{transform:scale(1.15)}"
".fan-ctl input[type=range]{flex:1}"
".fan-pct{font-size:16px;font-weight:700;color:var(--green);min-width:36px;text-align:center}"

/* ── Toggle switch ─────────────────────── */
".tgl{position:relative;display:inline-block;width:48px;height:28px;flex-shrink:0}"
".tgl input{opacity:0;width:0;height:0}"
".tgl .sl{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;"
"background:#d1d5db;border-radius:28px;transition:.3s}"
".tgl .sl:before{content:'';position:absolute;height:22px;width:22px;left:3px;bottom:3px;"
"background:#fff;border-radius:50%;transition:.3s;box-shadow:0 1px 3px rgba(0,0,0,.15)}"
".tgl input:checked+.sl{background:var(--green)}"
".tgl input:checked+.sl:before{transform:translateX(20px)}"

/* ── Info rows ─────────────────────────── */
".info-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:8px}"
".info-item{display:flex;justify-content:space-between;align-items:center;"
"padding:8px 12px;border-radius:var(--radius-xs);background:#f9fcfa;font-size:13px}"
".info-item .lbl{color:var(--text3);flex-shrink:0;margin-right:8px}"
".info-item .val{color:var(--text);text-align:right;word-break:break-all;font-weight:500}"
".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}"
".status-dot.ok{background:var(--green);box-shadow:0 0 6px var(--green);animation:pulse 2s ease-in-out infinite}"
".status-dot.err{background:#e53935;box-shadow:0 0 4px rgba(229,57,53,.4)}"
".val.ok{color:var(--green);font-weight:600}.val.err{color:#e53935;font-weight:600}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}"

/* ── MQTT config card ──────────────────── */
"input.cfg{width:100%;padding:10px 12px;border:1.5px solid var(--border);"
"border-radius:var(--radius-sm);background:#f9fbf9;color:var(--text);font-size:13px;"
"outline:none;transition:border-color .2s;margin-bottom:10px}"
"input.cfg:focus{border-color:var(--green-lt);box-shadow:0 0 0 3px rgba(102,187,106,.15)}"
"input.cfg::placeholder{color:#b0c0b3}"
".btn-row{display:flex;gap:8px;flex-wrap:wrap}"
"button{display:inline-flex;align-items:center;justify-content:center;gap:6px;"
"padding:10px 18px;border:none;border-radius:var(--radius-xs);font-size:13px;font-weight:600;"
"cursor:pointer;transition:all .2s;outline:none;white-space:nowrap}"
"button:active{transform:scale(.96)}"
".btn-primary{background:linear-gradient(135deg,var(--green),var(--green-lt));color:#fff;"
"box-shadow:0 2px 8px rgba(67,160,71,.25)}"
".btn-primary:hover{box-shadow:0 4px 14px rgba(67,160,71,.35)}"
".btn-ghost{background:var(--green-bg);color:#388e3c}"
".btn-ghost:hover{background:#d4edd6}"
".toast{text-align:center;font-size:12px;min-height:20px;margin-top:8px;"
"transition:color .3s;font-weight:500}"
".sched-stepper{display:flex;align-items:center;gap:0}"
".sched-stepper .sp-btn{width:28px;height:30px;border:1.5px solid var(--border);"
"background:var(--green-bg);color:#388e3c;font-size:14px;font-weight:700;"
"display:flex;align-items:center;justify-content:center;cursor:pointer;"
"transition:all .15s;outline:none;user-select:none;-webkit-user-select:none;"
"-webkit-tap-highlight-color:transparent;touch-action:manipulation}"
".sched-stepper .sp-btn:first-child{border-radius:var(--radius-xs) 0 0 var(--radius-xs);border-right:none}"
".sched-stepper .sp-btn:last-child{border-radius:0 var(--radius-xs) var(--radius-xs) 0;border-left:none}"
".sched-stepper .sp-btn:active{background:var(--green);color:#fff;transform:scale(.93)}"
".sched-stepper .sp-val{-webkit-appearance:none;appearance:none;text-align:center;"
"height:30px;min-width:42px;display:flex;align-items:center;justify-content:center;"
"border:1.5px solid var(--border);border-left:none;border-right:none;"
"background:#fff;font-size:14px;font-weight:600;color:var(--text);flex-shrink:0;outline:none;margin:0}"
".sched-stepper .sp-day .sp-val{min-width:56px;pointer-events:none}"
".sched-stepper input.sp-val:focus{border-color:var(--green-lt);box-shadow:0 0 0 3px rgba(102,187,106,.12)}"
/* Day preset buttons */
".dp-btn{padding:3px 8px;border:1.5px solid var(--border);border-radius:14px;"
"background:var(--green-bg);color:#388e3c;font-size:11px;font-weight:600;"
"cursor:pointer;transition:all .15s;outline:none;white-space:nowrap;"
"-webkit-tap-highlight-color:transparent;touch-action:manipulation}"
".dp-btn.active{background:var(--green);color:#fff;border-color:var(--green)}"
".dp-btn:active{transform:scale(.93)}"
/* Weekday toggle buttons */
".wd-btn{width:29px;height:26px;border:1.5px solid var(--border);border-radius:50%;"
"background:var(--green-bg);color:var(--text3);font-size:11px;font-weight:600;"
"cursor:pointer;transition:all .15s;outline:none;display:flex;"
"align-items:center;justify-content:center;"
"-webkit-tap-highlight-color:transparent;touch-action:manipulation}"
".wd-btn.active{background:var(--green);color:#fff;border-color:var(--green)}"
".wd-btn:active{transform:scale(.9)}"

/* ── Responsive: tablet ───────────────── */
"@media(min-width:640px){body{padding:22px 28px}.sensor-grid{grid-template-columns:repeat(4,1fr)}"
".card{padding:22px 20px}}"
/* ── Responsive: desktop ──────────────── */
"@media(min-width:1024px){body{padding:28px 36px}.container{gap:16px}}"
"#lang_btn{font-family:inherit}"
/* Modal overlay */
".modal-overlay{position:fixed;top:0;left:0;right:0;bottom:0;"
"background:rgba(0,0,0,.55);z-index:100;display:none;align-items:center;justify-content:center}"
".modal-overlay.show{display:flex}"
".modal-box{background:var(--card);border-radius:16px;padding:20px 16px;"
"width:95%;max-width:700px;max-height:85vh;display:flex;flex-direction:column;"
"box-shadow:0 8px 40px rgba(0,0,0,.25)}"
".modal-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}"
".modal-hdr h3{font-size:17px;color:var(--green);font-weight:600;margin:0}"
".modal-close{width:32px;height:32px;border-radius:50%;border:none;"
"background:var(--green-bg);color:var(--green);font-size:18px;cursor:pointer;"
"display:flex;align-items:center;justify-content:center}"
".modal-close:hover{background:#d4edd6}"
".range-btns{display:flex;gap:6px;margin-bottom:14px;flex-wrap:wrap}"
".r-btn{padding:6px 14px;border:1.5px solid var(--border);border-radius:16px;"
"background:var(--green-bg);color:#388e3c;font-size:12px;font-weight:600;"
"cursor:pointer;outline:none}"
".r-btn.active{background:var(--green);color:#fff;border-color:var(--green)}"
".chart-wrap{position:relative;width:100%;flex:1;min-height:280px}"
"@media(min-width:640px){.chart-wrap{min-height:340px}}"
"</style><script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script></head><body>"
"<div style='position:fixed;top:12px;right:16px;z-index:99'>"
"<button id=lang_btn onclick='toggleLang()' style='padding:6px 12px;border:1.5px solid #dce8df;"
"border-radius:14px;background:#fff;color:#555;font-size:12px;cursor:pointer;box-shadow:0 1px 4px rgba(0,0,0,.06);font-weight:500'>EN</button>"
"</div>"
"<div class=container>"

/* ═══ Header ═══ */
"<div class=header>"
"<svg viewBox='0 0 64 64'><defs><radialGradient id='hg'><stop offset='0%' stop-color='#81c784'/><stop offset='100%' stop-color='#66bb6a'/></radialGradient></defs>"
"<circle cx='32' cy='32' r='28' fill='url(#hg)' opacity='.12'/>"
"<path d='M16 34 Q22 18 32 20 Q42 18 48 34 M22 38 Q28 30 32 31 Q36 30 42 38' stroke='var(--green)' stroke-width='2.4' fill='none' stroke-linecap='round'/>"
"<path d='M20 42 Q28 36 32 37 Q36 36 44 42' stroke='var(--green-lt)' stroke-width='1.8' fill='none' stroke-linecap='round' opacity='.6'/>"
"</svg>"
"<h1>AiRFLOW</h1>"
"</div>"

/* ═══ Sensor Cards ═══ */
"<div class=card style='animation-delay:.05s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'><path d='M12 2a4 4 0 0 0-4 4v6a4 4 0 1 0 8 0V6a4 4 0 0 0-4-4Z'/><path d='M12 14v4'/><path d='M9 20h6'/></svg>"
"设备状态<span class='holiday-badge' id=holiday_badge style='display:none'></span>"
"</div>"
"<div class=card-desc>实时监测温度、TVOC、CO₂、甲醛</div>"
"<div class=sensor-grid>"
"<div class=s-cell style='animation-delay:.1s;cursor:pointer' onclick='openHistory(0)'>"
"<svg class=s-icon viewBox='0 0 24 24' fill='none' stroke='#f59e0b' stroke-width='2'><path d='M12 2a4 4 0 0 0-4 4v6a4 4 0 1 0 8 0V6a4 4 0 0 0-4-4Z'/><circle cx='12' cy='18' r='2' fill='#f59e0b'/></svg>"
"<div class=s-value id=temp>--.-</div><div class=s-unit>°C</div><div class=s-label>温度</div></div>"
"<div class=s-cell style='animation-delay:.15s;cursor:pointer' onclick='openHistory(1)'>"
"<svg class=s-icon viewBox='0 0 24 24' fill='none' stroke='#6366f1' stroke-width='2'><path d='M3 9h18M7 3v6m4-6v6m4-6v6m4-6v6'/><circle cx='6' cy='17' r='2'/><circle cx='14' cy='17' r='2'/><path d='M8 17h4'/></svg>"
"<div class=s-value id=tvoc>---</div><div class=s-unit>µg/m³</div><div class=s-label>TVOC</div></div>"
"<div class=s-cell style='animation-delay:.2s;cursor:pointer' onclick='openHistory(2)'>"
"<svg class=s-icon viewBox='0 0 24 24' fill='none' stroke='#0891b2' stroke-width='2'><circle cx='12' cy='12' r='8'/><path d='M8 12h8M12 8v8'/></svg>"
"<div class=s-value id=co2>---</div><div class=s-unit>ppm</div><div class=s-label>CO₂</div></div>"
"<div class=s-cell style='animation-delay:.25s;cursor:pointer' onclick='openHistory(3)'>"
"<svg class=s-icon viewBox='0 0 24 24' fill='none' stroke='#8b5cf6' stroke-width='2'><path d='M12 2L2 7l10 5 10-5-10-5Z'/><path d='M2 17l10 5 10-5'/><path d='M2 12l10 5 10-5'/></svg>"
"<div class=s-value id=ch2o>---</div><div class=s-unit>μg/m³</div><div class=s-label>甲醛</div></div>"
"</div></div>"

/* ═══ Alarm Threshold Card ═══ */
"<div class=card style='animation-delay:.28s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='#f59e0b' stroke-width='2'><path d='M15 6.5a3.5 3.5 0 1 0-7 0A3.5 3.5 0 0 0 11.5 10c.9 0 1.7-.3 2.3-.9l4.7 4.7'/><circle cx='6' cy='18' r='2'/><circle cx='16' cy='18' r='2'/></svg>"
"报警设置"
"</div>"
"<div class=card-desc>设定传感器报警值与自动风扇</div>"
"<div class=info-grid>"
"<div class=info-item><span class=lbl>超标自动开启风扇</span>"
"<label class=tgl><input type=checkbox id=af_chk checked><span class=sl></span></label></div>"
"</div>"
"<div style='margin-top:10px'>"
"<div class='s-label' style='margin-bottom:2px'>TVOC 阈值 <span id=tvoc_thr_val style='color:var(--green);font-weight:600'>500</span> µg/m³</div>"
"<input type=range id=tvoc_thr_sl min=100 max=2000 value=500 oninput='onThrChange()'>"
"<div class='s-label' style='margin-bottom:2px;margin-top:8px'>CO₂ 阈值 <span id=co2_thr_val style='color:var(--green);font-weight:600'>1000</span> ppm</div>"
"<input type=range id=co2_thr_sl min=400 max=5000 value=1000 oninput='onThrChange()'>"
"<div class='s-label' style='margin-bottom:2px;margin-top:8px'>甲醛 阈值 <span id=ch2o_thr_val style='color:var(--green);font-weight:600'>100</span> μg/m³</div>"
"<input type=range id=ch2o_thr_sl min=20 max=500 value=100 oninput='onThrChange()'>"
"<div class='s-label' style='margin-bottom:2px;margin-top:8px'>报警冷却 <span id=alarm_cd_val style='color:var(--green);font-weight:600'>60</span> 秒</div>"
"<input type=range id=alarm_cd_sl min=5 max=600 value=60 oninput=\"document.getElementById('alarm_cd_val').textContent=this.value\">"
"</div>"
"<button class=btn-primary style='margin-top:12px;width:100%' onclick='saveAlarm()'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"保存阈值</button>"
"<div class=toast id=alarm_res></div>"
"</div>"

/* ═══ Home Screen Card ═══ */
"<div class=card style='animation-delay:.29s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'><path d='M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'/><polyline points='9 22 9 12 15 12 15 22'/></svg>"
"自定义首页"
"</div>"
"<div class=card-desc>选择设备启动后默认显示的页面</div>"
"<div style='display:flex;align-items:center;gap:10px;flex-wrap:wrap'>"
"<span style='font-size:13px;color:var(--text3);white-space:nowrap'>启动后默认显示</span>"
"<select id=hs_sel style='flex:1;min-width:180px;padding:10px 12px;border:1.5px solid var(--border);"
"border-radius:var(--radius-sm);background:#f9fbf9;color:var(--text);font-size:13px;outline:none;"
"transition:border-color .2s;cursor:pointer' onfocus=\"this.style.borderColor='var(--green-lt)'\""
"onblur=\"this.style.borderColor='var(--border)'\">"
"<option value=0>首页</option>"
"<option value=1>网络设置</option>"
"<option value=4>风扇控制</option>"
"<option value=3>设置</option>"
"<option value=4>声音</option>"
"<option value=5>电源</option>"
"</select>"
"<button class=btn-primary onclick='saveHomeScreen()' style='flex-shrink:0'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"保存并重启</button>"
"</div>"
"<div class=toast id=hs_res style='margin-top:8px'></div>"
"</div>"

/* ═══ Fan Control Card ═══ */
"<div class=card style='animation-delay:.3s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'><circle cx='12' cy='12' r='2'/><path d='M12 2v4m0 12v4M2 12h4m12 0h4M5.6 5.6l2.8 2.8m7.2 7.2 2.8 2.8M5.6 18.4l2.8-2.8m7.2-7.2 2.8-2.8'/></svg>"
"风扇控制"
"</div>"
"<div class=card-desc>调节风速，开关风扇与自动模式</div>"
"<div class=fan-row>"
"<svg class='fan-icon' id=fanIco viewBox='0 0 48 48' fill='none' stroke='#43a047' stroke-width='2.5'>"
"<circle cx='24' cy='24' r='3' fill='#43a047'/><g stroke-linecap='round'>"
"<path d='M24 24V6'/><path d='M24 24l10-4'/><path d='M24 24l8 10'/><path d='M24 24l-10-4'/><path d='M24 24l-8 10'/>"
"</g></svg>"
"<div class=fan-info>"
"<div class=fan-rpm id=frpm2>-- 转/分</div>"
"<div class=fan-label id=fstLabel>待机中</div>"
"</div>"
"<label class=tgl><input type=checkbox id=fo><span class=sl></span></label>"
"</div>"
"<div class=fan-ctl>"
"<span style='font-size:12px;color:var(--text3)'>风速</span>"
"<input type=range id=fs min=1 max=100 value=50>"
"<span class=fan-pct id=fv>0%</span>"
"</div></div>"

/* ═══ Network Info Card ═══ */
"<div class=card style='animation-delay:.35s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'>"
"<path d='M1 8.5A15 15 0 0 1 12 3a15 15 0 0 1 11 5.5'/><path d='M4.5 12A10 10 0 0 1 12 7.5 10 10 0 0 1 19.5 12'/>"
"<path d='M8 15.5A6 6 0 0 1 12 13a6 6 0 0 1 4 2.5'/><circle cx='12' cy='19' r='2'/></svg>"
"网络信息"
"</div>"
"<div class=card-desc>查看WiFi连接与MQTT状态</div>"
"<div class=info-grid>"
"<div class=info-item><span class=lbl>WiFi</span><span class=val id=ni_ssid>--</span></div>"
"<div class=info-item><span class=lbl>IP 地址</span><span class=val id=ni_ip>--</span></div>"
"<div class=info-item><span class=lbl>MQTT 代理</span><span class=val id=ni_mqtt>--</span></div>"
"<div class=info-item><span class=lbl>MQTT 状态</span><span class='val err' id=ni_mqtts><span class='status-dot err'></span>未连接</span></div>"
"</div></div>"

/* ═══ MQTT Config Card ═══ */
"<div class=card style='animation-delay:.4s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'>"
"<rect x='2' y='4' width='20' height='14' rx='2'/><path d='M8 8h8M8 12h6M8 16h4'/></svg>"
"MQTT 配置"
"</div>"
"<div class=card-desc>设置MQTT代理地址，连接智能家居平台</div>"
"<input class=cfg id=mqtt_inp placeholder='mqtt://192.168.1.1:1883' autocomplete='off'>"
"<div class=btn-row>"
"<button class=btn-ghost onclick='testMqtt()'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><polyline points='4 12 10 18 20 6'/></svg>"
"测试连接</button>"
"<button class=btn-primary onclick='saveMqtt()'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"保存并重启</button>"
"</div>"
"<div class=toast id=mqtt_res></div>"
"</div>"

/* ═══ Status LED Card ═══ */
"<div class=card style='animation-delay:.42s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'><circle cx='12' cy='12' r='4'/><path d='M12 2v4m0 12v4M2 12h4m12 0h4'/></svg>"
"状态指示灯"
"</div>"
"<div class=card-desc>调节LED颜色、亮度与显示效果</div>"
"<div style='display:flex;align-items:center;gap:10px;margin-bottom:8px'>"
"<label class=tgl><input type=checkbox id=led_chk onchange='onLedToggle()'><span class=sl></span></label>"
"<span style='font-size:13px;color:var(--text3)'>开关</span>"
"<span class=toast id=led_res style='margin-top:0'></span>"
"</div>"
"<div>"
"<span style='font-size:12px;color:var(--text3)'>亮度 <b id=led_bri_val>100</b>%</span>"
"<input type=range id=led_bri min=1 max=100 value=100 oninput='onLedBri()' style='width:100%'>"
"</div>"
"<div style='border-top:1px solid var(--border);padding-top:12px;margin-top:8px'>"
"<span style='font-size:12px;color:var(--text3)'>各状态 LED 颜色</span>"
"</div>"
"<div style='display:flex;align-items:center;gap:8px;margin-top:8px;flex-wrap:wrap'>"
"<select id=ls_sel onchange='onLedStateSel()' style='flex:1;min-width:80px;padding:7px 6px;"
"border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;"
"color:var(--text);font-size:12px;outline:none'>"
"<option value=0>正常</option><option value=1>报警</option>"
"<option value=2>关机</option><option value=3>WiFi失败</option>"
"<option value=4>WiFi连接中</option></select>"
"<input type=color id=ls_pick value='#00FF00' onchange='onLedStatePick()' "
"style='width:36px;height:30px;border:1.5px solid var(--border);border-radius:var(--radius-xs);padding:1px;cursor:pointer;background:var(--surface);flex-shrink:0'>"
"<span id=ls_prev style='width:22px;height:22px;border-radius:50%;background:rgb(0,255,0);"
"border:1.5px solid #dce8df;flex-shrink:0'></span>"
"<select id=ls_eff onchange='onLedStateEff()' style='flex:1;min-width:60px;padding:7px 4px;"
"border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;"
"color:var(--text);font-size:12px;outline:none'>"
"<option value=0>常亮</option><option value=1>慢闪</option><option value=2>呼吸</option>"
"<option value=3>快闪</option><option value=4>彩虹</option>"
"</select>"
"</div></div>"

/* ═══ Sound Settings Card ═══ */
"<div class=card style='animation-delay:.44s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='var(--green)' stroke-width='2'><path d='M11 5L6 9H2v6h4l5 4V5z'/><path d='M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07'/></svg>"
"声音设置"
"</div>"
"<div class=card-desc>选择按键提示音、开关机音效与音量</div>"

"<div style='border-bottom:1px solid var(--border);padding-bottom:10px;margin-bottom:10px'>"
"<div style='display:flex;align-items:center;justify-content:space-between'>"
"<span style='font-size:13px;color:var(--text3);font-weight:500'>按键音</span>"
"<label class=tgl><input type=checkbox id=key_en_chk onchange='onSoundChange()'><span class=sl></span></label>"
"</div>"
"<div style='display:flex;align-items:center;gap:8px;margin-top:6px'>"
"<span style='font-size:11px;color:var(--text3);white-space:nowrap'>音量</span>"
"<input type=range id=key_vol_sl min=0 max=100 value=70 oninput='onKeyVolInput()' onchange='onSoundChange()' style='flex:1'>"
"<span id=key_vol_val style='font-size:13px;font-weight:600;color:var(--green);min-width:32px'>70%</span>"
"</div>"
"<div style='display:flex;align-items:center;gap:6px;margin-top:6px'>"
"<select id=key_mel_sel onchange='onSoundChange()' style='flex:1;padding:7px 8px;border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;color:var(--text);font-size:11px;outline:none;cursor:pointer'>"
"<option value=0>马里奥金币</option><option value=1>短促滴答</option>"
"<option value=2>水滴</option><option value=3>电子哔</option>"
"<option value=4>轻声滴答</option><option value=5>双击</option>"
"</select>"
"<button class=btn-ghost onclick='previewSound(\"key\")' style='padding:6px 10px;font-size:11px;white-space:nowrap'>试听</button>"
"</div></div>"

"<div style='margin-top:12px'>"
"<div style='display:flex;align-items:center;justify-content:space-between'>"
"<span style='font-size:13px;color:var(--text3);font-weight:500'>开关机音效</span>"
"<label class=tgl><input type=checkbox id=pwr_en_chk onchange='onSoundChange()'><span class=sl></span></label>"
"</div>"
"<div style='display:flex;align-items:center;gap:8px;margin-top:6px'>"
"<span style='font-size:11px;color:var(--text3);white-space:nowrap'>音量</span>"
"<input type=range id=pwr_vol_sl min=0 max=100 value=70 oninput='onPwrVolInput()' onchange='onSoundChange()' style='flex:1'>"
"<span id=pwr_vol_val style='font-size:13px;font-weight:600;color:var(--green);min-width:32px'>70%</span>"
"</div>"
"<div style='display:flex;align-items:center;gap:6px;margin-top:6px'>"
"<select id=pwon_mel_sel onchange='onSoundChange()' style='flex:1;padding:7px 8px;border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;color:var(--text);font-size:11px;outline:none;cursor:pointer'>"
"<option value=0>马里奥主题</option><option value=1>上行音阶</option>"
"<option value=2>叮咚门铃</option><option value=3>大三和弦</option>"
"<option value=4>小星星</option><option value=5>号角</option>"
"</select>"
"<button class=btn-ghost onclick='previewSound(\"pwon\")' style='padding:6px 10px;font-size:11px;white-space:nowrap'>试听</button>"
"</div>"
"<div style='display:flex;align-items:center;gap:6px;margin-top:6px'>"
"<select id=pwoff_mel_sel onchange='onSoundChange()' style='flex:1;padding:7px 8px;border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;color:var(--text);font-size:11px;outline:none;cursor:pointer'>"
"<option value=0>下行GEC</option><option value=1>长下行音阶</option>"
"<option value=2>叮咚下行</option><option value=3>小调琶音</option>"
"<option value=4>再见</option><option value=5>渐慢风落</option>"
"</select>"
"<button class=btn-ghost onclick='previewSound(\"pwoff\")' style='padding:6px 10px;font-size:11px;white-space:nowrap'>试听</button>"
"</div></div>"
"<div style='margin-top:12px'>"
"<div style='display:flex;align-items:center;justify-content:space-between'>"
"<span style='font-size:13px;color:var(--text3);font-weight:500'>报警音效</span>"
"<label class=tgl><input type=checkbox id=alarm_en_chk onchange='onSoundChange()'><span class=sl></span></label>"
"</div>"
"<div style='display:flex;align-items:center;gap:8px;margin-top:6px'>"
"<span style='font-size:11px;color:var(--text3);white-space:nowrap'>音量</span>"
"<input type=range id=alarm_vol_sl min=0 max=100 value=70 oninput=\"document.getElementById('alarm_vol_val').textContent=this.value+'%'\" onchange='onSoundChange()' style='flex:1'>"
"<span id=alarm_vol_val style='font-size:13px;font-weight:600;color:var(--green);min-width:32px'>70%</span>"
"</div>"
"<div style='display:flex;align-items:center;gap:6px;margin-top:6px'>"
"<select id=alarm_mel_sel onchange='onSoundChange()' style='flex:1;padding:7px 8px;border:1.5px solid var(--border);border-radius:var(--radius-xs);background:#f9fbf9;color:var(--text);font-size:11px;outline:none;cursor:pointer'>"
"<option value=0>经典警笛</option><option value=1>急促哔哔</option>"
"<option value=2>高低扫频</option><option value=3>三连脉冲</option>"
"<option value=4>连续快滴</option><option value=5>低频轰鸣</option>"
"</select>"
"<button class=btn-ghost onclick='previewSound(\"alarm\")' style='padding:6px 10px;font-size:11px;white-space:nowrap'>试听</button>"
"</div></div>"
"<div class=toast id=sound_res></div>"
"<button id=sound_save_btn class=btn-primary style='margin-top:10px;width:100%' onclick='saveSound()'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"保存音效设置</button>"
"</div>"

/* ═══ Power Management Card ═══ */
"<div class=card style='animation-delay:.44s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='#f59e0b' stroke-width='2'><path d='M12 2v6m0 0l4-4m-4 4L8 4'/>"
"<circle cx='12' cy='16' r='1'/><path d='M18.4 6.6a9 9 0 1 1-12.8 0'/></svg>"
"电源管理"
"</div>"
"<div class=card-desc>关屏幕、关机与定时开关机</div>"
/* ── Manual controls ── */
"<div class=btn-row style='margin-bottom:12px'>"
"<button class=btn-ghost onclick='onPower(\"screen_off\")' style='flex:1'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z'/><circle cx='12' cy='12' r='3'/></svg>"
"关屏幕</button>"
"<button class=btn-ghost onclick='onPower(\"shutdown\")' style='flex:1;color:#e53935'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M18.4 6.6a9 9 0 1 1-12.8 0'/><line x1='12' y1='2' x2='12' y2='12'/></svg>"
"关机</button>"
"</div>"
"<div class=btn-row style='margin-bottom:12px'>"
"<button class=btn-ghost onclick='onPower(\"screen_on\")' style='flex:1;color:#43a047'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><circle cx='12' cy='12' r='4'/><path d='M12 2v4m0 12v4M2 12h4m12 0h4'/></svg>"
"开屏幕</button>"
"<button class=btn-ghost onclick='onPower(\"wake\")' style='flex:1;color:#43a047'>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M12 2v6m0 0l4-4m-4 4L8 4'/><circle cx='12' cy='16' r='1'/><path d='M18.4 6.6a9 9 0 1 1-12.8 0'/></svg>"
"开机</button>"
"</div>"
"<span class=toast id=power_res></span>"
/* ── Schedule ── */
"<div style='border-top:1px solid var(--border);padding-top:12px;margin-top:8px'>"
"<span style='font-size:13px;color:var(--text3)'>定时开关机</span>"
"</div>"
"<div id=sched_status style='margin:8px 0;padding:8px 10px;border-radius:6px;background:var(--card);font-size:12px;display:none'></div>"
"<div style='margin-top:8px'>"
/* ── Off time ── */
"<span style='font-size:11px;color:var(--text3)'>关机时间</span>"
"<label class=tgl style='float:right'><input type=checkbox id=sched_off_en onchange='toggleSched(\"off\")'><span class=sl></span></label>"
"<div class=day-presets id=off_presets style='display:flex;flex-wrap:wrap;gap:4px;margin:4px 0'>"
"<button class=dp-btn data-v=0>今天</button><button class=dp-btn data-v=1>明天</button><button class=dp-btn data-v=2>后天</button>"
"<button class=dp-btn data-v=3>每天</button><button class=dp-btn data-v=4>工作日</button><button class=dp-btn data-v=5>周末</button>"
"<button class=dp-btn data-v=13>自定义</button>"
"</div>"
"<div class=day-wdays id=off_wdays style='display:flex;gap:4px;margin:2px 0 6px'>"
"<button class=wd-btn data-v=0>一</button><button class=wd-btn data-v=1>二</button><button class=wd-btn data-v=2>三</button>"
"<button class=wd-btn data-v=3>四</button><button class=wd-btn data-v=4>五</button><button class=wd-btn data-v=5>六</button>"
"<button class=wd-btn data-v=6>日</button>"
"</div>"
"<div style='display:flex;gap:12px;align-items:center;justify-content:center'>"
"<div class=sched-stepper id=sp_off_h><button class=sp-btn onclick='spin(-1,\"off_h\")'>&#9664;</button><input class=sp-val value=00 data-v=0 oninput='stepperInput(\"off_h\")'><button class=sp-btn onclick='spin(1,\"off_h\")'>&#9654;</button></div>"
"<span style='font-size:16px;font-weight:700;color:var(--text3)'>:</span>"
"<div class=sched-stepper id=sp_off_m><button class=sp-btn onclick='spin(-1,\"off_m\")'>&#9664;</button><input class=sp-val value=00 data-v=0 oninput='stepperInput(\"off_m\")'><button class=sp-btn onclick='spin(1,\"off_m\")'>&#9654;</button></div>"
"</div>"
"<button class=btn-primary style='width:100%;margin-top:6px;padding:6px' id=btn_save_off onclick='saveOffSchedule()'>"
"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"<span id=btn_save_off_lbl>保存关机</span></button>"
"<span class=toast id=sched_off_res></span>"
/* ── On time ── */
"<span style='font-size:11px;color:var(--text3);margin-top:8px;display:block'>开机时间</span>"
"<label class=tgl style='float:right'><input type=checkbox id=sched_on_en onchange='toggleSched(\"on\")'><span class=sl></span></label>"
"<div class=day-presets id=on_presets style='display:flex;flex-wrap:wrap;gap:4px;margin:4px 0'>"
"<button class=dp-btn data-v=0>今天</button><button class=dp-btn data-v=1>明天</button><button class=dp-btn data-v=2>后天</button>"
"<button class=dp-btn data-v=3>每天</button><button class=dp-btn data-v=4>工作日</button><button class=dp-btn data-v=5>周末</button>"
"<button class=dp-btn data-v=13>自定义</button>"
"</div>"
"<div class=day-wdays id=on_wdays style='display:flex;gap:4px;margin:2px 0 6px'>"
"<button class=wd-btn data-v=0>一</button><button class=wd-btn data-v=1>二</button><button class=wd-btn data-v=2>三</button>"
"<button class=wd-btn data-v=3>四</button><button class=wd-btn data-v=4>五</button><button class=wd-btn data-v=5>六</button>"
"<button class=wd-btn data-v=6>日</button>"
"</div>"
"<div style='display:flex;gap:12px;align-items:center;justify-content:center'>"
"<div class=sched-stepper id=sp_on_h><button class=sp-btn onclick='spin(-1,\"on_h\")'>&#9664;</button><input class=sp-val value=00 data-v=0 oninput='stepperInput(\"on_h\")'><button class=sp-btn onclick='spin(1,\"on_h\")'>&#9654;</button></div>"
"<span style='font-size:16px;font-weight:700;color:var(--text3)'>:</span>"
"<div class=sched-stepper id=sp_on_m><button class=sp-btn onclick='spin(-1,\"on_m\")'>&#9664;</button><input class=sp-val value=00 data-v=0 oninput='stepperInput(\"on_m\")'><button class=sp-btn onclick='spin(1,\"on_m\")'>&#9654;</button></div>"
"</div>"
"<button class=btn-primary style='width:100%;margin-top:6px;padding:6px' id=btn_save_on onclick='saveOnSchedule()'>"
"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/></svg>"
"<span id=btn_save_on_lbl>保存开机</span></button>"
"<span class=toast id=sched_on_res></span>"
"</div>"
"</div>"

/* ═══ OTA Update Card ═══ */
"<div class=card style='animation-delay:.46s'>"
"<div class=card-title>"
"<svg viewBox='0 0 24 24' fill='none' stroke='#6366f1' stroke-width='2'><path d='M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4'/><polyline points='7 10 12 15 17 10'/><line x1='12' y1='15' x2='12' y2='3'/></svg>"
"固件升级"
"</div>"
"<div class=card-desc>选择 .bin 文件上传，设备自动重启</div>"
"<input type=file id=ota_file accept='.bin' style='display:block;width:100%;padding:8px 0;font-size:13px;color:var(--text);'>"
"<button class=btn-primary style='margin-top:8px;width:100%' onclick='doOta()' id=ota_btn>"
"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5'><path d='M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4'/><polyline points='7 10 12 15 17 10'/><line x1='12' y1='15' x2='12' y2='3'/></svg>"
"上传并升级</button>"
"<div class=toast id=ota_res style='margin-top:8px'></div>"
"<div id=ota_prog style='display:none;margin-top:8px'>"
"<div style='background:#dce8df;border-radius:4px;height:6px;overflow:hidden'>"
"<div id=ota_bar style='height:100%;width:0;background:var(--green);transition:width .3s'></div></div>"
"<div id=ota_pct style='text-align:center;font-size:13px;color:var(--green);margin-top:4px'></div></div>"
"</div>"


"</div>"  /* /.container */

/* ═══ History Modal ═══ */
"<div class=modal-overlay id=mod_ov onclick='closeHistory(event)'>"
"<div class=modal-box onclick='event.stopPropagation()'>"
"<div class=modal-hdr>"
"<h3 id=mod_title>温度趋势</h3>"
"<button class=modal-close onclick='closeHistory()'>&times;</button>"
"</div>"
"<div class=range-btns>"
"<button class='r-btn active' data-h='1' onclick='setRange(1)'>1h</button>"
"<button class=r-btn data-h='3' onclick='setRange(3)'>3h</button>"
"<button class=r-btn data-h='6' onclick='setRange(6)'>6h</button>"
"<button class=r-btn data-h='12' onclick='setRange(12)'>12h</button>"
"<button class=r-btn data-h='24' onclick='setRange(24)'>24h</button>"
"</div>"
"<div class=chart-wrap><canvas id=hist_canvas></canvas><div id=hist_empty style='display:none;position:absolute;inset:0;justify-content:center;align-items:center;color:#5f7d6b;font-size:14px'>暂无数据，请等待采集</div></div>"
"</div></div>"

/* ═══ JavaScript ═══ */
"<script>"
"var L=localStorage.lang||'zh';"
"function t(k){var m={zh:{"
"off:'关',led_on:'已开启',led_off:'已关闭',shutdown:'关机',screen_off:'关屏幕',screen_on:'开屏幕',"
"wake:'开机',wake_done:'已开机',"
"running:'运行中',standby:'待机中',rpm:'转/分',connected:'已连接',disconnected:'未连接',"
"applied:'已应用',saved:'已保存',failed:'失败',saving:'保存中...',testing:'测试中...',"
"executing:'执行中...',shut_down:'已关机',screen_off_done:'屏幕已关',screen_on_done:'屏幕已开',"
"connect_ok:'连接成功',mqtt_restart:'已保存! 即将重启...',hs_restart:'已保存! 即将重启...',today:'今天',tomorrow:'明天',day_after:'后天',every_day:'每天',weekdays:'工作日',weekends:'周末',mon:'周一',tue:'周二',wed:'周三',thu:'周四',fri:'周五',sat:'周六',sun:'周日',"
"uploading:'上传中...',ota_ok:'升级成功，设备重启中...',ota_fail:'升级失败',network_err:'网络错误',pick_bin:'请选择 .bin 文件',upload_btn:'上传并升级',"
"sound_saved:'音效设置已保存',sound_preview:'试听中...'"
"},en:{"
"off:'Off',led_on:'On',led_off:'Off',shutdown:'Shutdown',screen_off:'Screen Off',screen_on:'Screen On',"
"wake:'Power On',wake_done:'Powered on',"
"running:'Running',standby:'Standby',rpm:'RPM',connected:'Connected',disconnected:'Disconnected',"
"applied:'Applied',saved:'Saved',failed:'Failed',saving:'Saving...',testing:'Testing...',"
"executing:'Running...',shut_down:'Shut down',screen_off_done:'Screen off',screen_on_done:'Screen on',"
"connect_ok:'Connected',mqtt_restart:'Saved! Restarting...',hs_restart:'Saved! Restarting...',today:'Today',tomorrow:'Tomorrow',day_after:'Day After',every_day:'Every Day',weekdays:'Weekdays',weekends:'Weekends',mon:'Mon',tue:'Tue',wed:'Wed',thu:'Thu',fri:'Fri',sat:'Sat',sun:'Sun',"
"uploading:'Uploading...',ota_ok:'Upgrade successful, rebooting...',ota_fail:'Upgrade failed',network_err:'Network Error',pick_bin:'Select a .bin file',upload_btn:'Upload & Upgrade',"
"sound_saved:'Sound settings saved',sound_preview:'Preview...'"
"}};return m[L]&&m[L][k]||k;}"
"function toggleLang(){localStorage.lang=L==='zh'?'en':'zh';location.reload();}"
"document.getElementById('lang_btn').textContent=L==='zh'?'EN':'中';"
"if(L==='en'){"
"var M={"
"'AiRFLOW 配网':'AiRFLOW Config','清新空气 智慧生活':'Fresh Air, Smart Living',"
"'手机连接':'Connect to','AiRFLOW 热点':'AiRFLOW Hotspot','选择你的':'Select your','WiFi 网络':'WiFi Network',"
"'输入密码':'Enter Password','完成连接':'Connect',"
"'&#9432; 手机通常会自动弹出此页面。<br>如未弹出，请在浏览器地址栏输入':'&#9432; Phone usually opens this page automatically.<br>If not, enter',"
"'&#9432; 本设备已连接互联网，此页面仅用于查看状态与管理。':'&#9432; Device is online. This page is for status and management only.',"
"'传感器数据':'Sensor Data','风扇控制':'Fan Control','网络信息':'Network Info','MQTT 配置':'MQTT Config',"
"'设备状态':'Device Status','报警设置':'Alarm Settings','自定义首页':'Custom Home',"
"'状态指示灯':'Status LED','电源管理':'Power Management','报警阈值':'Alarm Thresholds',"
"'TVOC 阈值':'TVOC Threshold','CO₂ 阈值':'CO₂ Threshold','甲醛 阈值':'CH₂O Threshold',"
"'自动风扇':'Auto Fan','保存':'Save','返回':'Back',"
"'超标自动开启风扇':'Auto fan on alarm','启动后默认显示':'Default after boot',"
"'开关':'On/Off','亮度':'Brightness','效果':'Effect','风速':'Speed',"
"'WiFi':'WiFi','IP 地址':'IP Address','MQTT 代理':'MQTT Broker','MQTT 状态':'MQTT Status',"
"'未连接':'Disconnected','已连接':'Connected',"
"'控制面板':'Control Panel',"
"'测试连接':'Test','保存并重启':'Save & Restart','保存定时':'Save Schedule',"
"'关屏幕':'Screen Off','关机':'Shutdown',"
"'定时开关机':'Scheduled Power','关机时间':'Off Time','开机时间':'On Time',"
"'今天':'Today','明天':'Tomorrow','后天':'Day After','每天':'Every Day','工作日':'Weekdays','周末':'Weekends','周一':'Mon','周二':'Tue','周三':'Wed','周四':'Thu','周五':'Fri','周六':'Sat','周日':'Sun',"
"'实时监测温度、TVOC、CO₂、甲醛':'Real-time temperature, TVOC, CO₂, CH₂O monitoring',"
"'设定传感器报警值与自动风扇':'Set alarm thresholds and auto fan',"
"'选择设备启动后默认显示的页面':'Choose default screen after boot',"
"'调节风速，开关风扇与自动模式':'Adjust speed, on/off and auto mode',"
"'查看WiFi连接与MQTT状态':'View WiFi connection and MQTT status',"
"'设置MQTT代理地址，连接智能家居平台':'Set MQTT broker for smart home',"
"'调节LED颜色、亮度与显示效果':'Adjust LED color, brightness and effects',"
"'关屏幕、关机与定时开关机':'Screen off, shutdown and scheduling',"
"'开屏幕':'Screen On','开机':'Power On',"
"'常亮':'Steady','慢闪':'Slow Blink','呼吸':'Breathe','快闪':'Fast Blink','彩虹':'Rainbow',"
"'转/分':'RPM','待机中':'Standby','运行中':'Running',"
"'已连接':'Connected','未连接':'Disconnected',"
"'已应用':'Applied','已保存':'Saved','失败':'Failed','保存中...':'Saving...','测试中...':'Testing...',"
"'执行中...':'Running...','已关机':'Shut down','屏幕已关':'Screen off',"
"'连接成功':'Connected','已保存! 即将重启...':'Saved! Restarting...',"
"'输入密码':'Enter Password','显示密码':'Show','隐藏':'Hide',"
"'首页':'Home','仪表盘':'Dashboard','网络设置':'Network',"
"'设置':'Settings','声音':'Sound','电源':'Power','固件升级':'Firmware Update',"
"'选择 .bin 文件上传，设备自动重启':'Select .bin file to upload, device will reboot','上传并升级':'Upload & Upgrade',"
"'各状态 LED 颜色':'Per-state LED Color','正常':'Normal','报警':'Alarm',"
"'WiFi失败':'WiFi Fail','WiFi连接中':'WiFi Connecting',"
"'温度':'Temperature','甲醛':'CH₂O','保存阈值':'Save Thresholds','调色盘':'Color Picker',"
"'声音设置':'Sound Settings','选择按键提示音、开关机音效与音量':'Select key tone, power melodies and volume',"
"'按键音':'Key Click','开关机音效':'Power Sound','音量':'Volume',"
"'马里奥金币':'Mario Coin','短促滴答':'Short Tick','水滴':'Water Drop','电子哔':'Elec Beep',"
"'轻声滴答':'Soft Tick','双击':'Dbl Click',"
"'马里奥主题':'Mario Theme','上行音阶':'Asc. Scale','叮咚门铃':'Ding Dong',"
"'大三和弦':'Maj. Arp','小星星':'Twinkle','号角':'Fanfare',"
"'下行GEC':'Desc. GEC','长下行音阶':'Desc. Scale','叮咚下行':'Ding Dong Dn',"
"'小调琶音':'Min. Arp','再见':'Bye Bye','渐慢风落':'Wind Down',"
"'试听':'Preview','保存音效设置':'Save Sound Settings'"
	"};"
"function walk(n){"
"if(n.nodeType===3&&n.parentNode.nodeName!=='SCRIPT'&&n.parentNode.nodeName!=='STYLE'){"
"var s=n.textContent,t;"
"var keys=Object.keys(M).sort(function(a,b){return b.length-a.length;});"
"for(var ki=0;ki<keys.length;ki++){var k=keys[ki];var i=s.indexOf(k);if(i!==-1){t=M[k];"
"n.textContent=s.substring(0,i)+t+s.substring(i+k.length);s=n.textContent;}}"
"}else if(n.nodeType===1)for(var c=n.firstChild;c;c=c.nextSibling)walk(c);"
"}"
"walk(document.body);"
"document.title='AiRFLOW Config';"
"}"
"var fanOn=false,fanSpd=0,changing=false,lastFanFetch=0,thrInit=false;"

"function poll(){"
"fetch('/status').then(function(r){return r.json();}).then(function(j){"
"var el;"
"el=document.getElementById('temp');el.textContent=j.t.toFixed(1);"
"el.className=j.t<-10?'s-value bad':j.t>50?'s-value warn':'s-value';"
"el=document.getElementById('tvoc');el.textContent=j.tvoc;"
"el.className=j.tvoc>500?'s-value bad':j.tvoc>200?'s-value warn':'s-value';"
"el=document.getElementById('co2');el.textContent=j.co2;"
"el.className=j.co2>1500?'s-value bad':j.co2>800?'s-value warn':'s-value';"
"el=document.getElementById('ch2o');el.textContent=j.ch2o;"
"el.className=j.ch2o>100?'s-value bad':j.ch2o>50?'s-value warn':'s-value';"
"if(!changing&&Date.now()-lastFanFetch>2000){"
"document.getElementById('fo').checked=j.fo;"
"document.getElementById('fs').value=j.fs;"
"document.getElementById('fv').textContent=(j.fo?j.fs:0)+'%';"
"}else if(!changing){"
"document.getElementById('fv').textContent=(document.getElementById('fs').value)+'%';"
"}"
"document.getElementById('frpm2').textContent=j.fo&&j.rpm?j.rpm+' '+t('rpm'):'-- '+t('rpm');"
"document.getElementById('fstLabel').textContent=j.fo?t('running'):t('standby');"
"var ico=document.getElementById('fanIco');"
"if(j.fo&&j.fs>0)ico.classList.add('on');else ico.classList.remove('on');"
"if(!thrInit){"
"document.getElementById('tvoc_thr_sl').value=j.tvoc_thr||500;"
"document.getElementById('co2_thr_sl').value=j.co2_thr||1000;"
"document.getElementById('ch2o_thr_sl').value=j.ch2o_thr||100;"
"document.getElementById('af_chk').checked=j.auto_fan!==0;"
"document.getElementById('alarm_cd_sl').value=j.alarm_cd||60;"
"document.getElementById('alarm_cd_val').textContent=(j.alarm_cd||60)+'秒';"
"onThrChange();thrInit=true;"
"}"
"var hb=document.getElementById('holiday_badge');if(hb){"
"if(j.holiday){hb.textContent='🎉'+j.holiday;hb.style.display='';}"
"else{hb.style.display='none';}}"
"});"
"/* ── network info (one-shot init + live MQTT polling) ── */"
"fetch('/netinfo').then(function(r){return r.json();}).then(function(j){"
"var inp=document.getElementById('mqtt_inp');if(!inp.value&&j.mqtt!='N/A')inp.value=j.mqtt;"
"var hs=document.getElementById('hs_sel');if(hs&&!hs._init){hs.value=j.home_scr||0;hs._init=true;}"
"var led=document.getElementById('led_chk');if(led && !led._init){led.checked=j.led_on;led._init=true;}"
"var lr=document.getElementById('led_r');if(lr && !lr._init){"
"var r=j.led_r!==undefined?j.led_r:0,g=j.led_g!==undefined?j.led_g:255,b=j.led_b!==undefined?j.led_b:0;"
"syncLedUI(r,g,b);"
"document.getElementById('led_bri').value=j.led_bri||100;"
"document.getElementById('led_bri_val').textContent=j.led_bri||100;"
"lr._init=true;}"
"pollNet(j);"
"});"
"function pollNet(j){"
"document.getElementById('ni_ssid').textContent=j.ssid;"
"document.getElementById('ni_ip').textContent=j.ip;"
"document.getElementById('ni_mqtt').textContent=j.mqtt;"
"var e=document.getElementById('ni_mqtts');"
"if(j.mqtt_ok){e.innerHTML=\"<span class='status-dot ok'></span>\"+t('connected');e.className='val ok';}"
"else{e.innerHTML=\"<span class='status-dot err'></span>\"+t('disconnected');e.className='val err';}"
"}"
"}"
"setInterval(function(){fetch('/netinfo').then(function(r){return r.json();}).then(function(j){pollNet(j);});},5000);"
"setInterval(poll,2000);poll();"

/* ── Alarm threshold functions ────────────────── */
"function onThrChange(){"
"document.getElementById('tvoc_thr_val').textContent=document.getElementById('tvoc_thr_sl').value;"
"document.getElementById('co2_thr_val').textContent=document.getElementById('co2_thr_sl').value;"
"document.getElementById('ch2o_thr_val').textContent=document.getElementById('ch2o_thr_sl').value;"
"}"
"function saveAlarm(){"
"var el=document.getElementById('alarm_res');"
"el.textContent=t('saving');el.style.color='var(--text3)';"
"var body='tvoc_thr='+document.getElementById('tvoc_thr_sl').value"
"+'&co2_thr='+document.getElementById('co2_thr_sl').value"
"+'&ch2o_thr='+document.getElementById('ch2o_thr_sl').value"
"+'&alarm_cd='+document.getElementById('alarm_cd_sl').value"
"+'&auto_fan='+(document.getElementById('af_chk').checked?1:0);"
"fetch('/alarm_cfg',{method:'POST',body:body}).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){el.textContent=t('saved');el.style.color='var(--green)';}"
"else{el.textContent=t('failed');el.style.color='#e53935';}"
"});"
"}"

"var fsTimer=null;"
"document.getElementById('fs').oninput=function(){"
"changing=true;"
"var v=parseInt(this.value);if(v<1)v=1;"
"document.getElementById('fv').textContent=v+'%';"
"clearTimeout(fsTimer);"
"fsTimer=setTimeout(function(){"
"changing=false;lastFanFetch=Date.now();"
"fetch('/set',{method:'POST',body:'fs='+v+'&fo='+(document.getElementById('fo').checked?1:0)});"
"},150);"
"};"
"document.getElementById('fo').onchange=function(){"
"changing=false;lastFanFetch=Date.now();"
"var v=parseInt(document.getElementById('fs').value);"
"if(!this.checked){document.getElementById('fs').value=0;document.getElementById('fv').textContent='0%';}"
"fetch('/set',{method:'POST',body:'fs='+v+'&fo='+(this.checked?1:0)});"
"};"

"function testMqtt(){"
"var inp=document.getElementById('mqtt_inp').value.trim(),res=document.getElementById('mqtt_res');"
"if(!inp){res.textContent='请输入代理地址';res.style.color='#e53935';return;}"
"res.textContent=t('testing');res.style.color='var(--text3)';"
"fetch('/mqtt_test?broker='+encodeURIComponent(inp)).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){res.textContent=t('connect_ok')+' — '+j.ip+':'+j.port;res.style.color='var(--green)';}"
"else{res.textContent=t('failed')+': '+j.error;res.style.color='#e53935';}"
"});}"
"function saveMqtt(){"
"var inp=document.getElementById('mqtt_inp').value.trim(),res=document.getElementById('mqtt_res');"
"if(!inp){res.textContent='请输入代理地址';res.style.color='#e53935';return;}"
"res.textContent=t('saving');res.style.color='var(--text3)';"
"fetch('/mqtt_save',{method:'POST',body:'broker='+encodeURIComponent(inp)}).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){res.textContent=t('mqtt_restart');res.style.color='var(--green)';}"
"else{res.textContent=t('failed');res.style.color='#e53935';}"
"});}"
"function saveHomeScreen(){"
"var v=document.getElementById('hs_sel').value,res=document.getElementById('hs_res');"
"res.textContent=t('saving');res.style.color='var(--text3)';"
"fetch('/home_screen',{method:'POST',body:'hs='+v}).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){res.textContent=t('hs_restart');res.style.color='var(--green)';}"
"else{res.textContent=t('failed');res.style.color='#e53935';}"
"});}"
"function onLedToggle(){"
"var ck=document.getElementById('led_chk').checked,res=document.getElementById('led_res');"
"fetch('/led',{method:'POST',body:'on='+(ck?1:0)}).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){res.textContent=ck?t('led_on'):t('led_off');res.style.color='var(--green)';"
"setTimeout(function(){res.textContent='';},2000);}"
"else{res.textContent=t('failed');res.style.color='#e53935';"
"document.getElementById('led_chk').checked=!ck;}"
"});}"
"var rh=function(x){var s=x.toString(16);return s.length<2?'0'+s:s;};"
"function onLedBri(){"
"var bri=document.getElementById('led_bri').value;"
"document.getElementById('led_bri_val').textContent=bri;"
"fetch('/led_cfg',{method:'POST',body:'bri='+bri});"
"}"
"fetch('/led_cfg').then(function(r){return r.json();}).then(function(j){"
"if(!j||j.bri===undefined)return;"
"document.getElementById('led_bri').value=j.bri;"
"document.getElementById('led_bri_val').textContent=j.bri;"
"});"
"var g_led_states=[];"
"fetch('/led_states').then(function(r){return r.json();}).then(function(j){"
"if(!j||!j.length)return;g_led_states=j;onLedStateSel();"
"});"
"function onLedStateSel(){"
"var id=+document.getElementById('ls_sel').value;"
"var s=g_led_states[id];if(!s)return;"
"var hex='#'+rh(s.r)+rh(s.g)+rh(s.b);"
"document.getElementById('ls_pick').value=hex;"
"document.getElementById('ls_prev').style.background='rgb('+s.r+','+s.g+','+s.b+')';"
"document.getElementById('ls_eff').value=s.eff;"
"}"
"function saveLedState(){"
"var id=+document.getElementById('ls_sel').value;"
"var v=document.getElementById('ls_pick').value;"
"var r=parseInt(v.substr(1,2),16),g=parseInt(v.substr(3,2),16),b=parseInt(v.substr(5,2),16);"
"var eff=+document.getElementById('ls_eff').value;"
"document.getElementById('ls_prev').style.background='rgb('+r+','+g+','+b+')';"
"g_led_states[id]={r:r,g:g,b:b,eff:eff};"
"fetch('/led_states',{method:'POST',body:'id='+id+'&r='+r+'&g='+g+'&b='+b+'&eff='+eff});"
"}"
"function onLedStatePick(){saveLedState();}"
"function onLedStateEff(){saveLedState();}"

/* ── Power management ─────────────────────────── */
"function onPower(action){"
"var res=document.getElementById('power_res');"
"res.textContent=t('executing');res.style.color='var(--text3)';"
"fetch('/power',{method:'POST',body:'action='+action})"
".then(function(r){return r.json();}).then(function(j){"
"if(j.ok){res.textContent=action==='shutdown'?t('shut_down'):action==='screen_off'?t('screen_off_done'):action==='wake'?t('wake_done'):t('screen_on_done');"
"res.style.color='var(--green)';"
"setTimeout(function(){res.textContent='';},3000);}"
"else{res.textContent=t('failed');res.style.color='#e53935';}"
"});}"
/* ── Build hour/min options ───────────────────── */
"var g_presets=['今天','明天','后天','每天','工作日','周末','自定义'];"
"var g_wdays_cn=['一','二','三','四','五','六','日'];"
"var g_wdays_en=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];"
/* Hour/minute stepper (simplified — no day) */
"function spin(dir,id){"
"var sp=document.getElementById('sp_'+id);if(!sp)return;"
"var parts=id.split('_');var max=parts[1]==='h'?23:59;"
"var el=sp.querySelector('.sp-val');"
"var v=(parseInt(el.value||0))+dir;"
"if(v<0)v=max;if(v>max)v=0;"
"el.value=(v<10?'0':'')+v;el.setAttribute('data-v',v);"
"}"
"function stepperVal(id){"
"var sp=document.getElementById('sp_'+id);if(!sp)return 0;"
"var el=sp.querySelector('.sp-val');return el?parseInt(el.getAttribute('data-v')||0):0;"
"}"
"function setStepperHm(id,val){"
"var sp=document.getElementById('sp_'+id);if(!sp)return;"
"var el=sp.querySelector('.sp-val');if(el){el.value=(val<10?'0':'')+val;el.setAttribute('data-v',val);}"
"}"
"function stepperInput(id){"
"var sp=document.getElementById('sp_'+id);if(!sp)return;"
"var parts=id.split('_');var max=parts[1]==='h'?23:59;"
"var el=sp.querySelector('.sp-val');if(!el)return;"
"var v=parseInt(el.value);if(isNaN(v)||v<0)v=0;if(v>max)v=max;"
"el.value=(v<10?'0':'')+v;el.setAttribute('data-v',v);"
"}"
/* ── Day picker (presets + multi-select weekdays) ──── */
"var g_day_mode={off:0,on:0};"
"var g_day_mask={off:0,on:0};"
"function daySelectPreset(prefix,mode){"
"g_day_mode[prefix]=mode;"
"var mask=0;"
"if(mode==3)mask=0x7F;"        /* every day */
"else if(mode==4)mask=0x1F;"   /* weekdays Mon-Fri */
"else if(mode==5)mask=0x60;"   /* weekends Sat-Sun */
"else if(mode>=6&&mode<=12){mask=1<<(mode==12?6:mode-6);}"  /* single weekday */
"else if(mode==13)mask=g_day_mask[prefix];"  /* custom: keep existing mask */
"g_day_mask[prefix]=mask;"
"updateDayUI(prefix);"
"}"
"function dayToggleWday(prefix,bit){"
"g_day_mode[prefix]=13;"
"g_day_mask[prefix]^=(1<<bit);"
"updateDayUI(prefix);"
"}"
"function updateDayUI(prefix){"
"var mode=g_day_mode[prefix],mask=g_day_mask[prefix];"
"var btns=document.querySelectorAll('#'+prefix+'_presets .dp-btn');"
"btns.forEach(function(b){var v=+b.getAttribute('data-v');b.classList.toggle('active',v===mode);});"
"var wds=document.querySelectorAll('#'+prefix+'_wdays .wd-btn');"
"wds.forEach(function(b){var v=+b.getAttribute('data-v');b.classList.toggle('active',!!(mask&(1<<v)));});"
"}"
/* Wire up day presets */
"function setupDayPickers(){"
"var isZh=document.documentElement.lang.indexOf('zh')===0;"
"['off','on'].forEach(function(px){"
"document.getElementById(px+'_presets').addEventListener('click',function(e){"
"var b=e.target.closest('.dp-btn');if(!b)return;"
"daySelectPreset(px,+b.getAttribute('data-v'));"
"});"
"document.getElementById(px+'_wdays').addEventListener('click',function(e){"
"var b=e.target.closest('.wd-btn');if(!b)return;"
"dayToggleWday(px,+b.getAttribute('data-v'));"
"});"
"var pbs=document.getElementById(px+'_presets').querySelectorAll('.dp-btn');"
"var gdl=isZh?['今天','明天','后天','每天','工作日','周末','自定义']:['Today','Tmrw','D+2','Every','Wdys','Wknd','Custom'];"
"pbs.forEach(function(b,i){b.textContent=gdl[i]||b.textContent;});"
"var wbs=document.getElementById(px+'_wdays').querySelectorAll('.wd-btn');"
"var wdl=isZh?['一','二','三','四','五','六','日']:['Mo','Tu','We','Th','Fr','Sa','Su'];"
"wbs.forEach(function(b,i){b.textContent=wdl[i];});"
"});"
"document.getElementById('btn_save_off_lbl').textContent=isZh?'保存关机':'Save Off';"
"document.getElementById('btn_save_on_lbl').textContent=isZh?'保存开机':'Save On';"
"}"
"function toggleSched(which){"
"var el=document.getElementById('sched_'+which+'_en');"
"fetch('/schedule',{method:'POST',body:which+'_en='+(el.checked?1:0)});}"
"function saveOffSchedule(){"
"var res=document.getElementById('sched_off_res');"
"var oh=stepperVal('off_h'),om=stepperVal('off_m');"
"var ts=(oh<10?'0':'')+oh+':'+(om<10?'0':'')+om;"
"res.textContent=t('saving')+' '+ts+'...';res.style.color='var(--text3)';"
"var el=document.getElementById('sched_off_en');el.checked=true;"
"var body='off_en=1'"
"+'&off_day='+g_day_mode.off"
"+'&off_h='+oh"
"+'&off_m='+om"
"+'&off_days='+g_day_mask.off;"
"fetch('/schedule',{method:'POST',body:body})"
".then(function(r){return r.json();}).then(function(j){"
"if(j.ok){"
"setStepperHm('off_h',j.off_h);setStepperHm('off_m',j.off_m);"
"g_day_mode.off=j.off_day;"
"var ts2=(j.off_h<10?'0':'')+j.off_h+':'+(j.off_m<10?'0':'')+j.off_m;"
"res.textContent=t('saved')+': '+ts2;res.style.color='var(--green)';"
"updateSchedStatus(j);"
"setTimeout(function(){res.textContent='';},3000);}"
"else{res.textContent=t('failed');res.style.color='#e53935';}"
"});}"
"function saveOnSchedule(){"
"var res=document.getElementById('sched_on_res');"
"var oh=stepperVal('on_h'),om=stepperVal('on_m');"
"var ts=(oh<10?'0':'')+oh+':'+(om<10?'0':'')+om;"
"res.textContent=t('saving')+' '+ts+'...';res.style.color='var(--text3)';"
"var el=document.getElementById('sched_on_en');el.checked=true;"
"var body='on_en=1'"
"+'&on_day='+g_day_mode.on"
"+'&on_h='+oh"
"+'&on_m='+om"
"+'&on_days='+g_day_mask.on;"
"fetch('/schedule',{method:'POST',body:body})"
".then(function(r){return r.json();}).then(function(j){"
"if(j.ok){"
"setStepperHm('on_h',j.on_h);setStepperHm('on_m',j.on_m);"
"g_day_mode.on=j.on_day;"
"var ts2=(j.on_h<10?'0':'')+j.on_h+':'+(j.on_m<10?'0':'')+j.on_m;"
"res.textContent=t('saved')+': '+ts2;res.style.color='var(--green)';"
"updateSchedStatus(j);"
"setTimeout(function(){res.textContent='';},3000);}"
"else{res.textContent=t('failed');res.style.color='#e53935';}"
"});}"
"function loadSchedule(){"
"fetch('/schedule').then(function(r){return r.json();}).then(function(j){"
"if(!j)return;"
"var el=document.getElementById('sched_off_en');"
"if(el&&!el._init){el.checked=j.off_en!=0;el._init=true;}"
"el=document.getElementById('sched_on_en');"
"if(el&&!el._init){el.checked=j.on_en!=0;}"
"if(j.off_h!==undefined)setStepperHm('off_h',j.off_h);"
"if(j.off_m!==undefined)setStepperHm('off_m',j.off_m);"
"if(j.on_h!==undefined)setStepperHm('on_h',j.on_h);"
"if(j.on_m!==undefined)setStepperHm('on_m',j.on_m);"
"if(j.off_day!==undefined){g_day_mode.off=j.off_day;}"
"if(j.off_days!==undefined){g_day_mask.off=j.off_days;}"
"if(j.on_day!==undefined){g_day_mode.on=j.on_day;}"
"if(j.on_days!==undefined){g_day_mask.on=j.on_days;}"
"updateDayUI('off');updateDayUI('on');"
"updateSchedStatus(j);"
"});}"
"var g_day_names=['今天','明天','后天','每天','工作日','周末','周一','周二','周三','周四','周五','周六','周日','自定义'];"
"function schedTimeStr(day,h,m,en){"
"if(!en)return null;"
"var ts=(h<10?'0':'')+h+':'+(m<10?'0':'')+m;"
"var dn=g_day_names[day]||('mode'+day);"
"return dn+' '+ts;}"
"function cancelSched(which){"
"var el=document.getElementById('sched_'+which+'_en');if(el)el.checked=false;"
"fetch('/schedule',{method:'POST',body:which+'_en=0'})"
".then(function(r){return r.json();}).then(function(j){"
"if(j.ok){loadSchedule();}"
"});}"
"function updateSchedStatus(j){"
"var el=document.getElementById('sched_status');if(!el)return;"
"var parts=[];"
"if(j.off_en){var s=schedTimeStr(j.off_day,j.off_h,j.off_m,true);if(s)parts.push('关机: '+s+\" <span onclick=event.stopPropagation();cancelSched('off') style=cursor:pointer;color:var(--red);font-size:14px title='取消'>&#10005;</span>\");}"
"if(j.on_en){var s=schedTimeStr(j.on_day,j.on_h,j.on_m,true);if(s)parts.push('开机: '+s+\" <span onclick=event.stopPropagation();cancelSched('on') style=cursor:pointer;color:var(--red);font-size:14px title='取消'>&#10005;</span>\");}"
"if(parts.length){"
"el.innerHTML='⏰ '+parts.join(' &nbsp;|&nbsp; ');"
"el.style.display='block';"
"}else{el.style.display='none';}"
"}"
"setupDayPickers();"
"loadSchedule();"
/* ── OTA firmware upload ─────────────────────── */
"function doOta(){"
"var f=document.getElementById('ota_file').files[0];"
"if(!f){var r=document.getElementById('ota_res');r.textContent=t('pick_bin');"
"r.style.color='#e53935';return;}"
"var b=document.getElementById('ota_btn'),r=document.getElementById('ota_res');"
"b.disabled=true;b.textContent=t('uploading');r.textContent='';"
"var x=new XMLHttpRequest();"
"x.upload.onprogress=function(e){"
"if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);"
"document.getElementById('ota_prog').style.display='';"
"document.getElementById('ota_bar').style.width=p+'%';"
"document.getElementById('ota_pct').textContent=p+'%';}};"
"x.onload=function(){"
"if(x.status===200){"
"r.textContent=t('ota_ok');r.style.color='var(--green)';"
"setTimeout(function(){location.reload();},8000);}"
"else{r.textContent=t('ota_fail')+' ('+x.status+')';r.style.color='#e53935';b.disabled=false;b.textContent=t('upload_btn');}};"
"x.onerror=function(){r.textContent=t('network_err');r.style.color='#e53935';b.disabled=false;b.textContent=t('upload_btn');};"
"x.open('POST','/update',true);x.send(f);"
"}"
"/* ── Sensor history chart ── */"
"var g_chart=null,g_metric=0,g_range=1;"
"var g_colors=['#f59e0b','#6366f1','#0891b2','#8b5cf6'];"
"var g_bg=['rgba(245,158,11,.1)','rgba(99,102,241,.1)','rgba(8,145,178,.1)','rgba(139,92,246,.1)'];"
"var g_label_zh=['温度 (°C)','TVOC (μg/m³)','CO₂ (ppm)','甲醛 (μg/m³)'];"
"var g_label_en=['Temperature (°C)','TVOC (μg/m³)','CO₂ (ppm)','CH₂O (μg/m³)'];"
"var g_title_zh=['温度趋势','TVOC 趋势','CO₂ 趋势','甲醛趋势'];"
"var g_title_en=['Temperature Trend','TVOC Trend','CO₂ Trend','CH₂O Trend'];"
"function openHistory(m){g_metric=m;g_range=1;"
"document.getElementById('mod_ov').classList.add('show');"
"var lang=localStorage.lang||'zh';"
"document.getElementById('mod_title').textContent=(lang==='zh'?g_title_zh:g_title_en)[m];"
"document.querySelectorAll('.r-btn').forEach(function(b){b.classList.remove('active');});"
"var b=document.querySelector('.r-btn[data-h=\"1\"]');if(b)b.classList.add('active');"
"loadHistory();}"
"function closeHistory(e){if(e&&e.target!==document.getElementById('mod_ov'))return;"
"document.getElementById('mod_ov').classList.remove('show');"
"if(g_chart){g_chart.destroy();g_chart=null;}}"
"function setRange(h){g_range=h;"
"document.querySelectorAll('.r-btn').forEach(function(b){b.classList.remove('active');});"
"var b=document.querySelector('.r-btn[data-h=\"'+h+'\"]');if(b)b.classList.add('active');"
"loadHistory();}"
"function loadHistory(){if(typeof Chart==='undefined'){console.log('Chart.js not loaded');return;}"
"var c=document.getElementById('hist_canvas');"
"if(g_chart){g_chart.destroy();g_chart=null;}"
"var emp=document.getElementById('hist_empty');"
"fetch('/history?hours='+g_range).then(function(r){return r.json();}).then(function(d){"
"if(!d||!d.length){"
"if(emp){c.style.display='none';emp.style.display='flex';}"
"return;}"
"c.style.display='block';if(emp)emp.style.display='none';"
"var labels=[],vals=[],key=g_metric===0?'t':g_metric===1?'tvoc':g_metric===2?'co2':'ch2o';"
"var lang=localStorage.lang||'zh',lbls=lang==='zh'?g_label_zh:g_label_en;"
"for(var i=0;i<d.length;i++){labels.push(fmtTime(d[i].ts));vals.push(g_metric===0?d[i].t:d[i][key]);}"
"var co=g_colors[g_metric];"
"g_chart=new Chart(c,{type:'line',data:{labels:labels,datasets:[{label:lbls[g_metric],data:vals,borderColor:co,backgroundColor:g_bg[g_metric],borderWidth:2,pointRadius:0,pointHoverRadius:5,pointHoverBorderWidth:2,pointHoverBorderColor:co,tension:.3,fill:true}]},"
"options:{responsive:true,maintainAspectRatio:false,"
"interaction:{mode:'index',intersect:false},"
"scales:{x:{ticks:{maxTicksLimit:12,color:'#81a089'},grid:{color:'rgba(129,160,137,.12)'}},"
"y:{ticks:{color:'#81a089'},grid:{color:'rgba(129,160,137,.12)'},beginAtZero:g_metric!==0}},"
"plugins:{legend:{display:false},tooltip:{backgroundColor:'rgba(30,40,35,.92)',titleColor:'#a0b8a5',bodyColor:'#fff',borderColor:co,borderWidth:1,cornerRadius:6,displayColors:false,"
"callbacks:{label:function(ctx){return ctx.dataset.label+': '+ctx.raw;}}}}}});"
"}).catch(function(e){console.log('history fetch error',e);"
"if(emp){c.style.display='none';emp.style.display='flex';}});}"
"function fmtTime(ts){var d=new Date(ts*1000),h=d.getHours(),m=d.getMinutes();"
"return (h<10?'0':'')+h+':'+(m<10?'0':'')+m;}"
"fetch('/version').then(function(r){return r.json();}).then(function(v){"
"document.getElementById('fw_ver').textContent=v.ver+' ('+v.time+')';"
"}).catch(function(){});"
"function initSoundCfg(){if(g_soundInit)return;"
"fetch('/sound_cfg').then(function(r){return r.json();}).then(function(j){"
"document.getElementById('key_en_chk').checked=j.key_en!=0;"
"document.getElementById('key_vol_sl').value=j.key_vol;"
"document.getElementById('key_vol_val').textContent=j.key_vol+'%';"
"document.getElementById('key_mel_sel').value=j.key_mel;"
"document.getElementById('pwr_en_chk').checked=j.pwr_en!=0;"
"document.getElementById('pwr_vol_sl').value=j.pwr_vol;"
"document.getElementById('pwr_vol_val').textContent=j.pwr_vol+'%';"
"document.getElementById('pwon_mel_sel').value=j.pwon_mel;"
"document.getElementById('pwoff_mel_sel').value=j.pwoff_mel;"
"document.getElementById('alarm_en_chk').checked=j.alarm_en!=0;"
"document.getElementById('alarm_vol_sl').value=j.alarm_vol||70;"
"document.getElementById('alarm_mel_sel').value=j.alarm_mel;"
"g_soundInit=true;"
"});}"
"function onSoundChange(){"
"var b=document.getElementById('sound_save_btn');if(b)b.style.background='linear-gradient(135deg,#e53935,#ef5350)';"
"}"
"function onKeyVolInput(){"
"document.getElementById('key_vol_val').textContent=document.getElementById('key_vol_sl').value+'%';"
"}"
"function onPwrVolInput(){"
"document.getElementById('pwr_vol_val').textContent=document.getElementById('pwr_vol_sl').value+'%';"
"}"
"function previewSound(type){"
"var selMap={key:'key_mel_sel',pwon:'pwon_mel_sel',pwoff:'pwoff_mel_sel'};"
"var sel=document.getElementById(selMap[type]);if(!sel)return;"
"var idx=sel.value;"
"fetch('/sound_preview?type='+type+'&idx='+idx);"
"}"
"function saveSound(){"
"var el=document.getElementById('sound_res');"
"el.textContent=t('saving');el.style.color='var(--text3)';"
"var body='key_en='+(document.getElementById('key_en_chk').checked?1:0)"
"+'&key_vol='+document.getElementById('key_vol_sl').value"
"+'&key_mel='+document.getElementById('key_mel_sel').value"
"+'&pwr_en='+(document.getElementById('pwr_en_chk').checked?1:0)"
"+'&pwr_vol='+document.getElementById('pwr_vol_sl').value"
"+'&pwon_mel='+document.getElementById('pwon_mel_sel').value"
"+'&pwoff_mel='+document.getElementById('pwoff_mel_sel').value"
"+'&alarm_en='+(document.getElementById('alarm_en_chk').checked?1:0)"
"+'&alarm_vol='+document.getElementById('alarm_vol_sl').value"
"+'&alarm_mel='+document.getElementById('alarm_mel_sel').value;"
"fetch('/sound_cfg',{method:'POST',body:body}).then(function(r){return r.json();}).then(function(j){"
"if(j.ok){el.textContent=t('sound_saved');el.style.color='var(--green)';g_soundInit=false;"
"var b=document.getElementById('sound_save_btn');if(b)b.style.background='';}"
"else{el.textContent=t('failed');el.style.color='#e53935';}"
"});}"
"var g_soundInit=false;"
"initSoundCfg();"
"</script>"
"<footer style='text-align:center;padding:12px 0;font-size:13px;color:#5a7a60;font-weight:500;'>"
"<span id=fw_ver>--</span> &nbsp; "
"<a href='/logs.txt' style='color:#43a047;text-decoration:underline;font-size:12px'>下载日志</a></footer>"
"</body></html>";

/* /history?hours=N — sensor data for line charts */
static esp_err_t http_history_handler(httpd_req_t *req)
{
    char qbuf[16];
    int hours = 1;

    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK) {
        char val[8];
        if (httpd_query_key_value(qbuf, "hours", val, sizeof(val)) == ESP_OK) {
            int n = atoi(val);
            if (n == 1 || n == 3 || n == 6 || n == 12 || n == 24) {
                hours = n;
            }
        }
    }

    time_t now = time(NULL);
    time_t since = now - (hours * 3600);

    sensor_sample_t buf[SENSOR_HISTORY_QUERY_LIMIT];
    int count = sensor_history_query(since, buf, SENSOR_HISTORY_QUERY_LIMIT);

    ESP_LOGI(TAG, "/history: hours=%d since=%lld count=%d", hours, (long long)since, count);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr_chunk(req, "[");

    for (int i = 0; i < count; i++) {
        char line[128];
        snprintf(line, sizeof(line),
            "%c{\"ts\":%lld,\"t\":%.1f,\"tvoc\":%u,\"co2\":%u,\"ch2o\":%u}",
            i == 0 ? ' ' : ',',
            (long long)buf[i].ts, buf[i].temp,
            buf[i].tvoc_ugm3, buf[i].co2_ppm, buf[i].ch2o_ugm3);
        if (httpd_resp_sendstr_chunk(req, line) != ESP_OK) break;
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);  /* terminate chunked response */
    return ESP_OK;
}

static esp_err_t http_calibrate_handler(httpd_req_t *req)
{
    char buf[16];
    float t = 25.0f;

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char val[8];
        if (httpd_query_key_value(buf, "t", val, sizeof(val)) == ESP_OK) {
            t = atof(val);
        }
    }

    bool ok = ds18b20_calibrate(t);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    if (ok) {
        char rsp[64];
        snprintf(rsp, sizeof(rsp), "{\"ok\":true,\"ref\":%.1f}", (double)t);
        httpd_resp_sendstr(req, rsp);
    } else {
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"sensor unavailable\"}");
    }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════
   HTTP handlers
   ═══════════════════════════════════════════════════════════════════ */

static esp_err_t http_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (g_http_normal_mode) {
        httpd_resp_send(req, HTML_NORMAL, strlen(HTML_NORMAL));
    } else {
        httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    }
    return ESP_OK;
}

/* /version — returns build info as JSON */
static esp_err_t http_version_handler(httpd_req_t *req)
{
    char json[128];
    snprintf(json, sizeof(json), "{\"ver\":\"v3\",\"time\":\"%s %s\"}",
             __DATE__, __TIME__);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static esp_err_t http_save_handler(httpd_req_t *req)
{
    char buf[200];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_sendstr(req, "");
        return ESP_OK;
    }
    buf[len] = '\0';

    char ssid[33] = {0}, pass[65] = {0};

    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';

        char *val = eq + 1;
        size_t vlen = strlen(val);
        char decoded[128] = {0};
        size_t di = 0;
        for (size_t i = 0; i < vlen && di < sizeof(decoded) - 1; i++) {
            if (val[i] == '+') decoded[di++] = ' ';
            else if (val[i] == '%' && i + 2 < vlen) {
                char hex[3] = {val[i+1], val[i+2], 0};
                decoded[di++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else decoded[di++] = val[i];
        }

        if      (strcmp(p, "s") == 0) strncpy(ssid, decoded, sizeof(ssid) - 1);
        else if (strcmp(p, "p") == 0) strncpy(pass, decoded, sizeof(pass) - 1);

        p = amp ? amp + 1 : NULL;
    }

    if (strlen(ssid) == 0) {
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_sendstr(req, "");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Saving WiFi: SSID=%s", ssid);

    settings_save_str(NVS_KEY_WIFI_SSID, ssid);
    settings_save_str(NVS_KEY_WIFI_PASS, pass);
    settings_commit();

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML_DONE, strlen(HTML_DONE));

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t http_scan_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, g_scan_cache);
    return ESP_OK;
}

static esp_err_t http_rescan_handler(httpd_req_t *req)
{
    scan_cache_refresh();
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, g_scan_cache);
    return ESP_OK;
}

/* /status — current sensor readings + alarm thresholds */
static esp_err_t http_status_handler(httpd_req_t *req)
{
    uint16_t tvoc_thr = 500, co2_thr = 1000, ch2o_thr = 100;
    bool auto_fan = true;
    uint32_t u32;
    if (settings_get_u32(NVS_KEY_TVOC_ALARM, &u32) == ESP_OK) tvoc_thr = u32;
    if (settings_get_u32(NVS_KEY_CO2_ALARM, &u32) == ESP_OK) co2_thr = u32;
    if (settings_get_u32(NVS_KEY_CH2O_ALARM, &u32) == ESP_OK) ch2o_thr = u32;
    if (settings_get_u32(NVS_KEY_AUTO_FAN_ENABLE, &u32) == ESP_OK) auto_fan = (u32 != 0);

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    const char *holiday_name = holiday_get_name(ti.tm_mon + 1, ti.tm_mday);

    char json[512];
    snprintf(json, sizeof(json),
        "{\"t\":%.1f,\"tvoc\":%u,\"co2\":%u,\"ch2o\":%u,"
        "\"fs\":%u,\"fo\":%d,\"rpm\":%u,"
        "\"tvoc_thr\":%u,\"co2_thr\":%u,\"ch2o_thr\":%u,\"auto_fan\":%d,"
        "\"alarm_cd\":%u,\"holiday\":\"%s\"}",
        g_sensor.temp_c, g_sensor.tvoc_ugm3, g_sensor.co2_ppm,
        g_sensor.ch2o_ugm3, g_sensor.fan_speed,
        g_sensor.fan_on ? 1 : 0, g_sensor.fan_rpm,
        tvoc_thr, co2_thr, ch2o_thr, auto_fan ? 1 : 0,
        app_controller_get_alarm_cooldown(),
        holiday_name ? holiday_name : "");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* /set — fan control from web dashboard */
static esp_err_t http_set_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    uint8_t fs = g_sensor.fan_speed;
    bool fo = g_sensor.fan_on;

    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';

        int val = atoi(eq + 1);
        if (val < 0) val = 0;

        if (strcmp(p, "fs") == 0)      { fs = val > 100 ? 100 : (uint8_t)val; }
        else if (strcmp(p, "fo") == 0) { fo = (val != 0); }

        p = amp ? amp + 1 : NULL;
    }

    /* Actually control the motor */
    if (fs != g_sensor.fan_speed || fo != g_sensor.fan_on) {
        app_controller_set_fan_speed(fo ? fs : 0);
    }

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /netinfo — network & MQTT status */
static esp_err_t http_netinfo_handler(httpd_req_t *req)
{
    char ssid[33] = "N/A", ip[16] = "0.0.0.0", mqtt[128] = "N/A";
    bool mqtt_ok = (mqtt_ha_get_state() == MQTT_STATE_CONNECTED);

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
        }
        wifi_config_t wifi_cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
            strncpy(ssid, (const char *)wifi_cfg.sta.ssid, sizeof(ssid) - 1);
        }
    }

    settings_get_str(NVS_KEY_MQTT_URI, mqtt, sizeof(mqtt));
    if (mqtt[0] == '\0') strncpy(mqtt, "mqtt://homeassistant.local:1883", sizeof(mqtt) - 1);

    uint32_t home_scr = 0;
    settings_get_u32(NVS_KEY_HOME_SCREEN, &home_scr);
    if (home_scr >= 5) home_scr = 0;   /* UI_SCREEN_COUNT = 5 */

    uint8_t lr, lg, lb;
    app_controller_get_led_rgb(&lr, &lg, &lb);
    char json[512];
    snprintf(json, sizeof(json),
        "{\"ssid\":\"%s\",\"ip\":\"%s\",\"mqtt\":\"%s\",\"mqtt_ok\":%d,\"home_scr\":%lu,\"led_on\":%d,"
        "\"led_r\":%u,\"led_g\":%u,\"led_b\":%u,\"led_bri\":%u}",
        ssid, ip, mqtt, mqtt_ok ? 1 : 0, home_scr, app_controller_get_status_led() ? 1 : 0,
        lr, lg, lb, app_controller_get_led_brightness());
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* /mqtt_test — test broker connectivity */
static esp_err_t http_mqtt_test_handler(httpd_req_t *req)
{
    char qbuf[128], raw[96], host[64] = {0};
    int port = 1883;

    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK &&
        httpd_query_key_value(qbuf, "broker", raw, sizeof(raw)) == ESP_OK) {

        /* URL-decode: httpd_query_key_value should decode, but some ESP-IDF
         * versions don't.  Do it manually so encodeURIComponent'd URIs work. */
        char val[96];
        size_t di = 0;
        for (size_t i = 0; raw[i] && di < sizeof(val) - 1; i++) {
            if (raw[i] == '+') val[di++] = ' ';
            else if (raw[i] == '%' && raw[i+1] && raw[i+2]) {
                char hex[3] = {raw[i+1], raw[i+2], 0};
                val[di++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else val[di++] = raw[i];
        }
        val[di] = '\0';

        const char *p = val;
        if (strncmp(p, "mqtt://", 7) == 0) p += 7;
        else if (strncmp(p, "tcp://", 6) == 0) p += 6;
        const char *s = p;
        while (*p && *p != ':') p++;
        size_t hl = p - s;
        if (hl > sizeof(host) - 1) hl = sizeof(host) - 1;
        memcpy(host, s, hl);
        host[hl] = '\0';
        if (*p == ':') port = atoi(p + 1);
        if (port <= 0) port = 1883;
    }

    char json[192];
    if (host[0] == '\0') {
        snprintf(json, sizeof(json), "{\"ok\":0,\"error\":\"未输入地址\"}");
    } else {
        /* Try numeric IP first, bypass DNS for robustness */
        struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
        int is_numeric = inet_aton(host, &addr.sin_addr);

        if (!is_numeric) {
            struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
            struct addrinfo *res;
            if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
                snprintf(json, sizeof(json), "{\"ok\":0,\"error\":\"DNS 解析失败\"}");
                httpd_resp_set_type(req, "application/json; charset=utf-8");
                httpd_resp_sendstr(req, json);
                return ESP_OK;
            }
            addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
            freeaddrinfo(res);
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct timeval tv = {.tv_sec = 3, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        int ok = (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
        close(sock);
        snprintf(json, sizeof(json),
            "{\"ok\":%d,\"ip\":\"%s\",\"port\":%d%s}",
            ok, inet_ntoa(addr.sin_addr), port,
            ok ? "" : ",\"error\":\"TCP 连接失败\"");
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* /mqtt_save — save MQTT broker (normalize URI) and restart */
static esp_err_t http_mqtt_save_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    char *eq = strchr(buf, '=');
    if (!eq) {
        httpd_resp_sendstr(req, "{\"ok\":0,\"error\":\"请求格式错误\"}");
        return ESP_OK;
    }
    *eq = '\0';
    char *val = eq + 1;

    /* URL-decode the value */
    char decoded[128] = {0};
    size_t di = 0;
    for (size_t i = 0; val[i] && di < sizeof(decoded) - 1; i++) {
        if (val[i] == '+') decoded[di++] = ' ';
        else if (val[i] == '%' && val[i+1] && val[i+2]) {
            char hex[3] = {val[i+1], val[i+2], 0};
            decoded[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else decoded[di++] = val[i];
    }

    if (strcmp(buf, "broker") != 0 || decoded[0] == '\0') {
        httpd_resp_sendstr(req, "{\"ok\":0,\"error\":\"缺少代理地址\"}");
        return ESP_OK;
    }

    /* Normalize: ensure mqtt:// prefix and :1883 default port */
    char uri[160];
    if (strncmp(decoded, "mqtt://", 7) != 0) {
        snprintf(uri, sizeof(uri), "mqtt://%s", decoded);
    } else {
        strncpy(uri, decoded, sizeof(uri) - 1);
    }
    uri[sizeof(uri) - 1] = '\0';
    /* If no port specified, append default */
    char *host_start = uri + 7; /* after mqtt:// */
    if (!strchr(host_start, ':')) {
        snprintf(uri + strlen(uri), sizeof(uri) - strlen(uri), ":1883");
    }

    ESP_LOGI(TAG, "Saving MQTT broker: %s", uri);
    settings_save_str(NVS_KEY_MQTT_URI, uri);
    settings_commit();
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* /alarm_cfg — save alarm thresholds + auto-fan */
static esp_err_t http_alarm_cfg_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    uint16_t tvoc_thr = 500, co2_thr = 1000, ch2o_thr = 100;
    int auto_fan = 1, alarm_cd = 60;

    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';

        int val = atoi(eq + 1);
        if      (strcmp(p, "tvoc_thr") == 0)  tvoc_thr = val > 0 ? (uint16_t)val : 500;
        else if (strcmp(p, "co2_thr") == 0)   co2_thr  = val > 0 ? (uint16_t)val : 1000;
        else if (strcmp(p, "ch2o_thr") == 0)  ch2o_thr = val > 0 ? (uint16_t)val : 100;
        else if (strcmp(p, "auto_fan") == 0)  auto_fan = val ? 1 : 0;

        p = amp ? amp + 1 : NULL;
    }

    settings_save_u32(NVS_KEY_TVOC_ALARM, tvoc_thr);
    settings_save_u32(NVS_KEY_CO2_ALARM, co2_thr);
    settings_save_u32(NVS_KEY_CH2O_ALARM, ch2o_thr);
    settings_save_u32(NVS_KEY_AUTO_FAN_ENABLE, auto_fan);
    settings_save_u32(NVS_KEY_ALARM_COOLDOWN, alarm_cd);
    settings_commit();

    app_controller_set_alarm_threshold(tvoc_thr, co2_thr, ch2o_thr);
    app_controller_set_auto_fan(auto_fan != 0);
    app_controller_set_alarm_cooldown((uint16_t)alarm_cd);

    ESP_LOGI(TAG, "Alarm cfg saved: TVOC=%u CO2=%u CH2O=%u auto=%d cd=%d",
             tvoc_thr, co2_thr, ch2o_thr, auto_fan, alarm_cd);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /sound_cfg — GET: read all sound settings; POST: save sound settings */
static esp_err_t http_sound_cfg_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"key_en\":%d,\"key_vol\":%u,\"key_mel\":%u,"
            "\"pwr_en\":%d,\"pwr_vol\":%u,\"pwon_mel\":%u,\"pwoff_mel\":%u,"
            "\"alarm_en\":%d,\"alarm_vol\":%u,\"alarm_mel\":%u}",
            app_controller_get_key_sound() ? 1 : 0,
            app_controller_get_key_volume(),
            app_controller_get_key_melody(),
            app_controller_get_power_sound() ? 1 : 0,
            app_controller_get_power_volume(),
            app_controller_get_power_on_melody(),
            app_controller_get_power_off_melody(),
            app_controller_get_alarm_sound() ? 1 : 0,
            app_controller_get_alarm_volume(),
            app_controller_get_alarm_melody());
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    /* POST: parse and save */
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    int key_en = -1, key_vol = -1, key_mel = -1;
    int pwr_en = -1, pwr_vol = -1, pwon_mel = -1, pwoff_mel = -1;
    int alarm_en = -1, alarm_vol = -1, alarm_mel = -1;
    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';
        int val = atoi(eq + 1);
        if      (strcmp(p, "key_en")   == 0) key_en   = val;
        else if (strcmp(p, "key_vol")  == 0) key_vol  = val;
        else if (strcmp(p, "key_mel")  == 0) key_mel  = val;
        else if (strcmp(p, "pwr_en")   == 0) pwr_en   = val;
        else if (strcmp(p, "pwr_vol")  == 0) pwr_vol  = val;
        else if (strcmp(p, "pwon_mel") == 0) pwon_mel = val;
        else if (strcmp(p, "pwoff_mel")== 0) pwoff_mel= val;
        else if (strcmp(p, "alarm_en") == 0) alarm_en = val;
        else if (strcmp(p, "alarm_vol")== 0) alarm_vol= val;
        else if (strcmp(p, "alarm_mel")== 0) alarm_mel= val;
        p = amp ? amp + 1 : NULL;
    }

    if (key_en   >= 0) app_controller_set_key_sound(key_en != 0);
    if (key_vol  >= 0) app_controller_set_key_volume((uint8_t)key_vol);
    if (key_mel  >= 0) app_controller_set_key_melody((uint8_t)key_mel);
    if (pwr_en   >= 0) app_controller_set_power_sound(pwr_en != 0);
    if (pwr_vol  >= 0) app_controller_set_power_volume((uint8_t)pwr_vol);
    if (pwon_mel >= 0) app_controller_set_power_on_melody((uint8_t)pwon_mel);
    if (pwoff_mel>= 0) app_controller_set_power_off_melody((uint8_t)pwoff_mel);
    if (alarm_en >= 0) app_controller_set_alarm_sound(alarm_en != 0);
    if (alarm_vol>= 0) app_controller_set_alarm_volume((uint8_t)alarm_vol);
    if (alarm_mel>= 0) app_controller_set_alarm_melody((uint8_t)alarm_mel);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /sound_preview?type=key&idx=1 — preview a melody without persisting */
static esp_err_t http_sound_preview_handler(httpd_req_t *req)
{
    char qbuf[64];
    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK) {
        char val[8];
        int type = -1, idx = 0;
        if (httpd_query_key_value(qbuf, "type", val, sizeof(val)) == ESP_OK) {
            if (strcmp(val, "key") == 0) type = 0;
            else if (strcmp(val, "pwon") == 0) type = 1;
            else if (strcmp(val, "pwoff") == 0) type = 2;
            else if (strcmp(val, "alarm") == 0) type = 3;
        }
        if (httpd_query_key_value(qbuf, "idx", val, sizeof(val)) == ESP_OK) {
            idx = atoi(val);
        }
        if (type == 0)      app_controller_preview_key_melody((uint8_t)idx);
        else if (type == 1) app_controller_preview_power_on_melody((uint8_t)idx);
        else if (type == 2) app_controller_preview_power_off_melody((uint8_t)idx);
        else if (type == 3) app_controller_preview_alarm_melody((uint8_t)idx);
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /home_screen — save default home screen + restart */
static esp_err_t http_home_screen_handler(httpd_req_t *req)
{
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    int hs = 0;
    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        if (!eq) break;
        char *amp = strchr(p, '&');
        if (amp) *amp = '\0';
        *eq = '\0';
        if (strcmp(p, "hs") == 0) hs = atoi(eq + 1);
        p = amp ? amp + 1 : NULL;
    }

    if (hs < 0 || hs >= 5) hs = 0;  /* UI_SCREEN_COUNT = 5 */

    ESP_LOGI(TAG, "Saving home screen: %d", hs);
    settings_save_u32(NVS_KEY_HOME_SCREEN, (uint32_t)hs);
    settings_commit();

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* /led — toggle status LED */
static esp_err_t http_led_handler(httpd_req_t *req)
{
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        bool on = (strstr(buf, "on=1") != NULL);
        ESP_LOGI(TAG, "LED toggle: %s (raw='%s')", on ? "ON" : "OFF", buf);
        app_controller_set_status_led(on);
    } else {
        ESP_LOGW(TAG, "LED toggle: no body received (len=%d)", len);
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /led_cfg — GET: return current LED color + brightness */
static esp_err_t http_led_cfg_get_handler(httpd_req_t *req)
{
    uint8_t r, g, b;
    app_controller_get_led_rgb(&r, &g, &b);
    uint8_t bri = app_controller_get_led_brightness();
    const led_state_cfg_t *cfg = app_controller_get_led_state_cfg(LED_STATE_NORMAL);
    char json[80];
    snprintf(json, sizeof(json), "{\"r\":%u,\"g\":%u,\"b\":%u,\"bri\":%u,\"eff\":%u}",
             r, g, b, bri, cfg->effect);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* /led_cfg — POST: save LED color + brightness */
static esp_err_t http_led_cfg_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "LED cfg handler called");
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        ESP_LOGW(TAG, "LED cfg: no body (len=%d)", len);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    buf[len] = '\0';

    int r = -1, g = -1, b = -1, bri = -1, eff = -1;
    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(p, "r") == 0) r = val;
        else if (strcmp(p, "g") == 0) g = val;
        else if (strcmp(p, "b") == 0) b = val;
        else if (strcmp(p, "bri") == 0) bri = val;
        else if (strcmp(p, "eff") == 0) eff = val;
        p = amp ? amp + 1 : NULL;
    }

    if (r >= 0 && g >= 0 && b >= 0) {
        ESP_LOGI(TAG, "LED cfg: R=%d G=%d B=%d bri=%d eff=%d (raw='%s')", r, g, b, bri, eff, buf);
        app_controller_set_led_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    if (bri >= 0)
        app_controller_set_led_brightness((uint8_t)bri);
    if (eff >= 0)
        app_controller_set_led_effect((uint8_t)eff);

    /* Changing color implies user wants to see the LED */
    if (r >= 0 && g >= 0 && b >= 0 && !app_controller_get_status_led())
        app_controller_set_status_led(true);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /led_states — GET: all state LED configs; POST: save one state's config */
static esp_err_t http_led_states_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        char json[512];
        int off = snprintf(json, sizeof(json), "[");
        for (int i = 0; i < LED_STATE_COUNT; i++) {
            const led_state_cfg_t *c = app_controller_get_led_state_cfg((led_state_id_t)i);
            off += snprintf(json + off, sizeof(json) - off,
                "%s{\"id\":%d,\"r\":%u,\"g\":%u,\"b\":%u,\"eff\":%u}",
                i > 0 ? "," : "", i, c->r, c->g, c->b, c->effect);
        }
        snprintf(json + off, sizeof(json) - off, "]");
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "led_states POST hit: uri=%s", req->uri);

    /* POST: save one state */
    char buf[96];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        ESP_LOGW(TAG, "led_states POST: body read fail len=%d", len);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    buf[len] = '\0';
    ESP_LOGI(TAG, "led_states POST body: '%s'", buf);

    int id = -1, r = -1, g = -1, b = -1, eff = -1;
    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(p, "id") == 0) id = val;
        else if (strcmp(p, "r") == 0) r = val;
        else if (strcmp(p, "g") == 0) g = val;
        else if (strcmp(p, "b") == 0) b = val;
        else if (strcmp(p, "eff") == 0) eff = val;
        p = amp ? amp + 1 : NULL;
    }

    if (id < 0 || id >= LED_STATE_COUNT || r < 0 || g < 0 || b < 0 || eff < 0) {
        ESP_LOGW(TAG, "led_states POST: param error id=%d r=%d g=%d b=%d eff=%d",
                 id, r, g, b, eff);
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_sendstr(req, "{\"ok\":0,\"error\":\"缺少参数\"}");
        return ESP_OK;
    }

    led_state_cfg_t cfg = { .r = (uint8_t)r, .g = (uint8_t)g, .b = (uint8_t)b, .effect = (uint8_t)eff };
    app_controller_set_led_state_cfg((led_state_id_t)id, &cfg);
    ESP_LOGI(TAG, "LED state %d cfg: R=%d G=%d B=%d eff=%d", id, r, g, b, eff);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /schedule — GET: return current schedule; POST: save schedule */
static esp_err_t http_schedule_get_handler(httpd_req_t *req)
{
    uint8_t off_en, on_en, off_h, off_m, on_h, on_m, off_day, on_day, off_mask, on_mask;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_EN, &off_en) != ESP_OK) off_en = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_EN, &on_en) != ESP_OK) on_en = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_H, &off_h) != ESP_OK) off_h = 23;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_M, &off_m) != ESP_OK) off_m = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_H, &on_h) != ESP_OK) on_h = 7;
    if (settings_get_u8(NVS_KEY_SCHED_ON_M, &on_m) != ESP_OK) on_m = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_DAY, &off_day) != ESP_OK) off_day = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_DAY, &on_day) != ESP_OK) on_day = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_MASK, &off_mask) != ESP_OK) off_mask = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_MASK, &on_mask) != ESP_OK) on_mask = 0;
    char json[200];
    snprintf(json, sizeof(json),
        "{\"off_en\":%u,\"on_en\":%u,\"off_day\":%u,\"off_h\":%u,\"off_m\":%u,\"off_days\":%u,\"on_day\":%u,\"on_h\":%u,\"on_m\":%u,\"on_days\":%u}",
        off_en, on_en, off_day, off_h, off_m, off_mask, on_day, on_h, on_m, on_mask);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static esp_err_t http_schedule_handler(httpd_req_t *req)
{
    char buf[200];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';
    ESP_LOGI(TAG, "Schedule POST received: %s", buf);

    int off_en = -1, on_en = -1, off_day = -1, off_h = -1, off_m = -1;
    int on_day = -1, on_h = -1, on_m = -1;
    int off_days = -1, on_days = -1;
    char *p = buf;
    while (p && *p) {
        char *eq = strchr(p, '='), *amp = strchr(p, '&');
        if (!eq) break;
        if (amp) *amp = '\0';
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(p, "off_en") == 0) off_en = val;
        else if (strcmp(p, "on_en") == 0) on_en = val;
        else if (strcmp(p, "off_day") == 0) off_day = val;
        else if (strcmp(p, "off_h") == 0) off_h = val;
        else if (strcmp(p, "off_m") == 0) off_m = val;
        else if (strcmp(p, "off_days") == 0) off_days = val;
        else if (strcmp(p, "on_day") == 0) on_day = val;
        else if (strcmp(p, "on_h") == 0) on_h = val;
        else if (strcmp(p, "on_m") == 0) on_m = val;
        else if (strcmp(p, "on_days") == 0) on_days = val;
        p = amp ? amp + 1 : NULL;
    }
    if (off_en >= 0)  settings_save_u8(NVS_KEY_SCHED_OFF_EN, (uint8_t)off_en);
    if (on_en >= 0)   settings_save_u8(NVS_KEY_SCHED_ON_EN, (uint8_t)on_en);
    if (off_day >= 0) settings_save_u8(NVS_KEY_SCHED_OFF_DAY, (uint8_t)off_day);
    if (off_h >= 0)   settings_save_u8(NVS_KEY_SCHED_OFF_H, (uint8_t)off_h);
    if (off_m >= 0)   settings_save_u8(NVS_KEY_SCHED_OFF_M, (uint8_t)off_m);
    if (off_days >= 0) settings_save_u8(NVS_KEY_SCHED_OFF_MASK, (uint8_t)off_days);
    if (on_day >= 0)  settings_save_u8(NVS_KEY_SCHED_ON_DAY, (uint8_t)on_day);
    if (on_h >= 0)    settings_save_u8(NVS_KEY_SCHED_ON_H, (uint8_t)on_h);
    if (on_m >= 0)    settings_save_u8(NVS_KEY_SCHED_ON_M, (uint8_t)on_m);
    if (on_days >= 0) settings_save_u8(NVS_KEY_SCHED_ON_MASK, (uint8_t)on_days);
    settings_commit();
    ui_screen_power_sync_schedule();

    /* Read back saved values to confirm */
    uint8_t _on_h, _on_m, _on_day, _off_h, _off_m, _off_day, _on_en, _off_en;
    if (settings_get_u8(NVS_KEY_SCHED_ON_H, &_on_h) != ESP_OK) _on_h = 7;
    if (settings_get_u8(NVS_KEY_SCHED_ON_M, &_on_m) != ESP_OK) _on_m = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_DAY, &_on_day) != ESP_OK) _on_day = 0;
    if (settings_get_u8(NVS_KEY_SCHED_ON_EN, &_on_en) != ESP_OK) _on_en = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_H, &_off_h) != ESP_OK) _off_h = 23;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_M, &_off_m) != ESP_OK) _off_m = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_DAY, &_off_day) != ESP_OK) _off_day = 0;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_EN, &_off_en) != ESP_OK) _off_en = 0;

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"ok\":1,\"off_en\":%u,\"off_h\":%u,\"off_m\":%u,\"off_day\":%u,"
        "\"on_en\":%u,\"on_h\":%u,\"on_m\":%u,\"on_day\":%u}",
        _off_en, _off_h, _off_m, _off_day,
        _on_en, _on_h, _on_m, _on_day);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

/* /power — POST: action=shutdown|screen_off|screen_on */
static esp_err_t http_power_handler(httpd_req_t *req)
{
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_404(req); return ESP_FAIL; }
    buf[len] = '\0';

    char *eq = strchr(buf, '=');
    if (!eq || strncmp(buf, "action", 6) != 0) {
        httpd_resp_send_404(req); return ESP_FAIL;
    }
    char *val = eq + 1;

    if (strcmp(val, "shutdown") == 0) {
        app_controller_request_power_action(POWER_ACTION_SHUTDOWN);
    } else if (strcmp(val, "screen_off") == 0) {
        app_controller_request_power_action(POWER_ACTION_SCREEN_OFF);
    } else if (strcmp(val, "screen_on") == 0 || strcmp(val, "wake") == 0) {
        app_controller_request_power_action(POWER_ACTION_WAKE);
    } else {
        httpd_resp_send_404(req); return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/* /update — OTA firmware upload (POST raw binary) */
static esp_err_t http_ota_handler(httpd_req_t *req)
{
    /* Get Content-Length */
    char cl_buf[16];
    size_t cl_len = httpd_req_get_hdr_value_len(req, "Content-Length");
    if (cl_len == 0 || cl_len >= sizeof(cl_buf)) {
        httpd_resp_send_err(req, HTTPD_411_LENGTH_REQUIRED, "Missing Content-Length");
        return ESP_FAIL;
    }
    httpd_req_get_hdr_value_str(req, "Content-Length", cl_buf, sizeof(cl_buf));
    int total = atoi(cl_buf);
    if (total <= 0 || total > 4 * 1024 * 1024) {  /* 4MB max */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware size");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update starting, size=%d bytes", total);

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    int received = 0;
    char buf[4096];
    while (received < total) {
        int chunk = httpd_req_recv(req, buf, sizeof(buf));
        if (chunk <= 0) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, chunk);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }
        received += chunk;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update done (%d bytes), rebooting...", received);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req,
        "<!DOCTYPE html><html><head><meta charset='utf-8' name='viewport' "
        "content='width=device-width,initial-scale=1'><title>OTA OK</title><style>"
        "body{font-family:system-ui;display:flex;align-items:center;justify-content:center;"
        "min-height:100vh;background:#f0f7f4;color:#333}"
        ".card{background:#fff;padding:32px;border-radius:16px;text-align:center;"
        "box-shadow:0 2px 16px rgba(0,0,0,.06)}"
        "h2{color:#388e3c}.spinner{width:36px;height:36px;margin:16px auto;"
        "border:3px solid #dce8df;border-top:3px solid #66bb6a;border-radius:50%;"
        "animation:spin .8s linear infinite}"
        "@keyframes spin{to{transform:rotate(360deg)}}</style></head><body>"
        "<div class=card><h2>升级成功</h2><p>设备正在重启...</p>"
        "<div class=spinner></div></div></body></html>");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t http_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    ESP_LOGW(TAG, "404 Not Found: method=%s uri=%s (err=%d)",
             req->method == HTTP_GET ? "GET" : req->method == HTTP_POST ? "POST" : "?",
             req->uri, (int)err);
    httpd_resp_sendstr(req,
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='0;url=/'>"
        "</head><body></body></html>");
    return ESP_OK;
}

static esp_err_t http_logs_handler(httpd_req_t *req);
static esp_err_t http_logs_txt_handler(httpd_req_t *req);
static vprintf_like_t g_orig_vprintf = NULL;
static int log_vprint_hook(const char *fmt, va_list args);

static bool match_globstar(const char *ref, const char *test, size_t match_len)
{
    if (strcmp(ref, "/*") == 0) return true;
    return httpd_uri_match_wildcard(ref, test, match_len);
}

static void http_register_probe(httpd_handle_t server, const char *uri)
{
    httpd_uri_t probe = {
        .uri    = uri,
        .method = HTTP_GET,
        .handler = http_root_handler,
    };
    httpd_register_uri_handler(server, &probe);
}

static esp_err_t http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 64;
    cfg.stack_size       = 10240;
    cfg.recv_wait_timeout = 30;
    cfg.uri_match_fn     = match_globstar;
    cfg.keep_alive_enable = false;
    cfg.lru_purge_enable  = true;
    cfg.send_wait_timeout = 60;

    httpd_handle_t server;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: %s (%d)", esp_err_to_name(err), err);
        return err;
    }

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_handler);

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = http_root_handler };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t version_uri = { .uri = "/version", .method = HTTP_GET, .handler = http_version_handler };
    httpd_register_uri_handler(server, &version_uri);

    httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = http_save_handler };
    httpd_register_uri_handler(server, &save);

    httpd_uri_t scan = { .uri = "/scan", .method = HTTP_GET, .handler = http_scan_handler };
    httpd_register_uri_handler(server, &scan);

    httpd_uri_t rescan_uri = { .uri = "/rescan", .method = HTTP_GET, .handler = http_rescan_handler };
    httpd_register_uri_handler(server, &rescan_uri);

    httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = http_status_handler };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t history_uri = { .uri = "/history", .method = HTTP_GET, .handler = http_history_handler };
    httpd_register_uri_handler(server, &history_uri);

    httpd_uri_t cal_uri = { .uri = "/calibrate", .method = HTTP_GET, .handler = http_calibrate_handler };
    httpd_register_uri_handler(server, &cal_uri);

    httpd_uri_t set_uri = { .uri = "/set", .method = HTTP_POST, .handler = http_set_handler };
    httpd_register_uri_handler(server, &set_uri);

    httpd_uri_t netinfo = { .uri = "/netinfo", .method = HTTP_GET, .handler = http_netinfo_handler };
    httpd_register_uri_handler(server, &netinfo);

    httpd_uri_t mqtt_test = { .uri = "/mqtt_test", .method = HTTP_GET, .handler = http_mqtt_test_handler };
    httpd_register_uri_handler(server, &mqtt_test);

    httpd_uri_t mqtt_save = { .uri = "/mqtt_save", .method = HTTP_POST, .handler = http_mqtt_save_handler };
    httpd_register_uri_handler(server, &mqtt_save);

    httpd_uri_t alarm_cfg = { .uri = "/alarm_cfg", .method = HTTP_POST, .handler = http_alarm_cfg_handler };
    httpd_register_uri_handler(server, &alarm_cfg);

    httpd_uri_t led_uri = { .uri = "/led", .method = HTTP_POST, .handler = http_led_handler };
    httpd_register_uri_handler(server, &led_uri);

    httpd_uri_t led_cfg_get = { .uri = "/led_cfg", .method = HTTP_GET, .handler = http_led_cfg_get_handler };
    httpd_register_uri_handler(server, &led_cfg_get);

    httpd_uri_t led_cfg = { .uri = "/led_cfg", .method = HTTP_POST, .handler = http_led_cfg_handler };
    httpd_register_uri_handler(server, &led_cfg);

    httpd_uri_t led_states = { .uri = "/led_states", .method = HTTP_GET, .handler = http_led_states_handler };
    httpd_register_uri_handler(server, &led_states);
    /* Register POST for same URI (need separate entry for different method) */
    led_states.method = HTTP_POST;
    httpd_register_uri_handler(server, &led_states);

    httpd_uri_t home_screen = { .uri = "/home_screen", .method = HTTP_POST, .handler = http_home_screen_handler };
    httpd_register_uri_handler(server, &home_screen);

    httpd_uri_t sound_cfg_get = { .uri = "/sound_cfg", .method = HTTP_GET, .handler = http_sound_cfg_handler };
    httpd_register_uri_handler(server, &sound_cfg_get);
    httpd_uri_t sound_cfg_post = { .uri = "/sound_cfg", .method = HTTP_POST, .handler = http_sound_cfg_handler };
    httpd_register_uri_handler(server, &sound_cfg_post);
    httpd_uri_t sound_prev = { .uri = "/sound_preview", .method = HTTP_GET, .handler = http_sound_preview_handler };
    httpd_register_uri_handler(server, &sound_prev);
    httpd_uri_t logs_uri = { .uri = "/logs", .method = HTTP_GET, .handler = http_logs_handler };
    httpd_register_uri_handler(server, &logs_uri);
    httpd_uri_t logs_txt_uri = { .uri = "/logs.txt", .method = HTTP_GET, .handler = http_logs_txt_handler };
    httpd_register_uri_handler(server, &logs_txt_uri);

    httpd_uri_t schedule_get = { .uri = "/schedule", .method = HTTP_GET, .handler = http_schedule_get_handler };
    httpd_register_uri_handler(server, &schedule_get);

    httpd_uri_t schedule_uri = { .uri = "/schedule", .method = HTTP_POST, .handler = http_schedule_handler };
    httpd_register_uri_handler(server, &schedule_uri);

    httpd_uri_t power_uri = { .uri = "/power", .method = HTTP_POST, .handler = http_power_handler };
    httpd_register_uri_handler(server, &power_uri);

    httpd_uri_t ota_uri = { .uri = "/update", .method = HTTP_POST, .handler = http_ota_handler };
    httpd_register_uri_handler(server, &ota_uri);

    /* Captive portal probes */
    http_register_probe(server, "/generate_204");
    http_register_probe(server, "/hotspot-detect.html");
    http_register_probe(server, "/library/test/success.html");
    http_register_probe(server, "/success.txt");
    http_register_probe(server, "/ncsi.txt");
    http_register_probe(server, "/connecttest.txt");
    http_register_probe(server, "/redirect");
    http_register_probe(server, "/canonical.html");
    http_register_probe(server, "/favicon.ico");
    http_register_probe(server, "/mobile/status.php");
    http_register_probe(server, "/gen_204");
    http_register_probe(server, "/kindle-wifi/wifistub.html");
    http_register_probe(server, "/fwlink/");
    http_register_probe(server, "/check_network_status.txt");

    /* Wildcard: catch all other URIs */
    httpd_uri_t wc = { .uri = "/*", .method = HTTP_ANY, .handler = http_root_handler };
    httpd_register_uri_handler(server, &wc);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC API
   ═══════════════════════════════════════════════════════════════════ */

void wifi_prov_init(void)
{
    ESP_LOGI(TAG, "Initialized");
}

bool wifi_prov_is_provisioned(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "is_provisioned: NVS open failed (%s) → false",
                 esp_err_to_name(err));
        return false;
    }

    char ssid[33] = {0};
    size_t len = sizeof(ssid);
    err = nvs_get_str(handle, NVS_KEY_WIFI_SSID, ssid, &len);
    nvs_close(handle);
    bool provisioned = (err == ESP_OK && ssid[0] != '\0');
    ESP_LOGI(TAG, "is_provisioned: err=%s ssid='%s' → %s",
             esp_err_to_name(err), ssid, provisioned ? "true" : "false");
    return provisioned;
}

void wifi_prov_start(void)
{
    if (g_state == WIFI_PROV_STATE_STARTED) {
        ESP_LOGW(TAG, "Provisioning already in progress");
        return;
    }

    ESP_LOGI(TAG, "Starting provisioning AP...");
    set_state(WIFI_PROV_STATE_STARTED);

    /* AP netif already created in app_controller common init */

    /* Configure AP */
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = PROV_SOFTAP_SSID,
            .ssid_len = strlen(PROV_SOFTAP_SSID),
            .password = "",
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    /* Wait for STA interface ready, then scan */
    vTaskDelay(pdMS_TO_TICKS(800));
    scan_cache_refresh();

    /* Start DNS hijack + HTTP */
    xTaskCreate(dns_task, "dns", 3072, NULL, 5, NULL);
    http_start();

    ESP_LOGI(TAG, "AP '%s' ready — connect phone, open browser", PROV_SOFTAP_SSID);
    ESP_LOGI(TAG, "If captive portal doesn't pop up, go to http://192.168.4.1");
}

void wifi_prov_stop(void)
{
    set_state(WIFI_PROV_STATE_IDLE);
}

wifi_prov_state_t wifi_prov_get_state(void) { return g_state; }

void wifi_prov_erase_config(void)
{
    settings_erase_key(NVS_KEY_WIFI_SSID);
    settings_erase_key(NVS_KEY_WIFI_PASS);
    settings_commit();
}

void wifi_prov_set_state_callback(wifi_prov_state_cb_t cb, void *user_data)
{
    g_state_cb = cb;
    g_cb_user_data = user_data;
}

void wifi_prov_http_start_normal(void)
{
    if (!g_orig_vprintf) { g_orig_vprintf = esp_log_set_vprintf(log_vprint_hook); }
    g_http_normal_mode = true;

    /* LWIP TCP stack may not be ready immediately after WiFi connect.
     * Retry with backoff: 0, 400, 800, 1600, 3200ms delays. */
    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt > 0) {
            int delay_ms = 200 * (1 << attempt);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            ESP_LOGI(TAG, "HTTP start retry %d/4 (waited %dms)...", attempt, delay_ms);
        }
        if (http_start() == ESP_OK) {
            ESP_LOGI(TAG, "Normal-mode HTTP server started (attempt %d)", attempt + 1);
            return;
        }
    }
    ESP_LOGE(TAG, "HTTP server failed after 5 attempts");
}

/* ── Log capture ────────────────────────────────────────────────────── */
#define LOG_BUF_SIZE  60
#define LOG_ENTRY_LEN 120
static char g_log_buf[LOG_BUF_SIZE][LOG_ENTRY_LEN];
static int g_log_idx = 0;
static int g_log_count = 0;

static void log_add_raw(const char *entry)
{
    strncpy(g_log_buf[g_log_idx], entry, LOG_ENTRY_LEN - 1);
    g_log_buf[g_log_idx][LOG_ENTRY_LEN - 1] = '\0';
    g_log_idx = (g_log_idx + 1) % LOG_BUF_SIZE;
    if (g_log_count < LOG_BUF_SIZE) g_log_count++;
}

void wifi_prov_log(const char *tag, const char *fmt, ...)
{
    char buf[LOG_ENTRY_LEN];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) {
        char entry[LOG_ENTRY_LEN + 64];
        uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        snprintf(entry, sizeof(entry), "[%lu.%03lu] %s: %s",
                 ms / 1000, ms % 1000, tag ? tag : "-", buf);
        log_add_raw(entry);
    }
}

int wifi_prov_get_logs(char *buf, int max_len)
{
    int total = g_log_count < LOG_BUF_SIZE ? g_log_count : LOG_BUF_SIZE;
    int pos = 0;
    for (int i = 0; i < total && pos < max_len - 2; i++) {
        int idx = (g_log_idx - total + i + LOG_BUF_SIZE) % LOG_BUF_SIZE;
        pos += snprintf(buf + pos, max_len - pos, "%s\"%s\"",
                        i > 0 ? "," : "", g_log_buf[idx]);
    }
    return pos;
}

static esp_err_t http_logs_handler(httpd_req_t *req)
{
    char *buf = malloc(LOG_BUF_SIZE * LOG_ENTRY_LEN + 64);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    int len = wifi_prov_get_logs(buf, LOG_BUF_SIZE * LOG_ENTRY_LEN + 64);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

static esp_err_t http_logs_txt_handler(httpd_req_t *req)
{
    char *buf = malloc(LOG_BUF_SIZE * LOG_ENTRY_LEN + 64);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    int len = wifi_prov_get_logs(buf, LOG_BUF_SIZE * LOG_ENTRY_LEN + 64);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"air_purifier_logs.txt\"");
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

/* Hook ESP-IDF logging into our buffer */
static int log_vprint_hook(const char *fmt, va_list args)
{
    int ret = 0;
    if (g_orig_vprintf) ret = g_orig_vprintf(fmt, args);
    char buf[256];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    if (n > 0) {
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        if (n > 0) log_add_raw(buf);
    }
    return ret;
}

void wifi_prov_update_sensors(const wifi_prov_sensor_t *data, uint32_t mask)
{
    if (!data) return;
    if (mask & WIFI_PROV_SENSOR_TEMP) {
        g_sensor.temp_c = data->temp_c;
        ESP_LOGI("web", "temp updated: %.1f", data->temp_c);
    }
    if (mask & WIFI_PROV_SENSOR_TVOC) g_sensor.tvoc_ugm3  = data->tvoc_ugm3;
    if (mask & WIFI_PROV_SENSOR_CO2)  g_sensor.co2_ppm    = data->co2_ppm;
    if (mask & WIFI_PROV_SENSOR_CH2O) g_sensor.ch2o_ugm3  = data->ch2o_ugm3;
    if (mask & WIFI_PROV_SENSOR_FAN) {
        g_sensor.fan_rpm   = data->fan_rpm;
        g_sensor.fan_on    = data->fan_on;
        g_sensor.fan_speed = data->fan_speed;
    }
}
