Add-Type -AssemblyName System.Drawing

$assetsDir = Join-Path (Split-Path $PSScriptRoot -Parent) "assets"
$orbsPath = Join-Path $assetsDir "orbs_sheet.jpg"

if (-not (Test-Path $orbsPath)) {
    Write-Error "Missing $orbsPath"
    exit 1
}

$orbsImg = [System.Drawing.Bitmap]::FromFile($orbsPath)
$cellW = $orbsImg.Width / 4.0
$cellH = $orbsImg.Height / 2.0
$size = 180
$numFrames = 16

for ($r = 0; $r -lt 2; $r++) {
    for ($c = 0; $c -lt 4; $c++) {
        $idx = $r * 4 + $c
        $strip = New-Object System.Drawing.Bitmap($size, ($size * $numFrames), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($strip)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)

        $srcRect = New-Object System.Drawing.RectangleF(($c * $cellW + $cellW * 0.06), ($r * $cellH + $cellH * 0.04), ($cellW * 0.88), ($cellH * 0.92))

        for ($f = 0; $f -lt $numFrames; $f++) {
            $t = $f / [double]($numFrames - 1)
            $fy = $f * $size
            $pad = 8
            $orbRect = New-Object System.Drawing.RectangleF($pad, ($fy + $pad), ($size - $pad * 2), ($size - $pad * 2))

            # Brightness/contrast ramp from dormant (0.35) to energized (1.15)
            $gain = [float](0.35 + 0.80 * $t)
            $cm = New-Object System.Drawing.Imaging.ColorMatrix
            $cm.Matrix00 = $gain
            $cm.Matrix11 = $gain
            $cm.Matrix22 = $gain
            $cm.Matrix33 = 1.0
            $cm.Matrix44 = 1.0

            $ia = New-Object System.Drawing.Imaging.ImageAttributes
            $ia.SetColorMatrix($cm)

            # Clip orb to circle
            $path = New-Object System.Drawing.Drawing2D.GraphicsPath
            $path.AddEllipse($orbRect)
            $state = $g.Save()
            $g.SetClip($path)
            $g.DrawImage($orbsImg, [System.Drawing.Rectangle]::Round($orbRect), $srcRect.X, $srcRect.Y, $srcRect.Width, $srcRect.Height, [System.Drawing.GraphicsUnit]::Pixel, $ia)
            $g.Restore($state)
            $path.Dispose()
            $ia.Dispose()

            # Outer energy ring growing with t
            if ($t -gt 0.02) {
                $alpha = [int](40 + 215 * $t)
                $ringCol = if ($r -eq 0) { [System.Drawing.Color]::FromArgb($alpha, 0, 229, 255) } else { [System.Drawing.Color]::FromArgb($alpha, 255, 77, 141) }
                $penW = [float](1.2 + 1.8 * $t)
                $pen = New-Object System.Drawing.Pen($ringCol, $penW)
                $ringRect = New-Object System.Drawing.RectangleF(($pad - 2), ($fy + $pad - 2), ($size - $pad * 2 + 4), ($size - $pad * 2 + 4))
                $g.DrawArc($pen, $ringRect, -225, [float](270 * $t))
                $pen.Dispose()
            }
        }

        $g.Dispose()
        $outPath = Join-Path $assetsDir ("orb_strip_" + $idx + ".png")
        $strip.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $strip.Dispose()
        Write-Host "Wrote $outPath"
    }
}

$orbsImg.Dispose()

# Stack gate_pat_0..7.png into a single 8-frame vertical FilmStrip (gate_pat_strip.png)
$gw = 918
$gh = 107
$gateStrip = New-Object System.Drawing.Bitmap($gw, ($gh * 8), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gg = [System.Drawing.Graphics]::FromImage($gateStrip)
for ($i = 0; $i -lt 8; $i++) {
    $gpPath = Join-Path $assetsDir ("gate_pat_" + $i + ".png")
    if (Test-Path $gpPath) {
        $gpImg = [System.Drawing.Bitmap]::FromFile($gpPath)
        $gg.DrawImage($gpImg, 0, ($i * $gh), $gw, $gh)
        $gpImg.Dispose()
    }
}
$gg.Dispose()
$gateStripPath = Join-Path $assetsDir "gate_pat_strip.png"
$gateStrip.Save($gateStripPath, [System.Drawing.Imaging.ImageFormat]::Png)
$gateStrip.Dispose()
Write-Host "Wrote $gateStripPath"
