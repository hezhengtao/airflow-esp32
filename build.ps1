Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue
Remove-Item Env:IDF_PATH -ErrorAction SilentlyContinue

$env:IDF_PATH = "D:\esp\.espressif\v6.0.1\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "D:\esp\Espressif\python_env\idf6.0_py3.13_env"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_TARGET = "esp32s3"

$env:PATH = @(
    "C:\Espressif\tools\cmake\4.0.3\bin",
    "C:\Espressif\tools\ninja\1.12.1",
    "C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin",
    $env:PATH
) -join ";"

$python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
$idf = "$env:IDF_PATH\tools\idf.py"

Set-Location "d:\c\esp32\jhq"

Write-Host "=== Clean ==="
Remove-Item -Recurse -Force "d:\c\esp32\jhq\build" -ErrorAction SilentlyContinue

Write-Host "=== Build ==="
& $python $idf build -- -j2
