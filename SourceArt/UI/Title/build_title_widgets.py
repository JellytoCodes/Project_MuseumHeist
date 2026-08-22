import os
import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "SourceArt", "UI", "Title")
RUNTIME_TEXTURE_ROOT = "/Game/Assets/UI/Title"
WIDGET_ROOT = "/Game/Blueprints/UI/Title"

WHITE = unreal.LinearColor(0.96, 0.95, 0.91, 1.0)
GOLD = unreal.LinearColor(0.94, 0.66, 0.22, 1.0)
MINT = unreal.LinearColor(0.55, 0.88, 0.72, 1.0)
ERROR = unreal.LinearColor(0.95, 0.32, 0.28, 1.0)
PANEL = unreal.LinearColor(0.025, 0.022, 0.02, 0.96)
DIM = unreal.LinearColor(0.0, 0.0, 0.0, 0.68)


def require_type(type_name):
    value = getattr(unreal, type_name, None)
    if value is None:
        raise RuntimeError(f"Required Unreal Python type is unavailable: {type_name}")
    return value


UMG_CLASS = require_type("UMGToolSet")


class UMGReflectionAdapter:
    """Calls AICallable UMGToolSet functions through Unreal reflection.

    UE 5.8 intentionally exposes these functions to the Toolset Registry rather
    than as regular Python methods, so UObject.call_method is the supported
    bridge when this script runs inside UnrealEditor-Cmd.
    """

    def __init__(self):
        self._toolset = unreal.get_default_object(UMG_CLASS)

    def _call(self, method_name, **kwargs):
        return self._toolset.call_method(method_name, kwargs=kwargs)

    def create_widget_blueprint(self, folder_path, asset_name, parent_class):
        return self._call(
            "CreateWidgetBlueprint",
            folder_path=folder_path,
            asset_name=asset_name,
            parent_class=parent_class,
        )

    def get_widgets(self, widget_blueprint):
        return self._call("GetWidgets", widget_blueprint=widget_blueprint)

    def add_widget(self, widget_blueprint, widget_class, widget_display_name, parent_widget=None, child_index=-1):
        return self._call(
            "AddWidget",
            widget_blueprint=widget_blueprint,
            widget_class=widget_class,
            widget_display_name=widget_display_name,
            parent_widget=parent_widget,
            child_index=child_index,
        )

    def remove_widget(self, widget_blueprint, widget):
        return self._call("RemoveWidget", widget_blueprint=widget_blueprint, widget=widget)

    def compile_widget_blueprint(self, widget_blueprint):
        return self._call("CompileWidgetBlueprint", widget_blueprint=widget_blueprint)

    def toggle_widget_as_variable(self, widget_blueprint, widget, is_variable):
        return self._call(
            "ToggleWidgetAsVariable",
            widget_blueprint=widget_blueprint,
            widget=widget,
            is_variable=is_variable,
        )


UMG = UMGReflectionAdapter()


def load_native_class(class_name):
    native_class = unreal.load_class(None, f"/Script/Project_MuseumHeist.{class_name}")
    if not native_class:
        raise RuntimeError(f"Native class not found: {class_name}. Build the Editor target first.")
    return native_class


def import_textures():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for name in (
        "T_TitleButton_Normal",
        "T_TitleButton_Hovered",
        "T_TitleButton_Pressed",
        "T_TitleLogo_Emblem",
    ):
        filename = os.path.join(SOURCE_ROOT, f"{name}.png")
        if not os.path.isfile(filename):
            raise RuntimeError(f"Missing source texture: {filename}")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", filename)
        task.set_editor_property("destination_path", RUNTIME_TEXTURE_ROOT)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    asset_tools.import_asset_tasks(tasks)

    imported = {}
    for task in tasks:
        name = task.get_editor_property("destination_name")
        texture = unreal.load_asset(f"{RUNTIME_TEXTURE_ROOT}/{name}")
        if not texture:
            raise RuntimeError(f"Texture import failed: {name}")
        try:
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        except Exception:
            unreal.log_warning(f"Could not set UI LOD group on {name}; keeping importer default.")
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        imported[name] = texture
    return imported


