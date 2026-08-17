import json
from pathlib import Path

import unreal

from editor_toolset.toolsets.blueprint import BlueprintTools


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "W7" / "Generated"
MAP_TABLE_PATH = "/Game/Data/DataTable/DT_MapPresentation"
MAP_TABLE_JSON = PROJECT_ROOT / "DataTableImports" / "DT_MapPresentation.json"
MAP_DESTINATION = "/Game/Assets/UI/Map"
STATUS_ICON_DESTINATION = "/Game/Assets/UI/Status"
AUDIO_DESTINATION = "/Game/Assets/Audio/W7"
MASTER_SOUND_CLASS_PATH = "/Engine/EngineSounds/Master.Master"
STUN_SOUND_MIX_NAME = "SMX_HeistStunLowPass"
PLAYER_CHARACTER_BLUEPRINT_PATH = "/Game/Blueprints/Player/BP_HeistPlayerCharacter"
HUD_WIDGET_BLUEPRINT_PATH = "/Game/Blueprints/UI/HUD/WBP_HeistHUD"
NAMEPLATE_WIDGET_BLUEPRINT_PATH = "/Game/Blueprints/UI/HUD/WBP_HeistNameplate"


def log(message):
    unreal.log(f"[W7PresentationAssets] {message}")


def import_audio(asset_name, looping):
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

    sound_wave.set_editor_property("looping", looping)
    if bool(sound_wave.get_editor_property("looping")) != looping:
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


