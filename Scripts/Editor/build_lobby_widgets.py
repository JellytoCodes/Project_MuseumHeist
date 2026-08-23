import json
from pathlib import Path

import unreal
from editor_toolset.toolsets.object import ObjectTools


ROOT = Path(r"D:\Dev\UE5.8\Project_MuseumHeist")
LOBBY_BP_FOLDER = "/Game/Blueprints/UI/Lobby"
LOBBY_ASSET_FOLDER = "/Game/Assets/UI/Lobby"

UMG = unreal.UMGToolSet.get_default_object()


def log(message):
    unreal.log(f"[LobbyWBP] {message}")


def widget_tree(blueprint):
    return UMG.call_method("GetWidgets", (blueprint,))


def add_widget(blueprint, widget_class, name, parent=None, index=-1):
    info = UMG.call_method("AddWidget", (blueprint, widget_class, name, parent, index))
    # UMGToolSet requires property discovery for every returned widget and slot.
    ObjectTools.list_properties(info.widget)
    if info.slot:
        ObjectTools.list_properties(info.slot)
    return info


def schema_for(instance):
    return json.loads(ObjectTools.list_properties(instance))


def set_props(instance, values):
    if not instance or not values:
        return
    schema = schema_for(instance)
    filtered = {key: value for key, value in values.items() if key in schema}
    if not filtered:
        return
    ObjectTools.get_properties(instance, list(filtered.keys()))
    if not ObjectTools.set_properties(instance, json.dumps(filtered, ensure_ascii=False)):
        raise RuntimeError(f"Failed to set {list(filtered.keys())} on {instance.get_path_name()}")


def get_prop(instance, name, default=None):
    schema = schema_for(instance)
    if name not in schema:
        return default
    return json.loads(ObjectTools.get_properties(instance, [name])).get(name, default)


def object_ref(instance):
    return {"refPath": instance.get_path_name()} if instance else None


def set_text(text_widget, text, size=24, color=(0.96, 0.95, 0.91, 1.0), center=True):
    values = {"text": text}
    color_value = get_prop(text_widget, "color_and_opacity")
    if isinstance(color_value, dict):
        color_value["specifiedColor"] = {
            "r": color[0], "g": color[1], "b": color[2], "a": color[3]
        }
        values["color_and_opacity"] = color_value
    if center:
        values["justification"] = "Center"

    font_value = get_prop(text_widget, "font")
    if isinstance(font_value, dict):
        font_value["fontObject"] = object_ref(FONT)
        font_value["size"] = size
        values["font"] = font_value
    set_props(text_widget, values)


def set_image(image_widget, texture, size=(64.0, 64.0), draw_as=None):
    brush = get_prop(image_widget, "brush")
    if not isinstance(brush, dict):
        return
    brush["resourceObject"] = object_ref(texture)
    brush["imageSize"] = {"x": float(size[0]), "y": float(size[1])}
    if draw_as:
        brush["drawAs"] = draw_as
    set_props(image_widget, {"brush": brush})


def set_border_texture(border_widget, texture, color=(1.0, 1.0, 1.0, 1.0)):
    brush = get_prop(border_widget, "background")
    if isinstance(brush, dict):
        brush["resourceObject"] = object_ref(texture)
        brush["drawAs"] = "Image"
        set_props(border_widget, {"background": brush})
    set_props(border_widget, {
        "brush_color": {"r": color[0], "g": color[1], "b": color[2], "a": color[3]},
        "padding": {"left": 12.0, "top": 8.0, "right": 12.0, "bottom": 8.0},
    })


def set_slot(slot, padding=None, horizontal=None, vertical=None, fill=None):
    values = {}
    if padding is not None:
        if isinstance(padding, (int, float)):
            padding = (padding, padding, padding, padding)
        values["padding"] = {
            "left": float(padding[0]), "top": float(padding[1]),
            "right": float(padding[2]), "bottom": float(padding[3])
        }
    if horizontal:
        values["horizontal_alignment"] = horizontal
    if vertical:
        values["vertical_alignment"] = vertical
    if fill is not None:
        values["size"] = {"value": float(fill), "sizeRule": "Fill"}
    set_props(slot, values)


