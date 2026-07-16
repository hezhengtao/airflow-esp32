# AiRFLOW — 智能空气净化器

基于 ESP32-S3 的智能空气净化器固件，集成 4.3 寸触摸屏、多气体传感器、BLDC 无刷电机、WiFi 配网、MQTT（Home Assistant 自动发现）、Web 控制台、OTA 固件升级。

## 项目简介

本项目是一个完整的嵌入式智能空气净化器方案。设备通过 Y01 激光传感器实时监测 TVOC、CO₂、甲醛浓度，通过 DS18B20 检测室内温度。数据在本地触摸屏和 Web 控制台同步展示，同时通过 MQTT 推送至 Home Assistant 等智能家居平台实现联动。

风扇驱动采用 BLDC 无刷电机，支持手动调速和**自动模式**——任一气体指标超过报警阈值后自动开启风扇，达标后延时关闭。定时开关机功能内置中国法定节假日适配（调休补班 = 工作日，法定假日 = 休息日）。

## 功能列表

### 空气质量监测
- Y01 激光传感器：实时读取 TVOC、CO₂、甲醛，UART 通信
- DS18B20 数字温度传感器：高精度温度检测，1-Wire 接口
- 传感器数据每 60 秒自动记录，环形缓冲最多保留 720 条（12 小时）
- Web 控制台支持查询历史数据并绘制时间序列图表

### 触摸屏界面（LVGL 9.5）
- 800×480 横屏显示，左侧图标导航栏常驻，5 个页面一键切换
- 首页：温度大字体 + 实时数值 + 四条折线图（温度/TVOC/CO₂/CH₂O）+ 风扇调速
- 网络页：WiFi 连接管理 + MQTT 配置 + 设备二维码
- 设置页：中英文切换 / 暗色·亮色·自动主题 / 屏幕亮度 / 出厂复位
- 声音页：按键音 / 开关机音 / 提醒音，各有 6 种可选旋律，支持试听
- 电源页：息屏 / 关机 / 定时开关机（支持每天/工作日/周末/自定义，自动适配中国节假日）

### WiFi 配网 & Web 控制台
- 首次开机自动弹窗引导配网
- Web 控制台功能：传感器面板、风扇控制、MQTT 配置、报警阈值设置、LED 调色、定时开关机、**OTA 固件升级**（网页上传 .bin，进度条 + 百分比）

### MQTT & Home Assistant
- 自动发现（Auto Discovery）：温度、TVOC、CO₂、CH₂O、风扇开关/转速、定时状态
- 支持远程控制：风扇开关与调速、屏幕亮度、LED 颜色与效果、关机/唤醒
- 自定义 Broker 地址（含用户名/密码认证），网页端一键测试连接

### 其他
- WS2812 状态指示灯：正常/报警/关机/WiFi 失败/WiFi 连接中 5 种状态各独立 RGB 颜色与效果
- PWM 音频输出（PAM8302 D 类功放），静音时自动切断功放供电消除底噪
- 出厂复位：连续快速点击复位按钮 3 次
- 屏幕亮度可调，息屏后双击唤醒

## 硬件

| 模块 | 型号 | 接口 |
| --- | --- | --- |
| MCU | ESP32-S3（16MB Flash, N16R8, 无 PSRAM） | — |
| 屏幕 | TK043F1509（NT35510, 800×480） | 8-bit 8080 并口 |
| 触摸 | FT5x06 / FT6336（I2C, 地址 0x38） | I2C |
| 气体传感器 | Y01 激光模组（TVOC / CO₂ / CH₂O） | UART, 9600bps |
| 温度传感器 | DS18B20 | 1-Wire（GPIO8, 4.7kΩ 上拉） |
| 电机 | 48F704P840 BLDC | LEDC PWM（调速）+ PCNT（转速反馈） |
| 状态灯 | WS2812-2020 RGB | RMT |
| 扬声器 | 8Ω 2W + PAM8302 D 类 | LEDC PWM（RC 低通） |

