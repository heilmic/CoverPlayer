[CmdletBinding()]
param(
    [string[]]$Targets,
    [string]$User = 'root',
    [string]$Password,
    [switch]$IncludeTest,
    [switch]$TestOnly,
    [switch]$Build,
    [switch]$PreflightOnly
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$releaseRoot = Join-Path $repositoryRoot 'build/release'
$pscp = 'C:\Program Files\PuTTY\pscp.exe'
$plink = 'C:\Program Files\PuTTY\plink.exe'
$python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
$configPath = Join-Path $PSScriptRoot 'deploy.local.json'
$config = $null
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
}
if (!$PSBoundParameters.ContainsKey('Targets')) {
    if (!$config -or !$config.knulliTargets) {
        throw 'Configure scripts/deploy.local.json or pass -Targets explicitly.'
    }
    $Targets = @($config.knulliTargets | ForEach-Object { $_.address })
}

if (!(Test-Path -LiteralPath $pscp) -or !(Test-Path -LiteralPath $plink)) {
    throw 'PuTTY pscp.exe and plink.exe are required.'
}
if ([string]::IsNullOrWhiteSpace($python)) { throw 'Python is required for package verification.' }

if ($Build) {
    & (Join-Path $PSScriptRoot 'Build-And-Test.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build and tests failed.' }
}

if ($TestOnly -and $IncludeTest) { throw 'Choose -TestOnly or -IncludeTest, not both.' }

$packages = @(
    [pscustomobject]@{ Name = 'CoverPlayer-Knulli.zip'; Remote = '/tmp/CoverPlayer-Knulli.zip'; App = 'CoverPlayer' }
)
if ($TestOnly) { $packages = @() }
if ($IncludeTest -or $TestOnly) {
    $packages += [pscustomobject]@{ Name = 'CoverPlayer-Knulli-Test.zip'; Remote = '/tmp/CoverPlayer-Knulli-Test.zip'; App = 'CoverPlayer-Test' }
}

foreach ($package in $packages) {
    $localPath = Join-Path $releaseRoot $package.Name
    if (!(Test-Path -LiteralPath $localPath -PathType Leaf)) {
        throw "Missing package: $localPath. Run Build-Packages.ps1 first."
    }
    & $python (Join-Path $PSScriptRoot 'verify-package.py') $localPath
    if ($LASTEXITCODE -ne 0) { throw "Package verification failed: $($package.Name)" }
}
if ($Targets.Count -eq 0) { throw 'At least one target is required.' }
foreach ($target in $Targets) {
    if ($target -notmatch '^(\d{1,3}\.){3}\d{1,3}$') { throw "Invalid target address: $target" }
}
if ($PreflightOnly) {
    Write-Host "Knulli preflight passed for $($packages.Count) package(s) and $($Targets.Count) target(s)."
    return
}

if ([string]::IsNullOrWhiteSpace($Password)) {
    $securePassword = Read-Host 'SSH password' -AsSecureString
    $passwordPtr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
    try { $Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPtr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPtr) }
}

$hostKeys = @{}
if ($config -and $config.knulliTargets) {
    foreach ($configuredTarget in $config.knulliTargets) {
        if (![string]::IsNullOrWhiteSpace($configuredTarget.hostKey)) {
            $hostKeys[[string]$configuredTarget.address] = [string]$configuredTarget.hostKey
        }
    }
}

foreach ($target in $Targets) {
    $hostKeyArgs = @()
    if ($hostKeys.ContainsKey($target)) { $hostKeyArgs = @('-hostkey', $hostKeys[$target]) }
    Write-Host "Deploying CoverPlayer to $target..."

    & $plink @('-batch', '-ssh') @hostKeyArgs @('-pw', $Password, "$User@$target", 'set -e; command -v unzip >/dev/null; if ps -eo args= | grep -Eq "^([^ ]*/)?coverplayer( |$)"; then echo "CoverPlayer is still running" >&2; exit 4; fi') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Cannot connect to $target or unzip is unavailable." }

    foreach ($package in $packages) {
        $localPath = Join-Path $releaseRoot $package.Name
        & $pscp @('-batch', '-scp') @hostKeyArgs @('-pw', $Password, $localPath, "$User@${target}:$($package.Remote)")
        if ($LASTEXITCODE -ne 0) { throw "Upload failed on ${target}: $($package.Name)" }
        $localHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $localPath).Hash
        $remoteHashLine = & $plink @('-batch', '-ssh') @hostKeyArgs @('-pw', $Password, "$User@$target", "sha256sum $($package.Remote)")
        $remoteHashLine = [string]($remoteHashLine | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0) {
            throw "Could not verify uploaded archive on ${target}: $($package.Name)"
        }
        if ($remoteHashLine -match '^([0-9a-fA-F]{64})\s') { $remoteHash = $Matches[1] }
        else { throw "Could not read archive SHA256 on ${target}: $($package.Name)" }
        if ($remoteHash -ne $localHash) { throw "Archive SHA256 mismatch on ${target}: $($package.Name)" }
    }

    $remoteSteps = @('set -e')
    if ($TestOnly) {
        # Pause only running Syncthing processes, and resume them even on failure.
        $remoteSteps += 'ps -eo args= | grep -Eq "^([^ ]*/)?coverplayer( |$)" && exit 4'
        $remoteSteps += 'sync_pids=""'
        $remoteSteps += 'trap ''for pid in $sync_pids; do kill -CONT "$pid" 2>/dev/null || true; done'' EXIT HUP INT TERM'
        $remoteSteps += 'for pid in $(pidof syncthing 2>/dev/null); do state=$(awk ''{print $3}'' /proc/$pid/stat); case "$state" in T|t) ;; *) kill -STOP "$pid"; sync_pids="$sync_pids $pid" ;; esac; done'
        $remoteSteps += 'mkdir -p /userdata/system/coverplayer-backups'
        $remoteSteps += 'backup=/userdata/system/coverplayer-backups/CoverPlayer-Test-$(date +%Y%m%d-%H%M%S)-$$.tar.gz'
        $remoteSteps += 'cd /userdata/roms/ports'
        $remoteSteps += 'set --; for item in roms/ports/CoverPlayer-Test roms/ports/CoverPlayer-Test.sh system/configs/coverplayer-test; do if [ -e "/userdata/$item" ]; then set -- "$@" "$item"; fi; done; if [ "$#" -gt 0 ]; then tar -C /userdata -czf "$backup" "$@"; tar -tzf "$backup" >/dev/null; echo "Backup: $backup"; fi'
    }
    foreach ($package in $packages) {
        $remoteSteps += "unzip -o $($package.Remote) -d /userdata >/dev/null"
        $remoteSteps += 'cd /userdata'
        $remoteSteps += 'sha256sum -c MANIFEST.sha256 >/dev/null'
        $remoteSteps += "cd /userdata/roms/ports/$($package.App)"
        $remoteSteps += 'sha256sum -c MANIFEST.sha256 >/dev/null'
        $remoteSteps += "rm -f $($package.Remote)"
    }
    $remoteCommand = $remoteSteps -join '; '
    & $plink @('-batch', '-ssh') @hostKeyArgs @('-pw', $Password, "$User@$target", $remoteCommand)
    if ($LASTEXITCODE -ne 0) { throw "Install or manifest verification failed on $target." }
    Write-Host "Deployment verified on $target."
}

Write-Host 'Knulli deployment completed.'
