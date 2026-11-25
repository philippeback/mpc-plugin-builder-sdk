param(
    [string]$SkinsDir = (Join-Path (Split-Path -Parent $PSScriptRoot) "skins")
)

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"

function New-ButtonCanvas([string]$BasePath) {
    $src = [System.Drawing.Bitmap]::FromFile($BasePath)
    $bmp = New-Object System.Drawing.Bitmap($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.DrawImage($src, 0, 0, $src.Width, $src.Height)
    $src.Dispose()
    return @($bmp, $g)
}

function Save-ButtonCanvas($bmp, $g, [string]$OutPath) {
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Get-StrokeColor([bool]$IsOn) {
    if ($IsOn) {
        return [System.Drawing.Color]::FromArgb(255, 13, 15, 18)
    }
    return [System.Drawing.Color]::FromArgb(235, 210, 222, 234)
}

function Get-AccentColor([bool]$IsOn) {
    if ($IsOn) {
        return [System.Drawing.Color]::FromArgb(255, 13, 15, 18)
    }
    return [System.Drawing.Color]::FromArgb(230, 0, 229, 255)
}

function Draw-Polyline($g, $pen, [float[]]$pts) {
    $arr = New-Object System.Drawing.PointF[] ($pts.Length / 2)
    for ($i = 0; $i -lt $arr.Length; $i++) {
        $arr[$i] = New-Object System.Drawing.PointF($pts[2 * $i], $pts[2 * $i + 1])
    }
    $g.DrawLines($pen, $arr)
}

function Draw-Curve($g, $pen, [float[]]$pts) {
    $arr = New-Object System.Drawing.PointF[] ($pts.Length / 2)
    for ($i = 0; $i -lt $arr.Length; $i++) {
        $arr[$i] = New-Object System.Drawing.PointF($pts[2 * $i], $pts[2 * $i + 1])
    }
    $g.DrawCurve($pen, $arr, 0.45)
}

function Draw-CenteredText($g, [string]$text, [float]$cx, [float]$cy, [float]$size, $color) {
    $font = New-Object System.Drawing.Font("Segoe UI", $size, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $brush = New-Object System.Drawing.SolidBrush($color)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $rect = New-Object System.Drawing.RectangleF(($cx - 40), ($cy - 14), 80, 28)
    $g.DrawString($text, $font, $brush, $rect, $sf)
    $sf.Dispose()
    $brush.Dispose()
    $font.Dispose()
}

function New-SegCanvas([string]$BasePath, [int]$Width) {
    $src = [System.Drawing.Bitmap]::FromFile($BasePath)
    $bmp = New-Object System.Drawing.Bitmap($Width, 40, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $leftW = [int][Math]::Floor($Width / 2.0)
    $rightW = $Width - $leftW
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle(0, 0, $leftW, 40)), (New-Object System.Drawing.Rectangle(0, 0, $leftW, 40)), [System.Drawing.GraphicsUnit]::Pixel)
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle($leftW, 0, $rightW, 40)), (New-Object System.Drawing.Rectangle(($src.Width - $rightW), 0, $rightW, 40)), [System.Drawing.GraphicsUnit]::Pixel)
    $src.Dispose()
    return @($bmp, $g)
}

# 1. Filter Type (seg4: 58x40): 0=Clean, 1=Vowel, 2=Acid, 3=Digital
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg4_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.2)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: Clean sine wave
    $bmp, $g = New-ButtonCanvas $base
    $pts = @()
    for ($i = 0; $i -le 16; $i++) {
        $t = $i / 16.0
        $x = 13.0 + $t * 32.0
        $y = 20.0 - 8.0 * [Math]::Sin($t * 2.0 * [Math]::PI)
        $pts += [float]$x; $pts += [float]$y
    }
    Draw-Curve $g $pen $pts
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_ftype_0_$state.png")

    # 1: Vowel (dual vocal formant peaks)
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(10, 28, 16, 25, 22, 10, 29, 22, 36, 12, 42, 25, 48, 28)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_ftype_1_$state.png")

    # 2: Acid (303 resonant spike)
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(12, 18, 22, 18, 29, 9, 34, 19, 44, 29)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_ftype_2_$state.png")

    # 3: Digital (stepped staircase)
    $bmp, $g = New-ButtonCanvas $base
    Draw-Polyline $g $pen @(13, 27, 21, 27, 21, 20, 29, 20, 29, 13, 37, 13, 37, 27, 45, 27)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_ftype_3_$state.png")

    $pen.Dispose()
}

