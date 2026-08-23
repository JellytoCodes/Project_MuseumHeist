param(
    [string]$OutputDirectory = "D:\Dev\UE5.8\Project_MuseumHeist\SourceArt\UI\Concepts\Monochrome"
)

Add-Type -AssemblyName System.Drawing

$Charcoal = [System.Drawing.Color]::FromArgb(255, 45, 43, 40)
$CharcoalHover = [System.Drawing.Color]::FromArgb(255, 73, 69, 63)
$CharcoalPressed = [System.Drawing.Color]::FromArgb(255, 32, 30, 28)
$DarkOutline = [System.Drawing.Color]::FromArgb(255, 22, 21, 20)
$Ivory = [System.Drawing.Color]::FromArgb(255, 216, 208, 191)
$MutedLine = [System.Drawing.Color]::FromArgb(220, 103, 98, 90)

function New-Canvas {
    param([int]$Width, [int]$Height)

    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    return @($bitmap, $graphics)
}

function New-ChamferedPath {
    param(
        [int]$Left,
        [int]$Top,
        [int]$Right,
        [int]$Bottom,
        [int]$Chamfer
    )

    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $points = [System.Drawing.Point[]]@(
        [System.Drawing.Point]::new($Left + $Chamfer, $Top),
        [System.Drawing.Point]::new($Right - $Chamfer, $Top),
        [System.Drawing.Point]::new($Right, $Top + $Chamfer),
        [System.Drawing.Point]::new($Right, $Bottom - $Chamfer),
        [System.Drawing.Point]::new($Right - $Chamfer, $Bottom),
        [System.Drawing.Point]::new($Left + $Chamfer, $Bottom),
        [System.Drawing.Point]::new($Left, $Bottom - $Chamfer),
        [System.Drawing.Point]::new($Left, $Top + $Chamfer)
    )
    $path.AddPolygon($points)
    return $path
}

function Save-Canvas {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [System.Drawing.Graphics]$Graphics,
        [string]$FileName
    )

    $outputPath = Join-Path $OutputDirectory $FileName
    $Bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $Graphics.Dispose()
    $Bitmap.Dispose()
}

function New-ButtonImage {
    param(
        [System.Drawing.Color]$FillColor,
        [int]$ContentOffsetY,
        [string]$FileName
    )

    $canvas = New-Canvas 2048 512
    $bitmap = $canvas[0]
    $graphics = $canvas[1]

    $path = New-ChamferedPath 48 (52 + $ContentOffsetY) 2000 (460 + $ContentOffsetY) 72
    $fillBrush = [System.Drawing.SolidBrush]::new($FillColor)
    $outlinePen = [System.Drawing.Pen]::new($DarkOutline, 24)
    $outlinePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.FillPath($fillBrush, $path)
    $graphics.DrawPath($outlinePen, $path)

    $innerPath = New-ChamferedPath 76 (80 + $ContentOffsetY) 1972 (432 + $ContentOffsetY) 52
    $innerPen = [System.Drawing.Pen]::new($Ivory, 8)
    $innerPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.DrawPath($innerPen, $innerPath)

    $accentPen = [System.Drawing.Pen]::new($MutedLine, 8)
    $graphics.DrawLine($accentPen, 168, 104 + $ContentOffsetY, 424, 104 + $ContentOffsetY)
    $graphics.DrawLine($accentPen, 1624, 104 + $ContentOffsetY, 1880, 104 + $ContentOffsetY)

    $accentPen.Dispose()
    $innerPen.Dispose()
    $innerPath.Dispose()
    $outlinePen.Dispose()
    $fillBrush.Dispose()
    $path.Dispose()
    Save-Canvas $bitmap $graphics $FileName
}

function New-IconBadgeCanvas {
    $canvas = New-Canvas 2048 2048
    $bitmap = $canvas[0]
    $graphics = $canvas[1]

    $badgePath = New-ChamferedPath 240 240 1808 1808 184
    $fillBrush = [System.Drawing.SolidBrush]::new($Charcoal)
    $outlinePen = [System.Drawing.Pen]::new($DarkOutline, 64)
    $outlinePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.FillPath($fillBrush, $badgePath)
    $graphics.DrawPath($outlinePen, $badgePath)

    $innerPath = New-ChamferedPath 312 312 1736 1736 132
    $innerPen = [System.Drawing.Pen]::new($Ivory, 24)
    $innerPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.DrawPath($innerPen, $innerPath)

    $innerPen.Dispose()
    $innerPath.Dispose()
    $outlinePen.Dispose()
    $fillBrush.Dispose()
    $badgePath.Dispose()
    return @($bitmap, $graphics)
}

