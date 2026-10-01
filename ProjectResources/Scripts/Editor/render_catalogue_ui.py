"""Render transient UI fixtures in a dedicated Editor; never save packages.

By default, legacy shells receive an unsaved 1920x1080 SizeBox/ScaleBox wrapper.
USE_SAVED_DESIGN_FIT instead requires the existing saved 1280x720 WBP wrapper
and adds no render-only layout. Both paths use actual Slate font rasterization.
These are presentation fixtures, not natural input or Steam QA.
"""
import hashlib
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation' / globals().get('QA_FOLDER', 'UIUX20260930') / 'Visual'
OUT.mkdir(parents=True, exist_ok=True)
RESOLUTIONS = [(1920, 1080), (1280, 720)]
ASSETS = ['Title/WBP_TitleMenu', 'Lobby/WBP_Lobby', 'HUD/WBP_HeistHUD',
          'Inventory/WBP_Inventory', 'HUD/WBP_HeistFloorPlanMap',
          'Forgery/WBP_HeistForgery', 'Result/WBP_Result',
          'Title/WBP_Settings', 'Title/WBP_SessionJoin']
CASES = [(a, 'Default') for a in ASSETS] + [
    ('HUD/WBP_HeistHUD', 'LongNames'), ('HUD/WBP_HeistHUD', 'Alert'),
    ('HUD/WBP_HeistHUD', 'Tutorial'), ('Forgery/WBP_HeistForgery', 'PaletteSelected5'),
    ('Forgery/WBP_HeistForgery', 'NeedsWork'), ('Forgery/WBP_HeistForgery', 'AlmostReady'),
    ('Title/WBP_SessionJoin', 'Error'), ('Result/WBP_Result', 'Partial'),
    ('Lobby/WBP_Lobby', 'TwoPlayers')]
if 'RENDER_ASSETS' in globals():
    ASSETS = [a for a in ASSETS if a in RENDER_ASSETS]
    CASES = [c for c in CASES if c[0] in RENDER_ASSETS]
if 'RENDER_CASES' in globals():
    CASES = RENDER_CASES
PALETTE = [unreal.LinearColor(*c) for c in [
    (.82, .74, .57, 1), (.74, .45, .11, 1), (.10, .24, .28, 1),
    (.025, .06, .06, 1), (.42, .10, .065, 1)]]
# HeistGameMode DefaultPlayerColors and HeistCrewStatus presentation values.
PLAYER_COLORS = [unreal.LinearColor(*c) for c in
                 [(1, 0, 0, 1), (0, 1, 0, 1), (0, 0, 1, 1), (1, 1, 0, 1)]]
CREW_STATES = [('활동 중', (.22, .42, .62, 1), None),
               ('위조 중', (.90, .38, .08, 1), 'forging_status_icon'),
               ('원본 운반', (.85, .60, .08, 1), 'carrying_original_status_icon'),
               ('체포', (.68, .06, .12, 1), 'arrested_status_icon')]
FILES = sorted(set(list((ROOT/'Content/Blueprints/UI').rglob('*.uasset')) +
                   list((ROOT/'Content/Assets/UI').rglob('*.uasset')) +
                   list((ROOT/'Content/Maps').rglob('*.umap'))))


def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in FILES}


BEFORE = hashes()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {'phase': 'start', 't': time.monotonic(), 'resolution': 0, 'cb': None,
         'errors': [], 'measurement_warnings': [], 'screens': [], 'geometry': [], 'native_selection': [],
         'instance_ticks': [], 'finished': False}
items, kept_objects, widget_cache = [], [], {}
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
editor_world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)


def widgets(widget):
    prefix = widget.get_path_name()
    if prefix not in widget_cache:
        widget_cache[prefix] = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                                if w.get_path_name().startswith((prefix+'.', prefix+':'))}
    return widget_cache[prefix]


