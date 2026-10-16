param(
    [string]$TokenPath = 'D:\SEPLUGINS\RAClientLite\auth.txt',
    [switch]$Logout,
    [string]$PythonExecutable = (Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe')
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $PythonExecutable -PathType Leaf)) {
    throw 'Python 3 не найден. Укажи существующий python.exe через -PythonExecutable; установка не выполняется.'
}
$helper = Join-Path $PSScriptRoot 'configure_psp_login.py'
$helperArguments = @($helper, '--output', $TokenPath)
if ($Logout) { $helperArguments += '--logout' }
& $PythonExecutable @helperArguments
if ($LASTEXITCODE -ne 0) { throw 'Настройка входа не выполнена. Секреты в вывод не записываются.' }
