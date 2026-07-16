@echo off
chcp 65001 >nul
title AiRFLOW 烧录工具

echo/
echo    ╔══════════════════════════════╗
echo    ║   AiRFLOW 空气净化器       ║
echo    ║   固件烧录工具             ║
echo    ╚══════════════════════════════╝
echo/

:: Auto-detect COM port
set PORT=
for /f "tokens=2 delims==" %%a in ('wmic path Win32_SerialPort get DeviceID /value 2^>nul ^| find "COM"') do set PORT=%%a
if "%PORT%"=="" (
    echo [错误] 未检测到串口!
    echo 请将 ESP32-S3 通过 USB 连接电脑后重试。
    pause
    exit /b 1
)

echo 检测到串口: %PORT%
echo/
echo 正在烧录固件...
echo/

esptool.exe --chip esp32s3 --port %PORT% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_size 16MB 0x0 firmware.bin

if %errorlevel% equ 0 (
    echo/
    echo ╔══════════════════════════════╗
    echo ║     烧录成功!              ║
    echo ╚══════════════════════════════╝
    echo/
) else (
    echo/
    echo [错误] 烧录失败!
    echo 请确认 COM 口没有被其他程序占用。
    echo/
)

pause
