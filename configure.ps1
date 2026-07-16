Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue

$env:IDF_PATH = "D:\esp\.espressif\v6.0.1\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "D:\esp\Espressif\python_env\idf6.0_py3.13_env"
$env:ESP_IDF_VERSION = "6.0.1"

Write-Host "IDF_PATH: $env:IDF_PATH"
Write-Host "IDF_TOOLS_PATH: $env:IDF_TOOLS_PATH"
Write-Host "IDF_PYTHON_ENV_PATH: $env:IDF_PYTHON_ENV_PATH"

$python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
$idf = "$env:IDF_PATH\tools\idf.py"

Write-Host "Python: $python"
Write-Host "idf.py: $idf"

Write-Host "Setting target to esp32s3..."
& $python $idf set-target esp32s3