function New-ReadyCheckImage {
    $canvas = New-IconBadgeCanvas
    $bitmap = $canvas[0]
    $graphics = $canvas[1]

    $checkPen = [System.Drawing.Pen]::new($Ivory, 176)
    $checkPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $checkPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $checkPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawLines($checkPen, [System.Drawing.Point[]]@(
        [System.Drawing.Point]::new(608, 1048),
        [System.Drawing.Point]::new(896, 1336),
        [System.Drawing.Point]::new(1464, 720)
    ))
    $checkPen.Dispose()

    Save-Canvas $bitmap $graphics "T_UIIcon_ReadyCheck_Monochrome_2048.png"
}

function New-CopyImage {
    $canvas = New-IconBadgeCanvas
    $bitmap = $canvas[0]
    $graphics = $canvas[1]

    $copyPen = [System.Drawing.Pen]::new($Ivory, 96)
    $copyPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawRectangle($copyPen, 576, 544, 688, 688)
    $graphics.DrawRectangle($copyPen, 824, 792, 688, 688)
    $copyPen.Dispose()

    Save-Canvas $bitmap $graphics "T_UIIcon_Copy_Monochrome_2048.png"
}

function New-LogoImage {
    $canvas = New-Canvas 2048 2048
    $bitmap = $canvas[0]
    $graphics = $canvas[1]

    $charcoalBrush = [System.Drawing.SolidBrush]::new($Charcoal)
    $ivoryBrush = [System.Drawing.SolidBrush]::new($Ivory)
    $outlinePen = [System.Drawing.Pen]::new($DarkOutline, 48)
    $ivoryPen = [System.Drawing.Pen]::new($Ivory, 28)
    $ivoryPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter

    $roofPoints = [System.Drawing.Point[]]@(
        [System.Drawing.Point]::new(304, 704),
        [System.Drawing.Point]::new(1024, 264),
        [System.Drawing.Point]::new(1744, 704),
        [System.Drawing.Point]::new(1640, 816),
        [System.Drawing.Point]::new(408, 816)
    )
    $graphics.FillPolygon($charcoalBrush, $roofPoints)
    $graphics.DrawPolygon($outlinePen, $roofPoints)
    $graphics.DrawLines($ivoryPen, [System.Drawing.Point[]]@(
        [System.Drawing.Point]::new(488, 676),
        [System.Drawing.Point]::new(1024, 356),
        [System.Drawing.Point]::new(1560, 676)
    ))

    $graphics.FillRectangle($charcoalBrush, 352, 816, 1344, 120)
    $graphics.FillRectangle($charcoalBrush, 288, 1632, 1472, 152)
    $graphics.FillRectangle($charcoalBrush, 368, 1536, 1312, 96)
    $graphics.DrawRectangle($ivoryPen, 392, 840, 1264, 696)

    foreach ($columnX in (456, 736, 1248, 1528))
    {
        $graphics.FillRectangle($charcoalBrush, $columnX, 952, 128, 488)
        $graphics.DrawRectangle($ivoryPen, $columnX + 20, 976, 88, 440)
    }

    $doorPath = New-ChamferedPath 820 1008 1228 1536 48
    $graphics.FillPath($charcoalBrush, $doorPath)
    $graphics.DrawPath($ivoryPen, $doorPath)

    $graphics.FillEllipse($ivoryBrush, 936, 1120, 176, 176)
    $keyholePoints = [System.Drawing.Point[]]@(
        [System.Drawing.Point]::new(984, 1260),
        [System.Drawing.Point]::new(1064, 1260),
        [System.Drawing.Point]::new(1136, 1452),
        [System.Drawing.Point]::new(912, 1452)
    )
    $graphics.FillPolygon($ivoryBrush, $keyholePoints)

    $doorPath.Dispose()
    $ivoryPen.Dispose()
    $outlinePen.Dispose()
    $ivoryBrush.Dispose()
    $charcoalBrush.Dispose()

    Save-Canvas $bitmap $graphics "T_UILogo_MuseumHeist_Monochrome_2048.png"
}

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

New-ButtonImage -FillColor $Charcoal -ContentOffsetY 0 -FileName "T_UIButton_Monochrome_Normal_2048x512.png"
New-ButtonImage -FillColor $CharcoalHover -ContentOffsetY 0 -FileName "T_UIButton_Monochrome_Hovered_2048x512.png"
New-ButtonImage -FillColor $CharcoalPressed -ContentOffsetY 12 -FileName "T_UIButton_Monochrome_Pressed_2048x512.png"
New-LogoImage
New-ReadyCheckImage
New-CopyImage

