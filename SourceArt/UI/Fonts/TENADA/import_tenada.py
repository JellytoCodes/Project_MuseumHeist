import json
import os
import unreal


SOURCE_FILE = os.path.normpath(
    os.path.join(unreal.Paths.project_dir(), "SourceArt", "UI", "Fonts", "TENADA", "Tenada.ttf")
)
DESTINATION_PATH = "/Game/Blueprints/UI/Fonts"
FONT_FACE_PATH = f"{DESTINATION_PATH}/FF_TENADA"
COMPOSITE_FONT_PATH = f"{DESTINATION_PATH}/F_TENADA"
BASE_COMPOSITE_FONT_PATH = f"{DESTINATION_PATH}/F_HeistKorean"


def import_font_face():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_FILE)
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", "FF_TENADA")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    font_face = unreal.load_asset(FONT_FACE_PATH)
    if not font_face:
        raise RuntimeError(f"TENADA FontFace import failed: {FONT_FACE_PATH}")
    return font_face, list(task.get_editor_property("imported_object_paths"))


def create_composite_font(font_face):
    if not unreal.EditorAssetLibrary.does_asset_exist(COMPOSITE_FONT_PATH):
        if not unreal.EditorAssetLibrary.duplicate_asset(
            BASE_COMPOSITE_FONT_PATH, COMPOSITE_FONT_PATH
        ):
            raise RuntimeError(f"TENADA composite font duplicate failed: {COMPOSITE_FONT_PATH}")

    font = unreal.load_asset(COMPOSITE_FONT_PATH)
    if not font:
        raise RuntimeError(f"TENADA composite font load failed: {COMPOSITE_FONT_PATH}")

    composite = font.get_editor_property("composite_font")
    composite_text = composite.export_text()
    old_face = (
        "/Game/Blueprints/UI/Fonts/FF_HeistKorean_Regular."
        "FF_HeistKorean_Regular"
    )
    new_face = "/Game/Blueprints/UI/Fonts/FF_TENADA.FF_TENADA"
    if old_face not in composite_text and new_face not in composite_text:
        raise RuntimeError("Unexpected base composite font structure")

    if old_face in composite_text:
        composite.import_text(composite_text.replace(old_face, new_face))
        font.set_editor_property("composite_font", composite)

    font_face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.LAZY_LOAD)
    unreal.EditorAssetLibrary.save_loaded_asset(font_face, False)
    unreal.EditorAssetLibrary.save_loaded_asset(font, False)
    return font, composite.export_text()


font_face_asset, imported_paths = import_font_face()
font_asset, composite_definition = create_composite_font(font_face_asset)

print(
    "TENADA_IMPORT_RESULT="
    + json.dumps(
        {
            "font_face": font_face_asset.get_path_name(),
            "font": font_asset.get_path_name(),
            "imported_paths": imported_paths,
            "source_filename": font_face_asset.get_editor_property("source_filename"),
            "composite_uses_tenada": "FF_TENADA.FF_TENADA" in composite_definition,
        },
        ensure_ascii=False,
    )
)
