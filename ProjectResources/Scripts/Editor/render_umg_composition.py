"""Render the saved approved WBP composition with the actual Slate renderer.

Run after application in a dedicated Editor with no unsaved map or active PIE.
Only transient fixture values are supplied. Layout, fonts, shared WBP, keycaps,
button styles and material icons come from the saved assets. No package is saved.
This is display-fixture evidence, never User PIE, multiplayer or Steam evidence.
Set QUIT_EDITOR_ON_FINISH=True only for a disposable dedicated Editor process.
"""
import hashlib
import json
import re
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation' / globals().get('QA_FOLDER', 'UIComposition20261003') / 'Visual'
OUT.mkdir(parents=True, exist_ok=True)
RESOLUTIONS = [tuple(map(int, size)) for size in globals().get(
    'RENDER_RESOLUTIONS', [(1280, 720), (1920, 1080), (3440, 1440)])]
assert RESOLUTIONS and all(len(size) == 2 and min(size) > 0 for size in RESOLUTIONS), \
    'RENDER_RESOLUTIONS must contain positive (width, height) pairs'
ASSETS = ['HUD/WBP_HeistHUD', 'Lobby/WBP_Lobby', 'Result/WBP_Result',
          'Title/WBP_Settings', 'Forgery/WBP_HeistForgery',
          'Title/WBP_TitleMenu', 'Inventory/WBP_Inventory']
CASES = [(asset, 'Default') for asset in ASSETS] + [
    ('HUD/WBP_HeistHUD', 'LongNames'), ('HUD/WBP_HeistHUD', 'FlashlightOn'),
    ('HUD/WBP_HeistHUD', 'FlashlightOff'), ('HUD/WBP_HeistHUD', 'Alert'),
    ('Lobby/WBP_Lobby', 'Random'), ('Lobby/WBP_Lobby', 'ScrollM03'),
    ('Lobby/WBP_Lobby', 'TwoPlayers'), ('Result/WBP_Result', 'Details'),
    ('Forgery/WBP_HeistForgery', 'NeedsWork'), ('Forgery/WBP_HeistForgery', 'AlmostReady'),
    ('Forgery/WBP_HeistForgery', 'PaletteSelected5'),
    ('Title/WBP_TitleMenu', 'SettingsOpen')]
if 'RENDER_CASES' in globals():
    CASES = list(RENDER_CASES)
    ASSETS = sorted({asset for asset, _ in CASES})
WHITE, MUTED, RED, ORANGE, GREEN = '#edece7', '#a4aaa9', '#e58b82', '#d6b681', '#9dbca7'
FILES = sorted(set(list((ROOT / 'Content/Blueprints/UI').rglob('*.uasset')) +
                   list((ROOT / 'Content/Assets/UI').rglob('*.uasset')) +
                   list((ROOT / 'Content/Maps').rglob('*.umap'))))


def hashes():
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in FILES}


def css(value):
    value = value.lstrip('#')
    def linear(channel):
        channel /= 255
        return channel / 12.92 if channel <= .04045 else ((channel + .055) / 1.055) ** 2.4
    return unreal.LinearColor(*(linear(int(value[i:i+2], 16)) for i in (0, 2, 4)), 1)


def widgets(widget):
    prefix = widget.get_path_name()
    # Populate again after native fixture creation; nested cards can be created later.
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_path_name().startswith((prefix + '.', prefix + ':'))}


def text(ws, name, value):
    ws[name].set_text(value)


def show(ws, name, visible=True, preserve_space=False):
    ws[name].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if visible else
                            unreal.SlateVisibility.HIDDEN if preserve_space else unreal.SlateVisibility.COLLAPSED)


def resource(image, obj):
    assert obj, 'Missing saved icon resource: ' + image.get_name()
    brush = image.get_editor_property('brush')
    brush.set_editor_property('resource_object', obj)
    image.set_brush(brush)


def load_texture(path):
    value = unreal.load_asset(path)
    assert isinstance(value, unreal.Texture2D), 'Missing fixture artwork: ' + path
    return value