def make_widget(asset, world):
    path = '/Game/Blueprints/UI/'+asset
    klass = unreal.load_class(None, path+'.'+asset.split('/')[-1]+'_C')
    if not klass:
        raise RuntimeError('Missing fixture class: '+path)
    widget = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (world, klass, None))
    if not widget:
        raise RuntimeError('Create Widget returned null: '+path)
    # Read only: native ticks remain enabled. Presentation fixtures are reapplied
    # after Slate tick, without modifying class or instance TickFrequency.
    state['instance_ticks'].append({
        'asset': path, 'class': klass.get_path_name(), 'instance': widget.get_path_name(),
        'cdo': unreal.get_default_object(klass).get_path_name(),
        'cdo_tick': str(unreal.get_default_object(klass).get_editor_property('tick_frequency')),
        'instance_tick': str(widget.get_editor_property('tick_frequency'))})
    widget.set_visibility(unreal.SlateVisibility.VISIBLE)
    kept_objects.append(widget)
    return widget


def prepare_design_canvases():
    if globals().get('USE_SAVED_DESIGN_FIT', False):
        tools = unreal.get_default_object(unreal.UMGToolSet)
        for asset in ASSETS:
            bp = unreal.load_asset('/Game/Blueprints/UI/'+asset)
            names = {str(i.widget_name) for i in tools.call_method('GetWidgets', (bp,)).widgets}
            assert {'HeistConceptDesignSize', 'HeistConceptViewportFit'} <= names, asset+' missing saved viewport fit'
        return
    # WidgetTree has protected Python access. Use the Editor's existing UMG
    # API to wrap each loaded shell in memory only. Never save these packages;
    # the dedicated render process discards the wrappers on exit.
    tools = unreal.get_default_object(unreal.UMGToolSet)
    for asset in ASSETS:
        bp = unreal.load_asset('/Game/Blueprints/UI/'+asset)
        original = tools.call_method('GetWidgets', (bp,)).widgets[0].widget
        design = tools.call_method('WrapWidgets', (bp, [original], unreal.SizeBox.static_class()))[0].widget
        design.set_width_override(1920)
        design.set_height_override(1080)
        fit = tools.call_method('WrapWidgets', (bp, [design], unreal.ScaleBox.static_class()))[0].widget
        fit.set_stretch(unreal.Stretch.SCALE_TO_FIT)
        fit.set_stretch_direction(unreal.StretchDirection.BOTH)
        assert tools.call_method('CompileWidgetBlueprint', (bp,)), asset
        kept_objects.extend([bp, design, fit])


def txt(ws, name, value):
    if name in ws:
        ws[name].set_text(value)


def visible(ws, name, show=True):
    if name in ws:
        ws[name].set_visibility(unreal.SlateVisibility.VISIBLE if show else unreal.SlateVisibility.COLLAPSED)


