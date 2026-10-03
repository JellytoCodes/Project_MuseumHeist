"""Apply the approved image-component composition to existing WidgetBlueprints.

The caller creates the shared visual assets and owns compile/save/render. This
module only edits presentation subtrees, preserving bound widgets and runtime
containers. It does not rerun the earlier Canvas layout or create packages.
"""
import unreal


ASSET_NAMES = {
    'WBP_HeistHUD', 'WBP_TeamCard', 'WBP_QuickSlot',
    'WBP_InteractionPrompt', 'WBP_Lobby', 'WBP_LobbyMapCard',
    'WBP_LobbyPlayerCard', 'WBP_HeistForgery', 'WBP_Settings',
    'WBP_Inventory', 'WBP_InventoryFrame', 'WBP_InventorySlot', 'WBP_TitleMenu', 'WBP_Result',
    'WBP_ResultPlayerRow', 'WBP_ResultReplicaCard',
}


def apply(bp, key, ws, helpers):
    if key not in ASSET_NAMES:
        return False
    tools = helpers['TOOLS']
    margin, box, pos, font = (helpers[n] for n in ('margin', 'box', 'pos', 'font'))
    frame, keycap, icon, button = (helpers[n] for n in ('frame', 'keycap', 'icon', 'button'))
    white = helpers['css_color']('#edece7')
    muted = helpers['css_color']('#a4aaa9')
    clear = unreal.LinearColor(0, 0, 0, 0)
    visible, collapsed = unreal.SlateVisibility.VISIBLE, unreal.SlateVisibility.COLLAPSED
    left = unreal.HorizontalAlignment.H_ALIGN_LEFT
    center = unreal.HorizontalAlignment.H_ALIGN_CENTER
    fill = unreal.HorizontalAlignment.H_ALIGN_FILL
    middle = unreal.VerticalAlignment.V_ALIGN_CENTER

    def refresh():
        ws.update({str(i.widget_name): i.widget
                   for i in tools.call_method('GetWidgets', (bp,)).widgets if i.widget})

    def w(name):
        return ws[name]

    def move(widget, parent, index=-1):
        if widget.get_parent() != parent or (index >= 0 and parent.get_child_index(widget) != index):
            original_name = widget.get_name()
            widget = tools.call_method('MoveWidget', (bp, widget, parent, index)).widget
            assert widget, 'MoveWidget failed: ' + original_name + ' -> ' + parent.get_name()
            ws[widget.get_name()] = widget
        return widget

    def node(cls, name, parent, index=-1):
        if name not in ws:
            ws[name] = tools.call_method('AddWidget', (bp, cls.static_class(), name, parent, index)).widget
        return move(w(name), parent, index)

    def align(widget, padding=(0, 0, 0, 0), horizontal=fill, vertical=middle, weight=None):
        slot = widget.slot
        if slot is None or isinstance(slot, unreal.CanvasPanelSlot):
            return widget
        slot.set_padding(margin(*padding))
        slot.set_horizontal_alignment(horizontal)
        slot.set_vertical_alignment(vertical)
        if weight is not None and isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot)):
            slot.set_size(unreal.SlateChildSize(weight, unreal.SlateSizeRule.FILL))
        return widget

    def container(parent, cls, name):
        # Single-content widgets must retain their old child until its bound
        # descendants have been moved. Wrap it via the Editor ownership API.
        # A previous application can put this local group in a shared NamedSlot.
        # Keep that ownership instead of pulling it out of the shared wrapper.
        if name in ws:
            return align(w(name), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        if isinstance(parent, (unreal.Border, unreal.SizeBox, unreal.Button, unreal.ScaleBox)) and parent.get_children_count():
            old = parent.get_child_at(0)
            wrapper = tools.call_method('WrapWidgets', (bp, [old], cls.static_class()))[0].widget
            ws[name] = tools.call_method('RenameWidget', (bp, wrapper, name)).widget
            refresh()
        else:
            node(cls, name, parent)
        return align(w(name), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)

    def cell(widget, parent, width=None, height=32, padding=(0, 0, 0, 0), weight=None):
        result = node(unreal.SizeBox, widget.get_name() + '_CompositionCell', parent)
        if width is None:
            result.clear_width_override()
        else:
            result.set_width_override(width)
        result.set_height_override(height)
        align(result, padding, weight=weight)
        move(widget, result)
        align(widget, vertical=unreal.VerticalAlignment.V_ALIGN_FILL if isinstance(widget, (unreal.PanelWidget, unreal.UserWidget)) else middle)
        return result

    def text(widget, size=16, value=None, muted_text=False, right=False, centered=False):
        font(widget, size, helpers['BODY'])
        if value is not None:
            widget.set_text(value)
        widget.set_color_and_opacity(helpers['sc'](muted if muted_text else white))
        widget.set_editor_property('min_desired_width', 0)
        widget.set_editor_property('auto_wrap_text', False)
        widget.set_editor_property('wrap_text_at', 0)
        widget.set_editor_property('justification', unreal.TextJustify.RIGHT if right else
                                   unreal.TextJustify.CENTER if centered else unreal.TextJustify.LEFT)
        widget.set_shadow_offset(unreal.Vector2D(0, 0))
        widget.set_shadow_color_and_opacity(clear)
        info = widget.get_editor_property('font')
        info.set_editor_property('font_material', None)
        outline = info.get_editor_property('outline_settings')
        outline.set_editor_property('outline_size', 0)
        outline.set_editor_property('outline_material', None)
        info.set_editor_property('outline_settings', outline)
        widget.set_editor_property('font', info)
        widget.set_visibility(visible)
        return widget

    def label(name, parent, value, size=16, muted_text=False):
        return text(node(unreal.TextBlock, name, parent), size, value, muted_text)

    def glyph(name, parent, kind, size=24):
        result = node(unreal.Image, name, parent)
        icon(result, kind, size, size)
        result.set_render_transform_angle(0)
        result.set_render_scale(unreal.Vector2D(1, 1))
        result.set_render_opacity(1)
        result.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
        result.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return result

    def hide(*names):
        for name in names:
            if name in ws:
                w(name).set_visibility(collapsed)

    def flat(widget):
        if isinstance(widget, unreal.Border):
            widget.set_brush_color(clear)
            widget.set_padding(margin(0))
            widget.set_horizontal_alignment(fill)
            widget.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            if widget.get_children_count():
                align(widget.get_child_at(0), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)

    def place(widget, parent, x, y, width, height, anchor=(0, 0), alignment=(0, 0)):
        move(widget, parent)
        pos(widget, x, y, width, height, anchor=anchor, align=alignment)
        return widget

    def key_widget(text_widget, parent, name, wide=False, value=None):
        text(text_widget, 12, value, muted_text=True, centered=True)
        if name in ws:
            # Existing generated labels can be moved out by label() on reapply;
            # restore them to this shared wrapper's parent-owned NamedSlot.
            result = move(w(name), parent)
            result = keycap(bp, text_widget, name, wide=wide)
            refresh()
            return align(result, horizontal=left)
        move(text_widget, parent)
        result = keycap(bp, text_widget, name, wide=wide)
        refresh()
        return align(result, horizontal=left)

    def sized_row(parent, name, height, padding=(0, 0, 0, 0)):
        bounds = node(unreal.SizeBox, name + 'Size', parent)
        bounds.clear_width_override()
        bounds.set_height_override(height)
        align(bounds, padding)
        return container(bounds, unreal.HorizontalBox, name)

    def footer(parent, name, x=32, y=640, width=1216):
        result = node(unreal.HorizontalBox, name, parent)
        place(result, parent, x, y, width, 48)
        return result

    def stretch(parent, name):
        spacer = node(unreal.Spacer, name, parent)
        spacer.set_size(unreal.Vector2D(4, 4))
        align(spacer, weight=1)
        return spacer

    if key == 'WBP_HeistHUD':
        hide('MissionColumn', 'CoopMissionCanvas', 'CoopRequiredTargetIcon',
             'AlertColumn', 'CoopAlertCanvas', 'InventoryShortcutPanel')
        flat(w('MissionPanel'))
        if 'CompositionMissionBounds' not in ws:
            bounds = tools.call_method('WrapWidgets', (bp, [w('MissionPanel')], unreal.SizeBox.static_class()))[0].widget
            tools.call_method('RenameWidget', (bp, bounds, 'CompositionMissionBounds'))
            refresh()
        bounds = w('CompositionMissionBounds')
        bounds.set_width_override(320)
        bounds.clear_height_override()
        place(bounds, w('HUDCanvas'), 32, 32, 320, 0)
        bounds.slot.set_auto_size(True)
        align(w('MissionPanel'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        mission = container(w('MissionPanel'), unreal.VerticalBox, 'CompositionMissionColumn')
        row = sized_row(mission, 'CompositionMissionTimeRow', 28, (0, 0, 0, 12))
        cell(text(w('MissionTitleText'), 12, muted_text=True), row, height=28, weight=1)
        cell(text(w('MissionTimeText'), 24, right=True), row, 104, 28)
        row = sized_row(mission, 'CompositionTargetRow', 28, (0, 0, 0, 12))
        cell(glyph('CompositionTargetIcon', row, 'painting'), row, 24, 28, (0, 0, 8, 0))
        cell(text(w('RequiredTargetNameText'), 20), row, height=28, weight=1)
        w('RequiredTargetNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        row = sized_row(mission, 'CompositionValueRow', 24, (0, 0, 0, 12))
        cell(text(w('ContractValueLabel'), 12, '운반·확보', True), row, 72, 24)
        cell(text(w('ContractValueText'), 16, right=True), row, height=24, weight=1)
        move(w('AlertMeterPanel'), mission)
        flat(w('AlertMeterPanel'))
        align(w('AlertMeterPanel'))
        alerts = container(w('AlertMeterPanel'), unreal.VerticalBox, 'CompositionAlertColumn')
        row = sized_row(alerts, 'CompositionAlertRow', 20)
        cell(text(w('AlertTitleText'), 12, muted_text=True), row, 40, 20, (0, 0, 8, 0))
        cell(w('AlertStarRow'), row, height=4, padding=(0, 0, 8, 0), weight=1)
        cell(text(w('AlertValueText'), 12, muted_text=True, right=True), row, 44, 20)
        # A fixed SizeBox would reserve space even when this native transient
        # text is collapsed. Keep it directly in the group so the panel shrinks.
        move(text(w('AlertEventText'), 12, muted_text=True), alerts)
        align(w('AlertEventText'), (0, 8, 0, 0))
        hide('AlertEventText_CompositionCell')
        frame(bp, mission, 'CompositionMissionPanel', padding=16)
        place(w('TeamCardsPanel'), w('HUDCanvas'), 32, 0, 256, 216, anchor=(0, .5), alignment=(0, .5))
        for i in range(1, 5):
            if 'TeamCard' + str(i) in ws:
                align(w('TeamCard' + str(i)), (0, 0, 0, 8 if i < 4 else 0))
        tools_row = w('HUDQuickSlotRow')
        place(tools_row, w('HUDCanvas'), -32, -32, 288, 96, anchor=(1, 1), alignment=(1, 1))
        flash = w('FlashlightControlRow')
        flash_column = node(unreal.VerticalBox, 'CompositionFlashlightColumn', tools_row, 0)
        flash_bounds = cell(flash_column, tools_row, 80, 96, (0, 0, 24, 0))
        move(flash_bounds, tools_row, 0)
        for name in ['FlashlightIcon_ImageBounds', 'FlashlightKeyText_ImageBounds', 'FlashlightStatusText_ImageBounds']:
            hide(name)
        flash_icon = cell(glyph('FlashlightIcon', flash_column, 'flashlight_off', 56), flash_column, 56, 56, (0, 0, 0, 8))
        align(flash_icon, (0, 0, 0, 8), horizontal=center)
        move(flash, flash_column)
        align(flash, horizontal=center)
        cap = key_widget(w('FlashlightKeyText'), flash, 'CompositionFlashlightKey', value='F')
        align(cap, (0, 0, 8, 0))
        cell(text(w('FlashlightStatusText'), 12, muted_text=True), flash, 28, 32)
        quick_bounds = cell(w('HUDQuickSlot1'), tools_row, 80, 96, (0, 0, 24, 0))
        move(quick_bounds, tools_row, 1)
        bag = node(unreal.VerticalBox, 'CompositionBagColumn', tools_row, 2)
        bag_bounds = cell(bag, tools_row, 80, 96)
        move(bag_bounds, tools_row, 2)
        bag_icon = cell(glyph('InventoryShortcutIcon', bag, 'bag', 56), bag, 56, 56, (0, 0, 0, 8))
        align(bag_icon, (0, 0, 0, 8), horizontal=center)
        cap = key_widget(w('InventoryShortcutKeyText'), bag, 'CompositionBagKey', wide=True, value='TAB')
        align(cap, horizontal=center)
        hide('CompositionBagRow')
        hide('SizeBox_0', 'HUDQuickSlot2', 'HUDQuickSlot3')

    elif key == 'WBP_TeamCard':
        box(w('SizeBox_0'), 256, 48)
        flat(w('TeamCardBorder'))
        row = container(w('TeamCardBorder'), unreal.HorizontalBox, 'CompositionTeamRow')
        hide('CoopTeamCanvas', 'TeamCardRow', 'CoopTeamProfileBackground',
             'CoopTeamProfileHead', 'CoopTeamProfileBody', 'ProfileImage_AspectFit_ImageBounds')
        profile = node(unreal.Overlay, 'CompositionTeamProfileOverlay', row)
        fit_profile = node(unreal.ScaleBox, 'CompositionTeamProfileFit', profile)
        fit_profile.set_stretch(unreal.Stretch.SCALE_TO_FIT)
        move(w('ProfileImage'), fit_profile)
        icon(w('ProfileImage'), 'profile', 32, 32)
        w('ProfileImage').set_render_opacity(1)
        w('ProfileImage').set_visibility(visible)
        cell(profile, row, 32, 32, (0, 0, 12, 0))
        move(w('PlayerColorMarker'), profile)
        align(w('PlayerColorMarker'), horizontal=left, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        w('PlayerColorMarker').set_brush_size(unreal.Vector2D(4, 8))
        column = node(unreal.VerticalBox, 'CompositionTeamIdentity', row)
        align(column, (0, 0, 8, 0), weight=1)
        cell(text(w('PlayerNameText'), 16), column, height=24)
        w('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        status = sized_row(column, 'CompositionTeamStatus', 20)
        cell(w('StatusIcon'), status, 12, 12, (0, 0, 4, 0))
        cell(text(w('StatusText'), 12, muted_text=True), status, height=20, weight=1)
        cell(glyph('MicStatusImage', row, 'microphone', 16), row, 16, 20)

    elif key == 'WBP_QuickSlot':
        box(w('SizeBox_0'), 80, 96)
        flat(w('SlotBackground'))
        row = container(w('SlotBackground'), unreal.VerticalBox, 'CompositionQuickSlotColumn')
        hide('CompositionQuickSlotRow')
        hide('CoopQuickSlotCanvas', 'SlotContent', 'CoopCoinRing', 'CoopCoinSymbol', 'PlaceholderIcon_AspectFit')
        coin = node(unreal.Overlay, 'CompositionCoinIconOverlay', row)
        coin_bounds = cell(coin, row, 56, 56, (0, 0, 0, 8))
        align(coin_bounds, (0, 0, 0, 8), horizontal=center)
        move(glyph('PlaceholderIcon', coin, 'coin', 56), coin)
        align(w('PlaceholderIcon'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        count = cell(text(w('CountText'), 12, centered=True), coin, 16, 20)
        align(count, (0, 0, -8, -8), horizontal=unreal.HorizontalAlignment.H_ALIGN_RIGHT, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        cap = key_widget(w('KeyLabelText'), row, 'CompositionQuickSlotKey', value='Q')
        align(cap, horizontal=center)

    elif key == 'WBP_InteractionPrompt':
        hide('CoopInteractionKeySize')
        flat(w('InteractionKeyContainer'))
        holder = container(w('InteractionKeyContainer'), unreal.HorizontalBox, 'CompositionInteractionKeyHolder')
        key_widget(w('KeyText'), holder, 'CompositionInteractionKey', value='E')
        align(w('InteractionKeyContainer'), (0, 0, 8, 0))
        text(w('TargetText'), 16)

    elif key == 'WBP_Lobby':
        canvas = w('CoopLobbyCanvas')
        already_framed = 'CompositionLobbyPanel' in ws
        main = w('CompositionLobbyMain') if already_framed else node(unreal.HorizontalBox, 'CompositionLobbyMain', canvas)
        if not already_framed:
            place(main, canvas, 32, 128, 1216, 472)
        map_column = node(unreal.VerticalBox, 'CompositionLobbyMaps', main)
        align(map_column, (0, 0, 72, 0), vertical=unreal.VerticalAlignment.V_ALIGN_TOP, weight=1)
        map_header = sized_row(map_column, 'CompositionLobbyMapHeader', 28, (0, 0, 0, 20))
        cell(text(w('MapSectionTitle'), 20), map_header, height=28, weight=1)
        cell(text(w('SelectedMapText'), 16, muted_text=True, right=True), map_header, 128, 28)
        list_box = node(unreal.SizeBox, 'CompositionLobbyMapListSize', map_column)
        list_box.clear_width_override()
        list_box.set_height_override(336)
        move(w('MapHorizontalScrollBox'), list_box)
        align(w('MapHorizontalScrollBox'))
        w('MapHorizontalScrollBox').set_orientation(unreal.Orientation.ORIENT_VERTICAL)
        w('MapHorizontalScrollBox').set_editor_property('scrollbar_thickness', unreal.Vector2D(4, 4))
        w('MapHorizontalScrollBox').set_scrollbar_padding(margin(0, 0, 0, 0))
        for name in ['MapRandomCard', 'MapM01Card', 'MapM02Card', 'MapM03Card']:
            align(w(name))
        crew_column = node(unreal.VerticalBox, 'CompositionLobbyCrew', main)
        align(crew_column, vertical=unreal.VerticalAlignment.V_ALIGN_TOP, weight=1)
        cell(text(w('CoopCrewSectionTitle'), 20), crew_column, height=28, padding=(0, 0, 0, 20))
        for i in range(1, 5):
            card_name = 'PlayerCard' + str(i)
            move(w(card_name), crew_column)
            align(w(card_name), (0, 0, 0, 0))
        wrapped = frame(bp, main, 'CompositionLobbyPanel', padding=24)
        place(wrapped, canvas, 32, 128, 1216, 472)
        hide('MapSection', 'CoopMapListSize', 'CoopLobbyCrewList', 'CoopLobbyEscapeFrame')
        row = footer(canvas, 'CompositionLobbyFooter')
        cap = key_widget(w('CoopLobbyEscapeKey'), row, 'CompositionLobbyEscapeKey', wide=True, value='ESC')
        align(cap, (0, 0, 8, 0))
        cell(text(w('CoopLobbyEscapeLabel'), 16, '나가기', True), row, 96, 48)
        stretch(row, 'CompositionLobbyFooterFill')
        cell(w('GlobalReadyButton'), row, 116, 48, (0, 0, 16, 0))
        cell(w('StartGameButton'), row, 116, 48)
        button(w('GlobalReadyButton'))
        button(w('StartGameButton'), primary=True)
        hide('SizeBox_0')
        header = node(unreal.HorizontalBox, 'CompositionLobbyHeaderActions', canvas)
        place(header, canvas, 800, 36, 448, 48)
        cell(text(w('JoinCodeLabel'), 16), header, 72, 48, (0, 0, 16, 0))
        cell(text(w('JoinCodeText'), 16), header, 88, 48, (0, 0, 16, 0))
        copy = cell(w('CopyJoinCodeButton'), header, 48, 48, (0, 0, 16, 0))
        icon(w('CoopLobbyCopyIcon'), 'copy', 24, 24)
        copy_content = container(w('CopyJoinCodeButton'), unreal.HorizontalBox, 'CompositionLobbyCopyContent')
        cell(w('CoopLobbyCopyIcon'), copy_content, 24, 24)
        cell(text(w('PlayerCountText'), 16, muted_text=True, centered=True), header, 64, 48, (0, 0, 16, 0))
        cell(w('LeaveSessionButton'), header, 112, 48)
        leave_content = container(w('LeaveSessionButton'), unreal.HorizontalBox, 'CompositionLobbyLeaveContent')
        leave_icon = node(unreal.Image, 'CompositionLobbyLeaveImage', leave_content)
        leave_icon.set_brush_from_texture(unreal.load_asset('/Game/Assets/UI/Heist/T_Heist_Exit'),False)
        leave_icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        leave_icon.set_color_and_opacity(white)
        cell(leave_icon, leave_content, 24, 24, (0, 0, 8, 0))
        cell(text(w('LeaveSessionButtonLabel'), 16, '나가기'), leave_content, 56, 24)
        hide('JoinCodeGroup', 'SizeBox_1', 'PlayerCountSize', 'LeaveSessionSize',
             'CoopLobbyCopyCanvas', 'CoopLobbyLeaveCanvas', 'CoopLobbyLeaveIcon')
        button(w('CopyJoinCodeButton'), quiet=True)
        button(w('LeaveSessionButton'), quiet=True)

    elif key == 'WBP_LobbyMapCard':
        # Width comes from the shared parent column (552 at 1280 design width).
        # A forced child width would overflow when the panel border changes.
        w('MapCardSizeBox').clear_width_override()
        w('MapCardSizeBox').set_height_override(112)
        w('MapThumbnailSize').clear_width_override()
        w('MapThumbnailSize').set_height_override(112)
        align(w('MapThumbnailSize'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        align(w('SelectMapButton'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        button(w('SelectMapButton'), quiet=True)
        overlay = container(w('SelectMapButton'), unreal.Overlay, 'CompositionMapOverlay')
        hide('CoopMapRowCanvas', 'MapCardOverlay', 'CoopMapNameCenter', 'CoopMapThumbnailBackground')
        move(w('SelectionBackground'), overlay, 0)
        align(w('SelectionBackground'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        row = node(unreal.HorizontalBox, 'CompositionMapRow', overlay)
        align(row, (16, 12, 12, 12))
        cell(text(w('MapNameText'), 20), row, height=88, padding=(0, 0, 16, 0), weight=1)
        cell(w('SelectedCheckImage'), row, 16, 24, (0, 0, 8, 0))
        thumb = node(unreal.Overlay, 'CompositionMapThumbnailOverlay', row)
        fit_thumb = node(unreal.ScaleBox, 'CompositionMapThumbnailFit', thumb)
        fit_thumb.set_stretch(unreal.Stretch.SCALE_TO_FIT)
        move(w('MapThumbnailImage'), fit_thumb)
        align(w('MapThumbnailImage'))
        move(w('RandomQuestionText'), thumb)
        text(w('RandomQuestionText'), 40, '?', centered=True)
        align(w('RandomQuestionText'), horizontal=center)
        cell(thumb, row, 160, 88)
        icon(w('SelectedCheckImage'), 'ready', 16, 16)
        # This button is a whole selectable row; unlike a text-only action its
        # Overlay must occupy the button instead of centering at desired width.
        align(overlay, vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        rule = cell(w('CoopMapRowRule'), overlay, height=4)
        align(rule, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        w('CoopMapRowRule').set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        w('CoopMapRowRule').set_brush(helpers['brush'](None, helpers['css_color']('#626b6f'), (4, 4)))
        w('CoopMapRowRule').set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
        w('CoopMapRowRule').set_render_scale(unreal.Vector2D(1, .25))

    elif key == 'WBP_LobbyPlayerCard':
        w('PlayerCardSizeBox').clear_width_override()
        w('PlayerCardSizeBox').set_height_override(80)
        flat(w('PlayerCardBorder'))
        row = container(w('PlayerCardBorder'), unreal.HorizontalBox, 'CompositionLobbyPlayerRow')
        hide('CoopLobbyPlayerCanvas', 'PlayerCardContent', 'PlayerSlotText', 'SizeBox_0',
             'CoopLobbyProfileBackground', 'CoopLobbyProfileHead', 'CoopLobbyProfileBody')
        move(w('ProfileImageSizeBox'), row)
        box(w('ProfileImageSizeBox'), 48, 48)
        align(w('ProfileImageSizeBox'), (0, 0, 16, 0))
        icon(w('ProfileImage'), 'profile', 48, 48)
        w('ProfileImage').set_render_opacity(1)
        w('ProfileImage').set_visibility(visible)
        column = node(unreal.VerticalBox, 'CompositionLobbyPlayerIdentity', row)
        align(column, (0, 0, 16, 0), weight=1)
        cell(text(w('PlayerNameText'), 16), column, height=24)
        w('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        ready_row = sized_row(column, 'CompositionLobbyReadyRow', 20)
        cell(w('ReadyCheckImage'), ready_row, 16, 16, (0, 0, 8, 0))
        icon(w('ReadyCheckImage'), 'ready', 16, 16)
        cell(text(w('ReadyStatusText'), 12, muted_text=True), ready_row, height=20, weight=1)
        cell(glyph('CompositionLobbyMicImage', row, 'microphone', 16), row, 16, 20)
        overlay = container(w('PlayerCardBorder'), unreal.Overlay, 'CompositionLobbyPlayerOverlay')
        align(row, (16, 0, 16, 0), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        rule = cell(w('CoopLobbyPlayerRule'), overlay, height=4)
        align(rule, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        w('CoopLobbyPlayerRule').set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        w('CoopLobbyPlayerRule').set_brush(helpers['brush'](None, helpers['css_color']('#626b6f'), (4, 4)))
        w('CoopLobbyPlayerRule').set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
        w('CoopLobbyPlayerRule').set_render_scale(unreal.Vector2D(1, .25))

    elif key == 'WBP_HeistForgery':
        canvas = w('RootCanvas')
        timer = node(unreal.HorizontalBox, 'CompositionForgeryTimer', canvas)
        place(timer, canvas, 1116, 48, 132, 32)
        cell(w('CoopForgeryTimerIcon'), timer, 24, 32, (0, 0, 8, 0))
        cell(text(w('DrawingTimeRemainingText'), 24, right=True), timer, 100, 32)
        work = node(unreal.HorizontalBox, 'CompositionForgeryWork', canvas)
        place(work, canvas, 168, 108, 944, 500)
        drawing = node(unreal.VerticalBox, 'CompositionDrawingColumn', work)
        align(drawing, (0, 0, 24, 0), vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        cell(text(w('HeistCanvasLabel'), 12, '복제 작업', True), drawing, 412, 16, (0, 0, 0, 8))
        cell(w('DrawingContainer'), drawing, 412, 412)
        box(w('DrawingSurfaceSizeBox'), 412, 412)
        palette_bounds = cell(w('DrawingContent'), work, 72, 412, (0, 0, 24, 0))
        align(palette_bounds, (0, 0, 24, 0), vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        reference = node(unreal.VerticalBox, 'CompositionReferenceColumn', work)
        align(reference, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        similarity = node(unreal.VerticalBox, 'CompositionSimilarityColumn', reference)
        align(similarity, (0, 0, 0, 24))
        cell(text(w('PreviewScoreText'), 16, '작품 유사도'), similarity, 412, 24, (0, 0, 0, 8))
        cell(w('PreviewQualityBar'), similarity, 412, 8)
        cell(text(w('HeistReferenceLabel'), 12, '관찰한 원본', True), reference, 412, 16, (0, 0, 0, 8))
        cell(w('ReferenceImage_AspectFit'), reference, 412, 412)
        w('ReferenceImage_AspectFit').set_stretch(unreal.Stretch.SCALE_TO_FIT)
        w('ReferenceImage_AspectFit').set_stretch_direction(unreal.StretchDirection.BOTH)
        align(w('ReferenceImage'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        hide('PreviewQualityBar_ImageBounds', 'FooterActionRow', 'FooterHint', 'CoopForgeryDrawKey', 'CoopForgeryEraseKey')
        row = footer(canvas, 'CompositionForgeryFooter')
        cap_text = label('CompositionForgeryLeftMouse', row, 'LMB', 12, True)
        cap = key_widget(cap_text, row, 'CompositionForgeryLeftMouseKey', wide=True)
        align(cap, (0, 0, 8, 0))
        cell(label('CompositionForgeryDrawLabel', row, '그리기', 12, True), row, 48, 48, (0, 0, 16, 0))
        cap_text = label('CompositionForgeryRightMouse', row, 'RMB', 12, True)
        cap = key_widget(cap_text, row, 'CompositionForgeryRightMouseKey', wide=True)
        align(cap, (0, 0, 8, 0))
        cell(label('CompositionForgeryEraseLabel', row, '지우기', 12, True), row, 48, 48, (0, 0, 16, 0))
        reset_row = container(w('ResetDrawingButton'), unreal.HorizontalBox, 'CompositionForgeryResetRow')
        hide('CoopForgeryResetText')
        cap_text = label('CompositionForgeryResetLetter', reset_row, 'R', 12, True)
        cap = key_widget(cap_text, reset_row, 'CompositionForgeryResetKey')
        align(cap, (0, 0, 8, 0))
        cell(label('CompositionForgeryResetLabel', reset_row, '전체 지우기', 12, True), reset_row, 80, 32)
        cell(w('ResetDrawingButton'), row, 136, 48)
        button(w('ResetDrawingButton'), quiet=True)
        stretch(row, 'CompositionForgeryFooterFill')
        cell(w('CancelButton'), row, 96, 48, (0, 0, 16, 0))
        cell(w('SubmitButton'), row, 96, 48)
        button(w('CancelButton'))
        button(w('SubmitButton'), primary=True)

    elif key == 'WBP_Settings':
        canvas = w('CoopSettingsCanvas')
        already_framed = 'CompositionSettingsPanel' in ws
        rows = w('CompositionSettingsRows') if already_framed else node(unreal.VerticalBox, 'CompositionSettingsRows', canvas)
        if not already_framed:
            place(rows, canvas, 128, 120, 1024, 384)
        sliders = {'FOV': ('FOVSlider', 'FOVValueText', 'FOVValueFill'),
                   'Sensitivity': ('MouseSensitivitySlider', 'MouseSensitivityValueText', 'MouseSensitivityValueFill'),
                   'Volume': ('MasterVolumeSlider', 'MasterVolumeValueText', 'MasterVolumeValueFill')}
        for prefix in ['FOV', 'Sensitivity', 'Volume', 'Resolution', 'WindowMode']:
            row = sized_row(rows, 'CompositionSettings' + prefix + 'Row', 64)
            cell(text(w(prefix + 'RowLabel'), 16), row, 192, 64, (0, 0, 16, 0))
            if prefix in sliders:
                control, value, track = sliders[prefix]
                overlay = node(unreal.Overlay, 'CompositionSettings' + prefix + 'Control', row)
                cell(overlay, row, height=48, padding=(0, 0, 16, 0), weight=1)
                if track in ws:
                    cell(w(track), overlay, height=8)
                move(w(control), overlay)
                align(w(control))
                cell(text(w(value), 16, right=True), row, 80, 64)
            else:
                cell(w(prefix + 'ComboBox'), row, height=48, weight=1)
            hide('CoopSettings' + prefix + 'Hover', 'CoopSettings' + prefix + 'Canvas', 'CoopSettings' + prefix + 'Rule')
        wrapped = frame(bp, rows, 'CompositionSettingsPanel', padding=24)
        place(wrapped, canvas, 128, 120, 1024, 384)
        actions = footer(canvas, 'CompositionSettingsActions', 128, 520, 1024)
        stretch(actions, 'CompositionSettingsActionFill')
        cell(w('RestoreDefaultSettingsButton'), actions, 104, 48, (0, 0, 16, 0))
        cell(w('ApplySettingsButton'), actions, 100, 48)
        button(w('RestoreDefaultSettingsButton'))
        button(w('ApplySettingsButton'), primary=True)
        button(w('SettingsCloseButton'), quiet=True)
        row = footer(canvas, 'CompositionSettingsFooter', 32, 652)
        cell(w('SettingsCloseButton'), row, 160, 48)
        close_content = container(w('SettingsCloseButton'), unreal.HorizontalBox, 'CompositionSettingsCloseContent')
        cap = key_widget(w('CoopSettingsEscText'), close_content, 'CompositionSettingsEscapeKey', wide=True, value='ESC')
        align(cap, (0, 0, 8, 0))
        cell(text(w('SettingsCloseButtonText'), 16, '닫기', True), close_content, 64, 32)
        hide('CoopSettingsEscKey', 'CoopSettingsKeys')

    elif key == 'WBP_Result':
        canvas = w('CanvasPanel_38')
        text(w('OutcomeTextBlock'), 32)
        text(w('TeamRewardTextBlock'), 40, right=True)
        place(w('OutcomeTextBlock'), canvas, 32, 52, 1056, 48)
        place(w('TeamRewardTextBlock'), canvas, 984, 148, 264, 48)
        for group, kind, suffixes in [('CoopResultSuccessCheck', 'ready', ['Short', 'Long']),
                                       ('CoopResultFailedCross', 'warning', ['Down', 'Up'])]:
            if group in ws:
                hide(*(group + suffix for suffix in suffixes))
                result_icon = glyph(group + 'Image', w(group), kind)
                place(result_icon, w(group), 0, 0, 24, 24)
        box(w('CoopResultCompactHeaderSize'), 1216, 32)
        box(w('CoopResultDetailHeaderSize'), 1216, 32)
        summary = container(w('CoopResultCompactHeaderSize'), unreal.HorizontalBox, 'CompositionResultSummaryHeader')
        details = container(w('CoopResultDetailHeaderSize'), unreal.HorizontalBox, 'CompositionResultDetailHeader')
        hide('CoopResultCompactHeader', 'CoopResultDetailHeader')
        for name, value, weight, right in [
                ('HeaderPlayer', '플레이어', 56, False), ('HeaderState', '상태', 12, False),
                ('HeaderSecured', '확보 가치', 16, True), ('HeaderBestQuality', '최고 유사도', 16, True)]:
            cell(text(w(name), 12, value, True, right), summary, height=32, padding=(8, 0, 8, 0), weight=weight)
        detail_names = [('HeaderDetailPlayer', '플레이어'), ('HeaderDrawing', '위조'),
                        ('HeaderOriginals', '원본 회수'), ('HeaderGuards', '경비 유인'),
                        ('HeaderRescues', '동료 구출'), ('HeaderAlarms', '경보 발생')]
        for i, (name, value) in enumerate(detail_names):
            target = w(name) if name in ws else node(unreal.TextBlock, name, details)
            cell(text(target, 12, value, True, i > 0), details, height=32,
                 padding=(8, 0, 8, 0), weight=40 if i == 0 else 12)
        place(w('ReplicaRecapVisualPanel'), canvas, 32, 204, 1216, 188)
        table = w('CoopResultTableLayout')
        if 'CompositionResultTableBackground' not in ws:
            # Keep both bound modes attached while wrapping their shared parent.
            wrapper = tools.call_method('WrapWidgets', (bp, [table], unreal.Border.static_class()))[0].widget
            ws['CompositionResultTableBackground'] = tools.call_method(
                'RenameWidget', (bp, wrapper, 'CompositionResultTableBackground')).widget
            refresh()
        background = w('CompositionResultTableBackground')
        background.set_brush(helpers['brush'](None, helpers['css_color']('#080d13', .72), (4, 4)))
        background.set_brush_color(unreal.LinearColor(1, 1, 1, 1))
        background.set_padding(margin(0))
        background.set_horizontal_alignment(fill)
        background.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        align(table, vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        place(background, canvas, 32, 400, 1216, 224)
        w('CoopResultCompactHeaderSize').set_visibility(visible)
        w('CoopResultDetailHeaderSize').set_visibility(collapsed)
        row = footer(canvas, 'CompositionResultFooter', 32, 640)
        esc_text = label('CompositionResultEscapeText', row, 'ESC', 12, True)
        cap = key_widget(esc_text, row, 'CompositionResultEscapeKey', wide=True)
        align(cap, (0, 0, 8, 0))
        cell(label('CompositionResultReturnHint', row, '로비로', 16, True), row, 80, 48, (0, 0, 16, 0))
        cell(w('RewardDetailsButton'), row, 128, 48)
        stretch(row, 'CompositionResultFooterFill')
        cell(w('ReturnToLobbyButton'), row, 176, 48)
        button(w('RewardDetailsButton'), quiet=True)
        text(w('RewardDetailsButtonLabel'), 16, '상세 보기', centered=True)
        button(w('ReturnToLobbyButton'), primary=True)

    elif key == 'WBP_ResultPlayerRow':
        box(w('CoopResultRowSize'), 1216, 48)
        flat(w('PlayerResultRowRoot'))
        box(w('CoopResultCompactFieldsSize'), 1216, 48)
        box(w('CoopResultDetailFieldsSize'), 1216, 48)
        summary = container(w('CoopResultCompactFieldsSize'), unreal.HorizontalBox, 'CompositionResultSummaryFields')
        details = container(w('CoopResultDetailFieldsSize'), unreal.HorizontalBox, 'CompositionResultDetailFields')
        hide('CoopResultRowCanvas', 'CoopResultDetailFields', 'PlayerResultRowLayout', 'CoopResultPlayerRule')
        for name, weight, right in [('PlayerNameText', 56, False), ('PlayerStateText', 12, False),
                                    ('SecuredLootValueText', 16, True), ('BestSurfaceQualityText', 16, True)]:
            cell(text(w(name), 16, right=right), summary, height=48, padding=(8, 0, 8, 0), weight=weight)
        w('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        names = ['DetailPlayerNameText', 'SurfaceForgeryCountText', 'ArtifactsRecoveredText',
                 'GuardsDistractedText', 'TeammatesRescuedText', 'AlarmsTriggeredText']
        for i, name in enumerate(names):
            target = w(name) if name in ws else node(unreal.TextBlock, name, details)
            text(target, 16, right=i > 0)
            target.set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
            cell(target, details, height=48, padding=(8, 0, 8, 0), weight=40 if i == 0 else 12)
        # Reuse the old divider outside the collapsed Canvas so both modes
        # share one separator, without changing their column widths or height.
        overlay = container(w('PlayerResultRowRoot'), unreal.Overlay, 'CompositionResultRowOverlay')
        align(w('CoopResultRowColumn'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        divider = w('CoopResultRowRule')
        divider.set_brush(helpers['brush'](None, helpers['css_color']('#ffffff', .16), (4, 4)))
        divider.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
        divider.set_render_scale(unreal.Vector2D(1, .25))
        divider.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        divider_bounds = cell(divider, overlay, 1216, 4)
        align(divider_bounds, vertical=unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        w('CoopResultCompactFieldsSize').set_visibility(visible)
        w('CoopResultDetailFieldsSize').set_visibility(collapsed)

    elif key == 'WBP_ResultReplicaCard':
        box(w('CoopReplicaCardSize'), 284, 188)
        box(w('ReplicaImageSize'), 284, 128)
        text(w('ArtifactNameText'), 16)
        text(w('QualityText'), 12, muted_text=True)
        text(w('RequiredTargetBadge'), 12, '필수', True)
        w('ReplicaImage_AspectFit').set_stretch(unreal.Stretch.SCALE_TO_FIT)

    elif key == 'WBP_Inventory':
        flat(w('InventoryPanel'))
        canvas = w('RootCanvas')
        column = node(unreal.VerticalBox, 'CompositionInventoryColumn', canvas)
        place(column, canvas, 0, 0, 448, 520, anchor=(.5, .5), alignment=(.5, .5))
        header = sized_row(column, 'CompositionInventoryHeader', 48, (0, 0, 0, 24))
        cell(text(w('InventoryTitleText'), 24, '가방'), header, height=48, weight=1)
        close_content = container(w('CloseButton'), unreal.HorizontalBox, 'CompositionInventoryCloseContent')
        tab_text = label('CompositionInventoryTabText', close_content, 'TAB', 12, True)
        cap = key_widget(tab_text, close_content, 'CompositionInventoryTabKey', wide=True)
        align(cap, (0, 0, 8, 0))
        cell(text(w('CloseButtonText'), 12, '닫기', True), close_content, 32, 32)
        cell(w('CloseButton'), header, 112, 48)
        button(w('CloseButton'), quiet=True)
        cell(w('InventoryFrameWidget'), column, 448, 448)
        hide('InventoryTitleText_CatalogueTab', 'InventoryPanel')

    elif key == 'WBP_InventoryFrame':
        box(w('SizeBox_0'), 448, 448)
        flat(w('InventoryFrame'))
        frame(bp, w('InventoryFrame'), 'CompositionInventoryPanel', padding=0)
        w('InventoryGrid').set_slot_padding(margin(0, 0, 4, 4))
        w('InventoryGrid').set_min_desired_slot_width(0)
        w('InventoryGrid').set_min_desired_slot_height(0)
        # InventoryGrid and ItemOverlay remain siblings in the original Overlay;
        # the runtime item coordinates are derived from their actual Geometry.

    elif key == 'WBP_InventorySlot':
        hide('CoordinateText', 'OccupancyText')

    elif key == 'WBP_TitleMenu':
        canvas = w('TitleRoot')
        for name in ['SettingsWidget', 'SessionJoinWidget']:
            if name in ws and isinstance(w(name).slot, unreal.CanvasPanelSlot):
                w(name).slot.set_z_order(100)
        menu = node(unreal.VerticalBox, 'CompositionTitleColumn', canvas)
        place(menu, canvas, 64, 168, 440, 440)
        # The inline code field uses the existing C++ title session request;
        # the existing session panel retains pending/error presentation.
        hide('TitleMenuColumn', 'LogoImage', 'HeistMenuRule',
             'GameSubtitleText', 'GameSubtitleText_CompositionCell')
        title = label('CompositionTitleName', menu, 'MUSEUM\nHEIST', 48)
        title.set_editor_property('auto_wrap_text', True)
        cell(title, menu, 440, 112, (0, 0, 0, 32))
        for name in ['HostSessionButton', 'SettingsButton', 'QuitGameButton']:
            padding = (0, 0, 0, 0 if name == 'QuitGameButton' else 12)
            bounds = cell(w(name), menu, 288, 56, padding)
            align(bounds, padding, horizontal=left)
            button(w(name), primary=name == 'HostSessionButton', quiet=name == 'QuitGameButton')
        move(w('HostSessionButton_CompositionCell'), menu, 1)
        join_bounds = node(unreal.SizeBox, 'CompositionTitleJoinBounds', menu, 2)
        box(join_bounds, 288, 56)
        align(join_bounds, (0, 0, 0, 12), horizontal=left)
        join = container(join_bounds, unreal.HorizontalBox, 'CompositionTitleJoinRow')
        code = node(unreal.EditableTextBox, 'TitleJoinCodeInput', join, 0)
        code.set_hint_text('참가 코드')
        code.set_editor_property('justification',unreal.TextJustify.LEFT)
        cell(code, join, height=56, padding=(0, 0, 8, 0), weight=1)
        align(code, vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        join_button_bounds = node(unreal.SizeBox, 'CompositionTitleJoinButtonSize', join)
        box(join_button_bounds, 72, 56)
        align(join_button_bounds)
        move(w('JoinSessionButton'), join_button_bounds)
        align(w('JoinSessionButton'), vertical=unreal.VerticalAlignment.V_ALIGN_FILL)
        button(w('JoinSessionButton'))
        join_button_style = w('JoinSessionButton').get_editor_property('widget_style')
        join_button_style.set_editor_property('normal_padding', margin(16, 0, 16, 0))
        join_button_style.set_editor_property('pressed_padding', margin(16, 0, 16, 0))
        w('JoinSessionButton').set_style(join_button_style)
        text(w('JoinSessionButtonText'), 16, '참가', centered=True)
        style = code.get_editor_property('widget_style')
        text_style = style.get_editor_property('text_style')
        text_style.set_editor_property('font', w('JoinSessionButtonText').get_editor_property('font'))
        text_style.set_editor_property('color_and_opacity', helpers['sc'](white))
        text_style.set_editor_property('shadow_offset', unreal.DeprecateSlateVector2D())
        text_style.set_editor_property('shadow_color_and_opacity', clear)
        style.set_editor_property('text_style', text_style)
        style.set_editor_property('padding', margin(16, 0, 16, 0))
        for state, art in [('normal', 'Button'), ('hovered', 'ButtonHover'),
                           ('focused', 'ButtonHover'), ('read_only', 'Button')]:
            brush = unreal.WidgetLibrary.make_brush_from_texture(
                unreal.load_asset('/Game/Assets/UI/Heist/T_Heist_' + art), 256, 64)
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
            brush.set_editor_property('margin', margin(0))
            brush.set_editor_property('tint_color', helpers['sc'](unreal.LinearColor(1, 1, 1, 1)))
            style.set_editor_property('background_image_' + state, brush)
        for property_name in ['foreground_color', 'focused_foreground_color', 'read_only_foreground_color']:
            style.set_editor_property(property_name, helpers['sc'](white))
        style.set_editor_property('background_color', helpers['sc'](unreal.LinearColor(1, 1, 1, 1)))
        code.set_editor_property('widget_style', style)
        hide('JoinSessionButton_CompositionCell')
        text(w('SettingsButtonText'), 16, '환경설정', centered=True)
        text(w('QuitGameButtonText'), 16, '종료', centered=True)
        move(w('SettingsButton_CompositionCell'), menu, 3)
        move(w('QuitGameButton_CompositionCell'), menu, 4)

    refresh()
    return True