def create_widget(asset, world):
    path = '/Game/Blueprints/UI/' + asset
    klass = unreal.load_class(None, path + '.' + asset.split('/')[-1] + '_C')
    assert klass, 'Missing actual saved WBP class: ' + path
    widget = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (world, klass, None))
    assert widget, 'Create Widget failed: ' + path
    kept_objects.append(widget)
    return widget


def configure_fixture_layout(asset, widget, resolution):
    if asset != 'HUD/WBP_HeistHUD':
        return
    ws = widgets(widget)
    fit = ws['HeistConceptViewportFit']
    bounds = ws['HeistConceptDesignSize']
    assert fit.get_editor_property('stretch') == unreal.Stretch.FILL, \
        'Saved HUD must fill the viewport before the transient DPI simulation'
    assert bounds.slot.get_editor_property('horizontal_alignment') == unreal.HorizontalAlignment.H_ALIGN_FILL and \
        bounds.slot.get_editor_property('vertical_alignment') == unreal.VerticalAlignment.V_ALIGN_FILL, \
        'Saved HUD ScaleBox child must fill the available viewport area'
    settings_class = unreal.load_class(None, '/Script/Engine.UserInterfaceSettings')
    assert settings_class, 'Engine UserInterfaceSettings reflection class is unavailable'
    settings = unreal.get_default_object(settings_class)
    # This native class and its enum are not Python-exported. Use the same
    # reflection JSON getter as ObjectTools, avoiding enum Python conversion.
    settings_values = json.loads(unreal.ToolsetLibrary.get_object_properties(
        settings, ['uIScaleRule', 'designScreenSize', 'applicationScale']))
    rule = settings_values['uIScaleRule']
    assert rule == 'ScaleToFit', 'DPI fixture supports the current project ScaleToFit rule only'
    design = settings_values['designScreenSize']
    assert design['x'] > 0 and design['y'] > 0, 'Invalid project DPI design screen size'
    # Mirrors UUserInterfaceSettings::GetDPIScaleBasedOnSize/CalculateScale.
    # WidgetComponent does not apply viewport DPI, so only this transient HUD
    # applies it once. Saved HUD remains Fill; other screens keep their saved fit.
    dpi = max(min(resolution[0] / design['x'], resolution[1] / design['y']) *
              settings_values['applicationScale'], .01)
    fit.set_stretch(unreal.Stretch.USER_SPECIFIED)
    fit.set_user_specified_scale(dpi)
    state['hud_dpi'][widget.get_path_name()] = {
        'saved_stretch': 'Fill', 'fixture_stretch': 'UserSpecified', 'scale': dpi,
        'project_rule': rule, 'project_design_size': [design['x'], design['y']],
        'logical_viewport_size': [resolution[0] / dpi, resolution[1] / dpi]}


def profile(widget, ws):
    resource(ws['ProfileImage'], widget.get_editor_property('default_profile_texture'))
    show(ws, 'ProfileImage')
    ws['ProfileImage'].set_render_opacity(1)


def template_rows():
    rows = json.loads((ROOT / 'ProjectResources/DataTableImports/DT_ForgeryTemplateRow.json').read_text(encoding='utf-8-sig'))
    return rows


def reference_path(row):
    return row['ReferenceImage'].split("'")[1]


def prepare_inventory(ws, world):
    grid, overlay = ws['InventoryGrid'], ws['ItemOverlay']
    grid.clear_children()
    overlay.clear_children()
    # Render the actual saved empty 5x5 grid without changing protected native
    # inventory state. Item placement/drag requires SetupInventoryWidget with a
    # real controller/component; this display fixture does not validate it.
    for index in range(25):
        cell = create_widget('Inventory/WBP_InventorySlot', world)
        slot = grid.add_child_to_uniform_grid(cell, index // 5, index % 5)
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)


