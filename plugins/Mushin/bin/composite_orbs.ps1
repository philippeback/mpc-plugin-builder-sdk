Add-Type -AssemblyName System.Drawing

$assetsDir = Join-Path (Split-Path $PSScriptRoot -Parent) "assets"
$orbsPath = Join-Path $assetsDir "orbs_sheet.jpg"
$bgPath = Join-Path $assetsDir "bg_main.png"

if (-not (Test-Path $orbsPath)) {
    Write-Error "Missing $orbsPath"
    exit 1
}

$orbsImg = [System.Drawing.Bitmap]::FromFile($orbsPath)
$bgBytes = [System.IO.File]::ReadAllBytes($bgPath)
$ms = New-Object System.IO.MemoryStream(,$bgBytes)
$bgOrig = [System.Drawing.Bitmap]::FromStream($ms)
$bgBmp = New-Object System.Drawing.Bitmap($bgOrig.Width, $bgOrig.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bgBmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.DrawImage($bgOrig, 0, 0, $bgOrig.Width, $bgOrig.Height)

$cellW = $orbsImg.Width / 4.0
$cellH = $orbsImg.Height / 2.0
$colCenters = @(162, 482, 802, 1122)
$rowCenters = @(244, 548)
$targetSize = 114

for ($r = 0; $r -lt 2; $r++) {
    for ($c = 0; $c -lt 4; $c++) {
        $srcRect = New-Object System.Drawing.RectangleF(($c * $cellW + $cellW * 0.06), ($r * $cellH + $cellH * 0.04), ($cellW * 0.88), ($cellH * 0.92))
        $dstX = $colCenters[$c] - ($targetSize / 2)
        $dstY = $rowCenters[$r] - ($targetSize / 2)
        $dstRect = New-Object System.Drawing.RectangleF($dstX, $dstY, $targetSize, $targetSize)

        # Clip to a soft circular path so the orb blends cleanly without any rectangular seam
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $path.AddEllipse($dstRect)
        $state = $g.Save()
        $g.SetClip($path)
        $g.DrawImage($orbsImg, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
        $g.Restore($state)
        $path.Dispose()
    }
}

$g.Dispose()
$bgOrig.Dispose()
$ms.Dispose()
$orbsImg.Dispose()

$bgBmp.Save($bgPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bgBmp.Dispose()
Write-Host "Composited 8 sculptural orbs into $bgPath"
