[CmdletBinding()]
param(
    [string]$Address,
    [int]$Port,
    [string]$NroPath,
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (!$pythonCommand) { throw 'Python 3 is required.' }
if ($Build) { & "$PSScriptRoot/Build-Switch.ps1" }
if (!$NroPath) { $NroPath = Join-Path $repositoryRoot 'build/switch-alpha/CoverPlayer.nro' }
$arguments = @("$PSScriptRoot/deploy-switch.py", '--nro', $NroPath,
    '--config', "$PSScriptRoot/deploy-switch.local.json")
if ($Address) { $arguments += @('--host', $Address) }
if ($PSBoundParameters.ContainsKey('Port')) { $arguments += @('--port', "$Port") }
Write-Host 'CoverPlayer must be closed; start the FTP server on the Switch.'
& $pythonCommand.Source @arguments
if ($LASTEXITCODE -ne 0) { throw 'Switch FTP deployment failed.' }