def set_button_style(button, image_size=(220.0, 64.0)):
    if TITLE_BUTTON_STYLE is not None:
        # Title 버튼 스타일은 재사용하되, 원본 이미지의 큰 원시 크기가
        # Lobby 레이아웃의 Desired Size를 밀어내지 않도록 로컬 사본만 정규화한다.
        style = json.loads(json.dumps(TITLE_BUTTON_STYLE))
        for brush_name in ("normal", "hovered", "pressed", "disabled"):
            brush = style.get(brush_name)
            if isinstance(brush, dict):
                brush["imageSize"] = {
                    "x": float(image_size[0]),
                    "y": float(image_size[1]),
                }
        set_props(button, {"widget_style": style})


def clear_widget_tree(blueprint):
    tree = widget_tree(blueprint)
    roots = [info for info in tree.widgets if not info.inherited and not info.parent and not info.named_slot_host]
    for info in roots:
        ObjectTools.list_properties(info.widget)
        UMG.call_method("RemoveWidget", (blueprint, info.widget))


def load_or_create_widget_blueprint(asset_name, parent_class):
    asset_path = f"{LOBBY_BP_FOLDER}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint = unreal.load_asset(asset_path)
    else:
        blueprint = UMG.call_method("CreateWidgetBlueprint", (LOBBY_BP_FOLDER, asset_name, parent_class))
    if not blueprint:
        raise RuntimeError(f"Unable to load or create {asset_path}")
    clear_widget_tree(blueprint)
    return blueprint


def compile_and_save(blueprint):
    compiled = UMG.call_method("CompileWidgetBlueprint", (blueprint,))
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    return bool(compiled)


def make_variable(blueprint, info):
    UMG.call_method("ToggleWidgetAsVariable", (blueprint, info.widget, True))


def import_lobby_textures():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for source_path in (
        ROOT / "SourceArt/UI/Lobby/T_LobbyCopy.png",
        ROOT / "SourceArt/UI/Lobby/T_LobbyReadyCheck.png",
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(f"{LOBBY_ASSET_FOLDER}/{source_path.stem}"):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", LOBBY_ASSET_FOLDER)
        task.set_editor_property("destination_name", source_path.stem)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])


def find_title_button_style():
    title_blueprint = unreal.load_asset("/Game/Blueprints/UI/Title/WBP_TitleMenu")
    if not title_blueprint:
        return None
    for info in widget_tree(title_blueprint).widgets:
        if isinstance(info.widget, unreal.Button):
            style = get_prop(info.widget, "widget_style")
            if style is not None:
                return style
    return None


def build_player_card():
    blueprint = load_or_create_widget_blueprint("WBP_LobbyPlayerCard", PLAYER_CARD_PARENT)

    root = add_widget(blueprint, unreal.SizeBox.static_class(), "PlayerCardSizeBox")
    set_props(root.widget, {"width_override": 285.0, "height_override": 350.0})

    border = add_widget(blueprint, unreal.Border.static_class(), "PlayerCardBorder", root.widget)
    set_border_texture(border.widget, TITLE_BUTTON_NORMAL, (0.16, 0.12, 0.09, 0.96))
    set_props(border.widget, {
        "padding": {"left": 12.0, "top": 12.0, "right": 12.0, "bottom": 12.0},
    })

    content = add_widget(blueprint, unreal.VerticalBox.static_class(), "PlayerCardContent", border.widget)

    slot_text = add_widget(blueprint, unreal.TextBlock.static_class(), "PlayerSlotText", content.widget)
    set_text(slot_text.widget, "플레이어 1", 22, GOLD)
    make_variable(blueprint, slot_text)
    set_slot(slot_text.slot, (0, 0, 0, 8), "Fill", "Center")

    profile_size = add_widget(blueprint, unreal.SizeBox.static_class(), "ProfileImageSizeBox", content.widget)
    set_props(profile_size.widget, {"height_override": 215.0, "width_override": 250.0})
    set_slot(profile_size.slot, (0, 0, 0, 8), "Center", "Center")

    profile = add_widget(blueprint, unreal.Image.static_class(), "ProfileImage", profile_size.widget)
    set_image(profile.widget, TITLE_EMBLEM, (215.0, 215.0))
    make_variable(blueprint, profile)

    player_name = add_widget(blueprint, unreal.TextBlock.static_class(), "PlayerNameText", content.widget)
    set_text(player_name.widget, "", 22, WHITE)
    make_variable(blueprint, player_name)
    set_slot(player_name.slot, (0, 0, 0, 8), "Fill", "Center")

    ready = add_widget(blueprint, unreal.Button.static_class(), "ReadyButton", content.widget)
    set_button_style(ready.widget, (240.0, 54.0))
    make_variable(blueprint, ready)
    set_slot(ready.slot, (0, 0, 0, 0), "Fill", "Center")

    ready_content = add_widget(blueprint, unreal.HorizontalBox.static_class(), "ReadyButtonContent", ready.widget)
    ready_label = add_widget(blueprint, unreal.TextBlock.static_class(), "ReadyButtonLabel", ready_content.widget)
    set_text(ready_label.widget, "준비", 20, WHITE)
    set_slot(ready_label.slot, (8, 4, 8, 4), "Center", "Center", 1.0)

    ready_check = add_widget(blueprint, unreal.Image.static_class(), "ReadyCheckImage", ready_content.widget)
    set_image(ready_check.widget, READY_CHECK_TEXTURE, (32.0, 32.0))
    set_props(ready_check.widget, {"visibility": "Hidden"})
    make_variable(blueprint, ready_check)
    set_slot(ready_check.slot, (4, 2, 8, 2), "Center", "Center")

    compiled = compile_and_save(blueprint)
    set_props(blueprint, {"default_profile_texture": object_ref(TITLE_EMBLEM)})
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    log(f"PlayerCard compile={compiled}")
    return blueprint


