import json
from pathlib import Path

import unreal
from editor_toolset.toolsets.object import ObjectTools


ROOT = Path(r"D:\Dev\UE5.8\Project_MuseumHeist")
SOURCE_FOLDER = ROOT / "SourceArt/UI/Concepts/Monochrome"
DESTINATION_FOLDER = "/Game/Assets/UI/Common/Monochrome"

UMG = unreal.UMGToolSet.get_default_object()


SOURCE_ASSETS = {
    "ButtonNormal": (
        SOURCE_FOLDER / "T_UIButton_Monochrome_Normal_2048x512.png",
        "T_UIButton_Monochrome_Normal",
    ),
    "ButtonHovered": (
        SOURCE_FOLDER / "T_UIButton_Monochrome_Hovered_2048x512.png",
        "T_UIButton_Monochrome_Hovered",
    ),
    "ButtonPressed": (
        SOURCE_FOLDER / "T_UIButton_Monochrome_Pressed_2048x512.png",
        "T_UIButton_Monochrome_Pressed",
    ),
    "Logo": (
        SOURCE_FOLDER / "T_UILogo_MuseumHeist_Monochrome_2048.png",
        "T_UILogo_MuseumHeist_Monochrome",
    ),
    "ReadyCheck": (
        SOURCE_FOLDER / "T_UIIcon_ReadyCheck_Monochrome_2048.png",
        "T_UIIcon_ReadyCheck_Monochrome",
    ),
    "Copy": (
        SOURCE_FOLDER / "T_UIIcon_Copy_Monochrome_2048.png",
        "T_UIIcon_Copy_Monochrome",
    ),
    "Border1x1": (
        SOURCE_FOLDER / "T_UIBorder_Monochrome_1x1_2048.png",
        "T_UIBorder_Monochrome_1x1",
    ),
    "Border2x1": (
        SOURCE_FOLDER / "T_UIBorder_Monochrome_2x1_2048x1024.png",
        "T_UIBorder_Monochrome_2x1",
    ),
    "Border1x2": (
        SOURCE_FOLDER / "T_UIBorder_Monochrome_1x2_1024x2048.png",
        "T_UIBorder_Monochrome_1x2",
    ),
}


WIDGET_REQUIREMENTS = {
    "/Game/Blueprints/UI/Lobby/WBP_Lobby": (
        "LobbyLogoImage",
        "CopyJoinCodeImage",
        "PlayerCountBorder",
        "LeaveSessionButton",
        "StartGameButton",
    ),
    "/Game/Blueprints/UI/Lobby/WBP_LobbyPlayerCard": (
        "PlayerCardBorder",
        "ReadyButton",
        "ReadyCheckImage",
    ),
    "/Game/Blueprints/UI/Lobby/WBP_LobbyMapCard": (
        "SelectedCheckImage",
    ),
}


def log(message):
    unreal.log(f"[MonochromeLobbyUI] {message}")


def schema_for(instance):
    return json.loads(ObjectTools.list_properties(instance))


def set_props(instance, values):
    schema = schema_for(instance)
    filtered = {key: value for key, value in values.items() if key in schema}
    if not filtered:
        return
    ObjectTools.get_properties(instance, list(filtered.keys()))
    if not ObjectTools.set_properties(instance, json.dumps(filtered, ensure_ascii=False)):
        raise RuntimeError(
            f"Failed to set {list(filtered.keys())} on {instance.get_path_name()}"
        )


def get_prop(instance, name, default=None):
    schema = schema_for(instance)
    if name not in schema:
        return default
    return json.loads(ObjectTools.get_properties(instance, [name])).get(name, default)


def object_ref(instance):
    return {"refPath": instance.get_path_name()} if instance else None


def get_widget_map(blueprint):
    result = {}
    tree = UMG.call_method("GetWidgets", (blueprint,))
    for info in tree.widgets:
        widget = info.widget
        ObjectTools.list_properties(widget)
        result[widget.get_name()] = widget
    return result


