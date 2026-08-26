param(
    [ValidateSet("M01", "M02", "M03")]
    [string]$PoolId = "M01",

    [string]$SourceRoot = "",

    [string]$DataTablePath = ""
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if (-not ([System.Management.Automation.PSTypeName]"HeistSurfaceForgeryPackScanner").Type)
{
    Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.Runtime.InteropServices;

public sealed class HeistSurfaceForgeryImageScanResult
{
    public int Width;
    public int Height;
    public int ForegroundPixels;
    public int MaskMismatchPixels;
    public int InvalidMaskPixels;
    public string[] ForegroundColors;
}

public static class HeistSurfaceForgeryPackScanner
{
    public static HeistSurfaceForgeryImageScanResult Scan(string referencePath, string maskPath, string backgroundHex)
    {
        Color background = ParseHex(backgroundHex);

        using (Bitmap reference = new Bitmap(referencePath))
        using (Bitmap mask = new Bitmap(maskPath))
        {
            if (reference.Width != mask.Width || reference.Height != mask.Height)
            {
                throw new InvalidOperationException("Reference and mask dimensions differ.");
            }

            Rectangle bounds = new Rectangle(0, 0, reference.Width, reference.Height);
            BitmapData referenceData = reference.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            BitmapData maskData = mask.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);

            int referenceByteCount = Math.Abs(referenceData.Stride) * referenceData.Height;
            int maskByteCount = Math.Abs(maskData.Stride) * maskData.Height;
            byte[] referencePixels = new byte[referenceByteCount];
            byte[] maskPixels = new byte[maskByteCount];
            Marshal.Copy(referenceData.Scan0, referencePixels, 0, referencePixels.Length);
            Marshal.Copy(maskData.Scan0, maskPixels, 0, maskPixels.Length);
            reference.UnlockBits(referenceData);
            mask.UnlockBits(maskData);

            HashSet<int> foregroundColors = new HashSet<int>();
            int foregroundPixels = 0;
            int maskMismatchPixels = 0;
            int invalidMaskPixels = 0;

            for (int y = 0; y < reference.Height; ++y)
            {
                for (int x = 0; x < reference.Width; ++x)
                {
                    int referenceOffset = y * referenceData.Stride + x * 4;
                    byte referenceBlue = referencePixels[referenceOffset];
                    byte referenceGreen = referencePixels[referenceOffset + 1];
                    byte referenceRed = referencePixels[referenceOffset + 2];
                    bool referenceForeground =
                        referenceRed != background.R || referenceGreen != background.G || referenceBlue != background.B;

                    int maskOffset = y * maskData.Stride + x * 4;
                    byte maskBlue = maskPixels[maskOffset];
                    byte maskGreen = maskPixels[maskOffset + 1];
                    byte maskRed = maskPixels[maskOffset + 2];
                    bool maskValueValid =
                        maskRed == maskGreen && maskGreen == maskBlue && (maskRed == 0 || maskRed == 255);
                    bool maskForeground = maskValueValid && maskRed == 255;

                    if (!maskValueValid)
                    {
                        ++invalidMaskPixels;
                    }
                    if (referenceForeground != maskForeground)
                    {
                        ++maskMismatchPixels;
                    }
                    if (referenceForeground)
                    {
                        ++foregroundPixels;
                        foregroundColors.Add((referenceRed << 16) | (referenceGreen << 8) | referenceBlue);
                    }
                }
            }

            List<string> colorHex = new List<string>();
            foreach (int color in foregroundColors)
            {
                colorHex.Add(String.Format(
                    CultureInfo.InvariantCulture,
                    "#{0:X2}{1:X2}{2:X2}",
                    (color >> 16) & 0xFF,
                    (color >> 8) & 0xFF,
                    color & 0xFF));
            }
            colorHex.Sort(StringComparer.OrdinalIgnoreCase);

            return new HeistSurfaceForgeryImageScanResult
            {
                Width = reference.Width,
                Height = reference.Height,
                ForegroundPixels = foregroundPixels,
                MaskMismatchPixels = maskMismatchPixels,
                InvalidMaskPixels = invalidMaskPixels,
                ForegroundColors = colorHex.ToArray()
            };
        }
    }

    private static Color ParseHex(string value)
    {
        string hex = value.Trim().TrimStart('#');
        if (hex.Length != 6)
        {
            throw new ArgumentException("Color must use #RRGGBB format: " + value);
        }

        return Color.FromArgb(
            255,
            Int32.Parse(hex.Substring(0, 2), NumberStyles.HexNumber),
            Int32.Parse(hex.Substring(2, 2), NumberStyles.HexNumber),
            Int32.Parse(hex.Substring(4, 2), NumberStyles.HexNumber));
    }
}
'@
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$resolvedSourceRoot = if ([string]::IsNullOrWhiteSpace($SourceRoot))
{
    Join-Path $projectRoot "ProjectResources\SourceArt\Forgery\$PoolId"
}
else
{
    [System.IO.Path]::GetFullPath($SourceRoot)
}
$resolvedDataTablePath = if ([string]::IsNullOrWhiteSpace($DataTablePath))
{
    Join-Path $projectRoot "ProjectResources\DataTableImports\DT_ForgeryTemplateRow.json"
}
else
{
    [System.IO.Path]::GetFullPath($DataTablePath)
}
$artifactDataTablePath = Join-Path $projectRoot "ProjectResources\DataTableImports\DT_ArtifactDataRow.json"

$failures = [System.Collections.Generic.List[string]]::new()
$manifest = $null

if (-not (Test-Path -LiteralPath $resolvedSourceRoot -PathType Container))
{
    throw "Surface forgery source root does not exist: $resolvedSourceRoot"
}
if (-not (Test-Path -LiteralPath $resolvedDataTablePath -PathType Leaf))
{
    throw "Surface forgery DataTable import does not exist: $resolvedDataTablePath"
}
if (-not (Test-Path -LiteralPath $artifactDataTablePath -PathType Leaf))
{
    throw "Artifact DataTable import does not exist: $artifactDataTablePath"
}

$manifestPath = Join-Path $resolvedSourceRoot "$($PoolId)_SourceManifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf))
{
    $failures.Add("Missing source manifest: $manifestPath")
}
else
{
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.pool_id -ne $PoolId -or $manifest.templates.Count -ne 40)
    {
        $failures.Add("Source manifest must declare Pool=$PoolId and exactly 40 templates.")
    }
}
$expectedResolution =
    if ($null -ne $manifest -and $null -ne $manifest.art_direction.final_resolution)
    {
        [int]$manifest.art_direction.final_resolution
    }
    else
    {
        0
    }

$candidateDirectory = Join-Path $resolvedSourceRoot "Candidates"
if (-not (Test-Path -LiteralPath $candidateDirectory -PathType Container))
{
    $failures.Add("Missing candidate source directory: $candidateDirectory")
}
elseif ($expectedResolution -gt 0)
{
    foreach ($candidateFile in @(Get-ChildItem -LiteralPath $candidateDirectory -Filter "*.png" -File))
    {
        $candidateImage = $null
        try
        {
            $candidateImage = [System.Drawing.Image]::FromFile($candidateFile.FullName)
            if ($candidateImage.Width -ne $expectedResolution -or $candidateImage.Height -ne $expectedResolution)
            {
                $failures.Add(
                    "Candidate resolution contract failed for $($candidateFile.Name): Expected=$($expectedResolution)x$expectedResolution Actual=$($candidateImage.Width)x$($candidateImage.Height)")
            }
        }
        catch
        {
            $failures.Add("Candidate image could not be read: $($candidateFile.FullName) Error=$($_.Exception.Message)")
        }
        finally
        {
            if ($null -ne $candidateImage)
            {
                $candidateImage.Dispose()
            }
        }
    }
}

$allRows = Get-Content -LiteralPath $resolvedDataTablePath -Raw -Encoding UTF8 | ConvertFrom-Json
$poolRows = @($allRows | Where-Object { $_.SurfacePoolId -eq $PoolId })
$artifactRows = Get-Content -LiteralPath $artifactDataTablePath -Raw -Encoding UTF8 | ConvertFrom-Json
$assetFiles = @(Get-ChildItem -LiteralPath $resolvedSourceRoot -Filter "*.asset.json" -File)
$assetDefinitions = @($assetFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json })

if ($poolRows.Count -ne 40)
{
    $failures.Add("DataTable must contain exactly 40 rows for $PoolId; found $($poolRows.Count).")
}
if ($assetDefinitions.Count -ne 40)
{
    $failures.Add("Source pack must contain exactly 40 asset metadata files; found $($assetDefinitions.Count).")
}

$manifestByTemplateId = @{}
if ($null -ne $manifest)
{
    foreach ($manifestTemplate in @($manifest.templates))
    {
        if ([string]::IsNullOrWhiteSpace($manifestTemplate.template_id))
        {
            $failures.Add("Source manifest contains a template without template_id.")
        }
        elseif ($manifestByTemplateId.ContainsKey($manifestTemplate.template_id))
        {
            $failures.Add("Duplicate source manifest TemplateId: $($manifestTemplate.template_id)")
        }
        else
        {
            $manifestByTemplateId[$manifestTemplate.template_id] = $manifestTemplate
        }
    }
}

$rowByTemplateId = @{}
foreach ($row in $poolRows)
{
    if ($rowByTemplateId.ContainsKey($row.TemplateId))
    {
        $failures.Add("Duplicate DataTable TemplateId: $($row.TemplateId)")
    }
    else
    {
        $rowByTemplateId[$row.TemplateId] = $row
    }
}

$expectedDifficultySettings = @{
    "Easy" = @{ Palette = 4; Duration = 35.0; StrokeLimit = 4096; BrushSize = 0.025 }
    "Medium" = @{ Palette = 5; Duration = 40.0; StrokeLimit = 5120; BrushSize = 0.020 }
    "Hard" = @{ Palette = 6; Duration = 45.0; StrokeLimit = 6144; BrushSize = 0.018 }
}

$categoryContractByPool = @{
    "M01" = @{ First = "Portrait"; Second = "Landscape" }
    "M02" = @{ First = "InkWash"; Second = "FoldingScreen" }
    "M03" = @{ First = "GeometricAbstract"; Second = "Poster" }
}
$categoryContract = $categoryContractByPool[$PoolId]
$firstCategoryName = $categoryContract.First
$secondCategoryName = $categoryContract.Second
$firstCategoryCount = 0
$secondCategoryCount = 0
$easyCount = 0
$mediumCount = 0
$hardCount = 0
$validReferenceCount = 0
$validMaskCount = 0
$validPaletteCount = 0
$validDataRowCount = 0
$emptySubmitPrerequisiteDataContractCount = 0
$fullFillAntiFillDataContractCount = 0

foreach ($assetDefinition in $assetDefinitions)
{
    if ($assetDefinition.category -eq $firstCategoryName)
    {
        ++$firstCategoryCount
    }
    elseif ($assetDefinition.category -eq $secondCategoryName)
    {
        ++$secondCategoryCount
    }
    else
    {
        $failures.Add(
            "Invalid category for $($assetDefinition.template_id): $($assetDefinition.category). Expected=$firstCategoryName|$secondCategoryName")
    }
    switch ($assetDefinition.difficulty)
    {
        "Easy" { ++$easyCount }
        "Medium" { ++$mediumCount }
        "Hard" { ++$hardCount }
        default { $failures.Add("Invalid difficulty for $($assetDefinition.template_id): $($assetDefinition.difficulty)") }
    }

    $difficultySettings = $expectedDifficultySettings[$assetDefinition.difficulty]
    if ($null -eq $difficultySettings)
    {
        continue
    }

    $manifestTemplate = $manifestByTemplateId[$assetDefinition.template_id]
    if ($null -eq $manifestTemplate)
    {
        $failures.Add("Missing source manifest entry for $($assetDefinition.template_id).")
    }
    else
    {
        $manifestPalette = @($manifestTemplate.palette_srgb | ForEach-Object { $_.ToUpperInvariant() })
        $assetPalette = @($assetDefinition.palette_srgb | ForEach-Object { $_.ToUpperInvariant() })
        $manifestContract =
            $manifestTemplate.category -eq $assetDefinition.category -and
            $manifestTemplate.difficulty -eq $assetDefinition.difficulty -and
            $manifestTemplate.artwork -eq $assetDefinition.source_artwork -and
            $manifestTemplate.artist -eq $assetDefinition.source_artist -and
            -not [string]::IsNullOrWhiteSpace($manifestTemplate.source_url) -and
            $manifestPalette.Count -eq $assetPalette.Count -and
            @(Compare-Object -ReferenceObject $manifestPalette -DifferenceObject $assetPalette -SyncWindow 0).Count -eq 0
        if (-not $manifestContract)
        {
            $failures.Add("Manifest-to-asset contract failed for $($assetDefinition.template_id).")
        }
    }

    $referencePath = Join-Path $resolvedSourceRoot $assetDefinition.source_file
    $maskPath = Join-Path $resolvedSourceRoot $assetDefinition.mask_source_file
    if (-not (Test-Path -LiteralPath $referencePath -PathType Leaf))
    {
        $failures.Add("Missing Reference PNG for $($assetDefinition.template_id): $referencePath")
        continue
    }
    ++$validReferenceCount
    if (-not (Test-Path -LiteralPath $maskPath -PathType Leaf))
    {
        $failures.Add("Missing Mask PNG for $($assetDefinition.template_id): $maskPath")
        continue
    }
    ++$validMaskCount

    $scan = [HeistSurfaceForgeryPackScanner]::Scan($referencePath, $maskPath, $assetDefinition.background_srgb)
    if ($expectedResolution -gt 0 -and
        ($scan.Width -ne $expectedResolution -or
            $scan.Height -ne $expectedResolution -or
            [int]$assetDefinition.final_resolution -ne $expectedResolution))
    {
        $failures.Add(
            "Resolution contract failed for $($assetDefinition.template_id): Expected=$($expectedResolution)x$expectedResolution Reference=$($scan.Width)x$($scan.Height) Metadata=$($assetDefinition.final_resolution)")
    }
    $expectedPalette = @($assetDefinition.palette_srgb | ForEach-Object { $_.ToUpperInvariant() } | Sort-Object -Unique)
    $actualPalette = @($scan.ForegroundColors | ForEach-Object { $_.ToUpperInvariant() } | Sort-Object -Unique)
    $paletteMatches =
        $expectedPalette.Count -eq $difficultySettings.Palette -and
        $actualPalette.Count -eq $expectedPalette.Count -and
        (@(Compare-Object -ReferenceObject $expectedPalette -DifferenceObject $actualPalette).Count -eq 0)
    if (-not $paletteMatches)
    {
        $failures.Add(
            "Palette mismatch for $($assetDefinition.template_id): Expected=$($expectedPalette -join ',') Actual=$($actualPalette -join ',')")
    }
    elseif ($scan.MaskMismatchPixels -eq 0 -and $scan.InvalidMaskPixels -eq 0)
    {
        ++$validPaletteCount
    }

    if ($scan.MaskMismatchPixels -ne 0 -or $scan.InvalidMaskPixels -ne 0)
    {
        $failures.Add(
            "Mask mismatch for $($assetDefinition.template_id): MismatchedPixels=$($scan.MaskMismatchPixels) InvalidMaskPixels=$($scan.InvalidMaskPixels)")
    }

    $calculatedForegroundRatio = $scan.ForegroundPixels / [double]($scan.Width * $scan.Height)
    if ([Math]::Abs($calculatedForegroundRatio - [double]$assetDefinition.foreground_ratio) -gt 0.0002)
    {
        $failures.Add(
            "Foreground ratio mismatch for $($assetDefinition.template_id): Metadata=$($assetDefinition.foreground_ratio) Actual=$($calculatedForegroundRatio.ToString('F4'))")
    }

    $row = $rowByTemplateId[$assetDefinition.template_id]
    if ($null -eq $row)
    {
        $failures.Add("Missing DataTable row for $($assetDefinition.template_id).")
        continue
    }

    $expectedUnrealDestination = "/Game/Data/Forgery/Textures/$PoolId"
    $expectedAssetPrefix = "T_Forgery_$($PoolId)_$($assetDefinition.category)_"
    $expectedReferenceAssetPath =
        "Texture2D'$expectedUnrealDestination/$($assetDefinition.asset_name).$($assetDefinition.asset_name)'"
    $expectedMaskAssetPath =
        "Texture2D'$expectedUnrealDestination/$($assetDefinition.mask_asset_name).$($assetDefinition.mask_asset_name)'"
    $referenceAssetContract =
        $row.Name -eq $assetDefinition.template_id -and
        $assetDefinition.unreal_destination -eq $expectedUnrealDestination -and
        $assetDefinition.asset_name.StartsWith($expectedAssetPrefix, [System.StringComparison]::Ordinal) -and
        $row.ReferenceImage -eq $expectedReferenceAssetPath
    $maskAssetContract =
        $assetDefinition.mask_asset_name -eq "$($assetDefinition.asset_name)_Mask" -and
        $row.ReferenceMask -eq $expectedMaskAssetPath
    $expectedAllowedPalette = @(
        $assetDefinition.palette_linear | ForEach-Object {
            [string]::Format(
                [System.Globalization.CultureInfo]::InvariantCulture,
                "(R={0:F6},G={1:F6},B={2:F6},A={3:F6})",
                [double]$_[0],
                [double]$_[1],
                [double]$_[2],
                [double]$_[3])
        })
    $actualAllowedPalette = @($row.AllowedPalette)
    $linearPaletteContract =
        $expectedAllowedPalette.Count -eq $actualAllowedPalette.Count -and
        @(Compare-Object -ReferenceObject $expectedAllowedPalette -DifferenceObject $actualAllowedPalette -SyncWindow 0).Count -eq 0
    $difficultyContract =
        @($row.AllowedPalette).Count -eq $difficultySettings.Palette -and
        [Math]::Abs([double]$row.ForgeryDuration - [double]$difficultySettings.Duration) -lt 0.0001 -and
        [int]$row.StrokeLimit -eq [int]$difficultySettings.StrokeLimit -and
        [Math]::Abs([double]$row.BrushSize - [double]$difficultySettings.BrushSize) -lt 0.0001
    $commonScoringContract =
        [Math]::Abs([double]$row.ObservationDuration - 1.0) -lt 0.0001 -and
        [Math]::Abs([double]$row.BackgroundColorTolerance - 0.08) -lt 0.0001 -and
        [Math]::Abs([double]$row.CoverageWeight - 0.45) -lt 0.0001 -and
        [Math]::Abs([double]$row.MajorShapeWeight - 0.55) -lt 0.0001 -and
        [Math]::Abs([double]$row.ExtraStrokePenaltyWeight - 0.15) -lt 0.0001 -and
        [Math]::Abs([double]$row.ShapeAccuracyWeight - 0.65) -lt 0.0001 -and
        [Math]::Abs([double]$row.ColorAccuracyWeight - 0.35) -lt 0.0001 -and
        [Math]::Abs([double]$row.MaximumPaintToReferenceRatio - 2.5) -lt 0.0001 -and
        [Math]::Abs([double]$row.OverpaintScoreCap - 20.0) -lt 0.0001
    $emptySubmitDataContract =
        $scan.ForegroundPixels -gt 0 -and
        ([double]$row.CoverageWeight + [double]$row.MajorShapeWeight) -gt 0.0 -and
        ([double]$row.ShapeAccuracyWeight + [double]$row.ColorAccuracyWeight) -gt 0.0
    if ($emptySubmitDataContract)
    {
        ++$emptySubmitPrerequisiteDataContractCount
    }

    $fullFillRatio =
        if ($scan.ForegroundPixels -gt 0)
        {
            ($scan.Width * $scan.Height) / [double]$scan.ForegroundPixels
        }
        else
        {
            0.0
        }
    $fullFillAntiFillDataContract =
        $emptySubmitDataContract -and
        [double]$row.MaximumPaintToReferenceRatio -gt 0.0 -and
        $fullFillRatio -gt [double]$row.MaximumPaintToReferenceRatio -and
        [double]$row.OverpaintScoreCap -ge 0.0 -and
        [double]$row.OverpaintScoreCap -le 20.0
    if ($fullFillAntiFillDataContract)
    {
        ++$fullFillAntiFillDataContractCount
    }
    if ($PoolId -eq "M03" -and -not $emptySubmitDataContract)
    {
        $failures.Add("M03 empty-submit prerequisite data contract failed for $($assetDefinition.template_id).")
    }
    if ($PoolId -eq "M03" -and -not $fullFillAntiFillDataContract)
    {
        $failures.Add(
            "M03 full-fill Anti-Fill data contract failed for $($assetDefinition.template_id): FullFillRatio=$($fullFillRatio.ToString('F3')) Threshold=$($row.MaximumPaintToReferenceRatio) Cap=$($row.OverpaintScoreCap)")
    }
    $rowContract =
        $row.TemplateId -eq $assetDefinition.template_id -and
        $row.SurfacePoolId -eq $PoolId -and
        $row.BackgroundFilterMode -eq "Black" -and
        $referenceAssetContract -and
        $maskAssetContract -and
        $linearPaletteContract -and
        $difficultyContract -and
        $commonScoringContract

    if ($rowContract)
    {
        ++$validDataRowCount
    }
    else
    {
        $failures.Add("DataTable row contract failed for $($assetDefinition.template_id).")
    }
}

if ($firstCategoryCount -ne 20 -or $secondCategoryCount -ne 20)
{
    $failures.Add(
        "Category distribution must be $firstCategoryName=20 $secondCategoryName=20; found $firstCategoryName=$firstCategoryCount $secondCategoryName=$secondCategoryCount.")
}
if ($easyCount -ne 14 -or $mediumCount -ne 17 -or $hardCount -ne 9)
{
    $failures.Add("Difficulty distribution must be Easy=14 Medium=17 Hard=9; found Easy=$easyCount Medium=$mediumCount Hard=$hardCount.")
}

$paintingArtifact = $artifactRows | Where-Object { $_.ArtifactId -eq "Artifact_Painting_M01" } | Select-Object -First 1
$artifactDefaultValid =
    $PoolId -ne "M01" -or
    ($null -ne $paintingArtifact -and
        $paintingArtifact.ForgeryType -eq "Drawing" -and
        $rowByTemplateId.ContainsKey($paintingArtifact.ForgeryTemplateId))
if (-not $artifactDefaultValid)
{
    $failures.Add("Artifact_Painting_M01 must reference one of the M01 Surface Template rows.")
}

foreach ($failure in $failures)
{
    Write-Warning $failure
}

$passed = $failures.Count -eq 0
$resultText = if ($passed) { "PASS" } else { "FAIL" }
$emptySubmitPrerequisiteResult = if ($PoolId -eq "M03")
{
    if ($emptySubmitPrerequisiteDataContractCount -eq 40) { "PASS" } else { "FAIL" }
}
else
{
    "NOT_REQUIRED"
}
$fullFillAntiFillResult = if ($PoolId -eq "M03")
{
    if ($fullFillAntiFillDataContractCount -eq 40) { "PASS" } else { "FAIL" }
}
else
{
    "NOT_REQUIRED"
}
$artifactDefaultResult = if ($PoolId -eq "M01")
{
    if ($artifactDefaultValid) { "PASS" } else { "FAIL" }
}
else
{
    "NOT_REQUIRED"
}
Write-Output (
    "Surface forgery source pack validation: Pool=$PoolId Templates=$($assetDefinitions.Count) $firstCategoryName=$firstCategoryCount $secondCategoryName=$secondCategoryCount " +
    "Easy=$easyCount Medium=$mediumCount Hard=$hardCount References=$validReferenceCount Masks=$validMaskCount Palettes=$validPaletteCount " +
    "DataRows=$validDataRowCount EmptySubmitPrerequisiteData=$emptySubmitPrerequisiteResult FullFillAntiFillData=$fullFillAntiFillResult " +
    "ArtifactDefault=$artifactDefaultResult Invalid=$($failures.Count) Result=$resultText")

if (-not $passed)
{
    exit 1
}
