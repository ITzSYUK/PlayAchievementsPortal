$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$lock = Get-Content -LiteralPath (Join-Path $projectDirectory 'dependencies.lock.json') -Raw | ConvertFrom-Json
$sdkImage = [string]$lock.pspdev.image
if ($sdkImage -notmatch '^ghcr\.io/pspdev/pspsdk@sha256:[0-9a-f]{64}$') { throw 'Invalid pinned SDK digest' }
$linuxOutput = & wsl -d Ubuntu -- wslpath -a $projectDirectory.Replace('\', '/')
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve WSL path' }
$linuxDirectory = ([string]$linuxOutput).Trim()
if (-not $linuxDirectory.StartsWith('/mnt/')) { throw 'Unexpected workspace path' }

$command = 'set -eu; psp-gcc --version; make -C experiments/psp/entropy; make -C experiments/psp/ingame/kernel; make -C experiments/psp/ingame/user -j4; psp-size experiments/psp/ingame/kernel/RAInGameProbe.elf experiments/psp/ingame/user/RAInGameWorker.elf; if psp-nm experiments/psp/ingame/user/RAInGameWorker.elf | grep -q sceKernelExitGame; then echo "Unsafe ExitGame symbol in library worker"; exit 1; fi'
& wsl -d Ubuntu -- docker run --rm --pull=never --network none --cap-drop ALL --security-opt no-new-privileges --read-only --tmpfs /tmp --mount "type=bind,source=$linuxDirectory,target=/workspace" --workdir /workspace $sdkImage /bin/sh -c $command
if ($LASTEXITCODE -ne 0) { throw 'In-game probe build failed' }

$outputDirectory = Join-Path $projectDirectory 'build\ingame'
$pluginDirectory = Join-Path $outputDirectory 'SEPLUGINS\RAInGameProbe'
New-Item -ItemType Directory -Path $pluginDirectory -Force | Out-Null
$copies = @{
    'experiments\psp\ingame\kernel\RAInGameProbe.prx' = 'RAInGameProbe.prx'
    'experiments\psp\ingame\user\RAInGameWorker.prx' = 'RAInGameWorker.prx'
    'experiments\psp\entropy\RAProbeEntropy.prx' = 'RAProbeEntropy.prx'
    'vendor\ca\cacert.crt' = 'cacert.crt'
    'experiments\ingame\reference.json' = 'reference.json'
    'experiments\ingame\profile.txt' = 'profile.txt'
}
foreach ($source in $copies.Keys) {
    Copy-Item -LiteralPath (Join-Path $projectDirectory $source) -Destination (Join-Path $pluginDirectory $copies[$source]) -Force
}
$notices = @{
    'docs\INGAME_PROBE.md' = 'INGAME_PROBE.md'
    'experiments\ingame\INSTALL_ENTRY.txt' = 'INSTALL_ENTRY.txt'
    'vendor\mbedtls\LICENSE' = 'MBEDTLS_LICENSE.txt'
    'vendor\rcheevos\LICENSE' = 'RCHEEVOS_LICENSE.txt'
    'vendor\ca\LICENSE.txt' = 'CA_LICENSE.txt'
    'vendor\pspsdk\LICENSE' = 'PSPSDK_LICENSE.txt'
}
foreach ($source in $notices.Keys) {
    Copy-Item -LiteralPath (Join-Path $projectDirectory $source) -Destination (Join-Path $outputDirectory $notices[$source]) -Force
}
$archiveFiles = @((Join-Path $outputDirectory 'SEPLUGINS'))
foreach ($notice in $notices.Values) { $archiveFiles += Join-Path $outputDirectory $notice }
Compress-Archive -LiteralPath $archiveFiles -DestinationPath (Join-Path $outputDirectory 'PSP_RA_InGameProbe_v0.2.zip') -Force
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $outputDirectory 'PSP_RA_InGameProbe_v0.2.zip')
