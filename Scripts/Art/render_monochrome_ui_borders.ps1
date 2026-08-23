param(
    [string]$OutputDirectory = "D:\Dev\UE5.8\Project_MuseumHeist\SourceArt\UI\Concepts\Monochrome"
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
    $innerGap = Convert-ToMultipleOfFour ($shortEdge * 0.020)
    $innerStroke = [Math]::Max(4, (Convert-ToMultipleOfFour ($shortEdge * 0.004)))
    $chamfer = Convert-ToMultipleOfFour ($shortEdge * 0.075)

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

    $innerInset = $outerPadding + $outerStroke + $innerGap
    $innerChamfer = [Math]::Max(12, $chamfer - $outerStroke - $innerGap)
    $innerPath = New-ChamferedPath $innerInset $innerInset ($Width - $innerInset) ($Height - $innerInset) $innerChamfer
    $innerPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(230, 216, 208, 191), $innerStroke)
    $innerPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $graphics.DrawPath($innerPen, $innerPath)

    # 네 모서리에만 짧은 보강선을 두어 비율이 달라도 동일한 패밀리로 보이게 한다.
    $accentLength = Convert-ToMultipleOfFour ($shortEdge * 0.105)
    $accentOffset = $outerPadding + $outerStroke + [int]($innerGap / 2)
    $accentPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(210, 96, 91, 84), [Math]::Max(4, $innerStroke))
    $accentPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Square
    $accentPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Square

    $graphics.DrawLine($accentPen, $accentOffset + $chamfer, $accentOffset, $accentOffset + $chamfer + $accentLength, $accentOffset)
    $graphics.DrawLine($accentPen, $Width - $accentOffset - $chamfer - $accentLength, $accentOffset, $Width - $accentOffset - $chamfer, $accentOffset)
    $graphics.DrawLine($accentPen, $accentOffset + $chamfer, $Height - $accentOffset, $accentOffset + $chamfer + $accentLength, $Height - $accentOffset)
    $graphics.DrawLine($accentPen, $Width - $accentOffset - $chamfer - $accentLength, $Height - $accentOffset, $Width - $accentOffset - $chamfer, $Height - $accentOffset)

    $outputPath = Join-Path $OutputDirectory $FileName
    $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

    $accentPen.Dispose()
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