def prepare_result(ws, world):
    ws['ContributionTableContainer'].clear_children()
    ws['ReplicaRecapVisualContainer'].clear_children()
    for i in range(4):
        row = create_widget('Result/WBP_ResultPlayerRow', world)
        ws['ContributionTableContainer'].add_child_to_vertical_box(row).set_padding(unreal.Margin(0))
        rw = widgets(row)
        name = ['진품을들고달리는플레이어12345', '짧은이름', 'PLAYER 123456789', '아주긴이름을쓰는플레이어123456'][i]
        text(rw, 'PlayerNameText', name)
        text(rw, 'DetailPlayerNameText', name)
        for key, value in {'PlayerStateText': '구금 중' if i == 3 else '탈출',
                           'SecuredLootValueText': ['6,200', '120', '24,800', '1,000'][i],
                           'BestSurfaceQualityText': ['84', '100', '70', '0'][i],
                           'SurfaceForgeryCountText': str(i + 1), 'ArtifactsRecoveredText': str(i + 1),
                           'GuardsDistractedText': str(i), 'TeammatesRescuedText': str(i % 2),
                           'AlarmsTriggeredText': str(i % 2)}.items():
            text(rw, key, value)
        if 'ProfileImage' in rw:
            profile(row, rw)
        card = create_widget('Result/WBP_ResultReplicaCard', world)
        card_slot = ws['ReplicaRecapVisualContainer'].add_child_to_horizontal_box(card)
        card_slot.set_padding(unreal.Margin(0, 0, 16 if i < 3 else 0, 0))
        card_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        cw = widgets(card)
        text(cw, 'ArtifactNameText', '황금빛 초상' if i == 0 else '전시 작품 ' + str(i + 1))
        text(cw, 'QualityText', '유사도 ' + str(84 + i * 3))
        show(cw, 'RequiredTargetBadge', i == 0)
        cw['ReplicaImage'].set_brush_from_texture(load_texture(reference_path(TEMPLATES[i])), False)
        show(cw, 'ReplicaImage')


