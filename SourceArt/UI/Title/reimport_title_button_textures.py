import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "SourceArt", "UI", "Title")
RUNTIME_TEXTURE_ROOT = "/Game/Assets/UI/Title"


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for name in (
        "T_TitleButton_Normal",
        "T_TitleButton_Hovered",
        "T_TitleButton_Pressed",
    ):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SOURCE_ROOT, f"{name}.png"))
        task.set_editor_property("destination_path", RUNTIME_TEXTURE_ROOT)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)

    asset_tools.import_asset_tasks(tasks)
    for task in tasks:
        name = task.get_editor_property("destination_name")
        texture = unreal.load_asset(f"{RUNTIME_TEXTURE_ROOT}/{name}")
        if not texture:
            raise RuntimeError(f"Title button texture reimport failed: {name}")
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        unreal.log(f"[HeistTitleUI][TextureOnly] {name}=PASS")

    unreal.log("[HeistTitleUI][TextureOnly] PASS: no Widget Blueprint mutation requested")


if __name__ == "__main__":
    main()
