param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^ghcr\.io/pspdev/pspsdk@sha256:[0-9a-f]{64}$')]
    [string]$SDKImage,
    [switch]$DownloadSDK
)

$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$linuxPathOutput = & wsl -d Ubuntu -- wslpath -a $projectDirectory.Replace('\', '/')
if ($LASTEXITCODE -ne 0 -or -not $linuxPathOutput) {
    throw 'Cannot resolve the workspace path in Ubuntu WSL'
}
$linuxDirectory = ([string]$linuxPathOutput).Trim()
if (-not $linuxDirectory.StartsWith('/mnt/')) { throw 'Unexpected WSL workspace path' }

# Download requires a separate explicit switch; run never pulls implicitly.
if ($DownloadSDK) {
    & wsl -d Ubuntu -- docker pull $SDKImage
    if ($LASTEXITCODE -ne 0) { throw 'PSPSDK image download failed' }
}

$buildCommand = 'set -eu; psp-gcc --version; psp-config --pspsdk-path; make; make dist; make -C experiments/psp/offline; make -C experiments/psp/https; make -C experiments/psp/entropy; make -C experiments/psp/mbedtls -j4; psp-size plugin/PSPAchievementsNG.elf experiments/psp/offline/RAOfflineProbe.elf experiments/psp/https/RAHttpsProbe.elf experiments/psp/mbedtls/RAMbedTLSProbe.elf experiments/psp/entropy/RAProbeEntropy.elf; mkdir -p build/psp; cp experiments/psp/offline/EBOOT.PBP build/psp/RAOfflineProbe.PBP; cp experiments/psp/https/EBOOT.PBP build/psp/RAHttpsProbe.PBP; cp experiments/psp/mbedtls/EBOOT.PBP build/psp/RAMbedTLSProbe.PBP; cp experiments/psp/entropy/RAProbeEntropy.prx build/psp/RAProbeEntropy.prx'
& wsl -d Ubuntu -- docker run --rm --pull=never --network none --cap-drop ALL --security-opt no-new-privileges --read-only --tmpfs /tmp --mount "type=bind,source=$linuxDirectory,target=/workspace" --workdir /workspace $SDKImage /bin/sh -c $buildCommand
if ($LASTEXITCODE -ne 0) { throw 'PSP build failed; review compiler diagnostics' }

$gameDirectory = Join-Path $projectDirectory 'build\psp\PSP\GAME'
foreach ($probeName in 'RAOfflineProbe', 'RAHttpsProbe', 'RAMbedTLSProbe') {
    $probeDirectory = Join-Path $gameDirectory $probeName
    New-Item -ItemType Directory -Path $probeDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectDirectory "build\psp\$probeName.PBP") -Destination (Join-Path $probeDirectory 'EBOOT.PBP') -Force
}
$tlsDirectory = Join-Path $gameDirectory 'RAMbedTLSProbe'
Copy-Item -LiteralPath (Join-Path $projectDirectory 'build\psp\RAProbeEntropy.prx') -Destination (Join-Path $tlsDirectory 'RAProbeEntropy.prx') -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'vendor\ca\cacert.crt') -Destination (Join-Path $tlsDirectory 'cacert.crt') -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'docs\PSP_PROBES.md') -Destination (Join-Path $projectDirectory 'build\psp\PSP_PROBES.md') -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'vendor\rcheevos\LICENSE') -Destination (Join-Path $projectDirectory 'build\psp\RCHEEVOS_LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'vendor\mbedtls\LICENSE') -Destination (Join-Path $projectDirectory 'build\psp\MBEDTLS_LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'vendor\ca\LICENSE.txt') -Destination (Join-Path $projectDirectory 'build\psp\CA_LICENSE.txt') -Force
Compress-Archive -LiteralPath (Join-Path $projectDirectory 'build\psp\PSP'), (Join-Path $projectDirectory 'build\psp\PSP_PROBES.md'), (Join-Path $projectDirectory 'build\psp\RCHEEVOS_LICENSE.txt'), (Join-Path $projectDirectory 'build\psp\MBEDTLS_LICENSE.txt'), (Join-Path $projectDirectory 'build\psp\CA_LICENSE.txt') -DestinationPath (Join-Path $projectDirectory 'build\psp\PSP_RA_Diagnostics.zip') -Force
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectDirectory 'build\psp\PSP_RA_Diagnostics.zip')
