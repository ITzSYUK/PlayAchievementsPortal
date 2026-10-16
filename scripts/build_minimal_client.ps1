$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$lock = Get-Content -LiteralPath (Join-Path $projectDirectory 'dependencies.lock.json') -Raw | ConvertFrom-Json
$sdkImage = [string]$lock.pspdev.image
if ($sdkImage -notmatch '^ghcr\.io/pspdev/pspsdk@sha256:[0-9a-f]{64}$') { throw 'Invalid pinned SDK digest' }
$linuxOutput = & wsl -d Ubuntu -- wslpath -a $projectDirectory.Replace('\', '/')
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve WSL path' }
$linuxDirectory = ([string]$linuxOutput).Trim()
if (-not $linuxDirectory.StartsWith('/mnt/')) { throw 'Unexpected workspace path' }
$command = 'set -eu; psp-gcc --version; make -s -C experiments/psp/ingame/client/kernel; make -s -C experiments/psp/ingame/client/user -j4; psp-size experiments/psp/ingame/client/kernel/RAClientBootstrap.elf experiments/psp/ingame/client/user/RAClientUser.elf; psp-nm experiments/psp/ingame/client/user/RAClientUser.elf > /tmp/client-user-symbols; if grep -E "sceKernelExitGame|sceNet|sceHttp|scePower|sceDisplay|sceCtrl" /tmp/client-user-symbols; then echo "Unexpected platform feature in idle-only runtime"; exit 1; fi; grep -q rc_client_create /tmp/client-user-symbols; grep -q rc_client_idle /tmp/client-user-symbols; grep -q rc_client_destroy /tmp/client-user-symbols; grep -q sce_newlib_heap_kb_size /tmp/client-user-symbols; psp-nm experiments/psp/ingame/client/kernel/RAClientBootstrap.elf > /tmp/client-kernel-symbols; if grep -E "__libcglue|malloc|sceNet|sceHttp|scePower|sceDisplay|sceCtrl|sceKernelSetDdrMemoryProtection|sctrl" /tmp/client-kernel-symbols; then echo "Unexpected bootstrap feature"; exit 1; fi; grep -q sceKernelStartModule /tmp/client-kernel-symbols; grep -q sceKernelStopModule /tmp/client-kernel-symbols'
$buildOutput = & wsl -d Ubuntu -- docker run --rm --pull=never --network none --cap-drop ALL --security-opt no-new-privileges --read-only --tmpfs /tmp --mount "type=bind,source=$linuxDirectory,target=/workspace" --workdir /workspace $sdkImage /bin/sh -c $command
$buildOutput | Write-Output
if ($LASTEXITCODE -ne 0) { throw 'Minimal client build failed' }
$sizeRows = [regex]::Matches(($buildOutput -join "`n"), '(?m)^\s*\d+\s+\d+\s+\d+\s+(\d+)\s+[0-9a-f]+\s+[^\r\n]*RAClientUser\.elf\s*$')
if ($sizeRows.Count -ne 1 -or [int]$sizeRows[0].Groups[1].Value -gt 294912) { throw 'User module size not verified or exceeds memory guard' }
if ((Get-Item -LiteralPath (Join-Path $projectDirectory 'experiments/psp/ingame/client/user/RAClientUser.prx')).Length -gt 327680) { throw 'User PRX exceeds loader file guard' }
$outputDirectory = Join-Path $projectDirectory 'build\minimal-client'
$entries = @{
    'SEPLUGINS/RAClientLite/RAClientBootstrap.prx' = 'experiments/psp/ingame/client/kernel/RAClientBootstrap.prx'
    'SEPLUGINS/RAClientLite/RAClientUser.prx' = 'experiments/psp/ingame/client/user/RAClientUser.prx'
    'MINIMAL_CLIENT.md' = 'docs/MINIMAL_CLIENT.md'
    'INSTALL_ENTRY.txt' = 'experiments/ingame/INSTALL_CLIENT_ENTRY.txt'
    'RCHEEVOS_LICENSE.txt' = 'vendor/rcheevos/LICENSE'
    'PSPSDK_LICENSE.txt' = 'vendor/pspsdk/LICENSE'
}
foreach ($entry in $entries.Keys) {
    $destination = Join-Path $outputDirectory $entry
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectDirectory $entries[$entry]) -Destination $destination -Force
}
$archivePath = Join-Path $outputDirectory 'PSP_RA_MinimalClient_v0.10.zip'
$stream = [System.IO.File]::Open($archivePath, [System.IO.FileMode]::Create)
try {
    $archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($entry in ($entries.Keys | Sort-Object)) {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Join-Path $outputDirectory $entry), $entry, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath
