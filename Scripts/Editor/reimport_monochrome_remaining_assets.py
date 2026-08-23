from pathlib import Path

import unreal


ROOT = Path(r"D:\Dev\UE5.8\Project_MuseumHeist")
SOURCE_FOLDER = ROOT / "SourceArt/UI/Concepts/Monochrome"
DESTINATION_FOLDER = "/Game/Assets/UI/Common/Monochrome"

SOURCE_ASSETS = (
    ("T_UIButton_Monochrome_Normal_2048x512.png", "T_UIButton_Monochrome_Normal"),
    ("T_UIButton_Monochrome_Hovered_2048x512.png", "T_UIButton_Monochrome_Hovered"),
    ("T_UIButton_Monochrome_Pressed_2048x512.png", "T_UIButton_Monochrome_Pressed"),
    ("T_UIIcon_Copy_Monochrome_2048.png", "T_UIIcon_Copy_Monochrome"),
    ("T_UIIcon_ReadyCheck_Monochrome_2048.png", "T_UIIcon_ReadyCheck_Monochrome"),
    ("T_UILogo_MuseumHeist_Monochrome_2048.png", "T_UILogo_MuseumHeist_Monochrome"),
)


def main():
    missing = [filename for filename, _ in SOURCE_ASSETS if not (SOURCE_FOLDER / filename).is_file()]
    if missing:
        raise RuntimeError(f"Missing source PNG files: {missing}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for filename, destination_name in SOURCE_ASSETS:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(SOURCE_FOLDER / filename))
        task.set_editor_property("destination_path", DESTINATION_FOLDER)
        task.set_editor_property("destination_name", destination_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)

    asset_tools.import_asset_tasks(tasks)

    saved = 0
    for _, destination_name in SOURCE_ASSETS:
        asset_path = f"{DESTINATION_FOLDER}/{destination_name}"
        texture = unreal.load_asset(asset_path)
        if not texture:
            raise RuntimeError(f"Failed to import {asset_path}")

        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("srgb", True)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
            raise RuntimeError(f"Failed to save {asset_path}")
        saved += 1

    unreal.log(f"MONOCHROME_REMAINING_REIMPORT_RESULT success=True textures={saved} widgets=0")


try:
    main()
except Exception as error:
    unreal.log_error(f"MONOCHROME_REMAINING_REIMPORT_RESULT success=False error={error}")
    raise
