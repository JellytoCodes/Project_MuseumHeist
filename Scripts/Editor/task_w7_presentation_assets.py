import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "W7" / "Generated"
MAP_TABLE_PATH = "/Game/Data/DataTable/DT_MapPresentation"
MAP_TABLE_JSON = PROJECT_ROOT / "DataTableImports" / "DT_MapPresentation.json"
MAP_DESTINATION = "/Game/Blueprints/UI/Map"
AUDIO_DESTINATION = "/Game/Blueprints/Audio/W7"


def log(message):
    unreal.log(f"[W7PresentationAssets] {message}")


def import_audio(asset_name):
    source_file = SOURCE_ROOT / f"{asset_name}.wav"
    if not source_file.is_file():
        raise RuntimeError(f"Missing audio source: {source_file}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", AUDIO_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{AUDIO_DESTINATION}/{asset_name}"
    sound_wave = unreal.load_asset(asset_path)
    if not sound_wave:
        raise RuntimeError(f"SoundWave import failed: {asset_path}")

    sound_wave.set_editor_property("looping", True)
    if not sound_wave.get_editor_property("looping"):
        raise RuntimeError(f"SoundWave looping setup failed: {asset_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(sound_wave, only_if_is_dirty=False)
    return sound_wave


def import_floor_plan(map_id):
    asset_name = f"T_FloorPlan_{map_id}"
    source_file = SOURCE_ROOT / f"{asset_name}.png"
    if not source_file.is_file():
        raise RuntimeError(f"Missing floor-plan source: {source_file}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", MAP_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{MAP_DESTINATION}/{asset_name}"
    texture = unreal.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Floor-plan texture import failed: {asset_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def refill_map_table():
    data_table = unreal.load_asset(MAP_TABLE_PATH)
    if not data_table:
        raise RuntimeError(f"DataTable not found: {MAP_TABLE_PATH}")
    if not MAP_TABLE_JSON.is_file():
        raise RuntimeError(f"DataTable JSON not found: {MAP_TABLE_JSON}")

    success = unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(
        data_table, str(MAP_TABLE_JSON)
    )
    if not success:
        raise RuntimeError("DT_MapPresentation JSON fill failed")
    unreal.EditorAssetLibrary.save_loaded_asset(data_table, only_if_is_dirty=False)
    return data_table


def main():
    floor_plans = [import_floor_plan(map_id) for map_id in ("M01", "M02", "M03")]
    map_table = refill_map_table()
    suspense = import_audio("SW_HeistSuspenseLoop")
    alarm = import_audio("SW_HeistAlarmLoop")
    payload = {
        "result": "PASS",
        "map_table": map_table.get_path_name(),
        "floor_plans": [floor_plan.get_path_name() for floor_plan in floor_plans],
        "suspense": suspense.get_path_name(),
        "alarm": alarm.get_path_name(),
    }
    log(json.dumps(payload, ensure_ascii=False))


main()
