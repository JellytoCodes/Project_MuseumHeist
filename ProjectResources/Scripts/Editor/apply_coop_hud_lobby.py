"""Apply the approved compact cooperative HUD and scrollable lobby presentation.

The caller owns compilation, saving and the single full-screen ScaleBox fit.
Authored dimensions/font display sizes use integer multiples of four on a
1280x720 canvas. This module never launches the Editor or creates packages.
"""
import unreal

ASSET_NAMES = {
    'WBP_HeistHUD', 'WBP_TeamCard', 'WBP_QuickSlot',
    'WBP_Lobby', 'WBP_LobbyMapCard', 'WBP_LobbyPlayerCard',
    'WBP_InteractionPrompt', 'WBP_ActionProgress', 'WBP_HeistPopupFeedback',
}


def apply(bp, key, ws, helpers):
    if key not in ASSET_NAMES:
        return False
    tools = helpers['TOOLS']
    margin = helpers['margin']
    box = helpers['box']
    pos = helpers['pos']
    font = helpers['font']
    brush = helpers['brush']
    sc = helpers['sc']
    css_color = helpers['css_color']
    white = css_color('#edece7')
    muted = css_color('#a4aaa9')
    clear = unreal.LinearColor(0, 0, 0, 0)
    line = css_color('#313536')
    foreground = css_color('#c4cac6')

    def get(name):
        return ws[name]

    def refresh():
        ws.update({str(item.widget_name): item.widget
                   for item in tools.call_method('GetWidgets', (bp,)).widgets
                   if item.widget})

    def add(cls, name, parent, index=-1):
        if name not in ws:
            ws[name] = tools.call_method(
                'AddWidget', (bp, cls.static_class(), name, parent, index)).widget
        return ws[name]

    def move(widget, parent, index=-1):
        if widget.get_parent() != parent or (index >= 0 and parent.get_child_index(widget) != index):
            widget = tools.call_method('MoveWidget', (bp, widget, parent, index)).widget
            ws[widget.get_name()] = widget
        return widget

    def rect(color, size=(4, 4), radius=0, outline_color=clear, outline_width=0):
        result = brush(color=color, size=size)
        outline = result.get_editor_property('outline_settings')
        outline.set_editor_property('corner_radii', unreal.Vector4(radius, radius, radius, radius))
        outline.set_editor_property('color', sc(outline_color))
        outline.set_editor_property('width', outline_width)
        outline.set_editor_property('use_brush_transparency', False)
        result.set_editor_property('outline_settings', outline)
        return result

    def flat(widget, color=clear):
        widget.set_brush(rect(color))
        widget.set_brush_color(unreal.LinearColor(1, 1, 1, 1))
        widget.set_padding(margin(0))

    def text(widget, size, color=white, bold=False):
        font(widget, size, helpers['BOLD'] if bold else helpers['BODY'])
        widget.set_color_and_opacity(sc(color))
        widget.set_editor_property('justification', unreal.TextJustify.LEFT)
        widget.set_editor_property('min_desired_width', 0)
        widget.set_editor_property('auto_wrap_text', False)
        widget.set_editor_property('wrap_text_at', 0)
        widget.set_shadow_offset(unreal.Vector2D(0, 0))

    def centered(widget, size, color=white, bold=False):
        text(widget, size, color, bold)
        widget.set_editor_property('justification', unreal.TextJustify.CENTER)

    def image(widget, width, height):
        widget.set_brush_size(unreal.Vector2D(width, height))
        widget.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))

    def canvas(parent, name):
        """Wrap a panel's existing content through the Editor ownership API."""
        if name in ws:
            result = get(name)
        else:
            child = parent.get_child_at(0)
            result = tools.call_method('WrapWidgets', (bp, [child], unreal.CanvasPanel.static_class()))[0].widget
            result = tools.call_method('RenameWidget', (bp, result, name)).widget
            refresh()
        result.slot.set_padding(margin(0))
        result.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        result.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        return result

    def place(name, parent, x, y, width, height, anchor=(0, 0), align=(0, 0)):
        widget = move(get(name), parent)
        pos(widget, x, y, width, height, anchor=anchor, align=align)
        widget.set_visibility(unreal.SlateVisibility.VISIBLE)
        return widget

    def collapse(*names):
        for name in names:
            if name in ws:
                get(name).set_visibility(unreal.SlateVisibility.COLLAPSED)

    def simple_button(widget, primary=False, quiet=False, row=False):
        style = widget.get_editor_property('widget_style')
        normal = white if primary else clear if quiet or row else css_color('#212526')
        hover = css_color('#d6d9d6') if primary else css_color('#171b1d' if row else '#323537')
        for state, color in [('normal', normal), ('hovered', hover),
                             ('pressed', css_color('#323537')),
                             ('disabled', css_color('#212526') if primary else clear if row or quiet else css_color('#171b1d'))]:
            style.set_editor_property(state, rect(color))
        for state in ['normal_foreground', 'hovered_foreground']:
            style.set_editor_property(state, sc(css_color('#151a1e') if primary else white))
        style.set_editor_property('pressed_foreground', sc(white))
        style.set_editor_property('disabled_foreground', sc(css_color('#767d83')))
        style.set_editor_property('normal_padding', margin(0))
        style.set_editor_property('pressed_padding', margin(0))
        widget.set_style(style)
        widget.set_background_color(unreal.LinearColor(1, 1, 1, 1))
        for child in widget.get_all_children():
            child.slot.set_padding(margin(0))
            child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            child.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    def rule(name, parent, x, y, width, bottom=False):
        result = add(unreal.Image, name, parent)
        result.set_brush(rect(line))
        pos(result, x, y, width, 4)
        # Paint scale is dimensionless; authored sizes stay multiples of four.
        result.set_render_transform_pivot(unreal.Vector2D(0, 1 if bottom else 0))
        result.set_render_scale(unreal.Vector2D(1, .25))
        result.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return result

    def outline(name, parent, x, y, width, height, radius=0, color=muted):
        result = add(unreal.Image, name, parent)
        result.set_brush(rect(clear, (width, height), radius, color, 1))
        pos(result, x, y, width, height)
        result.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return result

    def profile_placeholder(name, parent, x, y, size):
        background = add(unreal.Image, name + 'Background', parent)
        # Lobby has a fixed backdrop: store the CSS composite itself. HUD
        # overlays the world, so retain transparency at a subdued paint alpha.
        background.set_brush(rect(css_color('#171b1d') if size == 48
                                  else css_color('#ffffff', .008)))
        pos(background, x, y, size, size)
        background.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        # Existing empty-profile texture is a thief face. Two simple outlines
        # provide a neutral placeholder without creating a texture package.
        if size == 24:
            outline(name + 'Head', parent, x + 8, y + 4, 8, 8, 4)
            outline(name + 'Body', parent, x + 4, y + 16, 16, 8, 4)
        else:
            outline(name + 'Head', parent, x + 16, y + 8, 16, 16, 8)
            outline(name + 'Body', parent, x + 8, y + 28, 32, 12, 8)

    bp.modify()

    if key == 'WBP_HeistHUD':
        mission = get('MissionPanel'); flat(mission)
        pos(mission, 32, 32, 248, 92)
        content = canvas(mission, 'CoopMissionCanvas')
        collapse('MissionColumn', 'RequiredTargetLabelText')
        place('MissionTitleText', content, 0, 4, 80, 20); text(get('MissionTitleText'), 12, muted)
        place('MissionTimeText', content, 144, 0, 104, 28); text(get('MissionTimeText'), 24)
        get('MissionTimeText').set_editor_property('justification', unreal.TextJustify.RIGHT)
        target_icon = add(unreal.TextBlock, 'CoopRequiredTargetIcon', content)
        target_icon.set_text('◎'); centered(target_icon, 20); pos(target_icon, 0, 36, 20, 28)
        place('RequiredTargetNameText', content, 28, 36, 220, 28); text(get('RequiredTargetNameText'), 20)
        get('RequiredTargetNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        get('RequiredTargetNameText').set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        value_label = add(unreal.TextBlock, 'ContractValueLabel', content)
        value_label.set_text('운반·확보'); text(value_label, 12, muted); pos(value_label, 0, 76, 64, 20)
        place('ContractValueText', content, 72, 72, 176, 24); text(get('ContractValueText'), 16)

        alert = get('AlertMeterPanel'); flat(alert); pos(alert, 32, 136, 248, 48)
        content = canvas(alert, 'CoopAlertCanvas'); collapse('AlertColumn')
        place('AlertTitleText', content, 0, 0, 40, 20); text(get('AlertTitleText'), 12, muted)
        place('AlertStarRow', content, 48, 8, 148, 4)
        value = add(unreal.TextBlock, 'AlertValueText', content)
        text(value, 12, muted); value.set_editor_property('justification', unreal.TextJustify.RIGHT)
        pos(value, 204, 0, 44, 20)
        for index in range(1, 11):
            name = f'AlertStar{index:02d}'; segment = f'CoopAlertSegment{index:02d}'
            if segment not in ws:
                overlay = tools.call_method('WrapWidgets', (bp, [get(name)], unreal.Overlay.static_class()))[0].widget
                tools.call_method('RenameWidget', (bp, overlay, segment)); refresh()
            overlay = get(segment)
            overlay.slot.set_size(unreal.SlateChildSize(1, unreal.SlateSizeRule.FILL))
            overlay.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            overlay.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            overlay.slot.set_padding(margin(0, 0, 4 if index < 10 else 0, 0))
            background = add(unreal.Image, f'CoopAlertEmpty{index:02d}', overlay, 0)
            background.set_brush(rect(css_color('#ffffff', .04)))
            background.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            background.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            background.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            get(name).set_brush(rect(foreground))
            get(name).set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
            get(name).slot.set_padding(margin(0))
            get(name).slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            get(name).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            get(name).set_render_transform_pivot(unreal.Vector2D(0, .5))
        place('AlertEventText', content, 0, 28, 248, 20); text(get('AlertEventText'), 12, muted)
        get('AlertEventText').set_editor_property('auto_wrap_text', True)
        get('AlertEventText').set_editor_property('wrap_text_at', 248)
        cdo = unreal.get_default_object(bp.generated_class()); cdo.modify()
        cdo.set_editor_property('bUseRectangularAlertIndicators', True)
        pos(get('TeamCardsPanel'), 32, 0, 232, 184, anchor=(0, .5), align=(0, .5))
        for index in range(1, 5):
            slot = get(f'TeamCard{index}').slot
            slot.set_padding(margin(0, 0, 0, 8 if index < 4 else 0))
            slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
            slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)

        crosshair = get('CrosshairContainer')
        pos(crosshair, 0, 0, 8, 8, anchor=(.5, .5), align=(.5, .5))
        for name, width, fill, edge in [
                ('CrosshairIdleIndicator', 4, css_color('#e7e9e3', 153 / 255), clear),
                ('CrosshairFocusIndicator', 8, clear, white)]:
            if isinstance(get(name), unreal.TextBlock):
                legacy = tools.call_method('RenameWidget', (bp, get(name), 'CoopLegacy' + name)).widget
                legacy.set_visibility(unreal.SlateVisibility.COLLAPSED)
                ws.pop(name); refresh()
            bounds = add(unreal.SizeBox, name + 'Bounds', crosshair); box(bounds, width, width)
            bounds.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            bounds.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            indicator = add(unreal.Image, name, bounds)
            indicator.set_brush(rect(fill, (width, width), width // 2, edge, 1 if width == 8 else 0))
            indicator.set_visibility(unreal.SlateVisibility.COLLAPSED if width == 8 else unreal.SlateVisibility.HIT_TEST_INVISIBLE)

        gear = get('HUDQuickSlotRow')
        pos(gear, -32, -32, 280, 32, anchor=(1, 1), align=(1, 1))
        collapse('HUDQuickSlot2', 'HUDQuickSlot3', 'Spacer_1', 'CoopInventoryGap')
        flash = move(get('FlashlightControlRow'), gear, 0)
        flash.slot.set_padding(margin(0, 0, 16, 0))
        flash.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
        flash.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        for name, width in [('FlashlightIcon_ImageBounds', 24), ('FlashlightKeyText_ImageBounds', 24), ('FlashlightStatusText_ImageBounds', 28)]:
            box(get(name), width, 32)
            get(name).slot.set_padding(margin(0, 0, 8 if name != 'FlashlightStatusText_ImageBounds' else 0, 0))
        image(get('FlashlightIcon'), 24, 24)
        get('FlashlightIcon').set_render_transform_pivot(unreal.Vector2D(.5, .5))
        get('FlashlightIcon').set_render_transform_angle(-45)
        get('FlashlightIcon').set_render_scale(unreal.Vector2D(1.4, 1.4))
        centered(get('FlashlightKeyText'), 12, muted); centered(get('FlashlightStatusText'), 12, muted)
        coin = move(get('HUDQuickSlot1'), gear, 1)
        coin.slot.set_padding(margin(0, 0, 16, 0))
        coin.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
        coin.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        bag = move(get('SizeBox_0'), gear, 2)
        bag.slot.set_padding(margin(0)); bag.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
        bag.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER); box(bag, 76, 32)
        flat(get('Border_0')); content = canvas(get('Border_0'), 'CoopInventoryCanvas')
        collapse('InventoryShortcutPanel')
        place('InventoryShortcutIcon', content, 0, 4, 24, 24); image(get('InventoryShortcutIcon'), 24, 24)
        place('InventoryShortcutKeyText', content, 32, 8, 44, 20)
        get('InventoryShortcutKeyText').set_text('[TAB]'); centered(get('InventoryShortcutKeyText'), 12, muted)
        pos(get('InteractionPromptWidget'), 0, 40, 448, 96, anchor=(.5, .5), align=(.5, 0))
        pos(get('ActionProgressWidget'), 0, 96, 256, 64, anchor=(.5, .5), align=(.5, 0))
        pos(get('PopupFeedbackLayer'), 0, 176, 256, 0, anchor=(.5, .5), align=(.5, 0))

    elif key == 'WBP_TeamCard':
        box(get('SizeBox_0'), 232, 40); flat(get('TeamCardBorder'))
        row = canvas(get('TeamCardBorder'), 'CoopTeamCanvas'); collapse('TeamCardRow')
        profile_placeholder('CoopTeamProfile', row, 0, 8, 24)
        size = place('ProfileImage_AspectFit_ImageBounds', row, 0, 8, 24, 24); box(size, 24, 24)
        image(get('ProfileImage'), 24, 24); get('ProfileImage').set_visibility(unreal.SlateVisibility.HIDDEN)
        marker = place('PlayerColorMarker', row, 0, 24, 4, 8)
        marker.set_brush(rect(unreal.LinearColor(1, 1, 1, 1), (4, 8)))
        marker.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        place('PlayerNameText', row, 32, 0, 176, 24); text(get('PlayerNameText'), 16)
        get('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        get('PlayerNameText').set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        place('StatusIcon', row, 32, 24, 12, 12); image(get('StatusIcon'), 12, 12)
        place('StatusText', row, 48, 20, 160, 20); text(get('StatusText'), 12, css_color('#929b98'))
        place('MicStatusImage', row, 216, 12, 12, 12); image(get('MicStatusImage'), 12, 12)
        cdo = unreal.get_default_object(bp.generated_class()); cdo.modify(); cdo.set_editor_property('DefaultProfileTexture', None)

    elif key == 'WBP_QuickSlot':
        box(get('SizeBox_0'), 80, 32); flat(get('SlotBackground'))
        row = canvas(get('SlotBackground'), 'CoopQuickSlotCanvas'); collapse('SlotContent')
        place('PlaceholderIcon_AspectFit', row, 0, 4, 24, 24); image(get('PlaceholderIcon'), 24, 24)
        # Native keeps assigning the confirmed icon and opacity; render opacity
        # hides its ornate source while retaining the existing binding.
        get('PlaceholderIcon').set_render_opacity(0)
        coin = add(unreal.Image, 'CoopCoinRing', row)
        coin.set_brush(rect(clear, (24, 24), 12, white, 2)); pos(coin, 0, 4, 24, 24)
        coin.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        coin_symbol = add(unreal.TextBlock, 'CoopCoinSymbol', row)
        coin_symbol.set_text('$'); centered(coin_symbol, 20); pos(coin_symbol, 0, 4, 24, 24)
        coin_symbol.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        place('CountText', row, 32, 8, 16, 20); text(get('CountText'), 12)
        place('KeyLabelText', row, 52, 8, 28, 20); text(get('KeyLabelText'), 12, muted)

    elif key == 'WBP_LobbyMapCard':
        box(get('MapCardSizeBox'), 672, 112); box(get('MapThumbnailSize'), 672, 112)
        simple_button(get('SelectMapButton'), row=True)
        row = canvas(get('SelectMapButton'), 'CoopMapRowCanvas'); collapse('MapCardOverlay')
        selected_background = add(unreal.Image, 'SelectionBackground', row)
        selected_background.set_brush(rect(css_color('#212526')))
        pos(selected_background, 0, 0, 672, 112); selected_background.slot.set_z_order(-1)
        selected_background.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE); selected_background.set_render_opacity(0)
        name = get('MapNameText'); text(name, 20)
        if 'CoopMapNameCenter' not in ws:
            place('MapNameText', row, 16, 0, 456, 112)
            center = tools.call_method('WrapWidgets', (bp, [name], unreal.Border.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, center, 'CoopMapNameCenter')); refresh()
        center = get('CoopMapNameCenter'); move(name, center); flat(center); pos(center, 16, 0, 456, 112)
        center.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
        center.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
        name.slot.set_padding(margin(0)); name.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        name.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        background = add(unreal.Border, 'CoopMapThumbnailBackground', row)
        flat(background, css_color('#202725')); pos(background, 496, 12, 160, 88)
        thumbnail = add(unreal.Image, 'MapThumbnailImage', row); image(thumbnail, 160, 88)
        if 'CoopMapThumbnailFit' not in ws:
            pos(thumbnail, 496, 12, 160, 88)
            fit = tools.call_method('WrapWidgets', (bp, [thumbnail], unreal.ScaleBox.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, fit, 'CoopMapThumbnailFit')); refresh()
        fit = get('CoopMapThumbnailFit'); pos(fit, 496, 12, 160, 88)
        fit.set_stretch(unreal.Stretch.SCALE_TO_FIT); fit.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        question = add(unreal.TextBlock, 'RandomQuestionText', row)
        question.set_text('?'); centered(question, 40); pos(question, 496, 32, 160, 48)
        question.set_visibility(unreal.SlateVisibility.COLLAPSED)
        selected = place('SelectedCheckImage', row, 448, 44, 24, 24)
        selected.set_brush_from_texture(unreal.load_asset('/Game/Assets/UI/Common/Monochrome/T_UIIcon_ReadyCheck_Monochrome'), False)
        image(selected, 24, 24); selected.set_visibility(unreal.SlateVisibility.HIDDEN)
        rule('CoopMapRowRule', row, 0, 108, 672, bottom=True)
        cdo = unreal.get_default_object(bp.generated_class()); cdo.modify()
        # Exact CSS saturation needs a UI material, absent under no-new-assets.
        cdo.set_editor_property('ThumbnailTint', css_color('#a3a3a3'))
        cdo.set_editor_property('SelectedThumbnailTint', css_color('#d6d6d6'))

    elif key == 'WBP_LobbyPlayerCard':
        box(get('PlayerCardSizeBox'), 480, 72); flat(get('PlayerCardBorder'))
        row = canvas(get('PlayerCardBorder'), 'CoopLobbyPlayerCanvas')
        collapse('PlayerCardContent', 'PlayerSlotText', 'SizeBox_0')
        profile_placeholder('CoopLobbyProfile', row, 0, 12, 48)
        place('ProfileImageSizeBox', row, 0, 12, 48, 48); box(get('ProfileImageSizeBox'), 48, 48)
        image(get('ProfileImage'), 48, 48); get('ProfileImage').set_visibility(unreal.SlateVisibility.HIDDEN)
        place('PlayerNameText', row, 60, 12, 420, 24); text(get('PlayerNameText'), 16)
        get('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        get('PlayerNameText').set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        ready = add(unreal.TextBlock, 'ReadyStatusText', row)
        ready.set_text('준비 대기'); text(ready, 12); pos(ready, 80, 40, 400, 20)
        check = place('ReadyCheckImage', row, 60, 40, 16, 16)
        check.set_brush_from_texture(unreal.load_asset('/Game/Assets/UI/Common/Monochrome/T_UIIcon_ReadyCheck_Monochrome'), False)
        image(check, 16, 16)
        check.set_visibility(unreal.SlateVisibility.HIDDEN)
        rule('CoopLobbyPlayerRule', row, 0, 0, 480)
        cdo = unreal.get_default_object(bp.generated_class()); cdo.modify(); cdo.set_editor_property('DefaultProfileTexture', None)

    elif key == 'WBP_Lobby':
        root = get('LobbyRootBorder')
        root.set_brush(rect(css_color('#101416'), (1280, 720)))
        root.set_brush_color(unreal.LinearColor(1, 1, 1, 1)); root.set_padding(margin(0))
        root.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
        root.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
        layout = canvas(root, 'CoopLobbyCanvas')
        collapse('LobbyContent_ImageBounds', 'LobbyHeaderRow', 'LobbyLogoImage', 'BackdropImage')
        shade = add(unreal.Image, 'CoopLobbyShade', layout)
        shade.set_brush(rect(css_color('#101416'))); pos(shade, 0, 0, 1280, 720)
        shade.slot.set_z_order(-100); shade.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        kicker = add(unreal.TextBlock, 'CoopLobbyKicker', layout)
        kicker.set_text('MUSEUM HEIST'); text(kicker, 12, muted); pos(kicker, 32, 32, 256, 16)
        place('LobbyTitleText', layout, 32, 52, 448, 36)
        get('LobbyTitleText').set_text('작전 대기실'); text(get('LobbyTitleText'), 28)
        place('JoinCodeGroup', layout, 852, 36, 148, 48)
        text(get('JoinCodeLabel'), 16); text(get('JoinCodeText'), 20)
        for widget in get('JoinCodeGroup').get_all_children():
            widget.slot.set_padding(margin(0)); widget.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        copy_size = place('SizeBox_1', layout, 1016, 36, 56, 48); box(copy_size, 56, 48)
        simple_button(get('CopyJoinCodeButton'), quiet=True); collapse('CopyJoinCodeLabel')
        copy_canvas = canvas(get('CopyJoinCodeButton'), 'CoopLobbyCopyCanvas')
        icon = add(unreal.Image, 'CoopLobbyCopyIcon', copy_canvas)
        icon.set_brush_from_texture(unreal.load_asset('/Game/Assets/UI/Common/Monochrome/T_UIIcon_Copy_Monochrome'), True)
        image(icon, 24, 24); icon.set_color_and_opacity(white); pos(icon, 16, 12, 24, 24)
        place('PlayerCountSize', layout, 1088, 36, 32, 48); box(get('PlayerCountSize'), 32, 48)
        flat(get('PlayerCountBorder')); centered(get('PlayerCountText'), 16, muted)
        place('LeaveSessionSize', layout, 1136, 36, 112, 48); box(get('LeaveSessionSize'), 112, 48)
        simple_button(get('LeaveSessionButton'), quiet=True)
        leave_canvas = canvas(get('LeaveSessionButton'), 'CoopLobbyLeaveCanvas')
        collapse('CoopLobbyLeaveIcon')
        for name, x, y, width, height, scale, angle in [
                ('DoorSide', 16, 16, 4, 16, (.25, 1), 0),
                ('DoorTop', 16, 16, 12, 4, (1, .25), 0),
                ('DoorBottom', 16, 32, 12, 4, (1, .25), 0),
                ('ArrowLine', 24, 24, 12, 4, (1, .25), 0),
                ('ArrowTop', 32, 20, 4, 8, (.25, 1), -45),
                ('ArrowBottom', 32, 24, 4, 8, (.25, 1), 45)]:
            stroke = add(unreal.Image, 'CoopLobbyLeave' + name, leave_canvas)
            stroke.set_brush(rect(white)); pos(stroke, x, y, width, height)
            stroke.set_render_transform_pivot(unreal.Vector2D(0, 0))
            stroke.set_render_scale(unreal.Vector2D(*scale)); stroke.set_render_transform_angle(angle)
            stroke.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        place('LeaveSessionButtonLabel', leave_canvas, 48, 12, 56, 24)
        get('LeaveSessionButtonLabel').set_text('나가기'); centered(get('LeaveSessionButtonLabel'), 16)
        rule('CoopLobbyHeaderRule', layout, 32, 104, 1216)
        place('MapSection', layout, 32, 180, 688, 380)
        place('MapSectionTitle', layout, 32, 180, 480, 28)
        get('MapSectionTitle').set_text('침입 장소'); text(get('MapSectionTitle'), 20)
        selected = add(unreal.TextBlock, 'SelectedMapText', layout)
        selected.set_text('무작위'); text(selected, 16, muted)
        selected.set_editor_property('justification', unreal.TextJustify.RIGHT); pos(selected, 576, 180, 144, 28)
        collapse('MapSectionTitle_ImageBounds')
        scroll = get('MapHorizontalScrollBox')
        scroll.set_editor_property('orientation', unreal.Orientation.ORIENT_VERTICAL)
        scroll.set_editor_property('always_show_scrollbar', True); scroll.set_scroll_bar_visibility(unreal.SlateVisibility.VISIBLE)
        scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(4, 4))
        scroll.set_editor_property('scrollbar_padding', margin(12, 0, 0, 0))
        scroll.set_editor_property('consume_mouse_wheel', unreal.ConsumeMouseWheel.WHEN_SCROLLING_POSSIBLE)
        if 'CoopMapListSize' not in ws:
            size = tools.call_method('WrapWidgets', (bp, [scroll], unreal.SizeBox.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, size, 'CoopMapListSize')); refresh()
        size = place('CoopMapListSize', layout, 32, 224, 688, 336); box(size, 688, 336)
        scroll.slot.set_padding(margin(0))
        for code in ['Random', 'M01', 'M02', 'M03']:
            card = get('Map' + code + 'Card'); card.slot.set_padding(margin(0))
            card.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
            if code != 'Random':
                thumbnail = unreal.load_asset('/Game/Assets/UI/Catalogue/T_Catalogue_Map_' + code)
                assert thumbnail, 'Missing existing map thumbnail: ' + code
                card.set_editor_property('MapThumbnail', thumbnail)
        rule('CoopLobbyMapTopRule', layout, 32, 224, 672)
        title = add(unreal.TextBlock, 'CoopCrewSectionTitle', layout)
        title.set_text('참가 플레이어'); text(title, 20); pos(title, 768, 180, 480, 28)
        crew = add(unreal.VerticalBox, 'CoopLobbyCrewList', layout); pos(crew, 768, 224, 480, 312)
        for index in range(1, 5):
            card = move(get(f'PlayerCard{index}'), crew)
            card.slot.set_padding(margin(0, 0, 0, 8 if index < 4 else 0))
            card.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
            card.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        rule('CoopLobbyFooterRule', layout, 32, 624, 1216)
        esc = add(unreal.TextBlock, 'CoopLobbyEscapeKey', layout)
        esc.set_text('ESC'); centered(esc, 12, muted); pos(esc, 32, 652, 36, 28)
        outline('CoopLobbyEscapeFrame', layout, 32, 652, 36, 28, color=muted)
        label = add(unreal.TextBlock, 'CoopLobbyEscapeLabel', layout)
        label.set_text('나가기'); text(label, 16, muted); pos(label, 76, 652, 96, 28)
        ready = add(unreal.Button, 'GlobalReadyButton', layout); pos(ready, 1000, 640, 116, 48); simple_button(ready)
        label = add(unreal.TextBlock, 'GlobalReadyButtonLabel', ready)
        label.set_text('준비'); centered(label, 16)
        label.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
        label.slot.set_padding(margin(0)); label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        start = place('SizeBox_0', layout, 1132, 640, 116, 48); box(start, 116, 48)
        simple_button(get('StartGameButton'), True); centered(get('StartGameButtonLabel'), 16)
        get('StartGameButtonLabel').set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))

    elif key == 'WBP_ActionProgress':
        bounds = get('SizeBox_0')
        bounds.set_width_override(256); bounds.clear_height_override()
        bounds.set_min_desired_height(64)
        container = get('ActionProgressContainer'); flat(container, css_color('#070b0f', 153 / 255))
        container.set_padding(margin(16, 12, 16, 12))
        title = get('ActionTypeText'); centered(title, 16)
        title.set_editor_property('auto_wrap_text', False); title.set_editor_property('wrap_text_at', 0)
        title.set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        if 'CoopActionTitleBounds' not in ws:
            title_bounds = tools.call_method('WrapWidgets', (bp, [title], unreal.SizeBox.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, title_bounds, 'CoopActionTitleBounds')); refresh()
        title_bounds = get('CoopActionTitleBounds'); box(title_bounds, 224, 24)
        title_bounds.slot.set_padding(margin(0)); title_bounds.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
        title.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        title.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        if 'Spacer_55' in ws:
            get('Spacer_55').set_size(unreal.Vector2D(4, 8))
        progress = get('ActionProgressBar')
        if 'CoopActionTrackBounds' not in ws:
            track = tools.call_method('WrapWidgets', (bp, [progress], unreal.SizeBox.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, track, 'CoopActionTrackBounds')); refresh()
        track = get('CoopActionTrackBounds'); box(track, 160, 4)
        track.slot.set_padding(margin(0)); track.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))
        track.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        track.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        style = progress.get_editor_property('widget_style')
        style.set_editor_property('background_image', rect(css_color('#323638')))
        style.set_editor_property('fill_image', rect(unreal.LinearColor(1, 1, 1, 1)))
        progress.set_editor_property('widget_style', style); progress.set_fill_color_and_opacity(white)
        progress.set_editor_property('border_padding', unreal.Vector2D(0, 0))
        for name in ['ActionRemainingText', 'CancelHintText']:
            if name in ws:
                detail = get(name); centered(detail, 12, muted)
                detail.set_editor_property('auto_wrap_text', True); detail.set_editor_property('wrap_text_at', 224)
                detail.slot.set_padding(margin(0, 4, 0, 0))
                if isinstance(detail.slot, (unreal.VerticalBoxSlot, unreal.HorizontalBoxSlot)):
                    detail.slot.set_size(unreal.SlateChildSize(0, unreal.SlateSizeRule.AUTOMATIC))

    elif key == 'WBP_HeistPopupFeedback':
        bounds = get('PopupContainer_ImageBounds')
        bounds.set_width_override(256); bounds.clear_height_override(); bounds.set_min_desired_height(48)
        container = get('PopupContainer'); flat(container, css_color('#070b0f', 153 / 255))
        container.set_padding(margin(16, 12, 16, 12))
        popup = get('PopupText'); centered(popup, 16)
        popup.set_editor_property('auto_wrap_text', True); popup.set_editor_property('wrap_text_at', 224)
        popup.slot.set_padding(margin(0))
        popup.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        popup.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    elif key == 'WBP_InteractionPrompt':
        container = get('InteractionPromptContainer'); flat(container, css_color('#070b0f', 102 / 255))
        container.set_visibility(unreal.SlateVisibility.COLLAPSED)
        container.set_padding(margin(8, 4, 8, 4))
        bounds = get('InteractionPromptContainer_ImageBounds')
        bounds.clear_width_override(); bounds.clear_height_override()
        bounds.set_min_desired_width(0); bounds.set_min_desired_height(32)
        bounds.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        bounds.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        column = get('PromptColumn'); column.slot.set_padding(margin(0))
        main = add(unreal.HorizontalBox, 'CoopInteractionRow', column, 0)
        main.slot.set_padding(margin(0)); main.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        frame = add(unreal.Border, 'InteractionKeyContainer', main, 0)
        frame.set_brush(rect(clear, (24, 24), 0, white, 1)); frame.set_padding(margin(0))
        frame.slot.set_padding(margin(0, 0, 8, 0)); frame.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        size = add(unreal.SizeBox, 'CoopInteractionKeySize', frame); box(size, 24, 24)
        key_text = move(add(unreal.TextBlock, 'KeyText', size), size); key_text.set_text('E'); centered(key_text, 12)
        key_text.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        key_text.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        target = move(get('TargetText'), main); text(target, 16); target.slot.set_padding(margin(0))
        target.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        target.set_editor_property('auto_wrap_text', True); target.set_editor_property('wrap_text_at', 384)
        availability = move(get('AvailabilityText'), column); text(availability, 12, muted)
        availability.set_editor_property('auto_wrap_text', True); availability.set_editor_property('wrap_text_at', 400)
        availability.slot.set_padding(margin(0, 4, 0, 0))
        availability.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        availability.set_visibility(unreal.SlateVisibility.COLLAPSED)
        cdo = unreal.get_default_object(bp.generated_class()); cdo.modify(); cdo.set_editor_property('bUseCompactPrompt', True)

    refresh()
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'CatalogueFontDisplayDPI', '72')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'CoopUIPresentation', '20261001')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'CoopUIAuthorResolution', '1280x720')
    return True
