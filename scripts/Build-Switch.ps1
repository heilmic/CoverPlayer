[CmdletBinding()]
param(
    [string]$DockerImage = 'devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282'
)
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker is required.' }
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (!$pythonCommand) { throw 'Python 3 is required.' }
& docker run --rm --volume "${repositoryRoot}:/work" -w /work $DockerImage bash /work/scripts/build-switch.sh
if ($LASTEXITCODE -ne 0) { throw 'Switch cross-build failed.' }
& $pythonCommand.Source "$PSScriptRoot/package-switch.py" --docker-image $DockerImage
if ($LASTEXITCODE -ne 0) { throw 'Switch package verification failed.' }
Write-Host "Switch package: $repositoryRoot/build/release/CoverPlayer-Switch-1.0.0-alpha5.zip"
