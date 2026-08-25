param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [string]$Pattern = "*.png",

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [ValidateRange(1, 8)]
    [int]$Columns = 4,

    [ValidateRange(128, 512)]
    [int]$ThumbnailSize = 256
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$resolvedSourceDirectory = (Resolve-Path -LiteralPath $SourceDirectory).Path
$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$sourceFiles = @(
    Get-ChildItem -LiteralPath $resolvedSourceDirectory -Filter $Pattern -File |
        Where-Object {
            $_.Name -notlike "*_Mask.png" -and
            $_.Name -notlike "*_ContactSheet.png"
        } |
        Sort-Object Name)
if ($sourceFiles.Count -eq 0)
{
    throw "No images matched '$Pattern' in $resolvedSourceDirectory."
}

$labelHeight = 34
$cellPadding = 10
$cellWidth = $ThumbnailSize + ($cellPadding * 2)
$cellHeight = $ThumbnailSize + $labelHeight + ($cellPadding * 2)
$rows = [Math]::Ceiling($sourceFiles.Count / [double]$Columns)
$sheetWidth = $cellWidth * $Columns
$sheetHeight = $cellHeight * $rows

$sheet = [System.Drawing.Bitmap]::new($sheetWidth, $sheetHeight)
$graphics = [System.Drawing.Graphics]::FromImage($sheet)
$graphics.Clear([System.Drawing.Color]::FromArgb(24, 28, 34))
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$labelFont = [System.Drawing.Font]::new("Segoe UI", 9.0, [System.Drawing.FontStyle]::Regular)
$labelBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$labelFormat = [System.Drawing.StringFormat]::new()
$labelFormat.Alignment = [System.Drawing.StringAlignment]::Center
$labelFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
$labelFormat.Trimming = [System.Drawing.StringTrimming]::EllipsisCharacter

try
{
    for ($index = 0; $index -lt $sourceFiles.Count; ++$index)
    {
        $column = $index % $Columns
        $row = [Math]::Floor($index / $Columns)
        $cellX = $column * $cellWidth
        $cellY = $row * $cellHeight
        $imageRectangle = [System.Drawing.Rectangle]::new(
            $cellX + $cellPadding,
            $cellY + $cellPadding,
            $ThumbnailSize,
            $ThumbnailSize)
        $labelRectangle = [System.Drawing.RectangleF]::new(
            $cellX + $cellPadding,
            $cellY + $cellPadding + $ThumbnailSize,
            $ThumbnailSize,
            $labelHeight)

        $sourceImage = [System.Drawing.Image]::FromFile($sourceFiles[$index].FullName)
        try
        {
            $graphics.DrawImage($sourceImage, $imageRectangle)
        }
        finally
        {
            $sourceImage.Dispose()
        }

        $graphics.DrawString(
            [System.IO.Path]::GetFileNameWithoutExtension($sourceFiles[$index].Name),
            $labelFont,
            $labelBrush,
            $labelRectangle,
            $labelFormat)
    }

    $outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutputPath)
    [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    $sheet.Save($resolvedOutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally
{
    $labelFormat.Dispose()
    $labelBrush.Dispose()
    $labelFont.Dispose()
    $graphics.Dispose()
    $sheet.Dispose()
}

Write-Output "Surface forgery contact sheet: Images=$($sourceFiles.Count) Columns=$Columns Output=$resolvedOutputPath Result=PASS"
