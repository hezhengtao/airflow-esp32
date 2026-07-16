Set-Location d:\c\esp32\jhq
$env:IDF_TOOLS_PATH = "C:\Espressif"
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYSCON -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue
. "D:\esp\.espressif\v6.0.1\esp-idf\export.ps1"
idf.py build 2>&1
