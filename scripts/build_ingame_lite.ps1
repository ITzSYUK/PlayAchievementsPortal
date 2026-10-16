$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$lock = Get-Content -LiteralPath (Join-Path $projectDirectory 'dependencies.lock.json') -Raw | ConvertFrom-Json
$sdkImage = [string]$lock.pspdev.image
if ($sdkImage -notmatch '^ghcr\.io/pspdev/pspsdk@sha256:[0-9a-f]{64}$') { throw 'Invalid pinned SDK digest' }
$linuxOutput = & wsl -d Ubuntu -- wslpath -a $projectDirectory.Replace('\', '/')
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve WSL path' }
$linuxDirectory = ([string]$linuxOutput).Trim()
if (-not $linuxDirectory.StartsWith('/mnt/')) { throw 'Unexpected workspace path' }

$command = 'set -eu; psp-gcc --version; make -C experiments/psp/ingame/placement; make -C experiments/psp/ingame/lite; psp-size experiments/psp/ingame/lite/RAInGameLite.elf experiments/psp/ingame/placement/RAPlacementUser.elf; psp-nm experiments/psp/ingame/lite/RAInGameLite.elf > /tmp/lite-symbols; if grep -E "sceKernelStartModule|sceKernelStopModule|sceKernelExitGame|sceKernelSetDdrMemoryProtection|sceKernelCreateCallback|sceDisplay|sceCtrl|scePower|sceNet|sceHttp|sceIoRead|sceIoLseek|malloc|__libcglue|ra_psp_memory_read|sctrl" /tmp/lite-symbols; then echo "Unexpected feature in load-only probe"; exit 1; fi; grep -q sceKernelLoadModule /tmp/lite-symbols; grep -q sceKernelUnloadModule /tmp/lite-symbols; psp-nm experiments/psp/ingame/placement/RAPlacementUser.elf > /tmp/placement-symbols; if grep -E " [TU] (sceKernel|sceIo|malloc|__libcglue|_start$)" /tmp/placement-symbols; then echo "Unexpected runtime in unstarted placement target"; exit 1; fi'
# Empty .lib.stub.top/.btm sentinels are module metadata, not import entries.
$command += '; if psp-objdump -h experiments/psp/ingame/placement/RAPlacementUser.elf | grep -E "\.(lib\.stub|sceStub\.text|rodata\.sceNid)[[:space:]]"; then echo "Placement target must have no imports when NO_FIXUP_IMPORTS is used"; exit 1; fi'
& wsl -d Ubuntu -- docker run --rm --pull=never --network none --cap-drop ALL --security-opt no-new-privileges --read-only --tmpfs /tmp --mount "type=bind,source=$linuxDirectory,target=/workspace" --workdir /workspace $sdkImage /bin/sh -c $command
if ($LASTEXITCODE -ne 0) { throw 'Lite probe build failed' }

$outputDirectory = Join-Path $projectDirectory 'build\ingame-lite'
$pluginDirectory = Join-Path $outputDirectory 'SEPLUGINS\RAInGameLite'
New-Item -ItemType Directory -Path $pluginDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectDirectory 'experiments\psp\ingame\lite\RAInGameLite.prx') -Destination $pluginDirectory -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'experiments\psp\ingame\placement\RAPlacementUser.prx') -Destination $pluginDirectory -Force
$notices = @{
    'docs\INGAME_USER_PLACEMENT.md' = 'INGAME_USER_PLACEMENT.md'
    'experiments\ingame\INSTALL_LITE_ENTRY.txt' = 'INSTALL_ENTRY.txt'
    'vendor\pspsdk\LICENSE' = 'PSPSDK_LICENSE.txt'
}
foreach ($source in $notices.Keys) {
    Copy-Item -LiteralPath (Join-Path $projectDirectory $source) -Destination (Join-Path $outputDirectory $notices[$source]) -Force
}
# Add exact files, so old logs or unrelated PRXs cannot enter the archive.
$entries = @{
    'SEPLUGINS/RAInGameLite/RAInGameLite.prx' = Join-Path $pluginDirectory 'RAInGameLite.prx'
    'SEPLUGINS/RAInGameLite/RAPlacementUser.prx' = Join-Path $pluginDirectory 'RAPlacementUser.prx'
}
foreach ($notice in $notices.Values) { $entries[$notice] = Join-Path $outputDirectory $notice }
$archivePath = Join-Path $outputDirectory 'PSP_RA_UserPlacement_v0.8.zip'
$stream = [System.IO.File]::Open($archivePath, [System.IO.FileMode]::Create)
try {
    $archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($name in ($entries.Keys | Sort-Object)) {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $entries[$name], $name, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath
