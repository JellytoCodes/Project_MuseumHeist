param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("M01", "M02", "M03")]
    [string]$PoolId,

    [ValidateRange(256, 2048)]
    [int]$OutputSize = 1024
)

$ErrorActionPreference = "Stop"

if ($OutputSize -ne 1024)
{
    throw "Surface Forgery reference assets must be built at 1024x1024. Requested=$($OutputSize)x$OutputSize"
}

function Convert-SrgbChannelToLinear
{
    param([int]$Channel)

    $normalized = $Channel / 255.0
    if ($normalized -le 0.04045)
    {
        return $normalized / 12.92
    }

    return [Math]::Pow(($normalized + 0.055) / 1.055, 2.4)
}

function Convert-HexToLinearRgba
{
    param([string]$Hex)

    $normalizedHex = $Hex.Trim().TrimStart("#")
    if ($normalizedHex.Length -ne 6)
    {
        throw "Palette color must use #RRGGBB format: $Hex"
    }

    $red = [Convert]::ToInt32($normalizedHex.Substring(0, 2), 16)
    $green = [Convert]::ToInt32($normalizedHex.Substring(2, 2), 16)
    $blue = [Convert]::ToInt32($normalizedHex.Substring(4, 2), 16)
    return ,([double[]]@(
        [Math]::Round((Convert-SrgbChannelToLinear $red), 6),
        [Math]::Round((Convert-SrgbChannelToLinear $green), 6),
        [Math]::Round((Convert-SrgbChannelToLinear $blue), 6),
        1.0
    ))
}

function Format-LinearColor
{
    param([double[]]$Color)

    return [string]::Format(
        [System.Globalization.CultureInfo]::InvariantCulture,
        "(R={0:F6},G={1:F6},B={2:F6},A={3:F6})",
        $Color[0],
        $Color[1],
        $Color[2],
        $Color[3])
}

function Write-Utf8Json
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [object]$Value
    )

    $json = $Value | ConvertTo-Json -Depth 20
    $utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, $utf8WithoutBom)
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$sourceRoot = Join-Path $projectRoot "ProjectResources\SourceArt\Forgery\$PoolId"
$manifestPath = Join-Path $sourceRoot "$($PoolId)_SourceManifest.json"
$dataTablePath = Join-Path $projectRoot "ProjectResources\DataTableImports\DT_ForgeryTemplateRow.json"
$quantizeScript = Join-Path $PSScriptRoot "QuantizeForgeryReference.ps1"

foreach ($requiredPath in @($manifestPath, $dataTablePath, $quantizeScript))
{
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf))
    {
        throw "Required Surface Forgery pack file is missing: $requiredPath"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.pool_id -ne $PoolId -or @($manifest.templates).Count -ne 40)
{
    throw "Manifest must declare Pool=$PoolId and exactly 40 templates."
}

foreach ($template in @($manifest.templates))
{
    $candidateSourceFile = [string]$template.candidate_source_file
    if ([string]::IsNullOrWhiteSpace($candidateSourceFile))
    {
        $existingAssetMetadataPath = Join-Path $sourceRoot "$($template.slot).asset.json"
        if (Test-Path -LiteralPath $existingAssetMetadataPath -PathType Leaf)
        {
            $existingAssetMetadata = Get-Content -LiteralPath $existingAssetMetadataPath -Raw -Encoding UTF8 | ConvertFrom-Json
            $candidateSourceFile = [string]$existingAssetMetadata.candidate_source_file
        }
    }
    if ([string]::IsNullOrWhiteSpace($candidateSourceFile))
    {
        throw "Candidate source_file is not declared for $($template.template_id)."
    }
    $template | Add-Member -NotePropertyName resolved_candidate_source_file -NotePropertyValue $candidateSourceFile -Force
    $candidatePath = Join-Path $sourceRoot $candidateSourceFile
    if (-not (Test-Path -LiteralPath $candidatePath -PathType Leaf))
    {
        throw "Candidate image is missing for $($template.template_id): $candidatePath"
    }
}

$difficultySettings = @{
    "Easy" = [ordered]@{ Palette = 4; Duration = 35.0; StrokeLimit = 4096; BrushSize = 0.025; CleanupRadius = 4 }
    "Medium" = [ordered]@{ Palette = 5; Duration = 40.0; StrokeLimit = 5120; BrushSize = 0.020; CleanupRadius = 3 }
    "Hard" = [ordered]@{ Palette = 6; Duration = 45.0; StrokeLimit = 6144; BrushSize = 0.018; CleanupRadius = 2 }
}

$newRows = [System.Collections.Generic.List[object]]::new()
$assetDefinitions = [System.Collections.Generic.List[object]]::new()