def fixture(asset, variant, widget, world, first=False):
    # The values below are controlled display samples, reapplied after native tick.
    # Icon textures, keycap styles and static labels are never replaced with legacy art/copy.
    widget.set_visibility(unreal.SlateVisibility.VISIBLE)
    ws = widgets(widget)
    if asset.endswith('WBP_HeistHUD'):
        enabled = variant in ('FlashlightOn', 'Alert')
        if first:
            vm = unreal.new_object(unreal.HeistHUDViewModel, outer=widget)
            widget.set_editor_property('hud_view_model', vm)
            kept_objects.append(vm)
        vm = widget.get_editor_property('hud_view_model')
        vm.set_editor_property('flashlight_enabled', enabled)
        vm.set_editor_property('flashlight_status_text', '[F] 손전등 ' + ('ON' if enabled else 'OFF'))
        vm.set_editor_property('RequiredTargetDisplayName', '붉은 달 아래 황금빛 초상의 아주 긴 작품 이름' if variant == 'LongNames' else '황금빛 초상')
        vm.set_editor_property('ContractValueText', '전리품 가치 2,400 / 4,000')
        vm.set_editor_property('ContractValueAmountsText', '2,400 / 4,000')
        widget.call_method('RefreshHUDPresentation', ())
        text(ws, 'MissionTimeText', '00 : 45' if variant == 'Alert' else '14 : 32')
        ws['MissionTimeText'].set_color_and_opacity(unreal.SlateColor(css(RED if variant == 'Alert' else WHITE)))
        text(ws, 'RequiredTargetNameText', str(vm.get_editor_property('RequiredTargetDisplayName')))
        text(ws, 'ContractValueText', '2,400 / 4,000')
        show(ws, 'ContractValueText')
        show(ws, 'ContractValueLabel')
        text(ws, 'AlertValueText', '6.5 / 10' if variant == 'Alert' else '3.5 / 10')
        text(ws, 'AlertEventText', '경비 수색 중')
        show(ws, 'AlertEventText', variant == 'Alert')
        for i in range(1, 11):
            image = ws['AlertStar%02d' % i]
            amount = (6.5 if variant == 'Alert' else 3.5) - i + 1
            brush = image.get_editor_property('brush')
            brush.set_editor_property('resource_object', None)
            image.set_brush(brush)
            image.set_render_transform_pivot(unreal.Vector2D(0, .5))
            image.set_render_scale(unreal.Vector2D(1 if amount >= 1 else .5 if amount >= .5 else 0, 1))
        show(ws, 'HUDQuickSlot1')
        qw = widgets(ws['HUDQuickSlot1'])
        resource(qw['PlaceholderIcon'], widget.get_editor_property('coin_quick_slot_icon'))
        qw['PlaceholderIcon'].set_opacity(1)
        text(qw, 'KeyLabelText', 'Q')
        text(qw, 'CountText', '3')
        show(qw, 'CountText')
        show(ws, 'HUDQuickSlot2', False)
        show(ws, 'HUDQuickSlot3', False)
        states = [('활동 중', None), ('위조 중', 'forging_status_icon'),
                  ('원본 운반 중', 'carrying_original_status_icon'), ('구금 중', 'arrested_status_icon')]
        for i, (status, icon_property) in enumerate(states, 1):
            child = ws['TeamCard' + str(i)]
            cw = widgets(child)
            show(ws, 'TeamCard' + str(i))
            text(cw, 'PlayerNameText', '진품을들고달리는플레이어123456' if variant == 'LongNames' else 'PLAYER ' + str(i))
            text(cw, 'StatusText', status)
            cw['PlayerNameText'].set_color_and_opacity(unreal.SlateColor(css(WHITE)))
            cw['StatusText'].set_color_and_opacity(unreal.SlateColor(css(RED if i == 4 else MUTED)))
            profile(child, cw)
            if icon_property:
                resource(cw['StatusIcon'], child.get_editor_property(icon_property))
            show(cw, 'StatusIcon', bool(icon_property))
            show(cw, 'MicStatusImage')
            cw['MicStatusImage'].set_color_and_opacity(css(MUTED))
    elif asset.endswith('WBP_Lobby'):
        text(ws, 'JoinCodeText', 'MUSE24')
        text(ws, 'PlayerCountText', '2 / 4' if variant == 'TwoPlayers' else '4 / 4')
        text(ws, 'SelectedMapText', '무작위' if variant == 'Random' else 'M03')
        for i in range(1, 5):
            child = ws['PlayerCard' + str(i)]
            cw = widgets(child)
            occupied = variant != 'TwoPlayers' or i <= 2
            text(cw, 'PlayerNameText', ('진품을들고달리는플레이어123456' if i == 1 else 'PLAYER ' + str(i)) if occupied else '참가 대기')
            text(cw, 'ReadyStatusText', '준비 완료' if occupied and i != 4 else '준비 안 됨')
            show(cw, 'ReadyCheckImage', occupied and i != 4, preserve_space=True)
            profile(child, cw)
            child.set_render_opacity(1 if occupied else .45)
        scroll = ws['MapHorizontalScrollBox']
        scroll.set_scroll_offset(112 if variant == 'ScrollM03' else 0)
        for code in ['Random', 'M01', 'M02', 'M03']:
            child = ws['Map' + code + 'Card']
            cw = widgets(child)
            selected = code == ('Random' if variant == 'Random' else 'M03')
            show(cw, 'SelectedCheckImage', selected, preserve_space=True)
            cw['SelectionBackground'].set_render_opacity(1 if selected else 0)
            cw['MapThumbnailImage'].set_color_and_opacity(child.get_editor_property('SelectedThumbnailTint' if selected else 'ThumbnailTint'))
    elif asset.endswith('WBP_TitleMenu') and variant == 'SettingsOpen':
        child = widget.get_editor_property('SettingsWidget')
        assert child, 'Native title SettingsWidget binding missing'
        if first:
            # Exercise the existing title button handler and close handler on
            # the actual nested saved child, rather than a separate root WBP.
            # OpenSettings itself is native C++ and not a reflected function.
            widget.call_method('HandleSettingsClicked', ())
            assert child.get_visibility() == unreal.SlateVisibility.VISIBLE, 'Native title settings open failed'
            child.call_method('HandleSettingsCloseClicked', ())
            assert child.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Native settings close failed'
            widget.call_method('HandleSettingsClicked', ())
            assert child.get_visibility() == unreal.SlateVisibility.VISIBLE, 'Native title settings reopen failed'
            state['settings_native_open_close'][widget.get_path_name()] = True
        fixture('Title/WBP_Settings', 'Default', child, world, first=first)
    elif asset.endswith('WBP_Settings'):
        for name, value in [('FOVValueText', '90'), ('MouseSensitivityValueText', '1.0'), ('MasterVolumeValueText', '80%')]:
            text(ws, name, value)
        for slider, fill, ratio in [('FOVSlider', 'FOVValueFill', .5),
                                    ('MouseSensitivitySlider', 'MouseSensitivityValueFill', .4),
                                    ('MasterVolumeSlider', 'MasterVolumeValueFill', .8)]:
            ws[slider].set_value(ratio)
            ws[fill].set_percent(ratio)
        if first:
            for name, value in [('ResolutionComboBox', '1920 × 1080'), ('WindowModeComboBox', '전체 화면')]:
                ws[name].add_option(value)
                ws[name].set_selected_option(value)
    elif asset.endswith('WBP_HeistForgery'):
        if first:
            row = next(row for row in TEMPLATES if len(row['AllowedPalette']) == 5)
            palette = [unreal.LinearColor(*map(float, re.findall(r'[RGBA]=([\d.-]+)', color))) for color in row['AllowedPalette']]
            vm = unreal.new_object(unreal.HeistForgeryViewModel, outer=widget)
            for name, value in [('drawing_visible', True), ('presentation_visible', True),
                                ('allowed_palette', palette), ('reference_image', load_texture(reference_path(row))),
                                ('stroke_limit', row['StrokeLimit']), ('brush_size', .040)]:
                vm.set_editor_property(name, value)
            widget.set_editor_property('forgery_view_model', vm)
            kept_objects.append(vm)
            selected = 4 if variant == 'PaletteSelected5' else 0
            assert widget.call_method('SelectPaletteIndex', (selected,)), 'Native palette selection failed'
            ws['ReferenceImage'].set_brush_from_texture(load_texture(reference_path(row)), False)
        # Native refresh correctly hides controls without an owner session.
        # Reapply only the display state for this explicitly synthetic fixture.
        vm = widget.get_editor_property('forgery_view_model')
        vm.set_editor_property('drawing_visible', True)
        vm.set_editor_property('presentation_visible', True)
        for name in ['ReferenceImage','SubmitButton','CancelButton','ResetDrawingButton']:
            show(ws,name)
        score = 24 if variant == 'NeedsWork' else 62 if variant == 'AlmostReady' else 78
        text(ws, 'DrawingTimeRemainingText', '00:32')
        show(ws, 'DrawingContainer')
        show(ws, 'DrawingTimeRemainingText')
        ws['PreviewQualityBar'].set_percent(score / 100)
        ws['PreviewQualityBar'].set_fill_color_and_opacity(css(RED if score < 40 else ORANGE if score < 70 else GREEN))
        show(ws, 'PreviewQualityBar')
        ws['SubmitButton'].set_is_enabled(score >= 70)
        ws['CancelButton'].set_is_enabled(True)
        ws['ResetDrawingButton'].set_is_enabled(True)
        assert widget.get_editor_property('ResetDrawingButton') == ws['ResetDrawingButton'], 'Reset button native binding missing'
    elif asset.endswith('WBP_Result'):
        if first:
            prepare_result(ws, world)
            # Mirror native post-population compact state through the same
            # button handler, rather than assigning visibility in the fixture.
            widget.call_method('HandleRewardDetailsClicked', ())
            widget.call_method('HandleRewardDetailsClicked', ())
            if variant == 'Details':
                widget.call_method('HandleRewardDetailsClicked', ())
        text(ws, 'OutcomeTextBlock', '미션 성공')
        text(ws, 'OutcomeReasonTextBlock', '필수 목표를 반출하고 목표 가치를 달성했습니다.')
        text(ws, 'TeamRewardTextBlock', '$24,800')
        show(ws, 'CoopResultSuccessCheck')
        show(ws, 'CoopResultFailedCross', False)
        show(ws, 'ReturnToLobbyButton')
    elif asset.endswith('WBP_Inventory') and first:
        prepare_inventory(ws, world)
        # BackgroundBlur in a WidgetComponent samples earlier offscreen UI frames.
        # Its live-world backdrop is outside the scope of this Slate fixture.
        for child in ws.values():
            if isinstance(child,unreal.BackgroundBlur):
                child.set_blur_strength(0)