def import_status_icon(status_name):
    asset_name = f"T_HeistStatus_{status_name}"
    source_file = SOURCE_ROOT / f"{asset_name}.png"
    if not source_file.is_file():
        raise RuntimeError(f"Missing status-icon source: {source_file}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", STATUS_ICON_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{STATUS_ICON_DESTINATION}/{asset_name}"
    texture = unreal.load_asset(asset_path)
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Status-icon texture import failed: {asset_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def create_stun_sound_mix():
    asset_path = f"{AUDIO_DESTINATION}/{STUN_SOUND_MIX_NAME}"
    sound_mix = unreal.load_asset(asset_path)
    if not sound_mix:
        sound_mix = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            STUN_SOUND_MIX_NAME,
            AUDIO_DESTINATION,
            unreal.SoundMix,
            unreal.SoundMixFactory(),
        )
    if not sound_mix or not isinstance(sound_mix, unreal.SoundMix):
        raise RuntimeError(f"SoundMix creation failed: {asset_path}")

    master_sound_class = unreal.load_asset(MASTER_SOUND_CLASS_PATH)
    if not master_sound_class or not isinstance(master_sound_class, unreal.SoundClass):
        raise RuntimeError(f"Master SoundClass not found: {MASTER_SOUND_CLASS_PATH}")

    adjuster = unreal.SoundClassAdjuster()
    adjuster.set_editor_property("sound_class_object", master_sound_class)
    adjuster.set_editor_property("volume_adjuster", 1.0)
    adjuster.set_editor_property("pitch_adjuster", 1.0)
    adjuster.set_editor_property("low_pass_filter_frequency", 1200.0)
    adjuster.set_editor_property("apply_to_children", True)
    adjuster.set_editor_property("voice_center_channel_volume_adjuster", 1.0)

    sound_mix.set_editor_property("sound_class_effects", [adjuster])
    sound_mix.set_editor_property("fade_in_time", 0.08)
    sound_mix.set_editor_property("duration", -1.0)
    sound_mix.set_editor_property("fade_out_time", 0.20)
    unreal.EditorAssetLibrary.save_loaded_asset(sound_mix, only_if_is_dirty=False)

    effects = sound_mix.get_editor_property("sound_class_effects")
    if len(effects) != 1:
        raise RuntimeError(f"Unexpected SoundMix adjuster count: {asset_path}")
    saved_adjuster = effects[0]
    if saved_adjuster.get_editor_property("sound_class_object") != master_sound_class:
        raise RuntimeError(f"SoundMix SoundClass setup failed: {asset_path}")
    if abs(saved_adjuster.get_editor_property("volume_adjuster") - 1.0) > 0.001:
        raise RuntimeError(f"SoundMix volume setup failed: {asset_path}")
    if abs(saved_adjuster.get_editor_property("pitch_adjuster") - 1.0) > 0.001:
        raise RuntimeError(f"SoundMix pitch setup failed: {asset_path}")
    if abs(saved_adjuster.get_editor_property("low_pass_filter_frequency") - 1200.0) > 0.01:
        raise RuntimeError(f"SoundMix low-pass setup failed: {asset_path}")
    if not saved_adjuster.get_editor_property("apply_to_children"):
        raise RuntimeError(f"SoundMix child-class setup failed: {asset_path}")
    if abs(sound_mix.get_editor_property("fade_in_time") - 0.08) > 0.001:
        raise RuntimeError(f"SoundMix fade-in setup failed: {asset_path}")
    if abs(sound_mix.get_editor_property("duration") + 1.0) > 0.001:
        raise RuntimeError(f"SoundMix duration setup failed: {asset_path}")
    if abs(sound_mix.get_editor_property("fade_out_time") - 0.20) > 0.001:
        raise RuntimeError(f"SoundMix fade-out setup failed: {asset_path}")
    return sound_mix


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


def assign_presentation_assets(status_icons, one_shot_cues, stun_sound_mix):
    icon_by_status = {
        status_name: status_icon
        for status_name, status_icon in zip(
            ("Stunned", "Arrested", "CarryingOriginal", "Heavy"), status_icons
        )
    }
    cue_by_name = {
        cue.get_name(): cue
        for cue in one_shot_cues
    }

    assignments = {
        PLAYER_CHARACTER_BLUEPRINT_PATH: {
            "stun_sound_mix": stun_sound_mix,
            "carrying_original_footstep_sound": cue_by_name["SW_HeistCarryFootstep"],
            "heavy_footstep_sound": cue_by_name["SW_HeistHeavyFootstep"],
        },
        HUD_WIDGET_BLUEPRINT_PATH: {
            "stunned_status_icon": icon_by_status["Stunned"],
            "arrested_status_icon": icon_by_status["Arrested"],
            "carrying_original_status_icon": icon_by_status["CarryingOriginal"],
            "heavy_status_icon": icon_by_status["Heavy"],
            "arrested_sound": cue_by_name["SW_HeistArrested"],
            "rescued_sound": cue_by_name["SW_HeistRescue"],
        },
        NAMEPLATE_WIDGET_BLUEPRINT_PATH: {
            "stunned_status_icon": icon_by_status["Stunned"],
            "arrested_status_icon": icon_by_status["Arrested"],
            "carrying_original_status_icon": icon_by_status["CarryingOriginal"],
            "heavy_status_icon": icon_by_status["Heavy"],
        },
    }

    assigned = {}
    for blueprint_path, properties in assignments.items():
        blueprint = unreal.load_asset(blueprint_path)
        if not blueprint or not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError(f"Blueprint not found: {blueprint_path}")
        cdo = unreal.get_default_object(blueprint.generated_class())
        if not cdo:
            raise RuntimeError(f"Blueprint CDO not found: {blueprint_path}")
        for property_name, value in properties.items():
            cdo.set_editor_property(property_name, value)
            if cdo.get_editor_property(property_name) != value:
                raise RuntimeError(
                    f"Blueprint default assignment failed: {blueprint_path}.{property_name}"
                )
        BlueprintTools.compile_blueprint(blueprint, warnings_as_errors=False)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            blueprint, only_if_is_dirty=False
        ):
            raise RuntimeError(f"Blueprint save failed: {blueprint_path}")
        assigned[blueprint_path] = {
            property_name: value.get_path_name()
            for property_name, value in properties.items()
        }
    return assigned


def main():
    floor_plans = [import_floor_plan(map_id) for map_id in ("M01", "M02", "M03")]
    map_table = refill_map_table()
    status_icons = [
        import_status_icon(status_name)
        for status_name in ("Stunned", "Arrested", "CarryingOriginal", "Heavy")
    ]
    suspense = import_audio("SW_HeistSuspenseLoop", True)
    alarm = import_audio("SW_HeistAlarmLoop", True)
    one_shot_cues = [
        import_audio(asset_name, False)
        for asset_name in (
            "SW_HeistArrested",
            "SW_HeistRescue",
            "SW_HeistCarryFootstep",
            "SW_HeistHeavyFootstep",
        )
    ]
    stun_sound_mix = create_stun_sound_mix()
    assignments = assign_presentation_assets(
        status_icons, one_shot_cues, stun_sound_mix
    )
    payload = {
        "result": "PASS",
        "map_table": map_table.get_path_name(),
        "floor_plans": [floor_plan.get_path_name() for floor_plan in floor_plans],
        "status_icons": [status_icon.get_path_name() for status_icon in status_icons],
        "suspense": suspense.get_path_name(),
        "alarm": alarm.get_path_name(),
        "one_shot_cues": [cue.get_path_name() for cue in one_shot_cues],
        "stun_sound_mix": stun_sound_mix.get_path_name(),
        "assignments": assignments,
    }
    log(json.dumps(payload, ensure_ascii=False))


main()