def get_or_create_widget_blueprint(asset_name, parent_class):
    asset_path = f"{WIDGET_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint = unreal.load_asset(asset_path)
        if not blueprint:
            raise RuntimeError(f"Existing Widget Blueprint could not be loaded: {asset_name}")
        info = UMG.get_widgets(blueprint).info
        if info.parent_class != parent_class:
            raise RuntimeError(f"{asset_name} has an unexpected parent class: {info.parent_class}")
        clear_widget_tree(blueprint)
        return blueprint

    blueprint = UMG.create_widget_blueprint(WIDGET_ROOT, asset_name, parent_class)
    if not blueprint:
        raise RuntimeError(f"Failed to create Widget Blueprint: {asset_name}")
    return blueprint


def clear_widget_tree(blueprint):
    tree = UMG.get_widgets(blueprint)
    roots = [
        entry
        for entry in tree.widgets
        if entry.widget and not entry.parent and not entry.named_slot_host
    ]
    for root in roots:
        direct_children = [
            entry
            for entry in tree.widgets
            if entry.widget and entry.parent == root.widget
        ]
        for child in direct_children:
            if not UMG.remove_widget(blueprint, child.widget):
                raise RuntimeError(
                    f"Failed to remove old child {child.widget.get_name()} from {blueprint.get_name()}"
                )


def get_or_add_root(blueprint, widget_class, name):
    tree = UMG.get_widgets(blueprint)
    roots = [
        entry
        for entry in tree.widgets
        if entry.widget and not entry.parent and not entry.named_slot_host
    ]
    if roots:
        root = roots[0].widget
        expected_name = widget_class.__name__
        if root.get_class().get_name() != expected_name:
            raise RuntimeError(
                f"{blueprint.get_name()} root is {root.get_class().get_name()}, expected {expected_name}"
            )
        return root
    root, _ = add_widget(blueprint, widget_class, name, None)
    return root


def mark_bind_widgets(blueprint, required_names):
    tree = UMG.get_widgets(blueprint)
    by_name = {
        str(entry.widget_name): entry.widget
        for entry in tree.widgets
        if entry.widget
    }
    missing = sorted(set(required_names) - set(by_name))
    if missing:
        raise RuntimeError(f"{blueprint.get_name()} is missing BindWidgets: {missing}")
    for name in required_names:
        UMG.toggle_widget_as_variable(blueprint, by_name[name], True)


def add_widget(blueprint, widget_class, name, parent=None):
    info = UMG.add_widget(blueprint, widget_class, name, parent, -1)
    if not info.widget:
        raise RuntimeError(f"Failed to add {name} to {blueprint.get_name()}")
    return info.widget, info.slot


def set_canvas_layout(slot, x, y, width, height, anchors=None, alignment=None):
    if anchors is not None:
        slot.set_anchors(anchors)
    if alignment is not None:
        slot.set_alignment(alignment)
    slot.set_position(unreal.Vector2D(x, y))
    slot.set_size(unreal.Vector2D(width, height))


def set_fill_canvas(slot):
    slot.set_anchors(
        unreal.Anchors(
            minimum=unreal.Vector2D(0.0, 0.0),
            maximum=unreal.Vector2D(1.0, 1.0),
        )
    )
    slot.set_offsets(unreal.Margin(0.0, 0.0, 0.0, 0.0))


def set_panel_slot_fill(slot):
    if slot is None:
        return
    if hasattr(slot, "set_horizontal_alignment"):
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    if hasattr(slot, "set_vertical_alignment"):
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)


def set_slot_padding(slot, left=0.0, top=0.0, right=0.0, bottom=0.0):
    if slot is not None and hasattr(slot, "set_padding"):
        slot.set_padding(unreal.Margin(left, top, right, bottom))


def make_font(size):
    info = unreal.SlateFontInfo()
    font_asset = unreal.load_asset("/Game/Assets/UI/Fonts/F_TENADA")
    if font_asset:
        info.set_editor_property("font_object", font_asset)
    info.set_editor_property("size", size)
    return info


def slate_color(color):
    value = unreal.SlateColor()
    value.set_editor_property("specified_color", color)
    return value