def build_map_card():
    blueprint = load_or_create_widget_blueprint("WBP_LobbyMapCard", MAP_CARD_PARENT)

    root = add_widget(blueprint, unreal.SizeBox.static_class(), "MapCardSizeBox")
    set_props(root.widget, {"width_override": 320.0, "height_override": 185.0})

    button = add_widget(blueprint, unreal.Button.static_class(), "SelectMapButton", root.widget)
    set_button_style(button.widget, (320.0, 185.0))
    make_variable(blueprint, button)

    overlay = add_widget(blueprint, unreal.Overlay.static_class(), "MapCardOverlay", button.widget)

    image = add_widget(blueprint, unreal.Image.static_class(), "MapImage", overlay.widget)
    set_image(image.widget, FLOORPLAN_M01, (320.0, 185.0))
    make_variable(blueprint, image)
    set_slot(image.slot, 0, "Fill", "Fill")

    shade = add_widget(blueprint, unreal.Border.static_class(), "MapNameShade", overlay.widget)
    set_border_texture(shade.widget, TITLE_BUTTON_NORMAL, (0.08, 0.06, 0.04, 0.88))
    set_props(shade.widget, {
        "padding": {"left": 12.0, "top": 8.0, "right": 12.0, "bottom": 8.0},
    })
    set_slot(shade.slot, 0, "Fill", "Bottom")

    map_name = add_widget(blueprint, unreal.TextBlock.static_class(), "MapNameText", shade.widget)
    set_text(map_name.widget, "M01", 22, WHITE, False)
    make_variable(blueprint, map_name)

    selected = add_widget(blueprint, unreal.Image.static_class(), "SelectedCheckImage", overlay.widget)
    set_image(selected.widget, READY_CHECK_TEXTURE, (44.0, 44.0))
    set_props(selected.widget, {"visibility": "Hidden"})
    make_variable(blueprint, selected)
    set_slot(selected.slot, (0, 10, 10, 0), "Right", "Top")

    compiled = compile_and_save(blueprint)
    log(f"MapCard compile={compiled}")
    return blueprint