def capture_contract(asset, variant, widget, resolution):
    ws = widgets(widget)
    icons, geometry, common = [], [], []
    for name, child in ws.items():
        if isinstance(child, unreal.Image):
            obj = child.get_editor_property('brush').get_editor_property('resource_object')
            if obj and '/Game/Assets/UI/Heist/MI_HeistIcon_' in obj.get_path_name():
                icons.append({'widget': name, 'resource': obj.get_path_name(), 'visibility': str(child.get_visibility())})
        if isinstance(child, unreal.UserWidget) and '/Game/Blueprints/UI/Common/' in child.get_class().get_path_name():
            common.append({'widget': name, 'class': child.get_class().get_path_name()})
        if isinstance(child, unreal.TextBlock):
            g = child.get_paint_space_geometry()
            size = unreal.SlateLibrary.get_local_size(g)
            origin = unreal.SlateLibrary.local_to_absolute(g, unreal.Vector2D(0, 0))
            geometry.append({'widget': name, 'text': str(child.get_text()), 'size': [size.x, size.y],
                             'origin': [origin.x, origin.y], 'visibility': str(child.get_visibility())})
    checks = {}
    if asset.endswith('WBP_HeistHUD'):
        expected = widget.get_editor_property('flashlight_on_brush' if variant in ('FlashlightOn', 'Alert') else 'flashlight_off_brush')
        checks['native_flashlight_brush'] = ws['FlashlightIcon'].get_editor_property('brush').get_editor_property('resource_object') == expected.get_editor_property('resource_object')
        checks['new_coin_resource'] = any(i['widget'] == 'PlaceholderIcon' for i in icons)
        checks['new_bag_resource'] = any(i['widget'] == 'InventoryShortcutIcon' for i in icons)
        checks['hud_viewport_dpi_simulated'] = widget.get_path_name() in state['hud_dpi']
    if asset.endswith('WBP_Result'):
        details = variant == 'Details'
        checks['native_detail_header_toggle'] = ((ws['CoopResultDetailHeaderSize'].get_visibility() != unreal.SlateVisibility.COLLAPSED) == details and
                                                (ws['CoopResultCompactHeaderSize'].get_visibility() == unreal.SlateVisibility.COLLAPSED) == details)
        checks['native_detail_row_toggle'] = all((widgets(row)['CoopResultDetailFieldsSize'].get_visibility() != unreal.SlateVisibility.COLLAPSED) == details and
                                                (widgets(row)['CoopResultCompactFieldsSize'].get_visibility() == unreal.SlateVisibility.COLLAPSED) == details
                                                for row in ws['ContributionTableContainer'].get_all_children())
    if asset.endswith('WBP_HeistForgery'):
        checks['single_similarity_label'] = str(ws['PreviewScoreText'].get_text()) == '작품 유사도'
        checks['native_reset_binding'] = widget.get_editor_property('ResetDrawingButton') == ws['ResetDrawingButton']
    if asset.endswith('WBP_Inventory'):
        checks['bag_label'] = str(ws['InventoryTitleText'].get_text()) == '가방'
    if asset.endswith('WBP_TitleMenu'):
        checks['native_inline_join_binding'] = widget.get_editor_property('TitleJoinCodeInput') == ws['TitleJoinCodeInput']
        if variant == 'SettingsOpen':
            child = widget.get_editor_property('SettingsWidget')
            checks['native_settings_open_close'] = state['settings_native_open_close'].get(widget.get_path_name(), False)
            checks['nested_saved_settings_class'] = child.get_class().get_path_name() == \
                '/Game/Blueprints/UI/Title/WBP_Settings.WBP_Settings_C'
            checks['nested_settings_visible'] = child.get_visibility() == unreal.SlateVisibility.VISIBLE
            checks['nested_settings_parent'] = child.get_parent() == ws['TitleRoot']
            checks['settings_above_title_menu'] = isinstance(child.slot, unreal.CanvasPanelSlot) and \
                child.slot.get_editor_property('z_order') > ws['CompositionTitleColumn'].slot.get_editor_property('z_order')
            settings_ws = widgets(child)
            checks['settings_backdrop_visible'] = settings_ws['CompositionMenuBackdrop'].get_visibility() == unreal.SlateVisibility.VISIBLE
    checks['saved_viewport_fit'] = {'HeistConceptDesignSize', 'HeistConceptViewportFit'} <= set(ws)
    if asset != 'Title/WBP_TitleMenu':
        checks['shared_keycap_present'] = any(c['class'].endswith('WBP_HeistKeycap_C') for c in common)
    failed = [name for name, valid in checks.items() if not valid]
    state['errors'].extend(asset + '/' + variant + ': ' + name for name in failed)
    state['contracts'].append({'asset': asset, 'variant': variant, 'resolution': resolution,
                               'icons': icons, 'shared_widgets': common, 'text_geometry': geometry,
                               'checks': checks, 'hud_dpi': state['hud_dpi'].get(widget.get_path_name()),
                               'tick_frequency': str(widget.get_editor_property('tick_frequency'))})