def configure_text(widget, text, size=32, color=WHITE, justification=None):
    widget.set_text(text)
    widget.set_editor_property("font", make_font(size))
    widget.set_editor_property("color_and_opacity", slate_color(color))
    if justification is not None:
        widget.set_editor_property("justification", justification)


def make_slate_size(width, height):
    slate_vector_type = require_type("DeprecateSlateVector2D")
    slate_size = slate_vector_type()
    slate_size.set_editor_property("x", width)
    slate_size.set_editor_property("y", height)
    return slate_size


def make_brush(texture, image_size):
    brush = unreal.SlateBrush()
    brush.set_editor_property("resource_object", texture)
    slate_size = make_slate_size(image_size.x, image_size.y)
    brush.set_editor_property("image_size", slate_size)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.BOX)
    brush.set_editor_property("margin", unreal.Margin(0.24, 0.36, 0.24, 0.36))
    return brush


def configure_button_style(button, textures):
    style = button.get_editor_property("widget_style")
    image_size = unreal.Vector2D(520.0, 96.0)
    style.set_editor_property("normal", make_brush(textures["T_TitleButton_Normal"], image_size))
    style.set_editor_property("hovered", make_brush(textures["T_TitleButton_Hovered"], image_size))
    style.set_editor_property("pressed", make_brush(textures["T_TitleButton_Pressed"], image_size))
    style.set_editor_property("normal_padding", unreal.Margin(12.0, 6.0, 12.0, 6.0))
    style.set_editor_property("pressed_padding", unreal.Margin(12.0, 9.0, 12.0, 3.0))
    button.set_editor_property("widget_style", style)


def add_size_box(blueprint, parent, name, width, height, padding=(0.0, 0.0, 0.0, 0.0)):
    box, slot = add_widget(blueprint, unreal.SizeBox, name, parent)
    box.set_editor_property("width_override", width)
    box.set_editor_property("height_override", height)
    set_slot_padding(slot, *padding)
    return box


def add_text(blueprint, parent, name, text, size=32, color=WHITE, padding=(0.0, 0.0, 0.0, 0.0), centered=False):
    widget, slot = add_widget(blueprint, unreal.TextBlock, name, parent)
    configure_text(widget, text, size, color, unreal.TextJustify.CENTER if centered else unreal.TextJustify.LEFT)
    set_slot_padding(slot, *padding)
    return widget


def add_styled_button(blueprint, parent, name, label, textures, width=520.0, height=92.0, padding=(0.0, 8.0, 0.0, 8.0)):
    box = add_size_box(blueprint, parent, f"{name}Size", width, height, padding)
    button, button_slot = add_widget(blueprint, unreal.Button, name, box)
    set_panel_slot_fill(button_slot)
    configure_button_style(button, textures)
    label_widget, label_slot = add_widget(blueprint, unreal.TextBlock, f"{name}Text", button)
    configure_text(label_widget, label, 30, WHITE, unreal.TextJustify.CENTER)
    set_panel_slot_fill(label_slot)
    return button


def add_spacer(blueprint, parent, name, width=1.0, height=20.0):
    spacer, _ = add_widget(blueprint, unreal.Spacer, name, parent)
    spacer.set_editor_property("size", unreal.Vector2D(width, height))
    return spacer


def add_modal_background(blueprint, name):
    root = get_or_add_root(blueprint, unreal.Overlay, name)
    dim, dim_slot = add_widget(blueprint, unreal.Border, f"{name}Dim", root)
    dim.set_brush_color(DIM)
    set_panel_slot_fill(dim_slot)
    return root