def build_lobby(player_card_class, map_card_class):
    blueprint = unreal.load_asset(f"{LOBBY_BP_FOLDER}/WBP_Lobby")
    if not blueprint:
        blueprint = UMG.call_method("CreateWidgetBlueprint", (LOBBY_BP_FOLDER, "WBP_Lobby", LOBBY_PARENT))
    if not blueprint:
        raise RuntimeError("Unable to load or create WBP_Lobby")
    clear_widget_tree(blueprint)

    root = add_widget(blueprint, unreal.Border.static_class(), "LobbyRootBorder")
    set_border_texture(root.widget, TITLE_BACKGROUND, (0.12, 0.09, 0.065, 0.98))
    set_props(root.widget, {
        "padding": {"left": 28.0, "top": 24.0, "right": 28.0, "bottom": 24.0},
    })

    content = add_widget(blueprint, unreal.VerticalBox.static_class(), "LobbyContent", root.widget)

    header = add_widget(blueprint, unreal.HorizontalBox.static_class(), "LobbyHeaderRow", content.widget)
    set_slot(header.slot, (0, 0, 0, 18), "Fill", "Center")

    brand = add_widget(blueprint, unreal.HorizontalBox.static_class(), "LobbyBrand", header.widget)
    set_slot(brand.slot, (0, 0, 24, 0), "Left", "Center", 1.0)
    logo = add_widget(blueprint, unreal.Image.static_class(), "LobbyLogoImage", brand.widget)
    set_image(logo.widget, TITLE_EMBLEM, (64.0, 64.0))
    set_slot(logo.slot, (0, 0, 12, 0), "Center", "Center")
    title = add_widget(blueprint, unreal.TextBlock.static_class(), "LobbyTitleText", brand.widget)
    set_text(title.widget, "LOBBY", 42, WHITE, False)
    set_slot(title.slot, 0, "Left", "Center")

    join_group = add_widget(blueprint, unreal.HorizontalBox.static_class(), "JoinCodeGroup", header.widget)
    set_slot(join_group.slot, (0, 0, 24, 0), "Center", "Center")
    join_label = add_widget(blueprint, unreal.TextBlock.static_class(), "JoinCodeLabel", join_group.widget)
    set_text(join_label.widget, "참가 코드", 20, GOLD, False)
    set_slot(join_label.slot, (0, 0, 10, 0), "Left", "Center")
    join_code = add_widget(blueprint, unreal.TextBlock.static_class(), "JoinCodeText", join_group.widget)
    set_text(join_code.widget, "------", 28, WHITE, False)
    make_variable(blueprint, join_code)
    set_slot(join_code.slot, (0, 0, 6, 0), "Left", "Center")
    copy_button = add_widget(blueprint, unreal.Button.static_class(), "CopyJoinCodeButton", join_group.widget)
    set_button_style(copy_button.widget, (46.0, 46.0))
    make_variable(blueprint, copy_button)
    set_slot(copy_button.slot, 0, "Center", "Center")
    copy_image = add_widget(blueprint, unreal.Image.static_class(), "CopyJoinCodeImage", copy_button.widget)
    set_image(copy_image.widget, COPY_TEXTURE, (30.0, 30.0))

    count_border = add_widget(blueprint, unreal.Border.static_class(), "PlayerCountBorder", header.widget)
    set_border_texture(count_border.widget, TITLE_BUTTON_NORMAL)
    set_slot(count_border.slot, (0, 0, 20, 0), "Center", "Center")
    count_text = add_widget(blueprint, unreal.TextBlock.static_class(), "PlayerCountText", count_border.widget)
    set_text(count_text.widget, "0 / 4", 22, WHITE)
    make_variable(blueprint, count_text)

    leave_button = add_widget(blueprint, unreal.Button.static_class(), "LeaveSessionButton", header.widget)
    set_button_style(leave_button.widget, (200.0, 60.0))
    make_variable(blueprint, leave_button)
    set_slot(leave_button.slot, 0, "Right", "Center")
    leave_label = add_widget(blueprint, unreal.TextBlock.static_class(), "LeaveSessionButtonLabel", leave_button.widget)
    set_text(leave_label.widget, "세션 나가기", 20, WHITE)

    player_cards = add_widget(blueprint, unreal.HorizontalBox.static_class(), "PlayerCardsRow", content.widget)
    set_slot(player_cards.slot, (0, 0, 0, 20), "Center", "Center")
    for index in range(1, 5):
        card = add_widget(blueprint, player_card_class, f"PlayerCard{index}", player_cards.widget)
        make_variable(blueprint, card)
        set_slot(card.slot, (6, 0, 6, 0), "Center", "Center")

    map_section = add_widget(blueprint, unreal.VerticalBox.static_class(), "MapSection", content.widget)
    set_slot(map_section.slot, (0, 0, 0, 16), "Fill", "Center")
    map_title = add_widget(blueprint, unreal.TextBlock.static_class(), "MapSectionTitle", map_section.widget)
    set_text(map_title.widget, "맵 선택", 26, GOLD, False)
    set_slot(map_title.slot, (4, 0, 0, 8), "Left", "Center")

    map_scroll = add_widget(blueprint, unreal.ScrollBox.static_class(), "MapHorizontalScrollBox", map_section.widget)
    set_props(map_scroll.widget, {"orientation": "Orient_Horizontal", "scroll_bar_visibility": "Visible"})
    set_slot(map_scroll.slot, 0, "Fill", "Center")

    map_specs = (
        ("MapRandomCard", TITLE_BACKGROUND),
        ("MapM01Card", FLOORPLAN_M01),
        ("MapM02Card", FLOORPLAN_M02),
        ("MapM03Card", FLOORPLAN_M03),
    )
    for name, thumbnail in map_specs:
        card = add_widget(blueprint, map_card_class, name, map_scroll.widget)
        make_variable(blueprint, card)
        set_props(card.widget, {"map_thumbnail": object_ref(thumbnail)})
        set_slot(card.slot, (6, 0, 6, 0), "Center", "Center")

    start_button = add_widget(blueprint, unreal.Button.static_class(), "StartGameButton", content.widget)
    set_button_style(start_button.widget, (320.0, 72.0))
    make_variable(blueprint, start_button)
    set_slot(start_button.slot, (0, 0, 0, 0), "Center", "Center")
    start_label = add_widget(blueprint, unreal.TextBlock.static_class(), "StartGameButtonLabel", start_button.widget)
    set_text(start_label.widget, "게임 시작", 28, WHITE)

    compiled = compile_and_save(blueprint)
    description = UMG.call_method("GetWidgetDescription", (blueprint, None, -1))
    log(f"Lobby compile={compiled}\n{description.description}")
    return blueprint, compiled