# 2. Filter Mode (seg4: 58x40): 0=LP, 1=HP, 2=BP, 3=Notch
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg4_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.2)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: LP
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(13, 14, 24, 14, 30, 12, 37, 21, 45, 29)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_fmode_0_$state.png")

    # 1: HP
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(13, 29, 21, 21, 28, 12, 34, 14, 45, 14)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_fmode_1_$state.png")

    # 2: BP
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(13, 28, 22, 24, 29, 11, 36, 24, 45, 28)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_fmode_2_$state.png")

    # 3: Notch
    $bmp, $g = New-ButtonCanvas $base
    Draw-Polyline $g $pen @(13, 13, 23, 13, 29, 28, 35, 13, 45, 13)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_fmode_3_$state.png")

    $pen.Dispose()
}

# 3. Routing (seg2: 122x40): 0=Serial, 1=Parallel (from mushin/Source/Web/index.html:1559-1578)
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg2_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.0)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: Serial [A] -- [B]
    $bmp, $g = New-ButtonCanvas $base
    $g.DrawRectangle($pen, 31, 13, 16, 14)
    $g.DrawRectangle($pen, 75, 13, 16, 14)
    $dashPen = New-Object System.Drawing.Pen($col, 2.0)
    $dashPen.DashPattern = @(2.0, 2.0)
    $g.DrawLine($dashPen, 49, 20, 73, 20)
    $dashPen.Dispose()
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_route_0_$state.png")

    # 1: Parallel split -> [A] / [B] -> join
    $bmp, $g = New-ButtonCanvas $base
    $g.DrawLine($pen, 29, 20, 41, 20)
    $g.DrawLine($pen, 41, 13, 41, 27)
    $g.DrawLine($pen, 41, 13, 51, 13)
    $g.DrawLine($pen, 41, 27, 51, 27)
    $g.DrawRectangle($pen, 51, 9, 20, 8)
    $g.DrawRectangle($pen, 51, 23, 20, 8)
    $g.DrawLine($pen, 71, 13, 81, 13)
    $g.DrawLine($pen, 71, 27, 81, 27)
    $g.DrawLine($pen, 81, 13, 81, 27)
    $g.DrawLine($pen, 81, 20, 93, 20)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_route_1_$state.png")

    $pen.Dispose()
}

# 4. Noise Route (seg3: 80x40): 0=PRE-D, 1=PRE-F, 2=POST
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg3_$state.png"
    $col = Get-StrokeColor $isOn
    $acc = Get-AccentColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 1.8)
    $brush = New-Object System.Drawing.SolidBrush($acc)

    for ($idx = 0; $idx -lt 3; $idx++) {
        $bmp, $g = New-ButtonCanvas $base
        # Signal line + two blocks [D] and [F]
        $g.DrawLine($pen, 10, 20, 70, 20)
        $g.DrawRectangle($pen, 25, 14, 12, 12)
        $g.DrawRectangle($pen, 45, 14, 12, 12)
        if ($idx -eq 0) { $dx = 15 }
        elseif ($idx -eq 1) { $dx = 41 }
        else { $dx = 65 }
        $g.FillEllipse($brush, ($dx - 4), 16, 8, 8)
        Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_nroute_${idx}_$state.png")
    }

    $brush.Dispose()
    $pen.Dispose()
}

