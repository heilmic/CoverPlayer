[CmdletBinding()]
param(
    [string]$Mp3Path,
    [switch]$SkipPackages
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$msysBin = 'C:\msys64\ucrt64\bin'
$cmakePath = Join-Path $msysBin 'cmake.exe'
$ctestPath = Join-Path $msysBin 'ctest.exe'

if (!(Test-Path -LiteralPath $cmakePath) -or !(Test-Path -LiteralPath $ctestPath)) {
    throw 'MSYS2 UCRT64 CMake and CTest are required under C:\msys64\ucrt64\bin.'
}
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (!$pythonCommand) { throw 'Python is required on PATH.' }
$env:Path = "$msysBin;$env:Path"

Push-Location $repositoryRoot
try {
    & $cmakePath --preset desktop-debug
    if ($LASTEXITCODE -ne 0) { throw 'Desktop configuration failed.' }
    & $cmakePath --build --preset desktop-debug
    if ($LASTEXITCODE -ne 0) { throw 'Desktop build failed.' }
    & $ctestPath --test-dir "$repositoryRoot/build/desktop-debug" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Desktop tests failed.' }
    & $pythonCommand.Source "$repositoryRoot/tests/verify_package_test.py"
    if ($LASTEXITCODE -ne 0) { throw 'Package verifier tests failed.' }

    if ($Mp3Path) {
        if (!(Test-Path -LiteralPath $Mp3Path -PathType Leaf)) { throw "MP3 not found: $Mp3Path" }
        & "$repositoryRoot/build/desktop-debug/coverplayer_player_seek_test.exe" $Mp3Path
        if ($LASTEXITCODE -ne 0) { throw 'MP3 seek test failed.' }
    }

    if (!$SkipPackages) {
        & "$PSScriptRoot/Build-Packages.ps1"
        Write-Host "Packages: $repositoryRoot/build/release"
    }
    Write-Host 'Build and tests passed.'
}
finally {
    Pop-Location
}