> **GPIO 分配详见** [`main/board.h`](main/board.h)。  
> **接线注意事项**：GPIO43/44 被 ESP32-S3 内部 USB-Serial-JTAG 占用不可用；触摸 I2C 需 4.7kΩ 上拉；电机启动引脚需 4.7kΩ 下拉；电机电源需 470µF/25V 低 ESR 电解电容退耦。

## 软件架构

```
main/
├── main.c                 入口
├── app_controller.c/h     中央控制器（初始化、事件分发、WiFi/MQTT/传感器/报警/电源调度）
├── board.h                全部引脚定义 + NVS 键名
├── event_bus.c/h          轻量发布-订阅事件总线
├── ui/                    LVGL 界面层
│   ├── ui_manager.c/h     页面管理、导航栏、触摸处理、息屏/关机/唤醒动画
│   ├── ui_theme.c/h       主题引擎（暗/亮/自动日出日落主题切换）
│   ├── ui_lang.c/h        多语言支持（中文/English）
│   ├── ui_design.h        设计系统（颜色、间距、字体、扁平化助手）
│   ├── ui_screen_home.c   首页（传感器数值 + 折线图 + 风扇控制）
│   ├── ui_screen_network.c 网络配置（WiFi 扫描/MQTT 设置/二维码）
│   ├── ui_screen_settings.c 系统设置（语言/主题/亮度/出厂复位）
│   ├── ui_screen_sound.c  声音设置（按键/开关机/提醒音效 + 旋律选择）
│   ├── ui_screen_power.c  电源管理（息屏/关机/定时开关机）
│   ├── ui_boot.c          开机动画
│   ├── ui_provision_prompt.c 配网引导弹窗
│   ├── ui_apple_anim.c    按钮弹性动画
│   └── font_cjk_*.c       中文字体（思源黑体，4 个字号）
│   └── font_melody.c      旋律名前缀专用字体
├── holiday/               中国节假日查询（timor.tech API）
└── assets/                Logo 图片资源

components/
├── wifi_prov/             WiFi 配网 + HTTP 服务器 + 完整 Web 控制台 + OTA
├── mqtt_ha/               MQTT 客户端 + Home Assistant 自动发现
├── motor_bldc/            BLDC 无刷电机驱动
├── sensor_y01/            Y01 传感器 UART 协议解析
├── sensor_ds18b20/        DS18B20 1-Wire 驱动
├── sensor_history/        传感器历史数据环形缓冲
├── speaker/               PWM 音频输出（旋律合成 + 功放控制）
├── status_led/            WS2812 RGB LED 控制（RMT 驱动）
├── settings/              NVS 非易失存储读写封装
├── factory_reset/         快速上电 3 次复位检测
├── touch_ft5x06/          触摸 I2C 通信层
└── lcd_tk043f1509/         LCD 面板驱动
```

## 数据流

```
传感器(Y01/DS18B20) → app_controller → event_bus
                                      ├→ UI 更新（lv_label + chart）
                                      ├→ MQTT 发布（mqtt_ha）
                                      ├→ sensor_history 记录
                                      └→ 报警判断 → 自动风扇 + LED + 声音

用户操作（触摸屏/Web/MQTT）→ app_controller → 执行动作 + 持久化 NVS
```

## 开发环境

- **ESP-IDF** v6.0.1
- **LVGL** 9.5（通过 `espressif/esp_lvgl_port` 组件集成）
- **芯片** ESP32-S3
- **构建系统** CMake + Ninja

## 构建 & 烧录

```bash
# 克隆仓库
git clone <repo-url>
cd airflow-esp32

# 配置目标
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并查看日志
idf.py -p COMx flash monitor
```

## 配置说明

- **首次上电**：设备自动创建热点 `AiRFLOW-XXXX`，手机连接后弹出配网页面
- **MQTT**：Web 控制台「网络设置」中配置 Broker 地址，支持的格式 `mqtt://host:port`
- **DS18B20 温度校准**：游览器访问 `http://设备IP/calibrate?t=实际温度` 保存偏移量
- **OTA 升级**：Web 控制台 → 设置 → 固件升级 → 上传 air_purifier.bin → 自动重启

## 许可证

MIT License