def build_session_join(textures):
    blueprint = get_or_create_widget_blueprint("WBP_SessionJoin", load_native_class("HeistSessionJoinWidget"))
    root = add_modal_background(blueprint, "SessionJoinRoot")

    panel_box, panel_slot = add_widget(blueprint, unreal.SizeBox, "SessionJoinPanelSize", root)
    panel_box.set_editor_property("width_override", 720.0)
    panel_box.set_editor_property("height_override", 520.0)
    panel_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    panel_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    panel, panel_content_slot = add_widget(blueprint, unreal.Border, "SessionJoinPanel", panel_box)
    panel.set_brush_color(PANEL)
    panel.set_editor_property("padding", unreal.Margin(48.0, 36.0, 48.0, 36.0))
    set_panel_slot_fill(panel_content_slot)

    column, column_slot = add_widget(blueprint, unreal.VerticalBox, "SessionJoinColumn", panel)
    set_panel_slot_fill(column_slot)
    add_text(blueprint, column, "SessionJoinTitleText", "방 참가", 44, GOLD, centered=True)
    add_text(blueprint, column, "JoinCodeLabelText", "참가 코드", 24, WHITE, padding=(0.0, 28.0, 0.0, 8.0))

    input_box = add_size_box(blueprint, column, "JoinCodeInputSize", 600.0, 62.0)
    join_input, input_slot = add_widget(blueprint, unreal.EditableTextBox, "JoinCodeInput", input_box)
    join_input.set_editor_property("hint_text", "6자리 참가 코드를 입력하세요")
    input_style = join_input.get_editor_property("widget_style")
    text_style = input_style.get_editor_property("text_style")
    text_style.set_editor_property("font", make_font(24))
    input_style.set_editor_property("text_style", text_style)
    join_input.set_editor_property("widget_style", input_style)
    set_panel_slot_fill(input_slot)

    add_styled_button(blueprint, column, "SubmitJoinSessionButton", "참가하기", textures, 600.0, 82.0, (0.0, 18.0, 0.0, 4.0))

    action_row, action_row_slot = add_widget(blueprint, unreal.HorizontalBox, "SessionActionRow", column)
    set_slot_padding(action_row_slot, 0.0, 8.0, 0.0, 0.0)
    add_styled_button(blueprint, action_row, "CancelSessionButton", "요청 취소", textures, 190.0, 68.0, (0.0, 0.0, 8.0, 0.0))
    add_styled_button(blueprint, action_row, "RetrySessionButton", "다시 시도", textures, 190.0, 68.0, (8.0, 0.0, 8.0, 0.0))
    add_styled_button(blueprint, action_row, "JoinCloseButton", "닫기", textures, 190.0, 68.0, (8.0, 0.0, 0.0, 0.0))

    add_text(blueprint, column, "SessionStatusText", "온라인 상태를 확인하는 중...", 20, WHITE, padding=(0.0, 18.0, 0.0, 0.0), centered=True)
    add_text(blueprint, column, "SessionErrorText", "", 20, ERROR, padding=(0.0, 8.0, 0.0, 0.0), centered=True)
    add_text(blueprint, column, "SessionActionHintText", "", 18, MINT, padding=(0.0, 6.0, 0.0, 0.0), centered=True)

    mark_bind_widgets(
        blueprint,
        (
            "JoinCodeInput",
            "SubmitJoinSessionButton",
            "CancelSessionButton",
            "RetrySessionButton",
            "JoinCloseButton",
            "SessionStatusText",
            "SessionErrorText",
            "SessionActionHintText",
        ),
    )

    if not UMG.compile_widget_blueprint(blueprint):
        raise RuntimeError("WBP_SessionJoin compilation failed")
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def add_setting_slider_row(blueprint, parent, row_name, label, slider_name, value_name):
    row, row_slot = add_widget(blueprint, unreal.HorizontalBox, row_name, parent)
    set_slot_padding(row_slot, 0.0, 9.0, 0.0, 9.0)
    label_box = add_size_box(blueprint, row, f"{row_name}LabelSize", 220.0, 48.0)
    add_text(blueprint, label_box, f"{row_name}Label", label, 22, WHITE)
    slider_box = add_size_box(blueprint, row, f"{row_name}SliderSize", 360.0, 48.0, (12.0, 0.0, 12.0, 0.0))
    slider, slider_slot = add_widget(blueprint, unreal.Slider, slider_name, slider_box)
    set_panel_slot_fill(slider_slot)
    value_box = add_size_box(blueprint, row, f"{row_name}ValueSize", 110.0, 48.0)
    add_text(blueprint, value_box, value_name, "0", 22, GOLD, centered=True)


