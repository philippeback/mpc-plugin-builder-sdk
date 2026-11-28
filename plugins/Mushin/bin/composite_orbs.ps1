Add-Type -AssemblyName System.Drawing

$pluginDir = Split-Path $PSScriptRoot -Parent
$sdkRoot = Split-Path (Split-Path $pluginDir -Parent) -Parent
$assetsDir = Join-Path $pluginDir "assets"
$orbsPath = Join-Path $assetsDir "orbs_sheet.jpg"

if (-not (Test-Path $orbsPath)) {
    Write-Error "Missing $orbsPath"
    exit 1
}

$zigPy = Join-Path $sdkRoot ".sdk-tools\zigenv\Scripts\python.exe"
$genCpp = Join-Path $PSScriptRoot "gen_orbs.cpp"
if (-not (Test-Path $zigPy) -or -not (Test-Path $genCpp)) {
    Write-Error "Missing Zig Python ($zigPy) or gen_orbs.cpp ($genCpp)"
    exit 1
}

$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "mushin_orbs_build"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

# 1. Decode orbs_sheet.jpg -> raw 32bpp BGRA
$orbsImg = [System.Drawing.Bitmap]::FromFile($orbsPath)
$w = $orbsImg.Width
$h = $orbsImg.Height
$rawBmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($rawBmp)
$g.DrawImage($orbsImg, 0, 0, $w, $h)
$g.Dispose()
$orbsImg.Dispose()

$rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
$bd = $rawBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bytes = New-Object byte[] ($bd.Stride * $h)
[System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bytes, 0, $bytes.Length)
$rawBmp.UnlockBits($bd)
$rawBmp.Dispose()

$rawPath = Join-Path $workDir "orbs_sheet.bgra"
[System.IO.File]::WriteAllBytes($rawPath, $bytes)

# 2. Compile & run high-speed 2x2 SSAA cyber-mecha orb shader
$exePath = Join-Path $workDir "gen_orbs.exe"
& $zigPy -m ziglang c++ -O3 -std=c++17 $genCpp -o $exePath
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to compile gen_orbs.cpp"
    exit 1
}

& $exePath $rawPath $w $h $workDir
if ($LASTEXITCODE -ne 0) {
    Write-Error "gen_orbs.exe failed"
    exit 1
}

# 3. Encode each 180x3960 (22-frame) raw strip -> PNG in assets/
$size = 180
$numFrames = 22
$stripH = $size * $numFrames

for ($idx = 0; $idx -lt 8; $idx++) {
    $bgraPath = Join-Path $workDir ("orb_strip_" + $idx + ".bgra")
    $outBytes = [System.IO.File]::ReadAllBytes($bgraPath)
    $bmp = New-Object System.Drawing.Bitmap($size, $stripH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $oRect = New-Object System.Drawing.Rectangle(0, 0, $size, $stripH)
    $oBd = $bmp.LockBits($oRect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    [System.Runtime.InteropServices.Marshal]::Copy($outBytes, 0, $oBd.Scan0, $outBytes.Length)
    $bmp.UnlockBits($oBd)

    $outPath = Join-Path $assetsDir ("orb_strip_" + $idx + ".png")
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Wrote $outPath"
}

Remove-Item -Recurse -Force $workDir -ErrorAction SilentlyContinue

# 4. Stack gate_pat_0..7.png into a single 8-frame vertical FilmStrip (gate_pat_strip.png)
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
