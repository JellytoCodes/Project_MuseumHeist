import unreal


WIDGET_ROOT = "/Game/Blueprints/UI/Title"
TEXTURE_ROOT = "/Game/Assets/UI/Title"

EXPECTED_WIDGETS = {
    "WBP_TitleMenu": {
        "parent": "HeistTitleMenuWidget",
        "variables": (
            "HostSessionButton",
            "JoinSessionButton",
            "SettingsButton",
            "QuitGameButton",
            "SessionStatusText",
            "SessionErrorText",
            "SessionJoinWidget",
            "SettingsWidget",
        ),
    },
    "WBP_SessionJoin": {
        "parent": "HeistSessionJoinWidget",
        "variables": (
            "JoinCodeInput",
            "SubmitJoinSessionButton",
            "CancelSessionButton",
            "RetrySessionButton",
            "JoinCloseButton",
            "SessionStatusText",
            "SessionErrorText",
            "SessionActionHintText",
        ),
    },
    "WBP_Settings": {
        "parent": "HeistSettingsWidget",
        "variables": (
            "SettingsCloseButton",
            "ApplySettingsButton",
            "RestoreDefaultSettingsButton",
            "FOVSlider",
            "MouseSensitivitySlider",
            "MasterVolumeSlider",
            "ResolutionComboBox",
            "WindowModeComboBox",
            "FOVValueText",
            "MouseSensitivityValueText",
            "MasterVolumeValueText",
            "SettingsStatusText",
        ),
    },
}

TITLE_BUTTONS = (
    "HostSessionButton",
    "JoinSessionButton",
    "SettingsButton",
    "QuitGameButton",
)

EXPECTED_BRUSHES = {
    "normal": f"{TEXTURE_ROOT}/T_TitleButton_Normal.T_TitleButton_Normal",
    "hovered": f"{TEXTURE_ROOT}/T_TitleButton_Hovered.T_TitleButton_Hovered",
    "pressed": f"{TEXTURE_ROOT}/T_TitleButton_Pressed.T_TitleButton_Pressed",
}


def call(toolset, method_name, **kwargs):
    return toolset.call_method(method_name, kwargs=kwargs)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify_blueprint(toolset, asset_name, expected):
    asset_path = f"{WIDGET_ROOT}/{asset_name}"
    blueprint = unreal.load_asset(asset_path)
    require(blueprint is not None, f"Missing Widget Blueprint: {asset_path}")

    tree = call(toolset, "GetWidgets", widget_blueprint=blueprint)
    parent_class = tree.info.parent_class
    require(parent_class is not None, f"{asset_name} has no parent class")
    require(
        parent_class.get_name() == expected["parent"],
        f"{asset_name} parent is {parent_class.get_name()}, expected {expected['parent']}",
    )

    entries = {
        str(entry.widget_name): entry
        for entry in tree.widgets
        if entry.widget is not None
    }
    missing = sorted(set(expected["variables"]) - set(entries))
    require(not missing, f"{asset_name} missing required widgets: {missing}")

    not_variables = [
        name
        for name in expected["variables"]
        if not entries[name].is_variable
    ]
    require(not not_variables, f"{asset_name} widgets are not variables: {not_variables}")

    require(
        call(toolset, "CompileWidgetBlueprint", widget_blueprint=blueprint),
        f"{asset_name} compilation failed",
    )
    unreal.log(
        f"[HeistTitleUI][Verify] {asset_name}: parent={parent_class.get_name()} "
        f"required={len(expected['variables'])} variables=PASS compile=PASS"
    )
    return entries


def verify_title_button_brushes(entries):
    for button_name in TITLE_BUTTONS:
        button = entries[button_name].widget
        style = button.get_editor_property("widget_style")
        for state_name, expected_path in EXPECTED_BRUSHES.items():
            brush = style.get_editor_property(state_name)
            resource = brush.get_editor_property("resource_object")
            actual_path = resource.get_path_name() if resource else "None"
            require(
                actual_path == expected_path,
                f"{button_name}.{state_name} uses {actual_path}, expected {expected_path}",
            )
        unreal.log(f"[HeistTitleUI][Verify] {button_name}: Normal/Hovered/Pressed=PASS")


def main():
    toolset_class = getattr(unreal, "UMGToolSet", None)
    require(toolset_class is not None, "UMGToolSet is unavailable")
    toolset = unreal.get_default_object(toolset_class)

    title_entries = None
    for asset_name, expected in EXPECTED_WIDGETS.items():
        entries = verify_blueprint(toolset, asset_name, expected)
        if asset_name == "WBP_TitleMenu":
            title_entries = entries

    verify_title_button_brushes(title_entries)
    unreal.log("[HeistTitleUI][Verify] PASS")


if __name__ == "__main__":
    main()