# 5. Gate Rate (seg3: 80x40): 0=1/16, 1=1/8, 2=1/4
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg3_$state.png"
    $col = Get-StrokeColor $isOn
    $brush = New-Object System.Drawing.SolidBrush($col)

    # 0: 1/16 (4 bars + text)
    $bmp, $g = New-ButtonCanvas $base
    for ($b = 0; $b -lt 4; $b++) {
        $g.FillRectangle($brush, (22 + $b * 10), 8, 6, 8)
    }
    Draw-CenteredText $g "1/16" 40 27 12 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_grate_0_$state.png")

    # 1: 1/8 (2 bars + text)
    $bmp, $g = New-ButtonCanvas $base
    $g.FillRectangle($brush, 22, 8, 15, 8)
    $g.FillRectangle($brush, 43, 8, 15, 8)
    Draw-CenteredText $g "1/8" 40 27 12 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_grate_1_$state.png")

    # 2: 1/4 (1 wide bar + text)
    $bmp, $g = New-ButtonCanvas $base
    $g.FillRectangle($brush, 22, 8, 36, 8)
    Draw-CenteredText $g "1/4" 40 27 12 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_grate_2_$state.png")

    $brush.Dispose()
}

# 6. Delay Tempo (seg4: 58x40): 0=1/16, 1=1/8, 2=1/4, 3=1/2
$dLabels = @("1/16", "1/8", "1/4", "1/2")
$dDots = @(4, 3, 2, 1)
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg4_$state.png"
    $col = Get-StrokeColor $isOn
    $brush = New-Object System.Drawing.SolidBrush($col)

    for ($idx = 0; $idx -lt 4; $idx++) {
        $bmp, $g = New-ButtonCanvas $base
        Draw-CenteredText $g $dLabels[$idx] 29 15 12 $col
        $n = $dDots[$idx]
        $startX = 29 - (($n - 1) * 7) / 2.0
        for ($d = 0; $d -lt $n; $d++) {
            $g.FillEllipse($brush, ($startX + $d * 7 - 2), 27, 4, 4)
        }
        Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_dtempo_${idx}_$state.png")
    }
    $brush.Dispose()
}

# 7. Limiter Mode (seg2: 122x40): 0=CLEAN (brickwall), 1=LMC (soft squash)
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg2_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.2)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: CLEAN brickwall
    $bmp, $g = New-ButtonCanvas $base
    Draw-Polyline $g $pen @(26, 29, 44, 13, 62, 13)
    Draw-CenteredText $g "CLN" 86 20 13 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lmode_0_$state.png")

    # 1: LMC soft squash
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(26, 29, 38, 17, 50, 14, 62, 13)
    Draw-CenteredText $g "LMC" 86 20 13 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lmode_1_$state.png")

    $pen.Dispose()
}

# 8. LFO Wave (46x40): 0=Sine, 1=Triangle, 2=Saw, 3=Square, 4=Random
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg4_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.1)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: Sine
    $bmp, $g = New-SegCanvas $base 46
    $pts = @()
    for ($i = 0; $i -le 16; $i++) {
        $t = $i / 16.0
        $x = 10.0 + $t * 26.0
        $y = 20.0 - 7.5 * [Math]::Sin($t * 2.0 * [Math]::PI)
        $pts += [float]$x; $pts += [float]$y
    }
    Draw-Curve $g $pen $pts
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lfowave_0_$state.png")

    # 1: Triangle
    $bmp, $g = New-SegCanvas $base 46
    Draw-Polyline $g $pen @(10, 20, 16.5, 12, 29.5, 28, 36, 20)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lfowave_1_$state.png")

    # 2: Saw
    $bmp, $g = New-SegCanvas $base 46
    Draw-Polyline $g $pen @(10, 27, 23, 13, 23, 27, 36, 13)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lfowave_2_$state.png")

    # 3: Square
    $bmp, $g = New-SegCanvas $base 46
    Draw-Polyline $g $pen @(10, 26, 10, 13, 23, 13, 23, 27, 36, 27, 36, 14)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lfowave_3_$state.png")

    # 4: Random (stepped S&H)
    $bmp, $g = New-SegCanvas $base 46
    Draw-Polyline $g $pen @(10, 22, 16, 22, 16, 13, 22, 13, 22, 27, 29, 27, 29, 17, 36, 17)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_lfowave_4_$state.png")

    $pen.Dispose()
}

