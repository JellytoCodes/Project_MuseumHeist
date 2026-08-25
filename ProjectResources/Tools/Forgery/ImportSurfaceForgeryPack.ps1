param(
    [string]$UnrealEditorCmd = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$projectFile = Join-Path $projectRoot "Project_MuseumHeist.uproject"
$dataTableSource = Join-Path $projectRoot "ProjectResources\DataTableImports\DT_ForgeryTemplateRow.json"
$savedDirectory = Join-Path $projectRoot "Saved\Codex"
$temporaryPythonPath = Join-Path $savedDirectory "ImportSurfaceForgeryPack.py"
$importLogPath = Join-Path $savedDirectory "SurfaceForgeryImport.log"
$derivedDataCacheDirectory = Join-Path $savedDirectory "DerivedDataCache"

foreach ($requiredPath in @($UnrealEditorCmd, $projectFile, $dataTableSource))
{
    if (-not (Test-Path -LiteralPath $requiredPath))
    {
        throw "Required path is missing: $requiredPath"
    }
}

foreach ($poolId in @("M01", "M02", "M03"))
{
    $sourceDirectory = Join-Path $projectRoot "ProjectResources\SourceArt\Forgery\$poolId"
    $referenceCount = @(
        Get-ChildItem -LiteralPath $sourceDirectory -File -Filter "$($poolId)_*.png" |
            Where-Object {
                $_.Name -notlike "*_Mask.png" -and
                $_.Name -notlike "*_ContactSheet.png"
            }).Count
    $maskCount = @(Get-ChildItem -LiteralPath $sourceDirectory -File -Filter "$($poolId)_*_Mask.png").Count
    if ($referenceCount -ne 40 -or $maskCount -ne 40)
    {
        throw "Unexpected source count for $poolId. References=$referenceCount Masks=$maskCount"
    }
}

$pythonSource = @'
import glob
import os
import unreal

project_root = os.environ["HEIST_SURFACE_FORGERY_ROOT"]
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
texture_tasks = []
texture_records = []

for pool_id in ("M01", "M02", "M03"):
    source_directory = os.path.join(project_root, "ProjectResources", "SourceArt", "Forgery", pool_id)
    destination_path = "/Game/Data/Forgery/Textures/" + pool_id
    if unreal.EditorAssetLibrary.does_directory_exist(destination_path):
        unreal.EditorAssetLibrary.delete_directory(destination_path)
    if not unreal.EditorAssetLibrary.does_directory_exist(destination_path):
        if not unreal.EditorAssetLibrary.make_directory(destination_path):
            raise RuntimeError("Failed to create generated texture directory: " + destination_path)

    source_files = sorted(glob.glob(os.path.join(source_directory, pool_id + "_*.png")))
    source_files = [path for path in source_files if not path.endswith("_ContactSheet.png")]
    if len(source_files) != 80:
        raise RuntimeError("Unexpected PNG count for {0}: {1}".format(pool_id, len(source_files)))

    for source_file in source_files:
        source_stem = os.path.splitext(os.path.basename(source_file))[0]
        destination_name = "T_Forgery_" + source_stem
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_file)
        task.set_editor_property("destination_path", destination_path)
        task.set_editor_property("destination_name", destination_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        texture_tasks.append(task)
        texture_records.append((task, source_stem.endswith("_Mask")))

asset_tools.import_asset_tasks(texture_tasks)

imported_texture_count = 0
for task, is_mask in texture_records:
    imported_paths = task.get_editor_property("imported_object_paths")
    if len(imported_paths) != 1:
        raise RuntimeError("Texture import did not return one asset for: " + task.get_editor_property("filename"))

    texture = unreal.EditorAssetLibrary.load_asset(imported_paths[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Imported asset is not Texture2D: " + imported_paths[0])

    texture_width = texture.blueprint_get_size_x()
    texture_height = texture.blueprint_get_size_y()
    if texture_width != 1024 or texture_height != 1024:
        raise RuntimeError(
            "Texture resolution contract failed for {0}: Expected=1024x1024 Actual={1}x{2}".format(
                imported_paths[0], texture_width, texture_height))

    texture.set_editor_property(
        "compression_settings",
        unreal.TextureCompressionSettings.TC_MASKS if is_mask else unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", not is_mask)
    texture.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
        raise RuntimeError("Failed to save texture: " + imported_paths[0])
    imported_texture_count += 1

data_table_path = "/Game/Data/DataTable/DT_ForgeryTemplate"
data_table = unreal.EditorAssetLibrary.load_asset(data_table_path)
if not isinstance(data_table, unreal.DataTable):
    raise RuntimeError("DT_ForgeryTemplate is missing or invalid")

row_struct = unreal.load_object(None, "/Script/Project_MuseumHeist.HeistForgeryTemplateRow")
data_table_source = os.path.join(
    project_root, "ProjectResources", "DataTableImports", "DT_ForgeryTemplateRow.json")
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, data_table_source, row_struct):
    raise RuntimeError("Failed to fill DT_ForgeryTemplate from JSON")
data_table.modify()
if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, False):
    raise RuntimeError("Failed to save DT_ForgeryTemplate")

row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(data_table)
if len(row_names) != 120:
    raise RuntimeError("Unexpected DataTable row count: {0}".format(len(row_names)))

for pool_id in ("M01", "M02", "M03"):
    destination_path = "/Game/Data/Forgery/Textures/" + pool_id
    assets = unreal.EditorAssetLibrary.list_assets(destination_path, False, False)
    if len(assets) != 80 or any("/T_Forgery_" not in path for path in assets):
        raise RuntimeError("Unexpected Unreal texture set for {0}: {1}".format(pool_id, len(assets)))

unreal.log("SurfaceForgeryImport Result=PASS Resolution=1024x1024 Textures={0} DataRows={1}".format(
    imported_texture_count, len(row_names)))
'@

[System.IO.Directory]::CreateDirectory($savedDirectory) | Out-Null
[System.IO.Directory]::CreateDirectory($derivedDataCacheDirectory) | Out-Null
$utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($temporaryPythonPath, $pythonSource + [Environment]::NewLine, $utf8WithoutBom)

$previousProjectRoot = $env:HEIST_SURFACE_FORGERY_ROOT
$env:HEIST_SURFACE_FORGERY_ROOT = $projectRoot
try
{
    & $UnrealEditorCmd $projectFile `
        -run=PythonScript `
        "-script=$temporaryPythonPath" `
        -unattended `
        -nop4 `
        -NoSplash `
        -NoAudio `
        -NullRHI `
        -DDC=InstalledNoZenLocalFallback `
        "-LocalDataCachePath=$derivedDataCacheDirectory" `
        "-abslog=$importLogPath"
    if ($LASTEXITCODE -ne 0)
    {
        throw "Unreal asset import failed with exit code $LASTEXITCODE. Log=$importLogPath"
    }
}
finally
{
    $env:HEIST_SURFACE_FORGERY_ROOT = $previousProjectRoot
    if (Test-Path -LiteralPath $temporaryPythonPath)
    {
        Remove-Item -LiteralPath $temporaryPythonPath -Force
    }
}

Write-Output "Surface forgery Unreal import: References=120 Masks=120 DataRows=120 Log=$importLogPath Result=PASS"