def texture(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing fixture texture: '+path)
    return asset


def populate_inventory(ws, world):
    grid = ws['InventoryGrid']
    grid.clear_children()
    grid.set_editor_property('min_desired_slot_width', 128)
    grid.set_editor_property('min_desired_slot_height', 128)
    for index in range(25):
        cell = make_widget('Inventory/WBP_InventorySlot', world)
        slot = grid.add_child_to_uniform_grid(cell, index//5, index%5)
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        visible(widgets(cell), 'CoordinateText', False)
        visible(widgets(cell), 'OccupancyText', False)
    overlay = ws.get('ItemOverlay')
    if not isinstance(overlay, unreal.CanvasPanel):
        raise RuntimeError('Inventory fixture requires the real ItemOverlay CanvasPanel')
    overlay.clear_children()
    examples = [(0, 0, 1, 2, '/Game/Data/Forgery/Textures/M01/T_Forgery_M01_Portrait_01'),
                (1, 0, 2, 2, '/Game/Data/Forgery/Textures/M01/T_Forgery_M01_Portrait_02'),
                (0, 2, 3, 2, '/Game/Data/Forgery/Textures/T_Forgery_SunArchWave')]
    for x, y, width, height, path in examples:
        item = make_widget('Inventory/WBP_InventoryItem', world)
        widgets(item)['PlaceholderIcon'].set_brush_from_texture(texture(path), True)
        slot = overlay.add_child_to_canvas(item)
        slot.set_position(unreal.Vector2D(x*128+4, y*128+4))
        slot.set_size(unreal.Vector2D(width*128-8, height*128-8))
        slot.set_auto_size(False)


def populate_result(ws, world, variant):
    ws['ContributionTableContainer'].clear_children()
    ws['ReplicaRecapVisualContainer'].clear_children()
    for i in range(4):
        row = make_widget('Result/WBP_ResultPlayerRow', world)
        ws['ContributionTableContainer'].add_child_to_vertical_box(row).set_padding(unreal.Margin(0, 0, 0, 4))
        rw = widgets(row)
        for name, w in rw.items():
            if isinstance(w, unreal.TextBlock):
                values = {'PlayerNameText': ['진품을들고달리는플레이어12345', '짧은이름', 'PLAYER 123456789', '가장긴이름을쓰는플레이어123456'][i],
                          'PlayerStateText': '체포' if variant == 'Partial' and i == 3 else '탈출',
                          'SecuredLootValueText': ['6,200','120','24,800','1,000'][i], 'BestSurfaceQualityText': ['84','100','70','0'][i]}
                w.set_text(values.get(name, str(i)))
        fallback = row.get_editor_property('default_profile_texture')
        if fallback and 'ProfileImage' in rw:
            rw['ProfileImage'].set_brush_from_texture(fallback, True)
        card = make_widget('Result/WBP_ResultReplicaCard', world)
        ws['ReplicaRecapVisualContainer'].add_child_to_horizontal_box(card).set_padding(unreal.Margin(0, 0, 12, 0))
        cw = widgets(card)
        txt(cw, 'ArtifactNameText', '황금빛 초상' if i == 0 else '전시 작품 '+str(i+1))
        txt(cw, 'QualityText', '품질 '+str(84+i*3))
        visible(cw, 'RequiredTargetBadge', i == 0)
        cw['ReplicaImage'].set_brush_from_texture(texture('/Game/Data/Forgery/Textures/T_Forgery_SunArchWave'), False)
        visible(cw, 'ReplicaImage')


def fixture(asset, variant, widget, world, first=False):
    # NativeConstruct/NativeTick may refresh an unbound presentation fixture.
    # Reapply only transient display values from this Slate post-tick callback.
    widget.set_visibility(unreal.SlateVisibility.VISIBLE)
    ws = widgets(widget)
    # NativeConstruct can set initial state. Reassert only transient data while
    # rendering; no authored geometry/brush/style is changed for these fixtures.
    if asset.endswith('WBP_Settings'):
        for n, v in [('FOVValueText', '90'), ('MouseSensitivityValueText', '1.0'), ('MasterVolumeValueText', '80%')]:
            txt(ws, n, v)
        for n, v in [('FOVSlider', .5), ('MouseSensitivitySlider', .4), ('MasterVolumeSlider', .8)]:
            if n in ws:
                ws[n].set_value(v)
        if first:
            for n, v in [('ResolutionComboBox', '1920 × 1080'), ('WindowModeComboBox', '전체 화면')]:
                if n in ws:
                    ws[n].add_option(v)
                    ws[n].set_selected_option(v)
    elif asset.endswith('WBP_SessionJoin'):
        ws['JoinCodeInput'].set_text('MUSE24')
        txt(ws, 'SessionErrorText', '방을 찾을 수 없습니다. 참가 코드를 확인하세요.' if variant == 'Error' else '')
        visible(ws, 'SessionErrorText', variant == 'Error')
        visible(ws, 'RetrySessionSize', variant == 'Error')
        visible(ws, 'CancelSessionSize', False)
    elif asset.endswith('WBP_Lobby'):
        txt(ws, 'JoinCodeText', 'MUSE24')
        txt(ws, 'PlayerCountText', '2 / 4' if variant == 'TwoPlayers' else '4 / 4')
        for i in range(1, 5):
            child = ws.get('PlayerCard'+str(i))
            if child:
                cw = widgets(child)
                occupied = variant != 'TwoPlayers' or i <= 2
                txt(cw, 'PlayerNameText', ('진품을들고달리는플레이어123456' if i == 1 else 'PLAYER '+str(i)) if occupied else '')
                # Hidden preserves the fixed check slot just like ApplyPlayerData.
                cw['ReadyCheckImage'].set_visibility(unreal.SlateVisibility.VISIBLE if occupied and i != 4 else unreal.SlateVisibility.HIDDEN)
                txt(cw, 'ReadyButtonLabel', '준비 완료' if occupied and i != 4 else '준비')
                if 'ReadyButton' in cw:
                    cw['ReadyButton'].set_is_enabled(i == 1)
    elif asset.endswith('WBP_HeistHUD'):
        if first:
            vm = unreal.new_object(unreal.HeistHUDViewModel, outer=widget)
            vm.set_editor_property('flashlight_enabled', variant == 'Alert')
            vm.set_editor_property('flashlight_status_text', '[F] 손전등 ON' if variant == 'Alert' else '[F] 손전등 OFF')
            widget.set_editor_property('hud_view_model', vm)
            widget.call_method('RefreshHUDPresentation', ())
            kept_objects.append(vm)
        txt(ws, 'MissionTimeText', '00 : 45' if variant == 'Alert' else '14 : 32')
        ws['MissionTimeText'].set_color_and_opacity(unreal.SlateColor(
            unreal.LinearColor(.90, .12, .10, 1) if variant == 'Alert' else unreal.LinearColor(1, 1, 1, 1)))
        txt(ws, 'RequiredTargetNameText', '붉은 달 아래 황금빛 초상의 아주 긴 작품 이름' if variant == 'LongNames' else '황금빛 초상')
        txt(ws, 'ContractValueText', '운반·확보 2,800 / 4,000')
        txt(ws, 'FlashlightStatusText', 'ON' if variant == 'Alert' else 'OFF')
        ws['FlashlightStatusText'].set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(
            *(.92, .82, .64, 1) if variant == 'Alert' else (.65, .63, .57, 1))))
        visible(ws, 'HUDQuickSlot2', False)
        visible(ws, 'HUDQuickSlot3', False)
        if 'HUDQuickSlot1' in ws:
            qw = widgets(ws['HUDQuickSlot1'])
            txt(qw, 'KeyLabelText', 'Q')
            txt(qw, 'CountText', '3')
            qw['PlaceholderIcon'].set_brush_from_texture(texture('/Game/Assets/UI/HUD/QuickSlot/T_CoinQuickSlot'), False)
        for i in range(1, 5):
            child = ws.get('TeamCard'+str(i))
            if child:
                child.set_visibility(unreal.SlateVisibility.VISIBLE)
                cw = widgets(child)
                txt(cw, 'PlayerNameText', '진품을들고달리는플레이어' if variant == 'LongNames' else 'PLAYER '+str(i))
                cw['PlayerNameText'].set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(.92, .92, .88, 1)))
                text, color, icon_property = CREW_STATES[i-1]
                txt(cw, 'StatusText', text)
                cw['StatusText'].set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(*color)))
                if 'PlayerColorMarker' in cw:
                    cw['PlayerColorMarker'].set_color_and_opacity(PLAYER_COLORS[i-1])
                    visible(cw, 'PlayerColorMarker')
                if 'StatusIcon' in cw:
                    icon = child.get_editor_property(icon_property) if icon_property else None
                    if icon:
                        cw['StatusIcon'].set_brush_from_texture(icon, False)
                    visible(cw, 'StatusIcon', bool(icon))
        alert_color = unreal.LinearColor(.96, .34, .28, 1) if variant == 'Alert' else unreal.LinearColor(.88, .86, .80, 1)
        ws['AlertTitleText'].set_color_and_opacity(unreal.SlateColor(alert_color))
        for i in range(1, 11):
            star = ws.get('AlertStar%02d'%i)
            if star:
                filled = variant == 'Alert' and i <= 7
                star.set_brush_from_texture(texture('/Game/Assets/UI/HUD/Alert/T_AlertStar_'+('Full' if filled else 'Empty')), False)
                star.set_color_and_opacity(alert_color if filled else unreal.LinearColor(.36, .38, .38, 1))
        txt(ws, 'AlertEventText', '구속 해제까지 3초 · 동료가 구조할 수 있습니다')
        visible(ws, 'AlertEventText', variant == 'Alert')
        visible(ws, 'TutorialCardContainer', variant == 'Tutorial')
        txt(ws, 'TutorialTitleText', '작품 관찰')
        txt(ws, 'TutorialBodyText', '작품 앞에서 E를 눌러 관찰하세요. 주변의 경비와 CCTV를 주의하세요.')
        txt(ws, 'TutorialProgressText', '단계 2/8')
    elif asset.endswith('WBP_Inventory') and first:
        populate_inventory(ws, world)
    elif asset.endswith('WBP_HeistForgery'):
        if first:
            vm = unreal.new_object(unreal.HeistForgeryViewModel, outer=widget)
            for name, value in [('drawing_visible', True), ('presentation_visible', True),
                                ('allowed_palette', PALETTE), ('stroke_limit', 4096), ('brush_size', .018)]:
                vm.set_editor_property(name, value)
            widget.set_editor_property('forgery_view_model', vm)
            kept_objects.append(vm)
            selected = 4 if variant == 'PaletteSelected5' else 0
            selection_result = widget.call_method('SelectPaletteIndex', (selected,))
            state['native_selection'].append({'variant': variant, 'selected': selected, 'result': selection_result})
        visible(ws, 'DrawingContainer')
        visible(ws, 'DrawingTimeRemainingText')
        txt(ws, 'DrawingTimeRemainingText', '남은 시간  00:32')
        score = 24 if variant == 'NeedsWork' else 62 if variant == 'AlmostReady' else 78
        ready = score >= 70
        txt(ws, 'PreviewScoreText', '작품 유사도')
        ratio = min(score/70, 1)
        color = unreal.LinearColor(.32,.78,.48,1) if ready else unreal.LinearColor(.82+(.94-.82)*ratio,.22+(.60-.22)*ratio,.18+(.22-.18)*ratio,1)
        ws['PreviewScoreText'].set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(.72,.76,.82,1)))
        ws['PreviewQualityBar'].set_percent(score/100)
        ws['PreviewQualityBar'].set_fill_color_and_opacity(color)
        visible(ws, 'PreviewQualityBar')
        for name in ['SubmitButton', 'CancelButton']:
            visible(ws, name)
            ws[name].set_is_enabled(ready or name == 'CancelButton')
        ws['ReferenceImage'].set_brush_from_texture(texture('/Game/Data/Forgery/Textures/T_Forgery_SunArchWave'), False)
        parent = ws['DrawingTimeRemainingText'].get_parent()
        while parent:
            parent.set_visibility(unreal.SlateVisibility.VISIBLE)
            parent = parent.get_parent()
    elif asset.endswith('WBP_Result'):
        txt(ws, 'OutcomeTextBlock', '부분 성공' if variant == 'Partial' else '계약 성공')
        txt(ws, 'OutcomeReasonTextBlock', '일부 전리품을 확보하고 탈출했습니다.' if variant == 'Partial' else '필수 목표를 반출하고 계약 할당량을 달성했습니다.')
        txt(ws, 'TeamRewardTextBlock', '팀 확보 가치  $24,800')
        visible(ws, 'ReturnToLobbyButton')
        if first:
            populate_result(ws, world, variant)