# 9. Noise Type (38x40): 0=White, 1=Pink, 2=Sine, 3=Triangle, 4=Saw, 5=Square
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg4_$state.png"
    $col = Get-StrokeColor $isOn
    $acc = Get-AccentColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.0)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $pinkPen = New-Object System.Drawing.Pen($acc, 2.0)
    $pinkPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pinkPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pinkPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: White noise (dense uniform jagged spikes)
    $bmp, $g = New-SegCanvas $base 38
    Draw-Polyline $g $pen @(7, 20, 10, 11, 13, 28, 16, 12, 19, 27, 22, 10, 25, 29, 28, 13, 31, 20)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_0_$state.png")

    # 1: Pink noise (warmer 1/f filtered wave in accent colour)
    $bmp, $g = New-SegCanvas $base 38
    Draw-Curve $g $pinkPen @(7, 21, 11, 13, 15, 25, 20, 14, 25, 24, 29, 17, 31, 20)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_1_$state.png")

    # 2: Sine
    $bmp, $g = New-SegCanvas $base 38
    $pts = @()
    for ($i = 0; $i -le 14; $i++) {
        $t = $i / 14.0
        $x = 8.0 + $t * 22.0
        $y = 20.0 - 7.0 * [Math]::Sin($t * 2.0 * [Math]::PI)
        $pts += [float]$x; $pts += [float]$y
    }
    Draw-Curve $g $pen $pts
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_2_$state.png")

    # 3: Triangle
    $bmp, $g = New-SegCanvas $base 38
    Draw-Polyline $g $pen @(8, 20, 13.5, 12, 24.5, 28, 30, 20)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_3_$state.png")

    # 4: Saw
    $bmp, $g = New-SegCanvas $base 38
    Draw-Polyline $g $pen @(8, 27, 19, 13, 19, 27, 30, 13)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_4_$state.png")

    # 5: Square
    $bmp, $g = New-SegCanvas $base 38
    Draw-Polyline $g $pen @(8, 26, 8, 13, 19, 13, 19, 27, 30, 27, 30, 14)
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_noisetype_5_$state.png")

    $pinkPen.Dispose()
    $pen.Dispose()
}

# 10. Filter XYPad pucks (20x20): cyan (Filter A) & magenta (Filter B)
foreach ($puck in @(
    @{ Name = "puck_cyan.png";    R = 0;   G = 229; B = 255 },
    @{ Name = "puck_magenta.png"; R = 255; G = 77;  B = 141 }
)) {
    $bmp = New-Object System.Drawing.Bitmap(20, 20, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $glow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(75, $puck.R, $puck.G, $puck.B))
    $fill = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, $puck.R, $puck.G, $puck.B))
    $ring = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 240, 244, 248), 2.0)
    $g.FillEllipse($glow, 1, 1, 18, 18)
    $g.FillEllipse($fill, 4, 4, 12, 12)
    $g.DrawEllipse($ring, 4, 4, 12, 12)
    $ring.Dispose(); $fill.Dispose(); $glow.Dispose()
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir $puck.Name)
}

# 10b. LFO XYPad puck (22x22): dual cyan (LFO 1 X) + magenta (LFO 2 Y)
$bmp = New-Object System.Drawing.Bitmap(22, 22, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)
$glowC = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(75, 0, 229, 255))
$glowM = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(75, 255, 77, 141))
$fillC = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 229, 255))
$fillM = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 77, 141))
$ring  = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 240, 244, 248), 2.0)
$g.FillPie($glowC, 1, 1, 20, 20, 90, 180)
$g.FillPie($glowM, 1, 1, 20, 20, 270, 180)
$g.FillPie($fillC, 4, 4, 14, 14, 90, 180)
$g.FillPie($fillM, 4, 4, 14, 14, 270, 180)
$g.DrawEllipse($ring, 4, 4, 14, 14)
$ring.Dispose(); $fillM.Dispose(); $fillC.Dispose(); $glowM.Dispose(); $glowC.Dispose()
Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "puck_lfo.png")