foreach ($template in @($manifest.templates))
{
    $settings = $difficultySettings[$template.difficulty]
    if ($null -eq $settings)
    {
        throw "Unsupported difficulty for $($template.template_id): $($template.difficulty)"
    }
    if (@($template.palette_srgb).Count -ne $settings.Palette)
    {
        throw "Palette size mismatch for $($template.template_id): Expected=$($settings.Palette) Actual=$(@($template.palette_srgb).Count)"
    }

    $stem = $template.slot
    $assetName = "T_Forgery_$stem"
    $maskAssetName = "$assetName`_Mask"
    $sourceFile = "$stem.png"
    $maskSourceFile = "$stem`_Mask.png"
    $candidateSourceFile = [string]$template.resolved_candidate_source_file
    $candidatePath = Join-Path $sourceRoot $candidateSourceFile
    $outputPath = Join-Path $sourceRoot $sourceFile
    $maskOutputPath = Join-Path $sourceRoot $maskSourceFile

    $slotIndexMatch = [regex]::Match([string]$template.slot, "_(?<index>[0-9]+)$")
    $slotIndex = if ($slotIndexMatch.Success) { [int]$slotIndexMatch.Groups["index"].Value } else { 0 }
    $backgroundThreshold = if ($slotIndex -ge 7) { 4 } else { 24 }

    $quantizeOutput = & $quantizeScript `
        -InputPath $candidatePath `
        -OutputPath $outputPath `
        -MaskOutputPath $maskOutputPath `
        -BackgroundHex "#000000" `
        -PaletteHex @($template.palette_srgb) `
        -BackgroundThreshold $backgroundThreshold `
        -CleanupRadius $settings.CleanupRadius `
        -OutputSize $OutputSize
    $quantizeSummary = $quantizeOutput -join [Environment]::NewLine
    $foregroundRatioMatch = [regex]::Match($quantizeSummary, "ForegroundRatio=(?<ratio>[0-9]+\.[0-9]+)")
    if (-not $foregroundRatioMatch.Success)
    {
        throw "Could not read foreground ratio for $($template.template_id): $quantizeSummary"
    }

    $foregroundRatio = [double]::Parse(
        $foregroundRatioMatch.Groups["ratio"].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if ($foregroundRatio -le 0.0 -or $foregroundRatio -ge 0.9)
    {
        throw "Foreground ratio is outside the general content contract for $($template.template_id): $foregroundRatio"
    }
    if ($PoolId -eq "M03" -and $foregroundRatio -ge 0.4)
    {
        throw "M03 foreground ratio must remain below 0.4 for full-fill Anti-Fill: Template=$($template.template_id) Ratio=$foregroundRatio"
    }

    $paletteLinear = @()
    foreach ($paletteColor in @($template.palette_srgb))
    {
        $paletteLinear += ,(Convert-HexToLinearRgba $paletteColor)
    }
    $allowedPalette = @($paletteLinear | ForEach-Object { Format-LinearColor $_ })
    $unrealDestination = "/Game/Data/Forgery/Textures/$PoolId"

    $assetDefinition = [ordered]@{
        asset_name = $assetName
        source_file = $sourceFile
        mask_asset_name = $maskAssetName
        mask_source_file = $maskSourceFile
        unreal_destination = $unrealDestination
        template_id = $template.template_id
        category = $template.category
        difficulty = $template.difficulty
        source_artwork = $template.artwork
        source_artist = $template.artist
        source_manifest = "$($PoolId)_SourceManifest.json"
        candidate_source_file = $candidateSourceFile
        background_threshold = $backgroundThreshold
        background_srgb = "#000000"
        background_filter_mode = "Black"
        background_color_tolerance = 0.08
        palette_srgb = @($template.palette_srgb)
        palette_linear = $paletteLinear
        foreground_ratio = [Math]::Round($foregroundRatio, 4)
        quantize_cleanup_radius = $settings.CleanupRadius
        final_resolution = $OutputSize
        notes = "ReferenceImage is the owner UI preview. Result imagery is generated from the submitted palette raster; no static result texture is required."
    }
    $assetDefinitions.Add($assetDefinition)
    Write-Utf8Json -Path (Join-Path $sourceRoot "$stem.asset.json") -Value $assetDefinition

    $newRows.Add([ordered]@{
        Name = $template.template_id
        TemplateId = $template.template_id
        SurfacePoolId = $PoolId
        ReferenceImage = "Texture2D'$unrealDestination/$assetName.$assetName'"
        ReferenceMask = "Texture2D'$unrealDestination/$maskAssetName.$maskAssetName'"
        BackgroundFilterMode = "Black"
        BackgroundColorTolerance = 0.08
        AllowedPalette = $allowedPalette
        ObservationDuration = 1.0
        ForgeryDuration = $settings.Duration
        StrokeLimit = $settings.StrokeLimit
        BrushSize = $settings.BrushSize
        CoverageWeight = 0.45
        MajorShapeWeight = 0.55
        ExtraStrokePenaltyWeight = 0.15
        ShapeAccuracyWeight = 0.65
        ColorAccuracyWeight = 0.35
        MaximumPaintToReferenceRatio = 2.5
        OverpaintScoreCap = 20.0
    })
}

$parsedRows = Get-Content -LiteralPath $dataTablePath -Raw -Encoding UTF8 | ConvertFrom-Json
$normalizedRows = [System.Collections.Generic.List[object]]::new()
foreach ($parsedRow in @($parsedRows))
{
    $propertyNames = @($parsedRow.PSObject.Properties.Name)
    if ($propertyNames -contains "value" -and $null -ne $parsedRow.value)
    {
        foreach ($wrappedRow in @($parsedRow.value))
        {
            $normalizedRows.Add($wrappedRow)
        }
    }
    else
    {
        $normalizedRows.Add($parsedRow)
    }
}

$updatedRows = [System.Collections.Generic.List[object]]::new()
foreach ($existingRow in $normalizedRows)
{
    if ($existingRow.SurfacePoolId -ne $PoolId)
    {
        $updatedRows.Add($existingRow)
    }
}
foreach ($newRow in $newRows)
{
    $updatedRows.Add($newRow)
}
Write-Utf8Json -Path $dataTablePath -Value $updatedRows.ToArray()

Write-Output (
    "Surface forgery pack build: Pool=$PoolId Templates=$($assetDefinitions.Count) Resolution=$($OutputSize)x$($OutputSize) " +
    "DataRows=$($newRows.Count) DataTableTotal=$($updatedRows.Count) Result=PASS")
