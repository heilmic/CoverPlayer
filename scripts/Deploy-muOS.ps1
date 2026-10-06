[CmdletBinding()]
param(
    [string]$Target,
    [string]$User,
    [string]$HostKey,
    [string]$Password,
    [switch]$Build,
    [switch]$PreflightOnly
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$package = Join-Path $repositoryRoot 'build/release/CoverPlayer.muxapp'
$python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
$pscp = 'C:\Program Files\PuTTY\pscp.exe'
$plink = 'C:\Program Files\PuTTY\plink.exe'
$remotePackage = '/mnt/mmc/ARCHIVE/CoverPlayer-auto.muxapp'
$configPath = Join-Path $PSScriptRoot 'deploy.local.json'
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    if (!$PSBoundParameters.ContainsKey('Target')) { $Target = [string]$config.muosTarget.address }
    if (!$PSBoundParameters.ContainsKey('User')) { $User = [string]$config.muosTarget.user }
    if (!$PSBoundParameters.ContainsKey('HostKey')) { $HostKey = [string]$config.muosTarget.hostKey }
}
if ([string]::IsNullOrWhiteSpace($User)) { $User = 'root' }
if ([string]::IsNullOrWhiteSpace($Target)) {
    throw 'Configure scripts/deploy.local.json or pass -Target explicitly.'
}

if ($Target -notmatch '^(\d{1,3}\.){3}\d{1,3}$') { throw "Invalid target address: $Target" }
if (!(Test-Path -LiteralPath $pscp) -or !(Test-Path -LiteralPath $plink)) {
    throw 'PuTTY pscp.exe and plink.exe are required.'
}
if ([string]::IsNullOrWhiteSpace($python)) { throw 'Python is required for package verification.' }
if ($Build) {
    & (Join-Path $PSScriptRoot 'Build-And-Test.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build and tests failed.' }
}
if (!(Test-Path -LiteralPath $package -PathType Leaf)) {
    throw "Missing package: $package. Run Build-And-Test.ps1 first."
}
& $python (Join-Path $PSScriptRoot 'verify-package.py') $package
if ($LASTEXITCODE -ne 0) { throw 'muOS package verification failed.' }
if ($PreflightOnly) {
    Write-Host "muOS preflight passed for $Target."
    return
}

if ([string]::IsNullOrWhiteSpace($Password)) {
    $securePassword = Read-Host 'muOS SSH password' -AsSecureString
    $passwordPtr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
    try { $Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPtr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPtr) }
}

$hostKeyArgs = @()
if (![string]::IsNullOrWhiteSpace($HostKey)) { $hostKeyArgs = @('-hostkey', $HostKey) }
$sshArgs = @('-batch', '-ssh') + $hostKeyArgs + @('-pw', $Password, "$User@$Target")
$scpArgs = @('-batch', '-scp') + $hostKeyArgs + @('-pw', $Password)
& $plink @sshArgs 'set -e; test -f /opt/muos/script/mux/extract.sh; test -d /mnt/mmc/ARCHIVE; if pgrep -x coverplayer >/dev/null; then echo "CoverPlayer is still running" >&2; exit 4; fi'
if ($LASTEXITCODE -ne 0) { throw "muOS preflight failed on $Target." }

& $pscp @scpArgs $package "$User@${Target}:$remotePackage"
if ($LASTEXITCODE -ne 0) { throw "muOS upload failed on $Target." }
$localHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $package).Hash
$remoteHashLine = & $plink @sshArgs "sha256sum $remotePackage"
$remoteHashLine = [string]($remoteHashLine | Select-Object -First 1)
if ($LASTEXITCODE -ne 0) { throw "Could not verify uploaded muOS package on $Target." }
if ($remoteHashLine -match '^([0-9a-fA-F]{64})\s') { $remoteHash = $Matches[1] }
else { throw "Could not read uploaded muOS package SHA256 on $Target." }
if ($remoteHash -ne $localHash) { throw "muOS package SHA256 mismatch on $Target." }

& $plink @sshArgs "set -e; sh /opt/muos/script/mux/extract.sh $remotePackage; cd /run/muos/storage/application/CoverPlayer; sha256sum -c MANIFEST.sha256 >/dev/null; echo COVERPLAYER_MUOS_VERIFIED"
if ($LASTEXITCODE -ne 0) { throw "muOS install or manifest verification failed on $Target." }
Write-Host "CoverPlayer deployment verified on muOS device $Target."