# 11. Sidechain Source (seg2: 122x40): 0=INT, 1=TEST
$scSrcLabels = @("INT", "TEST")
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg2_$state.png"
    $col = Get-StrokeColor $isOn
    for ($idx = 0; $idx -lt 2; $idx++) {
        $bmp, $g = New-ButtonCanvas $base
        Draw-CenteredText $g $scSrcLabels[$idx] 61 20 14 $col
        Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_scsource_${idx}_$state.png")
    }
}

# 12. Sidechain Mode (seg2: 122x40): 0=PEAK, 1=RMS
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg2_$state.png"
    $col = Get-StrokeColor $isOn
    $pen = New-Object System.Drawing.Pen($col, 2.0)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # 0: PEAK (sharp transient spike)
    $bmp, $g = New-ButtonCanvas $base
    Draw-Polyline $g $pen @(22, 27, 30, 27, 36, 11, 42, 27, 52, 27)
    Draw-CenteredText $g "PEAK" 82 20 13 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_scmode_0_$state.png")

    # 1: RMS (smooth envelope)
    $bmp, $g = New-ButtonCanvas $base
    Draw-Curve $g $pen @(22, 27, 30, 17, 38, 15, 46, 18, 54, 27)
    Draw-CenteredText $g "RMS" 82 20 13 $col
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_scmode_1_$state.png")

    $pen.Dispose()
}

# 13. Sidechain Target (seg3: 80x40): 0=DRIVE, 1=CUT, 2=GAIN
$scTgtLabels = @("DRIVE", "CUT", "GAIN")
foreach ($state in @("on", "off")) {
    $isOn = ($state -eq "on")
    $base = Join-Path $SkinsDir "seg3_$state.png"
    $col = Get-StrokeColor $isOn
    for ($idx = 0; $idx -lt 3; $idx++) {
        $bmp, $g = New-ButtonCanvas $base
        Draw-CenteredText $g $scTgtLabels[$idx] 40 20 13 $col
        Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "btn_sctarget_${idx}_$state.png")
    }
}

# 14. Pump Meters (40x190): meter_in (cyan) & meter_duck (magenta)
foreach ($m in @(
    @{ Prefix = "meter_in";   R = 0;   G = 229; B = 255 },
    @{ Prefix = "meter_duck"; R = 255; G = 77;  B = 141 }
)) {
    foreach ($state in @("off", "on")) {
        $bmp = New-Object System.Drawing.Bitmap(40, 190, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.Clear([System.Drawing.Color]::FromArgb(255, 12, 16, 22))
        $alpha = if ($state -eq "on") { 245 } else { 40 }
        $segBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb($alpha, $m.R, $m.G, $m.B))
        for ($y = 4; $y -le 182; $y += 6) {
            $g.FillRectangle($segBrush, 4, $y, 32, 4)
        }
        $segBrush.Dispose()
        Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir ($m.Prefix + "_$state.png"))
    }
}

# 15. Limiter Horizontal GR Meter (558x40): meter_lim_off.png & meter_lim_on.png
foreach ($state in @("off", "on")) {
    $bmp = New-Object System.Drawing.Bitmap(558, 40, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(255, 12, 16, 22))
    $alpha = if ($state -eq "on") { 245 } else { 40 }
    for ($x = 2; $x -le 552; $x += 9) {
        $t = [double]$x / 552.0
        $r = [int](0 + 255 * $t)
        $gr = [int](229 + (77 - 229) * $t)
        $b = [int](255 + (141 - 255) * $t)
        $segBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb($alpha, $r, $gr, $b))
        $g.FillRectangle($segBrush, $x, 4, 6, 32)
        $segBrush.Dispose()
    }
    Save-ButtonCanvas $bmp $g (Join-Path $SkinsDir "meter_lim_$state.png")
}

Write-Host "Generated 90 icon/puck/meter PNGs in $SkinsDir"