def record_geometry(asset, variant, widget, resolution):
    entries = []
    selected_names = ['DrawingTimeRemainingText', 'DrawingSurface', 'MissionPanel',
                      'RequiredTargetNameText', 'TeamCardsPanel', 'InventoryPanel', 'InventoryGrid',
                      'ItemOverlay', 'SessionErrorText', 'SubmitButton', 'ContributionTablePanel', 'FlashlightStatusText']
    for name, w in widgets(widget).items():
        if name not in selected_names:
            continue
        # WidgetComponent draws without a normal viewport tick; record the
        # geometry actually used by the Slate paint rather than TickSpaceGeometry.
        g = w.get_paint_space_geometry()
        size = unreal.SlateLibrary.get_local_size(g)
        absolute_size = unreal.SlateLibrary.get_absolute_size(g)
        origin = unreal.SlateLibrary.local_to_absolute(g, unreal.Vector2D(0, 0))
        entries.append({'widget': name, 'local_size': [size.x, size.y],
                        'absolute_size': [absolute_size.x, absolute_size.y],
                        'origin': [origin.x, origin.y], 'visibility': str(w.get_visibility()),
                        'opacity': w.get_render_opacity(), 'text': str(w.get_text()) if isinstance(w, unreal.TextBlock) else None})
    if asset.endswith('WBP_HeistForgery'):
        timer = next(e for e in entries if e['widget'] == 'DrawingTimeRemainingText')
        bound_timer = widget.get_editor_property('drawing_time_remaining_text')
        timer['matches_native_binding'] = bound_timer == widgets(widget)['DrawingTimeRemainingText']
        if not timer['matches_native_binding']:
            state['errors'].append('Timer fixture does not match native BindWidget')
        if min(timer['local_size']) <= 0:
            # Offscreen WidgetComponent painting may leave UWidget's cached
            # geometry unset. Keep the limitation visible; exported pixels
            # still require a separate visual review, not a geometry PASS.
            state['measurement_warnings'].append('Cached timer geometry unavailable: '+str(resolution))
        for name, w in widgets(widget).items():
            if name.startswith('PaletteButton') and isinstance(w, unreal.Button):
                transform = w.get_editor_property('render_transform')
                entries.append({'widget': name, 'scale': [transform.scale.x, transform.scale.y],
                                'opacity': w.get_render_opacity(), 'native_selected_brush': str(w.get_editor_property('widget_style').normal)})
    state['geometry'].append({'asset': asset, 'variant': variant, 'resolution': resolution,
                              'instance_tick': str(widget.get_editor_property('tick_frequency')), 'entries': entries})


