param(
    [string]$OutputDirectory = "D:\Dev\UE5.8\Project_MuseumHeist\ProjectResources\SourceArt\UI\Concepts\Monochrome"
)

Add-Type -AssemblyName System.Drawing

function Convert-ToMultipleOfFour {
    param([double]$Value)
    return [int]([Math]::Round($Value / 4.0) * 4)
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

function New-MonochromeBorderPng {
    param(
        [int]$Width,
        [int]$Height,
        [string]$FileName
    )

    $shortEdge = [Math]::Min($Width, $Height)
    $outerPadding = Convert-ToMultipleOfFour ($shortEdge * 0.055)
    $outerStroke = Convert-ToMultipleOfFour ($shortEdge * 0.024)
    $innerStroke = [Math]::Max(4, (Convert-ToMultipleOfFour ($shortEdge * 0.004)))
    $chamfer = Convert-ToMultipleOfFour ($shortEdge * 0.075)
    $borderGap = 12

    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    $outerInset = $outerPadding + [int]($outerStroke / 2)
    $outerPath = New-ChamferedPath $outerInset $outerInset ($Width - $outerInset) ($Height - $outerInset) $chamfer
    $outerPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 45, 43, 40), $outerStroke)
    $outerPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.DrawPath($outerPen, $outerPath)

    # 획의 외곽선 사이가 직선과 45도 사선에서 모두 약 12px가 되도록
    # 중심선 간격과 안쪽 Chamfer 길이를 함께 보정한다.
    $centerLineOffset = [int]($outerStroke / 2) + $borderGap + [int]($innerStroke / 2)
    $innerInset = $outerInset + $centerLineOffset
    $innerChamferOffset = (2.0 - [Math]::Sqrt(2.0)) * $centerLineOffset
    $innerChamfer = [Math]::Max(12, (Convert-ToMultipleOfFour ($chamfer - $innerChamferOffset)))
    $innerPath = New-ChamferedPath $innerInset $innerInset ($Width - $innerInset) ($Height - $innerInset) $innerChamfer
    $innerPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(230, 216, 208, 191), $innerStroke)
    $innerPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.DrawPath($innerPen, $innerPath)

    $outputPath = Join-Path $OutputDirectory $FileName
    $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

    $innerPen.Dispose()
    $innerPath.Dispose()
    $outerPen.Dispose()
    $outerPath.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

New-MonochromeBorderPng -Width 2048 -Height 2048 -FileName "T_UIBorder_Monochrome_1x1_2048.png"
New-MonochromeBorderPng -Width 2048 -Height 1024 -FileName "T_UIBorder_Monochrome_2x1_2048x1024.png"
New-MonochromeBorderPng -Width 1024 -Height 2048 -FileName "T_UIBorder_Monochrome_1x2_1024x2048.png"
New-MonochromeBorderPng -Width 2048 -Height 1536 -FileName "T_UIBorder_Monochrome_4x3_2048x1536.png"
New-MonochromeBorderPng -Width 1536 -Height 2048 -FileName "T_UIBorder_Monochrome_3x4_1536x2048.png"
