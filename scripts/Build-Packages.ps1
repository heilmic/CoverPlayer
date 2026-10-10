[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repositoryRoot 'build'
$binary = Join-Path $buildRoot 'arm64-release/coverplayer'
$stagingRoot = Join-Path $buildRoot 'staging-arm64'
$releaseRoot = Join-Path $buildRoot 'release'

& "$PSScriptRoot/Build-Arm64.ps1"
if (!(Test-Path -LiteralPath $binary)) { throw 'ARM64 binary is missing.' }

# Never clear the shared release directory: it also contains Switch packages.
$allowedBuildRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
foreach ($ownedOutput in @($stagingRoot,
    (Join-Path $releaseRoot 'CoverPlayer-Knulli'),
    (Join-Path $releaseRoot 'CoverPlayer-Knulli-Test'),
    (Join-Path $releaseRoot 'CoverPlayer-muOS'))) {
    $resolvedOutput = [IO.Path]::GetFullPath($ownedOutput)
    if (!$resolvedOutput.StartsWith($allowedBuildRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package cleanup escaped the build directory: $resolvedOutput"
    }
    if (Test-Path -LiteralPath $resolvedOutput) { Remove-Item -LiteralPath $resolvedOutput -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path $stagingRoot,$releaseRoot | Out-Null

function Copy-AppPayload([string]$destination) {
    New-Item -ItemType Directory -Force -Path "$destination/bin","$destination/libs","$destination/assets/fonts","$destination/licenses" | Out-Null
    Copy-Item -LiteralPath $binary -Destination "$destination/bin/coverplayer"
    Copy-Item -LiteralPath "$repositoryRoot/assets/fonts/RobotoMono-Bold.ttf" -Destination "$destination/assets/fonts/RobotoMono-Bold.ttf"
    Copy-Item -LiteralPath "$repositoryRoot/LICENSE","$repositoryRoot/THIRD_PARTY_NOTICES.md" -Destination $destination
    Copy-Item -LiteralPath "$repositoryRoot/packaging/README.md" -Destination "$destination/README.md"
    Copy-Item -LiteralPath "$repositoryRoot/LICENSE" -Destination "$destination/licenses/RobotoMono-Apache-2.0.txt"
}

$knulliPorts = Join-Path $stagingRoot 'knulli/roms/ports'
$knulliApp = Join-Path $knulliPorts 'CoverPlayer-Test'
New-Item -ItemType Directory -Force -Path $knulliPorts | Out-Null
Copy-AppPayload $knulliApp
Copy-Item -LiteralPath "$repositoryRoot/packaging/knulli/CoverPlayer-Test.sh" -Destination "$knulliPorts/CoverPlayer-Test.sh"

$knulliProductionPorts = Join-Path $stagingRoot 'knulli-production/roms/ports'
$knulliProductionApp = Join-Path $knulliProductionPorts 'CoverPlayer'
New-Item -ItemType Directory -Force -Path $knulliProductionPorts | Out-Null
Copy-AppPayload $knulliProductionApp
Copy-Item -LiteralPath "$repositoryRoot/packaging/knulli/CoverPlayer.sh" -Destination "$knulliProductionPorts/CoverPlayer.sh"

$muosApp = Join-Path $stagingRoot 'muos/CoverPlayer'
Copy-AppPayload $muosApp
Copy-Item -LiteralPath "$repositoryRoot/packaging/muos/mux_launch.sh" -Destination "$muosApp/mux_launch.sh"
New-Item -ItemType Directory -Force -Path "$muosApp/glyph" | Out-Null
Copy-Item -LiteralPath "$repositoryRoot/packaging/muos/glyph/coverplayer.png" -Destination "$muosApp/glyph/coverplayer.png"

function Invoke-RuntimeCollection([string]$destination, [string]$licenseDestination, [string]$extraExcluded) {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    docker run --rm --volume "${repositoryRoot}:/work" coverplayer-arm64-build `
        sh /work/scripts/collect-arm64-runtime.sh /work/build/arm64-release/coverplayer $destination $licenseDestination $extraExcluded
    $dockerRunExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($dockerRunExitCode -ne 0) { throw 'ARM64 runtime collection failed.' }
}

# muOS provides its own framebuffer-capable SDL2 and ALSA/PipeWire stack.
# Debian's SDL2 requires /dev/dri; Debian's libasound searches for the
# PipeWire module in /usr/lib/aarch64-linux-gnu/alsa-lib instead of muOS's
# /usr/lib/alsa-lib, so neither library can be bundled on muOS.
Invoke-RuntimeCollection '/work/build/staging-arm64/runtime-muos' '/work/build/staging-arm64/muos/CoverPlayer/licenses' 'libSDL2-2.0.so.0 libSDL2_image-2.0.so.0 libSDL2_ttf-2.0.so.0 libasound.so.2'
Copy-Item -Path "$stagingRoot/runtime-muos/*" -Destination "$muosApp/libs"
foreach ($name in @('libSDL2-2.0.so.0','libSDL2_image-2.0.so.0','libSDL2_ttf-2.0.so.0','libasound.so.2')) {
    if (Test-Path -LiteralPath (Join-Path "$muosApp/libs" $name)) {
        throw "muOS package must use the system library: $name"
    }
}

# Knulli ships its own hardware-specific SDL2 build for its framebuffer
# stack - the generic Debian SDL2 only knows DRM/X11/Wayland and fails on
# these devices, so it (and ALSA, likewise device-specific there) must
# always come from the system, never from this bundle. Excluding both from
# the dependency walk itself - not just deleting the two files afterward -
# also drops everything that exists solely to support them: PulseAudio,
# X11, Wayland, D-Bus, Kerberos, NFS/RPC, tcp-wrappers, and their own
# transitive dependents. coverplayer never links or shells out to any of
# those directly (Bluetooth/volume control shells out to the `pactl`/
# `bluetoothctl` binaries, not their libraries), so none of it is actually
# missing on Knulli - it was only ever there because the desktop Debian
# SDL2 build pulls it in.
Invoke-RuntimeCollection '/work/build/staging-arm64/runtime-knulli' '/work/build/staging-arm64/knulli/roms/ports/CoverPlayer-Test/licenses' 'libSDL2-2.0.so.0 libasound.so.2'
Copy-Item -Path "$stagingRoot/runtime-knulli/*" -Destination "$knulliApp/libs"
Copy-Item -Path "$stagingRoot/runtime-knulli/*" -Destination "$knulliProductionApp/libs"
Copy-Item -Path "$knulliApp/licenses/*" -Destination "$knulliProductionApp/licenses" -Recurse -Force
Remove-Item -LiteralPath "$stagingRoot/runtime-muos","$stagingRoot/runtime-knulli" -Recurse -Force

function Write-Manifest([string]$root) {
    $manifest = Join-Path $root 'MANIFEST.sha256'
    $lines = Get-ChildItem -LiteralPath $root -File -Recurse |
        Where-Object FullName -ne $manifest |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\','/').Replace('\','/')
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
            "$hash  $relative"
        }
    # sha256sum -c on Linux treats CRLF's carriage return as part of each
    # filename, so manifests must use Unix line endings even when built here.
    [IO.File]::WriteAllText($manifest, (($lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
}

Write-Manifest $knulliApp
Write-Manifest $knulliProductionApp
Write-Manifest $muosApp
# A second manifest at each archive root covers every file, including the
# application's own manifest and Knulli's launcher beside the app directory.
Write-Manifest (Join-Path $stagingRoot 'knulli')
Write-Manifest (Join-Path $stagingRoot 'knulli-production')
Write-Manifest (Join-Path $stagingRoot 'muos')

# Unzipped copies alongside the archives, for deploying via WinSCP/SSH
# without needing to unzip on the device first. Content is identical to
# what each archive below contains - manifest included.
Copy-Item -LiteralPath $stagingRoot/knulli -Destination "$releaseRoot/CoverPlayer-Knulli-Test" -Recurse
Copy-Item -LiteralPath "$stagingRoot/knulli-production" -Destination "$releaseRoot/CoverPlayer-Knulli" -Recurse
Copy-Item -LiteralPath $stagingRoot/muos -Destination "$releaseRoot/CoverPlayer-muOS" -Recurse

$python = 'C:\msys64\ucrt64\bin\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'MSYS2 UCRT64 Python is missing.' }
& $python "$PSScriptRoot/create-package.py" "$stagingRoot/knulli" "$releaseRoot/CoverPlayer-Knulli-Test.zip"
if ($LASTEXITCODE -ne 0) { throw 'Knulli archive creation failed.' }
& $python "$PSScriptRoot/create-package.py" "$stagingRoot/knulli-production" "$releaseRoot/CoverPlayer-Knulli.zip"
if ($LASTEXITCODE -ne 0) { throw 'Production Knulli archive creation failed.' }
& $python "$PSScriptRoot/create-package.py" "$stagingRoot/muos" "$releaseRoot/CoverPlayer.muxapp"
if ($LASTEXITCODE -ne 0) { throw 'muOS archive creation failed.' }

& $python "$PSScriptRoot/verify-package.py" "$releaseRoot/CoverPlayer-Knulli-Test.zip"
if ($LASTEXITCODE -ne 0) { throw 'Knulli archive verification failed.' }
& $python "$PSScriptRoot/verify-package.py" "$releaseRoot/CoverPlayer-Knulli.zip"
if ($LASTEXITCODE -ne 0) { throw 'Production Knulli archive verification failed.' }
& $python "$PSScriptRoot/verify-package.py" "$releaseRoot/CoverPlayer.muxapp"
if ($LASTEXITCODE -ne 0) { throw 'muOS archive verification failed.' }

& $python "$PSScriptRoot/build-knulli-category.py"
if ($LASTEXITCODE -ne 0) { throw 'Knulli category archive creation failed.' }

Get-FileHash -Algorithm SHA256 "$releaseRoot/CoverPlayer-Knulli.zip","$releaseRoot/CoverPlayer-Knulli-Test.zip","$releaseRoot/CoverPlayer-Knulli-Category.zip","$releaseRoot/CoverPlayer.muxapp"
