$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$lock = Get-Content -LiteralPath (Join-Path $projectDirectory 'dependencies.lock.json') -Raw | ConvertFrom-Json
$sdkImage = [string]$lock.pspdev.image
if ($sdkImage -notmatch '^ghcr\.io/pspdev/pspsdk@sha256:[0-9a-f]{64}$') { throw 'Invalid pinned SDK digest' }
$linuxOutput = & wsl -d Ubuntu -- wslpath -a $projectDirectory.Replace('\', '/')
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve WSL path' }
$linuxDirectory = ([string]$linuxOutput).Trim()
if (-not $linuxDirectory.StartsWith('/mnt/')) { throw 'Unexpected workspace path' }
& wsl -d Ubuntu -- python3 "$linuxDirectory/scripts/extract_ra_tls_root.py"
if ($LASTEXITCODE -ne 0) { throw 'Pinned trust anchor verification failed' }
$command = 'set -eu; psp-gcc --version; make -s -C experiments/psp/ingame/https/kernel; make -s -C experiments/psp/ingame/https/user -j4; psp-size experiments/psp/ingame/https/kernel/RAClientBootstrap.elf experiments/psp/ingame/https/user/RAClientUser.elf; psp-nm experiments/psp/ingame/https/user/RAClientUser.elf > /tmp/https-user-symbols; if grep -E "sceKernelExitGame|sceHttp|sceDisplaySetMode|sceCtrl(Read|Set)|sceAudioOutputBlocking|sceKernelSuspendThread|__descriptor_data_pool|__ssvfiscanf_r|sceKernelSetDdrMemoryProtection|sctrl" /tmp/https-user-symbols; then echo "Unexpected exit, insecure HTTP or game-control feature in online client"; exit 1; fi; grep -q ra_runtime_input_probe /tmp/https-user-symbols; grep -q rc_client_do_frame /tmp/https-user-symbols; grep -q ra_session_enable_online_softcore /tmp/https-user-symbols; grep -q sceKernelPollSema /tmp/https-user-symbols; grep -q sceIoSync /tmp/https-user-symbols; grep -q ra_ui_draw /tmp/https-user-symbols; grep -q sceDisplaySetFrameBuf /tmp/https-user-symbols; grep -q sceDisplayWaitVblankStart /tmp/https-user-symbols; grep -q sceGeDrawSync /tmp/https-user-symbols; if grep -E " (sceDisplaySet|sceDisplayWait|sceGe)[A-Za-z0-9_]*$" /tmp/https-user-symbols | grep -Ev " (sceDisplaySetFrameBuf|sceDisplayWaitVblankStart|sceGeDrawSync)$"; then echo "Unexpected display/GE operation"; exit 1; fi; grep -q ra_session_prepare_ui /tmp/https-user-symbols; grep -q sceCtrlPeekBufferPositive /tmp/https-user-symbols; grep -q sceAudioOutput /tmp/https-user-symbols; grep -q sceKernelGetThreadStackFreeSize /tmp/https-user-symbols; grep -q sceDisplayGetVcount /tmp/https-user-symbols; grep -q sceDisplayGetFrameBuf /tmp/https-user-symbols; grep -q sceKernelDelayThread /tmp/https-user-symbols; grep -q module_stop /tmp/https-user-symbols; grep -q sce_newlib_heap_kb_size /tmp/https-user-symbols; psp-nm experiments/psp/ingame/https/kernel/RAClientBootstrap.elf > /tmp/https-kernel-symbols; if grep -E "__libcglue|malloc|sceNet|sceHttp|sceDisplay|sceCtrl|sceKernelSetDdrMemoryProtection|sctrl" /tmp/https-kernel-symbols; then echo "Unexpected bootstrap feature"; exit 1; fi; grep -q sceKernelStartModule /tmp/https-kernel-symbols; grep -q ra_game_table_probe /tmp/https-kernel-symbols; grep -q sceKernelQueryMemoryInfo /tmp/https-kernel-symbols; grep -q sceKernelDcacheWritebackInvalidateRange /tmp/https-kernel-symbols; grep -q sceKernelStopModule /tmp/https-kernel-symbols'
$buildOutput = & wsl -d Ubuntu -- docker run --rm --pull=never --network none --cap-drop ALL --security-opt no-new-privileges --read-only --tmpfs /tmp --mount "type=bind,source=$linuxDirectory,target=/workspace" --workdir /workspace $sdkImage /bin/sh -c $command
$buildOutput | Write-Output
if ($LASTEXITCODE -ne 0) { throw 'Bounded HTTPS build failed' }
& wsl -d Ubuntu -- python3 "$linuxDirectory/scripts/check_https_elf.py" "$linuxDirectory/experiments/psp/ingame/https/kernel/RAClientBootstrap.elf" "$linuxDirectory/experiments/psp/ingame/https/user/RAClientUser.elf"
if ($LASTEXITCODE -ne 0) { throw 'PSP module metadata/exports/heap verification failed' }
$sharedHeader = Get-Content -LiteralPath (Join-Path $projectDirectory 'experiments/ingame/client_shared.h') -Raw
$moduleLimit = [int]([regex]::Match($sharedHeader, '#define RA_CLIENT_MODULE_LIMIT (\d+)u').Groups[1].Value)
$fileLimit = [int]([regex]::Match($sharedHeader, '#define RA_CLIENT_FILE_LIMIT (\d+)u').Groups[1].Value)
$sizeRows = [regex]::Matches(($buildOutput -join "`n"), '(?m)^\s*\d+\s+\d+\s+\d+\s+(\d+)\s+[0-9a-f]+\s+[^\r\n]*RAClientUser\.elf\s*$')
if ($sizeRows.Count -ne 1 -or [int]$sizeRows[0].Groups[1].Value -gt $moduleLimit) { throw 'User module size not verified or exceeds memory guard' }
if ((Get-Item -LiteralPath (Join-Path $projectDirectory 'experiments/psp/ingame/https/user/RAClientUser.prx')).Length -gt $fileLimit) { throw 'User PRX exceeds loader file guard' }
$outputDirectory = Join-Path $projectDirectory 'build\client-https'
$entries = @{
    'SEPLUGINS/RAClientLite/RAClientBootstrap.prx' = 'experiments/psp/ingame/https/kernel/RAClientBootstrap.prx'
    'SEPLUGINS/RAClientLite/RAClientUser.prx' = 'experiments/psp/ingame/https/user/RAClientUser.prx'
    'SEPLUGINS/RAClientLite/profile.txt' = 'experiments/ingame/profile-https.txt'
    'SEPLUGINS/RAClientLite/mode.txt' = 'experiments/ingame/mode.txt'
    'SEPLUGINS/RAClientLite/duration.txt' = 'experiments/ingame/duration.txt'
    'HTTPS_CLIENT.md' = 'docs/HTTPS_CLIENT.md'
    'ONLINE_CLIENT_031.md' = 'docs/ONLINE_CLIENT_031.md'
    'ONLINE_CLIENT_032.md' = 'docs/ONLINE_CLIENT_032.md'
    'ONLINE_CLIENT_033.md' = 'docs/ONLINE_CLIENT_033.md'
    'ONLINE_CLIENT_034.md' = 'docs/ONLINE_CLIENT_034.md'
    'ONLINE_CLIENT_035.md' = 'docs/ONLINE_CLIENT_035.md'
    'ONLINE_CLIENT_036.md' = 'docs/ONLINE_CLIENT_036.md'
    'ONLINE_CLIENT_037.md' = 'docs/ONLINE_CLIENT_037.md'
    'ONLINE_CLIENT_038.md' = 'docs/ONLINE_CLIENT_038.md'
    'ONLINE_CLIENT_039.md' = 'docs/ONLINE_CLIENT_039.md'
    'ONLINE_CLIENT_040.md' = 'docs/ONLINE_CLIENT_040.md'
    'ONLINE_CLIENT_041.md' = 'docs/ONLINE_CLIENT_041.md'
    'SEPLUGINS/RAClientLite/encore.txt' = 'experiments/ingame/encore.txt'
    'PPSSPP_FRAME_AUDIT.md' = 'docs/PPSSPP_FRAME_AUDIT.md'
    'configure_psp_login.py' = 'scripts/configure_psp_login.py'
    'configure_psp_login.ps1' = 'scripts/configure_psp_login.ps1'
    'INSTALL_ENTRY.txt' = 'experiments/ingame/INSTALL_CLIENT_ENTRY.txt'
    'RCHEEVOS_LICENSE.txt' = 'vendor/rcheevos/LICENSE'
    'PSPSDK_LICENSE.txt' = 'vendor/pspsdk/LICENSE'
    'MBEDTLS_LICENSE.txt' = 'vendor/mbedtls/LICENSE'
    'CA_LICENSE.txt' = 'vendor/ca/LICENSE.txt'
    'CA_NOTICE.txt' = 'experiments/tls/compact/CA_NOTICE.txt'
}
foreach ($entry in $entries.Keys) {
    $destination = Join-Path $outputDirectory $entry
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectDirectory $entries[$entry]) -Destination $destination -Force
}
$archivePath = Join-Path $outputDirectory 'PSP_RA_HTTPS_v0.41.zip'
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
