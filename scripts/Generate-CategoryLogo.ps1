[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.Drawing
$fonts = [Drawing.Text.PrivateFontCollection]::new()
$outline = [Drawing.Drawing2D.GraphicsPath]::new()
try {
    $fonts.AddFontFile((Join-Path $repositoryRoot 'assets/fonts/RobotoMono-Bold.ttf'))
    foreach ($line in @(
        @{ Text = 'Cover'; Top = 14 },
        @{ Text = 'Player'; Top = 96 }
    )) {
        $lineOutline = [Drawing.Drawing2D.GraphicsPath]::new()
        $matrix = [Drawing.Drawing2D.Matrix]::new()
        try {
            $lineOutline.AddString($line.Text, $fonts.Families[0], [int][Drawing.FontStyle]::Bold,
                72, [Drawing.PointF]::new(0, 0), [Drawing.StringFormat]::GenericTypographic)
            $bounds = $lineOutline.GetBounds()
            $matrix.Translate(126 - $bounds.X, $line.Top - $bounds.Y)
            $lineOutline.Transform($matrix)
            $outline.AddPath($lineOutline, $false)
        } finally {
            $matrix.Dispose()
            $lineOutline.Dispose()
        }
    }
    $points = $outline.PathPoints
    $types = $outline.PathTypes
    $commands = [Collections.Generic.List[string]]::new()
    function Point-Text($point) {
        $point.X.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture) + ' ' +
            $point.Y.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)
    }
    for ($index = 0; $index -lt $points.Length; ++$index) {
        switch ($types[$index] -band 7) {
            0 { $commands.Add('M' + (Point-Text $points[$index])) }
            1 { $commands.Add('L' + (Point-Text $points[$index])) }
            3 {
                $commands.Add('C' + (Point-Text $points[$index]) + ' ' +
                    (Point-Text $points[$index + 1]) + ' ' + (Point-Text $points[$index + 2]))
                $index += 2
            }
            default { throw 'Unexpected font outline segment.' }
        }
        if ($types[$index] -band 128) { $commands.Add('Z') }
    }
    $glyphPath = $commands -join ' '
    $svg = @"
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="176" viewBox="0 0 400 176" preserveAspectRatio="xMidYMid meet">
  <title>CoverPlayer portable MP3 player and wordmark</title>
  <!-- Wordmark outlines use the bundled Apache-2.0 Roboto Mono Bold font. -->
  <g transform="translate(4 8) scale(0.5)">
    <rect x="28" y="8" width="184" height="304" rx="28" fill="none" stroke="#ffffff" stroke-width="8"/>
    <rect x="52" y="38" width="136" height="110" rx="10" fill="none" stroke="#ffffff" stroke-width="6"/>
    <path d="M112 59v51c-8-5-23-1-23 10 0 16 31 15 31-2V83l33-7v24c-8-5-23-1-23 10 0 16 31 15 31-2V49z" fill="#ffffff"/>
    <circle cx="120" cy="226" r="49" fill="none" stroke="#ffffff" stroke-width="6"/>
    <path d="m110 207 29 19-29 19z" fill="#ffffff"/>
  </g>
  <path d="$glyphPath" fill="#ffffff"/>
</svg>
"@
    foreach ($relativePath in @(
        'packaging/knulli-category/roms/coverplayer/coverplayer-logo.svg',
        'packaging/knulli-category/theme-customizations/art-book-next/logos/coverplayer.svg'
    )) {
        $destination = Join-Path $repositoryRoot $relativePath
        [IO.File]::WriteAllText($destination, $svg.Replace("`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
        Write-Host "Generated $destination"
    }
} finally {
    $outline.Dispose()
    $fonts.Dispose()
}