import_lobby_textures()

FONT = unreal.load_asset("/Game/Assets/UI/Fonts/F_TENADA")
TITLE_EMBLEM = unreal.load_asset("/Game/Assets/UI/Title/T_TitleLogo_Emblem")
TITLE_BACKGROUND = unreal.load_asset("/Game/Assets/UI/Title/wp2186239")
TITLE_BUTTON_NORMAL = unreal.load_asset("/Game/Assets/UI/Title/T_TitleButton_Normal")
COPY_TEXTURE = unreal.load_asset(f"{LOBBY_ASSET_FOLDER}/T_LobbyCopy")
READY_CHECK_TEXTURE = unreal.load_asset(f"{LOBBY_ASSET_FOLDER}/T_LobbyReadyCheck")
FLOORPLAN_M01 = unreal.load_asset("/Game/Assets/UI/Map/T_FloorPlan_M01")
FLOORPLAN_M02 = unreal.load_asset("/Game/Assets/UI/Map/T_FloorPlan_M02")
FLOORPLAN_M03 = unreal.load_asset("/Game/Assets/UI/Map/T_FloorPlan_M03")

required_assets = {
    "FONT": FONT,
    "TITLE_EMBLEM": TITLE_EMBLEM,
    "TITLE_BACKGROUND": TITLE_BACKGROUND,
    "TITLE_BUTTON_NORMAL": TITLE_BUTTON_NORMAL,
    "COPY_TEXTURE": COPY_TEXTURE,
    "READY_CHECK_TEXTURE": READY_CHECK_TEXTURE,
    "FLOORPLAN_M01": FLOORPLAN_M01,
    "FLOORPLAN_M02": FLOORPLAN_M02,
    "FLOORPLAN_M03": FLOORPLAN_M03,
}
missing = [name for name, asset in required_assets.items() if not asset]
if missing:
    raise RuntimeError(f"Missing assets: {missing}")

GOLD = (1.0, 0.73, 0.28, 1.0)
WHITE = (0.96, 0.95, 0.91, 1.0)

LOBBY_PARENT = unreal.load_class(None, "/Script/Project_MuseumHeist.HeistLobbyWidget")
PLAYER_CARD_PARENT = unreal.load_class(None, "/Script/Project_MuseumHeist.HeistLobbyPlayerCardWidget")
MAP_CARD_PARENT = unreal.load_class(None, "/Script/Project_MuseumHeist.HeistLobbyMapCardWidget")
if not all((LOBBY_PARENT, PLAYER_CARD_PARENT, MAP_CARD_PARENT)):
    raise RuntimeError("Lobby native widget classes were not loaded")

TITLE_BUTTON_STYLE = find_title_button_style()

player_bp = build_player_card()
map_bp = build_map_card()
player_class = player_bp.generated_class()
map_class = map_bp.generated_class()
lobby_bp, lobby_compiled = build_lobby(player_class, map_class)

unreal.EditorAssetLibrary.save_directory(LOBBY_BP_FOLDER, False, True)
unreal.EditorAssetLibrary.save_directory(LOBBY_ASSET_FOLDER, False, True)
print(f"LOBBY_WBP_BUILD_RESULT success={lobby_compiled} path={lobby_bp.get_path_name()}")
