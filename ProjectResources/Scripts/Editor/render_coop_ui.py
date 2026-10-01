"""Native Slate display fixtures for the approved co-op UI; no packages saved."""
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()


def css(value):
    value = value.lstrip('#')
    def linear(channel):
        channel /= 255.0
        return channel / 12.92 if channel <= .04045 else ((channel + .055) / 1.055) ** 2.4
    return unreal.LinearColor(*(linear(int(value[i:i+2], 16)) for i in (0, 2, 4)), 1)
CASES = [('HUD/WBP_HeistHUD', v) for v in ['Default', 'LongNames', 'Alert', 'Feedback']] + [
    ('Lobby/WBP_Lobby', v) for v in ['Default', 'TwoPlayers', 'Random', 'ScrollM03']] + [
    ('Title/WBP_Settings', 'Default'), ('Forgery/WBP_HeistForgery', 'NeedsWork'),
    ('Forgery/WBP_HeistForgery', 'AlmostReady'), ('Forgery/WBP_HeistForgery', 'PaletteSelected5'),
    ('Result/WBP_Result', 'Default'), ('Result/WBP_Result', 'Details')]


def callback(asset, variant, widget, world, ws):
    # Fixture values are reapplied after NativeTick; this is display QA only.
    if asset.endswith('WBP_HeistHUD'):
        vm = widget.get_editor_property('hud_view_model')
        vm.set_editor_property('RequiredTargetDisplayName',
            '붉은 달 아래 황금빛 초상의 아주 긴 작품 이름' if variant == 'LongNames' else '황금빛 초상')
        vm.set_editor_property('ContractValueText', '운반·확보 2,400 / 4,000')
        vm.set_editor_property('ContractValueAmountsText', '2,400 / 4,000')
        ws['ContractValueText'].set_text('2,400 / 4,000')
        # The isolated fixture has no contract snapshot. The real ViewModel
        # computes these amount-only values during RefreshPresentationState.
        ws['ContractValueText'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        ws['ContractValueLabel'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        ws['MissionTimeText'].set_text('00 : 45' if variant == 'Alert' else '14 : 32')
        ws['MissionTimeText'].set_color_and_opacity(unreal.SlateColor(css('#e58b82' if variant == 'Alert' else '#edece7')))
        flash_color = css('#edece7' if variant == 'Alert' else '#a4aaa9')
        ws['FlashlightStatusText'].set_color_and_opacity(unreal.SlateColor(flash_color))
        ws['FlashlightIcon'].set_color_and_opacity(flash_color)
        ws['HUDQuickSlot1'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        prefix = ws['HUDQuickSlot1'].get_path_name()
        qw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
              if w.get_path_name().startswith((prefix+'.', prefix+':'))}
        qw['CountText'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        qw['KeyLabelText'].set_text('[Q]')
        for i in range(1, 11):
            image = ws['AlertStar%02d' % i]
            b = image.get_editor_property('brush')
            b.set_editor_property('resource_object', None)
            b.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
            image.set_brush(b)
            fill = (6.5 if variant == 'Alert' else 3.5)-i+1
            image.set_render_transform_pivot(unreal.Vector2D(0, .5))
            image.set_render_scale(unreal.Vector2D(1 if fill >= 1 else .5 if fill >= .5 else 0, 1))
        for i in range(1, 5):
            child = ws['TeamCard'+str(i)]
            child.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            prefix = child.get_path_name()
            cw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_path_name().startswith((prefix+'.', prefix+':'))}
            cw['PlayerNameText'].set_color_and_opacity(unreal.SlateColor(css('#edece7')))
            cw['StatusText'].set_color_and_opacity(unreal.SlateColor(css('#e58b82' if i == 4 else '#a4aaa9')))
        ws['AlertTitleText'].set_color_and_opacity(unreal.SlateColor(css('#a4aaa9')))
        if 'AlertValueText' in ws:
            ws['AlertValueText'].set_text('6.5 / 10' if variant == 'Alert' else '3.5 / 10')
        ws['HUDQuickSlot2'].set_visibility(unreal.SlateVisibility.COLLAPSED)
        ws['HUDQuickSlot3'].set_visibility(unreal.SlateVisibility.COLLAPSED)
        if variant == 'Feedback':
            action = ws['ActionProgressWidget']
            action.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            prefix = action.get_path_name()
            aw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_path_name().startswith((prefix+'.', prefix+':'))}
            aw['ActionProgressContainer'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            aw['ActionTypeText'].set_text('작품 관찰')
            aw['ActionProgressBar'].set_percent(.6)
            key = widget.get_path_name()
            if key not in feedback_widgets:
                klass = unreal.load_class(None, '/Game/Blueprints/UI/HUD/WBP_HeistPopupFeedback.WBP_HeistPopupFeedback_C')
                popup = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (world, klass, None))
                ws['PopupFeedbackLayer'].add_child(popup)
                feedback_widgets[key] = popup
            popup = feedback_widgets[key]
            popup.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            prefix = popup.get_path_name()
            pw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_path_name().startswith((prefix+'.', prefix+':'))}
            pw['PopupText'].set_text('지금은 동전을 사용할 수 없습니다.')
    elif asset.endswith('WBP_Lobby'):
        ws['SelectedMapText'].set_text('무작위' if variant == 'Random' else 'M03')
        for i in range(1, 5):
            child = ws['PlayerCard'+str(i)]
            occupied = variant != 'TwoPlayers' or i <= 2
            child.set_render_opacity(1 if occupied else .45)
            if not occupied:
                prefix = child.get_path_name()
                cw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                      if w.get_path_name().startswith((prefix+'.', prefix+':'))}
                cw['PlayerNameText'].set_text('참가 대기')
        # Native Lobby::NativeConstruct configures the real map IDs/thumbnails.
        scroll = ws['MapHorizontalScrollBox']
        scroll.set_scroll_offset(112 if variant == 'ScrollM03' else 0)
        for code in ['Random', 'M01', 'M02', 'M03']:
            child = ws['Map'+code+'Card']
            prefix = child.get_path_name()
            cw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_path_name().startswith((prefix+'.', prefix+':'))}
            cw['MapNameText'].set_text('무작위' if code == 'Random' else code)
            cw['RandomQuestionText'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if code == 'Random' else unreal.SlateVisibility.COLLAPSED)
            cw['MapThumbnailImage'].set_visibility(unreal.SlateVisibility.HIDDEN if code == 'Random' else unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            cw['SelectedCheckImage'].set_visibility(unreal.SlateVisibility.VISIBLE if code == ('Random' if variant == 'Random' else 'M03') else unreal.SlateVisibility.HIDDEN)
            selected = code == ('Random' if variant == 'Random' else 'M03')
            if 'SelectionBackground' in cw:
                cw['SelectionBackground'].set_render_opacity(1 if selected else 0)
            cw['MapThumbnailImage'].set_color_and_opacity(child.get_editor_property('SelectedThumbnailTint' if selected else 'ThumbnailTint'))
        for i in range(1, 5):
            child = ws['PlayerCard'+str(i)]
            prefix = child.get_path_name()
            cw = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_path_name().startswith((prefix+'.', prefix+':'))}
            if 'ReadyStatusText' in cw:
                cw['ReadyStatusText'].set_text('준비 완료' if i != 4 else '준비 대기')
    elif asset.endswith('WBP_Settings'):
        # No settings ViewModel is attached to this isolated display sample.
        for fill, percent in [('FOVValueFill', .5), ('MouseSensitivityValueFill', .4), ('MasterVolumeValueFill', .8)]:
            ws[fill].set_percent(percent)
    elif asset.endswith('WBP_HeistForgery'):
        assert widget.get_editor_property('ResetDrawingButton') == ws['ResetDrawingButton'], 'ResetDrawingButton native binding missing'
        ws['ResetDrawingButton'].set_visibility(unreal.SlateVisibility.VISIBLE)
        ws['ResetDrawingButton'].set_is_enabled(True)
        ws['DrawingTimeRemainingText'].set_text('00:32')
        ws['PreviewScoreText'].set_color_and_opacity(unreal.SlateColor(css('#a4aaa9')))
        ws['PreviewQualityBar'].set_fill_color_and_opacity(css('#e58b82' if variant == 'NeedsWork' else '#d6b681' if variant == 'AlmostReady' else '#9dbca7'))
    elif asset.endswith('WBP_Result'):
        ws['TeamRewardTextBlock'].set_text('$24,800')
        ws['CoopResultSuccessCheck'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        ws['CoopResultFailedCross'].set_visibility(unreal.SlateVisibility.COLLAPSED)
        if variant == 'Details' and widget.get_path_name() not in details_opened:
            widget.call_method('HandleRewardDetailsClicked', ())
            details_opened.add(widget.get_path_name())


details_opened = set()
feedback_widgets = {}
runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/render_catalogue_ui.py'), init_globals={
    'QA_FOLDER': 'CoopUIFaithful20261001', 'RENDER_ASSETS': sorted({c[0] for c in CASES}),
    'RENDER_CASES': CASES, 'FIXTURE_CALLBACK': callback, 'USE_SAVED_DESIGN_FIT': True})
