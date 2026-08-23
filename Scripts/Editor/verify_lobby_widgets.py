import unreal


UMG = unreal.UMGToolSet.get_default_object()

SPECS = (
    (
        "/Game/Blueprints/UI/Lobby/WBP_LobbyPlayerCard",
        ("PlayerSlotText", "ProfileImage", "PlayerNameText", "ReadyButton", "ReadyCheckImage"),
    ),
    (
        "/Game/Blueprints/UI/Lobby/WBP_LobbyMapCard",
        ("SelectMapButton", "MapNameText", "MapImage", "SelectedCheckImage"),
    ),
    (
        "/Game/Blueprints/UI/Lobby/WBP_Lobby",
        (
            "CopyJoinCodeButton",
            "LeaveSessionButton",
            "StartGameButton",
            "JoinCodeText",
            "PlayerCountText",
            "PlayerCard1",
            "PlayerCard2",
            "PlayerCard3",
            "PlayerCard4",
            "MapRandomCard",
            "MapM01Card",
            "MapM02Card",
            "MapM03Card",
        ),
    ),
)


unreal.log("LOBBY_VERIFY_BEGIN")
all_valid = True

for asset_path, required_names in SPECS:
    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        unreal.log_error(f"LOBBY_VERIFY asset={asset_path} loaded=False")
        all_valid = False
        continue

    compiled = bool(UMG.call_method("CompileWidgetBlueprint", (blueprint,)))
    tree = UMG.call_method("GetWidgets", (blueprint,))
    widget_names = {info.widget.get_name() for info in tree.widgets}
    missing = [name for name in required_names if name not in widget_names]
    has_countdown = "ReadyCountdownText" in widget_names
    valid = compiled and not missing and not has_countdown
    all_valid = all_valid and valid

    unreal.log(
        "LOBBY_VERIFY "
        f"asset={asset_path} compile={compiled} "
        f"widgets={len(widget_names)} missing={missing} "
        f"ReadyCountdownText={has_countdown} valid={valid}"
    )

unreal.log(f"LOBBY_VERIFY_END success={all_valid}")