def finish(error=None):
    if state['finished']:
        return
    state['finished'] = True
    if error:
        state['errors'].append(error)
    if state['cb'] is not None:
        unreal.unregister_slate_post_tick_callback(state['cb'])
    items.clear()
    kept_objects.clear()
    globals()['editor_world'] = None
    if state['simulation_started']:
        unreal.EditorLevelLibrary.editor_end_play()
    after = hashes()
    (OUT / 'asset-hashes.json').write_text(json.dumps({'before': BEFORE, 'after': after, 'unchanged': BEFORE == after}, indent=2), encoding='utf-8')
    (OUT / 'composition-fixture-contracts.json').write_text(json.dumps(state['contracts'], ensure_ascii=False, indent=2), encoding='utf-8')
    report = {'screens': state['screens'], 'expected_screens': len(CASES) * len(RESOLUTIONS),
              'errors': state['errors'], 'visual_fixture': True, 'natural_input_tested': False,
              'steam_tested': False, 'multiplayer_tested': False, 'temporary_blank_map_sie': True,
              'package_save_requested': False, 'assets_unchanged': BEFORE == after,
              'design_size': [1280, 720], 'render_resolutions': RESOLUTIONS,
              'saved_design_fit_only': False,
              'hud_dpi_scope': 'Saved HUD Fill is checked before one transient UserSpecified viewport DPI scale; menus retain saved fit.',
              'settings_composite_scope': 'SettingsOpen renders the actual nested saved SettingsWidget after native title open/close/reopen handlers. Controlled values replace live settings data; this is not mouse-input or User PIE evidence.',
              'fixture_artwork': 'Existing template reference images as sample recap art; not live submitted Replica evidence',
              'inventory_scope': 'Empty saved 5x5 grid/frame only. Native confirmed item placement and drag were not exercised.',
              'background_blur_scope': 'Inventory backdrop blur disabled only in transient WidgetComponent fixture; live-world blur requires User PIE.',
              'tick_policy': 'Native ticks remain enabled; only transient display fixture data reapplied after tick',
              'geometry_limit': 'WidgetComponent may not cache positive paint-space geometry. Review exported pixels separately.',
              'visual_review': 'Required separately; successful export is not a visual PASS',
              'render_pass': BEFORE == after and not state['errors'] and len(state['screens']) == len(CASES) * len(RESOLUTIONS)}
    (OUT / 'render-complete.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    if globals().get('QUIT_EDITOR_ON_FINISH', False):
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    if state['finished']:
        return
    try:
        if state['phase'] == 'render':
            world = unreal.EditorLevelLibrary.get_game_world()
            for asset, variant, actor, component, widget in items:
                fixture(asset, variant, widget, world)
                component.request_render_update()
        if time.monotonic() - state['t'] < 4:
            return
        if state['phase'] == 'start':
            for asset in ASSETS:
                bp = unreal.load_asset('/Game/Blueprints/UI/' + asset)
                names = {str(entry.widget_name) for entry in TOOLS.call_method('GetWidgets', (bp,)).widgets}
                assert {'HeistConceptDesignSize', 'HeistConceptViewportFit'} <= names, asset + ' missing saved design fit'
            LEVEL.editor_play_simulate()
            state.update(phase='create', t=time.monotonic(), simulation_started=True)
        elif state['phase'] == 'create':
            world = unreal.EditorLevelLibrary.get_game_world()
            assert world, 'No transient simulation world'
            resolution = RESOLUTIONS[state['resolution']]
            gameplay = unreal.get_default_object(unreal.GameplayStatics)
            for asset, variant in CASES:
                actor = gameplay.call_method('BeginDeferredActorSpawnFromClass', (world, unreal.Actor.static_class(), unreal.Transform(),
                    unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                gameplay.call_method('FinishSpawningActor', (actor, unreal.Transform(), unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                component = actor.call_method('AddComponentByClass', (unreal.WidgetComponent.static_class(), False, unreal.Transform(), False))
                component.set_tick_when_offscreen(True)
                component.set_draw_size(unreal.Vector2D(*resolution))
                component.set_background_color(css('#101416'))
                widget = create_widget(asset, world)
                component.set_widget(widget)
                configure_fixture_layout(asset, widget, resolution)
                fixture(asset, variant, widget, world, first=True)
                component.request_render_update()
                items.append((asset, variant, actor, component, widget))
            state.update(phase='render', t=time.monotonic())
        elif state['phase'] == 'render':
            world = unreal.EditorLevelLibrary.get_game_world()
            resolution = RESOLUTIONS[state['resolution']]
            directory = OUT / ('%dx%d' % resolution)
            directory.mkdir(exist_ok=True)
            for asset, variant, actor, component, widget in items:
                target = component.get_render_target()
                assert target, 'No render target: ' + asset
                fmt = str(target.get_editor_property('render_target_format')).upper()
                suffix = '.exr' if '16F' in fmt or '32F' in fmt else '.png'
                name = asset.split('/')[-1] + ('' if variant == 'Default' else '_' + variant) + suffix
                unreal.RenderingLibrary.export_render_target(world, target, str(directory), name)
                assert (directory / name).is_file(), 'Missing export: ' + name
                capture_contract(asset, variant, widget, resolution)
                state['screens'].append(str((directory / name).relative_to(ROOT)))
                actor.destroy_actor()
            items.clear()
            state['resolution'] += 1
            state.update(phase='create' if state['resolution'] < len(RESOLUTIONS) else 'finish', t=time.monotonic())
        else:
            finish()
    except Exception:
        error = traceback.format_exc()
        unreal.log_error(error)
        finish(error)


BEFORE = hashes()
TEMPLATES = template_rows()
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
TOOLS = unreal.get_default_object(unreal.UMGToolSet)
state = {'phase': 'start', 't': time.monotonic(), 'resolution': 0, 'cb': None,
         'errors': [], 'screens': [], 'contracts': [], 'hud_dpi': {},
         'settings_native_open_close': {}, 'finished': False, 'simulation_started': False}
items, kept_objects = [], []
assert not unreal.EditorLevelLibrary.get_game_world(), 'Do not run during existing PIE/SIE'
dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not dirty_maps or all(p.get_path_name().startswith('/Temp/Untitled') for p in dirty_maps), \
    'Do not discard an authored dirty map during dedicated UI rendering: ' + repr([p.get_path_name() for p in dirty_maps])
# UE Python arrays retain their package references across a world change.
dirty_maps = None
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
editor_world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
editor_world = None
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['cb'] = unreal.register_slate_post_tick_callback(tick)
