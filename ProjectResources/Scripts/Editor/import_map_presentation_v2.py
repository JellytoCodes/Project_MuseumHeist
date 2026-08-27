import json
import os

import unreal


DATA_TABLE_PATH = "/Game/Data/DataTable/DT_MapPresentation"
SOURCE_PATH = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "ProjectResources", "DataTableImports", "DT_MapPresentation.json")
)
EXPECTED_BOUNDS = {
    "M01": ((-7200.0, -5200.0), (7200.0, 5200.0)),
    "M02": ((-6400.0, -5600.0), (6400.0, 5600.0)),
    "M03": ((-8000.0, -4400.0), (8000.0, 4400.0)),
}
EXPECTED_EXITS = {
    "M01": (-6800.0, -1200.0),
    "M02": (2400.0, 5200.0),
    "M03": (-7600.0, 0.0),
}
FLOOR_PLAN_SOURCE_DIRECTORY = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "ProjectResources", "SourceArt", "W7", "Generated")
)
FLOOR_PLAN_DESTINATION = "/Game/Assets/UI/Map"


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
texture_tasks = []
for row_name in sorted(EXPECTED_BOUNDS):
    source_path = os.path.join(FLOOR_PLAN_SOURCE_DIRECTORY, "T_FloorPlan_{}.png".format(row_name))
    if not os.path.isfile(source_path):
        raise RuntimeError("Floor plan source texture is missing: " + source_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", FLOOR_PLAN_DESTINATION)
    task.set_editor_property("destination_name", "T_FloorPlan_{}".format(row_name))
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    texture_tasks.append((row_name, task))

asset_tools.import_asset_tasks([task for _, task in texture_tasks])
imported_texture_paths = []
for row_name, task in texture_tasks:
    imported_paths = task.get_editor_property("imported_object_paths")
    if len(imported_paths) != 1:
        raise RuntimeError("Floor plan import failed for {}: {}".format(row_name, list(imported_paths)))
    texture = unreal.EditorAssetLibrary.load_asset(imported_paths[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Imported floor plan is not Texture2D: " + imported_paths[0])
    if texture.blueprint_get_size_x() != 1024 or texture.blueprint_get_size_y() != 640:
        raise RuntimeError(
            "Floor plan resolution mismatch for {}: {}x{}".format(
                row_name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()
            )
        )
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
        raise RuntimeError("Floor plan texture save failed: " + imported_paths[0])
    imported_texture_paths.append(imported_paths[0])


data_table = unreal.load_asset(DATA_TABLE_PATH)
if data_table is None:
    raise RuntimeError("Map presentation DataTable failed to load: " + DATA_TABLE_PATH)

if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, SOURCE_PATH):
    raise RuntimeError("Map presentation JSON import failed: " + SOURCE_PATH)

if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, False):
    raise RuntimeError("Map presentation DataTable save failed")

exported_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(data_table))
rows_by_name = {str(row.get("Name")): row for row in exported_rows}
failures = []
for row_name, bounds in EXPECTED_BOUNDS.items():
    row = rows_by_name.get(row_name)
    if row is None:
        failures.append("missing row " + row_name)
        continue
    actual_min = (float(row["WorldMin"]["X"]), float(row["WorldMin"]["Y"]))
    actual_max = (float(row["WorldMax"]["X"]), float(row["WorldMax"]["Y"]))
    if actual_min != bounds[0] or actual_max != bounds[1]:
        failures.append("{} bounds {}..{} != {}..{}".format(row_name, actual_min, actual_max, bounds[0], bounds[1]))
    zone_ids = {str(zone["ZoneId"]) for zone in row.get("ZoneAnchors", [])}
    if str(row.get("ContractTargetGalleryZoneId")) not in zone_ids:
        failures.append("{} target gallery zone missing from anchors".format(row_name))
    if not row.get("DefaultExitAnchors"):
        failures.append("{} has no exit anchor".format(row_name))
    else:
        exit_location = row["DefaultExitAnchors"][0]["WorldLocation"]
        actual_exit = (float(exit_location["X"]), float(exit_location["Y"]))
        if actual_exit != EXPECTED_EXITS[row_name]:
            failures.append("{} exit {} != {}".format(row_name, actual_exit, EXPECTED_EXITS[row_name]))

payload = {
    "asset": DATA_TABLE_PATH,
    "source": SOURCE_PATH.replace("\\", "/"),
    "rows": sorted(rows_by_name),
    "floor_plan_textures": sorted(imported_texture_paths),
    "failures": failures,
}
if failures:
    unreal.log_error("MH_MAP_PRESENTATION_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
    raise RuntimeError("Map presentation DataTable verification failed")

unreal.log_warning("MH_MAP_PRESENTATION_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
