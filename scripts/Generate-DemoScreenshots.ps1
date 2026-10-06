[CmdletBinding()]
param(
    [string]$CoverSource,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($CoverSource)) {
    $CoverSource = Join-Path $repositoryRoot 'docs/screenshots/source-covers'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'docs/screenshots/generated'
}

$requiredCovers = @(
    '236.png', '237.png', '238.png', '239.png', '240.png',
    'kids-070.jpg', 'kids-071.jpg', 'kids-103.jpg', 'kids-086.jpg', 'kids-066.jpg',
    'checkpod.jpg', 'hp-1.jpg', 'hp-2.jpg', 'hp-3.jpg', 'hp-4.jpg', 'hp-5.jpg',
    'music-foo.jpg', 'music-linkin.jpg', 'music-rhcp.jpg', 'music-acdc.jpg', 'music-stones.jpg'
)
foreach ($name in $requiredCovers) {
    if (!(Test-Path -LiteralPath (Join-Path $CoverSource $name) -PathType Leaf)) {
        throw "Missing demo cover: $(Join-Path $CoverSource $name)"
    }
}

$msysBin = 'C:\msys64\ucrt64\bin'
$cmake = Join-Path $msysBin 'cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) { throw "CMake not found: $cmake" }
$env:Path = "$msysBin;$env:Path"
Push-Location $repositoryRoot
try {
    & $cmake --preset desktop-debug
    if ($LASTEXITCODE -ne 0) { throw 'Desktop configuration failed.' }
    & $cmake --build --preset desktop-debug --target coverplayer_screenshot_tool
    if ($LASTEXITCODE -ne 0) { throw 'Screenshot tool build failed.' }
} finally {
    Pop-Location
}

$rawDirectory = Join-Path $repositoryRoot 'build/screenshots/demo-raw'
$toolDirectory = Join-Path $repositoryRoot 'build/desktop-debug'
$tool = Join-Path $toolDirectory 'coverplayer_screenshot_tool.exe'
New-Item -ItemType Directory -Force -Path $rawDirectory, $OutputDirectory | Out-Null
$renderProcess = Start-Process -FilePath $tool -ArgumentList @($rawDirectory, $CoverSource, 'de') `
    -WorkingDirectory $toolDirectory -Wait -PassThru -WindowStyle Hidden
if ($renderProcess.ExitCode -ne 0) { throw "Demo rendering failed with exit code $($renderProcess.ExitCode)." }

Add-Type -AssemblyName System.Drawing
$screenshots = Get-ChildItem -LiteralPath $rawDirectory -Filter '*.bmp' -File | Sort-Object Name
if ($screenshots.Count -ne 14) { throw "Expected 14 rendered screenshots; found $($screenshots.Count)." }
foreach ($screenshot in $screenshots) {
    $destination = Join-Path $OutputDirectory ($screenshot.BaseName + '.png')
    $bitmap = [System.Drawing.Bitmap]::new($screenshot.FullName)
    try { $bitmap.Save($destination, [System.Drawing.Imaging.ImageFormat]::Png) }
    finally { $bitmap.Dispose() }
}
Copy-Item -LiteralPath (Join-Path $OutputDirectory '01-coverflow.png') `
    -Destination (Join-Path $OutputDirectory 'coverplayer-coverflow.png') -Force
Write-Host "Saved $($screenshots.Count) CoverPlayer demo screenshots to $OutputDirectory"
