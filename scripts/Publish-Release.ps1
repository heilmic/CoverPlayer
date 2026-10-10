[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+(?:-rc\.\d+)?$')]
    [string]$Tag,
    [string]$Title,
    [switch]$SkipBuild,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$releaseRoot = Join-Path $repositoryRoot 'build/release'
$changelog = Join-Path $repositoryRoot 'CHANGELOG.md'
$python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
if ([string]::IsNullOrWhiteSpace($python)) { throw 'Python is required for package verification.' }
if (!(Get-Command git.exe -ErrorAction SilentlyContinue)) { throw 'Git is required.' }
if (!(Get-Command gh.exe -ErrorAction SilentlyContinue)) { throw 'GitHub CLI is required.' }

$lines = Get-Content -LiteralPath $changelog -Encoding UTF8
$headingPattern = '^##\s+' + [regex]::Escape($Tag) + '(?:\s|$)'
$headingIndex = -1
for ($index = 0; $index -lt $lines.Count; ++$index) {
    if ($lines[$index] -match $headingPattern) { $headingIndex = $index; break }
}
if ($headingIndex -lt 0) { throw "Release notes for $Tag are missing in CHANGELOG.md." }
$body = @()
for ($index = $headingIndex + 1; $index -lt $lines.Count -and $lines[$index] -notmatch '^##\s+'; ++$index) {
    $body += $lines[$index]
}
$notes = ($body -join "`n").Trim()
if ([string]::IsNullOrWhiteSpace($notes)) { throw "Release notes for $Tag are empty." }
if ([string]::IsNullOrWhiteSpace($Title)) {
    $Title = 'CoverPlayer ' + ($Tag -replace '^v', '' -replace '-rc\.', ' RC')
}

Push-Location $repositoryRoot
try {
    if (!$DryRun) {
        $branch = (& git branch --show-current).Trim()
        if ($branch -ne 'main') { throw "Release must run from main; current branch is $branch." }
        if (& git status --porcelain) { throw 'Commit all source and changelog changes before publishing.' }
        & gh auth status *> $null
        if ($LASTEXITCODE -ne 0) { throw 'GitHub CLI is not authenticated.' }
        & gh release view $Tag *> $null
        if ($LASTEXITCODE -eq 0) { throw "GitHub release $Tag already exists." }
        if (!$SkipBuild) {
            & (Join-Path $PSScriptRoot 'Build-And-Test.ps1')
            if ($LASTEXITCODE -ne 0) { throw 'Build and tests failed.' }
        }
    }

    $assets = @(
        (Join-Path $releaseRoot 'CoverPlayer-Knulli.zip'),
        (Join-Path $releaseRoot 'CoverPlayer-Knulli-Test.zip'),
        (Join-Path $releaseRoot 'CoverPlayer.muxapp')
    )
    foreach ($asset in $assets) {
        if (!(Test-Path -LiteralPath $asset -PathType Leaf)) { throw "Missing release asset: $asset" }
        & $python (Join-Path $PSScriptRoot 'verify-package.py') $asset
        if ($LASTEXITCODE -ne 0) { throw "Release asset verification failed: $asset" }
    }
    if ($DryRun) {
        Write-Host "Release preflight passed for $Tag ($Title), with $($assets.Count) verified assets."
        return
    }

    $existingTag = & git rev-parse -q --verify "refs/tags/$Tag"
    if ($LASTEXITCODE -eq 0) {
        $head = (& git rev-parse HEAD).Trim()
        if ($existingTag.Trim() -ne $head) { throw "Tag $Tag points to another commit." }
    }

    & git push origin main
    if ($LASTEXITCODE -ne 0) { throw 'Pushing main failed.' }
    if (!$existingTag) {
        & git tag $Tag
        if ($LASTEXITCODE -ne 0) { throw "Creating tag $Tag failed." }
    }
    & git push origin $Tag
    if ($LASTEXITCODE -ne 0) { throw "Pushing tag $Tag failed." }

    $notesFile = [IO.Path]::GetTempFileName()
    try {
        [IO.File]::WriteAllText($notesFile, $notes + "`n", [Text.UTF8Encoding]::new($false))
        $releaseArgs = @('release', 'create', $Tag) + $assets +
            @('--verify-tag', '--title', $Title, '--notes-file', $notesFile)
        if ($Tag -match '-rc\.') { $releaseArgs += '--prerelease' }
        else { $releaseArgs += '--latest' }
        & gh @releaseArgs
        if ($LASTEXITCODE -ne 0) { throw "Publishing GitHub release $Tag failed." }
    } finally {
        Remove-Item -LiteralPath $notesFile -Force
    }
} finally {
    Pop-Location
}
