$env:IDF_PATH='D:\esp\.espressif\v6.0.1\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif'
$env:PATH="C:\Espressif\tools\ccache\4.10.2;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\riscv32-esp-elf\esp-14.2_20241119\riscv32-esp-elf\bin;C:\Espressif\tools\esp32ulp-elf\2.38_20240822\esp32ulp-elf\bin;C:\Espressif\tools\idf-exe\1.0.3;C:\Espressif\python_env\idf6.0_py3.13_env\Scripts;$env:PATH"
# Clear MSYS detection variables
Remove-Item env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item env:TERM -ErrorAction SilentlyContinue
Remove-Item env:MINGW_PREFIX -ErrorAction SilentlyContinue
Remove-Item env:MSYSTEM_PREFIX -ErrorAction SilentlyContinue
D:\esp\Espressif\python_env\idf6.0_py3.13_env\Scripts\python.exe D:\esp\.espressif\v6.0.1\esp-idf\tools\idf.py -p COM12 flash