def finish(error=None):
    if state['finished']:
        return
    state['finished'] = True
    if error:
        state['errors'].append(error)
    try:
        unreal.EditorLevelLibrary.editor_end_play()
    except Exception:
        pass
    after = hashes()
    (OUT/'asset-hashes.json').write_text(json.dumps({'before': BEFORE, 'after': after, 'unchanged': BEFORE == after}, indent=2), encoding='utf-8')
    (OUT/'geometry.json').write_text(json.dumps(state['geometry'], ensure_ascii=False, indent=2), encoding='utf-8')
    (OUT/'render-complete.json').write_text(json.dumps({
        'screens': state['screens'], 'expected_screens': len(CASES)*len(RESOLUTIONS),
        'visual_fixture': True, 'natural_input_tested': False, 'steam_tested': False,
        'temporary_blank_map_sie': True, 'package_save_requested': False, 'assets_unchanged': BEFORE == after,
        'design_size': [1280, 720] if globals().get('USE_SAVED_DESIGN_FIT', False) else [1920, 1080],
        'scaling': 'Saved WBP SizeBox1280x720/ScaleBoxScaleToFit; no render-only layout wrapper' if globals().get('USE_SAVED_DESIGN_FIT', False) else 'Existing WBP shells wrapped in Editor memory with SizeBox1920x1080/ScaleBoxScaleToFit and compiled without saving; discarded on Editor exit',
        'tick_policy': 'Read-only actual tick records; native ticks enabled; post-tick display fixture reapplied',
        'instance_ticks': state['instance_ticks'],
        'native_selection': state['native_selection'], 'errors': state['errors'],
        'measurement_warnings': state['measurement_warnings'],
        'geometry_validation_pass': None if state['measurement_warnings'] else not state['errors'],
        'visual_review': 'Required separately against exported pixels',
        'render_pass': BEFORE == after and not state['errors'] and len(state['screens']) == len(CASES)*len(RESOLUTIONS)
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(state['cb'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    if state['finished']:
        return
    try:
        if state['phase'] == 'render':
            world = unreal.EditorLevelLibrary.get_game_world()
            for asset, variant, actor, component, widget in items:
                fixture(asset, variant, widget, world)
                if 'FIXTURE_CALLBACK' in globals():
                    FIXTURE_CALLBACK(asset, variant, widget, world, widgets(widget))
                component.request_render_update()
        if time.monotonic()-state['t'] < 4:
            return
        if state['phase'] == 'start':
            prepare_design_canvases()
            level.editor_play_simulate()
            state.update(phase='create', t=time.monotonic())
        elif state['phase'] == 'create':
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world:
                raise RuntimeError('No transient simulation world')
            resolution = RESOLUTIONS[state['resolution']]
            for asset, variant in CASES:
                actor = unreal.get_default_object(unreal.GameplayStatics).call_method('BeginDeferredActorSpawnFromClass',
                    (world, unreal.Actor.static_class(), unreal.Transform(), unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                unreal.get_default_object(unreal.GameplayStatics).call_method('FinishSpawningActor', (actor, unreal.Transform(), unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                component = actor.call_method('AddComponentByClass', (unreal.WidgetComponent.static_class(), False, unreal.Transform(), False))
                component.set_tick_when_offscreen(True)
                component.set_draw_size(unreal.Vector2D(*resolution))
                component.set_background_color(unreal.LinearColor(.018, .022, .027, 1))
                widget = make_widget(asset, world)
                component.set_widget(widget)
                for w in widgets(widget).values():
                    if isinstance(w, unreal.UserWidget):
                        state['instance_ticks'].append({
                            'asset': asset, 'class': w.get_class().get_path_name(),
                            'instance': w.get_path_name(), 'nested': True,
                            'instance_tick': str(w.get_editor_property('tick_frequency'))})
                fixture(asset, variant, widget, world, first=True)
                if 'FIXTURE_CALLBACK' in globals():
                    FIXTURE_CALLBACK(asset, variant, widget, world, widgets(widget))
                component.request_render_update()
                items.append((asset, variant, actor, component, widget))
            state.update(phase='render', t=time.monotonic())
        elif state['phase'] == 'render':
            world = unreal.EditorLevelLibrary.get_game_world()
            resolution = RESOLUTIONS[state['resolution']]
            directory = OUT/('%dx%d'%resolution)
            directory.mkdir(exist_ok=True)
            for asset, variant, actor, component, widget in items:
                target = component.get_render_target()
                if not target:
                    raise RuntimeError('No UI render target: '+asset)
                fmt = str(target.get_editor_property('render_target_format')).upper()
                suffix = '.exr' if '16F' in fmt or '32F' in fmt else '.png'
                name = asset.split('/')[-1]+('' if variant == 'Default' else '_'+variant)+suffix
                unreal.RenderingLibrary.export_render_target(world, target, str(directory), name)
                if not (directory/name).is_file():
                    raise RuntimeError('UI export was not written: '+name)
                record_geometry(asset, variant, widget, resolution)
                state['screens'].append(str((directory/name).relative_to(ROOT)))
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


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['cb'] = unreal.register_slate_post_tick_callback(tick)