def add_setting_combo_row(blueprint, parent, row_name, label, combo_name):
    row, row_slot = add_widget(blueprint, unreal.HorizontalBox, row_name, parent)
    set_slot_padding(row_slot, 0.0, 9.0, 0.0, 9.0)
    label_box = add_size_box(blueprint, row, f"{row_name}LabelSize", 220.0, 48.0)
    add_text(blueprint, label_box, f"{row_name}Label", label, 22, WHITE)
    combo_box = add_size_box(blueprint, row, f"{row_name}ComboSize", 470.0, 48.0, (12.0, 0.0, 0.0, 0.0))
    combo, combo_slot = add_widget(blueprint, unreal.ComboBoxString, combo_name, combo_box)
    set_panel_slot_fill(combo_slot)


def build_settings(textures):
    blueprint = get_or_create_widget_blueprint("WBP_Settings", load_native_class("HeistSettingsWidget"))
    root = add_modal_background(blueprint, "SettingsRoot")

    panel_box, panel_slot = add_widget(blueprint, unreal.SizeBox, "SettingsPanelSize", root)
    panel_box.set_editor_property("width_override", 860.0)
    panel_box.set_editor_property("height_override", 760.0)
    panel_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    panel_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    panel, panel_content_slot = add_widget(blueprint, unreal.Border, "SettingsPanel", panel_box)
    panel.set_brush_color(PANEL)
    panel.set_editor_property("padding", unreal.Margin(56.0, 38.0, 56.0, 38.0))
    set_panel_slot_fill(panel_content_slot)

    column, column_slot = add_widget(blueprint, unreal.VerticalBox, "SettingsColumn", panel)
    set_panel_slot_fill(column_slot)
    add_text(blueprint, column, "SettingsTitleText", "설정", 44, GOLD, centered=True)
    add_setting_slider_row(blueprint, column, "FOVRow", "시야각", "FOVSlider", "FOVValueText")
    add_setting_slider_row(blueprint, column, "SensitivityRow", "마우스 감도", "MouseSensitivitySlider", "MouseSensitivityValueText")
    add_setting_slider_row(blueprint, column, "VolumeRow", "전체 음량", "MasterVolumeSlider", "MasterVolumeValueText")
    add_setting_combo_row(blueprint, column, "ResolutionRow", "해상도", "ResolutionComboBox")
    add_setting_combo_row(blueprint, column, "WindowModeRow", "화면 모드", "WindowModeComboBox")

    add_text(blueprint, column, "SettingsStatusText", "", 19, MINT, padding=(0.0, 18.0, 0.0, 4.0), centered=True)
    action_row, action_row_slot = add_widget(blueprint, unreal.HorizontalBox, "SettingsActionRow", column)
    set_slot_padding(action_row_slot, 0.0, 12.0, 0.0, 0.0)
    add_styled_button(blueprint, action_row, "RestoreDefaultSettingsButton", "기본값", textures, 230.0, 72.0, (0.0, 0.0, 8.0, 0.0))
    add_styled_button(blueprint, action_row, "ApplySettingsButton", "적용", textures, 230.0, 72.0, (8.0, 0.0, 8.0, 0.0))
    add_styled_button(blueprint, action_row, "SettingsCloseButton", "닫기", textures, 230.0, 72.0, (8.0, 0.0, 0.0, 0.0))

    mark_bind_widgets(
        blueprint,
        (
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
    )

    if not UMG.compile_widget_blueprint(blueprint):
        raise RuntimeError("WBP_Settings compilation failed")
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def build_title_menu(textures, session_blueprint, settings_blueprint):
    blueprint = get_or_create_widget_blueprint("WBP_TitleMenu", load_native_class("HeistTitleMenuWidget"))
    root = get_or_add_root(blueprint, unreal.CanvasPanel, "TitleRootCanvas")

    menu_back, menu_back_slot = add_widget(blueprint, unreal.Border, "MenuBackdrop", root)
    menu_back.set_brush_color(unreal.LinearColor(0.015, 0.012, 0.01, 0.78))
    set_canvas_layout(menu_back_slot, 56.0, 54.0, 620.0, 972.0)

    logo_row, logo_slot = add_widget(blueprint, unreal.HorizontalBox, "LogoRow", root)
    set_canvas_layout(logo_slot, 92.0, 88.0, 520.0, 146.0)
    logo_size = add_size_box(blueprint, logo_row, "LogoImageSize", 132.0, 132.0, (0.0, 0.0, 20.0, 0.0))
    logo_image, logo_image_slot = add_widget(blueprint, unreal.Image, "LogoImage", logo_size)
    logo_brush = unreal.SlateBrush()
    logo_brush.set_editor_property("resource_object", textures["T_TitleLogo_Emblem"])
    logo_brush.set_editor_property("image_size", make_slate_size(128.0, 128.0))
    logo_image.set_editor_property("brush", logo_brush)
    set_panel_slot_fill(logo_image_slot)

    logo_text_column, _ = add_widget(blueprint, unreal.VerticalBox, "LogoTextColumn", logo_row)
    add_text(blueprint, logo_text_column, "GameTitleText", "박물관 하이스트", 45, WHITE, padding=(0.0, 14.0, 0.0, 0.0))
    add_text(blueprint, logo_text_column, "GameSubtitleText", "MUSEUM HEIST", 20, GOLD, padding=(2.0, 3.0, 0.0, 0.0))

    menu_column, menu_slot = add_widget(blueprint, unreal.VerticalBox, "TitleMenuColumn", root)
    set_canvas_layout(menu_slot, 98.0, 278.0, 520.0, 520.0)
    add_styled_button(blueprint, menu_column, "HostSessionButton", "방 만들기", textures)
    add_styled_button(blueprint, menu_column, "JoinSessionButton", "방 참가", textures)
    add_styled_button(blueprint, menu_column, "SettingsButton", "설정", textures)
    add_styled_button(blueprint, menu_column, "QuitGameButton", "게임 종료", textures)

    status, status_slot = add_widget(blueprint, unreal.TextBlock, "SessionStatusText", root)
    configure_text(status, "온라인 서비스를 확인하는 중...", 19, WHITE, unreal.TextJustify.CENTER)
    set_canvas_layout(status_slot, 96.0, 830.0, 524.0, 46.0)
    error, error_slot = add_widget(blueprint, unreal.TextBlock, "SessionErrorText", root)
    configure_text(error, "", 18, ERROR, unreal.TextJustify.CENTER)
    set_canvas_layout(error_slot, 96.0, 878.0, 524.0, 84.0)

    session_class = unreal.load_class(None, f"{WIDGET_ROOT}/WBP_SessionJoin.WBP_SessionJoin_C")
    settings_class = unreal.load_class(None, f"{WIDGET_ROOT}/WBP_Settings.WBP_Settings_C")
    if not session_class or not settings_class:
        raise RuntimeError("Child Widget Blueprint generated classes are unavailable")

    session_widget, session_slot = add_widget(blueprint, session_class, "SessionJoinWidget", root)
    set_fill_canvas(session_slot)
    session_widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    settings_widget, settings_slot = add_widget(blueprint, settings_class, "SettingsWidget", root)
    set_fill_canvas(settings_slot)
    settings_widget.set_visibility(unreal.SlateVisibility.COLLAPSED)

    mark_bind_widgets(
        blueprint,
        (
            "HostSessionButton",
            "JoinSessionButton",
            "SettingsButton",
            "QuitGameButton",
            "SessionStatusText",
            "SessionErrorText",
            "SessionJoinWidget",
            "SettingsWidget",
        ),
    )

    if not UMG.compile_widget_blueprint(blueprint):
        raise RuntimeError("WBP_TitleMenu compilation failed")
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def main():
    unreal.log("[HeistTitleUI] Importing generated textures")
    textures = import_textures()
    unreal.log("[HeistTitleUI] Building WBP_SessionJoin")
    session = build_session_join(textures)
    unreal.log("[HeistTitleUI] Building WBP_Settings")
    settings = build_settings(textures)
    unreal.log("[HeistTitleUI] Rebuilding WBP_TitleMenu")
    build_title_menu(textures, session, settings)
    unreal.EditorAssetLibrary.save_directory(WIDGET_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.EditorAssetLibrary.save_directory(RUNTIME_TEXTURE_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log("[HeistTitleUI] PASS: Title widgets and textures compiled and saved")


if __name__ == "__main__":
    main()
