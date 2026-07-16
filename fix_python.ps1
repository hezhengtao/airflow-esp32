Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue

$env:IDF_PATH = "D:\esp\.espressif\v6.0.1\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "D:\esp\Espressif\python_env\idf6.0_py3.13_env"

$python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
$reqFile = "$env:IDF_PATH\tools\requirements\requirements.core.txt"
$constraint = "C:\Espressif\espidf.constraints.v6.0.txt"

Write-Host "Installing Python requirements..."
& $python -m pip install --upgrade -r $reqFile --constraint $constraint