def import_textures():
    missing_sources = [str(path) for path, _ in SOURCE_ASSETS.values() if not path.is_file()]
    if missing_sources:
        raise RuntimeError(f"Missing source PNG files: {missing_sources}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for source_path, destination_name in SOURCE_ASSETS.values():
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", DESTINATION_FOLDER)
        task.set_editor_property("destination_name", destination_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)

    imported = {}
    for key, (_, destination_name) in SOURCE_ASSETS.items():
        asset_path = f"{DESTINATION_FOLDER}/{destination_name}"
        texture = unreal.load_asset(asset_path)
        if not texture:
            raise RuntimeError(f"Failed to import {asset_path}")

        # UI 전용 설정만 적용한다. 이미지 크기와 Widget Layout은 변경하지 않는다.
        for property_name, value in (
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
            ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
            ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
            ("srgb", True),
        ):
            try:
                texture.set_editor_property(property_name, value)
            except Exception as error:
                unreal.log_warning(
                    f"[MonochromeLobbyUI] Texture setting skipped "
                    f"{destination_name}.{property_name}: {error}"
                )
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
        imported[key] = texture
    return imported


def set_image_resource(image_widget, texture):
    brush = get_prop(image_widget, "brush")
    if not isinstance(brush, dict):
        raise RuntimeError(f"No Brush on {image_widget.get_path_name()}")
    brush["resourceObject"] = object_ref(texture)
    set_props(image_widget, {"brush": brush})


def set_border_resource(border_widget, texture):
    brush = get_prop(border_widget, "background")
    if not isinstance(brush, dict):
        raise RuntimeError(f"No Background Brush on {border_widget.get_path_name()}")
    brush["resourceObject"] = object_ref(texture)
    brush["drawAs"] = "Image"
    set_props(
        border_widget,
        {
            "background": brush,
            "brush_color": {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0},
        },
    )


def set_button_resources(button_widget, textures):
    schema = schema_for(button_widget)
    style_property = next(
        (
            candidate
            for candidate in ("widgetStyle", "widget_style", "style")
            if candidate in schema
        ),
        None,
    )
    if not style_property:
        style_keys = [key for key in schema if "style" in key.lower()]
        raise RuntimeError(
            f"No Button style property on {button_widget.get_path_name()}; "
            f"available style keys={style_keys}"
        )

    style = get_prop(button_widget, style_property)
    if not isinstance(style, dict):
        raise RuntimeError(
            f"Invalid {style_property} on {button_widget.get_path_name()}"
        )

    state_textures = {
        "normal": textures["ButtonNormal"],
        "hovered": textures["ButtonHovered"],
        "pressed": textures["ButtonPressed"],
        "disabled": textures["ButtonNormal"],
    }
    for state_name, texture in state_textures.items():
        brush = style.get(state_name)
        if not isinstance(brush, dict):
            raise RuntimeError(
                f"Missing {state_name} brush on {button_widget.get_path_name()}"
            )
        # 현재 Designer에서 조정한 ImageSize, Margin, Tint는 그대로 유지한다.
        brush["resourceObject"] = object_ref(texture)
        brush["drawAs"] = "Image"
        style[state_name] = brush

    set_props(button_widget, {style_property: style})


def load_and_validate_blueprints():
    loaded = {}
    widget_maps = {}
    missing = []
    for blueprint_path, required_names in WIDGET_REQUIREMENTS.items():
        blueprint = unreal.load_asset(blueprint_path)
        if not blueprint:
            missing.append(f"{blueprint_path}: Blueprint missing")
            continue
        widgets = get_widget_map(blueprint)
        loaded[blueprint_path] = blueprint
        widget_maps[blueprint_path] = widgets
        for widget_name in required_names:
            if widget_name not in widgets:
                missing.append(f"{blueprint_path}: {widget_name}")
    if missing:
        raise RuntimeError(f"Required Lobby widgets missing: {missing}")
    return loaded, widget_maps


blueprints, widgets_by_blueprint = load_and_validate_blueprints()
textures = import_textures()

lobby = widgets_by_blueprint["/Game/Blueprints/UI/Lobby/WBP_Lobby"]
player_card = widgets_by_blueprint[
    "/Game/Blueprints/UI/Lobby/WBP_LobbyPlayerCard"
]
map_card = widgets_by_blueprint["/Game/Blueprints/UI/Lobby/WBP_LobbyMapCard"]

set_image_resource(lobby["LobbyLogoImage"], textures["Logo"])
set_image_resource(lobby["CopyJoinCodeImage"], textures["Copy"])
set_border_resource(lobby["PlayerCountBorder"], textures["Border2x1"])
set_button_resources(lobby["LeaveSessionButton"], textures)
set_button_resources(lobby["StartGameButton"], textures)

set_border_resource(player_card["PlayerCardBorder"], textures["Border1x2"])
set_button_resources(player_card["ReadyButton"], textures)
set_image_resource(player_card["ReadyCheckImage"], textures["ReadyCheck"])

set_image_resource(map_card["SelectedCheckImage"], textures["ReadyCheck"])

compile_results = {}
for blueprint_path, blueprint in blueprints.items():
    compiled = bool(UMG.call_method("CompileWidgetBlueprint", (blueprint,)))
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    compile_results[blueprint_path] = compiled
    log(f"compile={compiled} asset={blueprint_path}")

unreal.EditorAssetLibrary.save_directory(DESTINATION_FOLDER, False, True)
unreal.EditorAssetLibrary.save_directory("/Game/Blueprints/UI/Lobby", False, True)

if not all(compile_results.values()):
    raise RuntimeError(f"Widget compile failed: {compile_results}")

print(
    "MONOCHROME_LOBBY_APPLY_RESULT "
    f"success=True textures={len(textures)} widgets=9 "
    f"blueprints={len(compile_results)}"
)
