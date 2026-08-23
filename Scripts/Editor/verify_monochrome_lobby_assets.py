import json

import unreal
from editor_toolset.toolsets.object import ObjectTools


UMG = unreal.UMGToolSet.get_default_object()
TEXTURE_ROOT = "/Game/Assets/UI/Common/Monochrome"


def get_property(instance, name):
    schema = json.loads(ObjectTools.list_properties(instance))
    if name not in schema:
        raise RuntimeError(
            f"Missing property {name} on {instance.get_path_name()}"
        )
    return json.loads(ObjectTools.get_properties(instance, [name]))[name]


def get_widgets(blueprint_path):
    blueprint = unreal.load_asset(blueprint_path)
    if not blueprint:
        raise RuntimeError(f"Missing Blueprint {blueprint_path}")
    tree = UMG.call_method("GetWidgets", (blueprint,))
    return {info.widget.get_name(): info.widget for info in tree.widgets}


def expected_texture(asset_name):
    return f"{TEXTURE_ROOT}/{asset_name}.{asset_name}"


def resource_path(brush):
    reference = brush.get("resourceObject")
    if isinstance(reference, dict):
        return reference.get("refPath")
    return reference


def verify_brush(widget, property_name, expected_asset):
    actual = resource_path(get_property(widget, property_name))
    expected = expected_texture(expected_asset)
    if actual != expected:
        raise RuntimeError(
            f"Resource mismatch {widget.get_path_name()} {property_name}: "
            f"actual={actual} expected={expected}"
        )


def verify_button(widget):
    style = get_property(widget, "widgetStyle")
    expected_by_state = {
        "normal": "T_UIButton_Monochrome_Normal",
        "hovered": "T_UIButton_Monochrome_Hovered",
        "pressed": "T_UIButton_Monochrome_Pressed",
        "disabled": "T_UIButton_Monochrome_Normal",
    }
    for state, expected_asset in expected_by_state.items():
        actual = resource_path(style[state])
        expected = expected_texture(expected_asset)
        if actual != expected:
            raise RuntimeError(
                f"Button mismatch {widget.get_path_name()} {state}: "
                f"actual={actual} expected={expected}"
            )


expected_texture_assets = (
    "T_UIButton_Monochrome_Normal",
    "T_UIButton_Monochrome_Hovered",
    "T_UIButton_Monochrome_Pressed",
    "T_UILogo_MuseumHeist_Monochrome",
    "T_UIIcon_ReadyCheck_Monochrome",
    "T_UIIcon_Copy_Monochrome",
    "T_UIBorder_Monochrome_1x1",
    "T_UIBorder_Monochrome_2x1",
    "T_UIBorder_Monochrome_1x2",
)
for asset_name in expected_texture_assets:
    if not unreal.EditorAssetLibrary.does_asset_exist(
        f"{TEXTURE_ROOT}/{asset_name}"
    ):
        raise RuntimeError(f"Missing imported texture {asset_name}")

lobby = get_widgets("/Game/Blueprints/UI/Lobby/WBP_Lobby")
player_card = get_widgets("/Game/Blueprints/UI/Lobby/WBP_LobbyPlayerCard")
map_card = get_widgets("/Game/Blueprints/UI/Lobby/WBP_LobbyMapCard")

verify_brush(
    lobby["LobbyLogoImage"],
    "brush",
    "T_UILogo_MuseumHeist_Monochrome",
)
verify_brush(lobby["CopyJoinCodeImage"], "brush", "T_UIIcon_Copy_Monochrome")
verify_brush(lobby["PlayerCountBorder"], "background", "T_UIBorder_Monochrome_2x1")
verify_button(lobby["LeaveSessionButton"])
verify_button(lobby["StartGameButton"])

verify_brush(
    player_card["PlayerCardBorder"],
    "background",
    "T_UIBorder_Monochrome_1x2",
)
verify_button(player_card["ReadyButton"])
verify_brush(
    player_card["ReadyCheckImage"],
    "brush",
    "T_UIIcon_ReadyCheck_Monochrome",
)

verify_brush(
    map_card["SelectedCheckImage"],
    "brush",
    "T_UIIcon_ReadyCheck_Monochrome",
)

print(
    "MONOCHROME_LOBBY_VERIFY_RESULT "
    "success=True textures=9 widgets=9 blueprints=3"
)
