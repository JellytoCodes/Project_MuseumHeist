"""Apply the approved restrained co-op presentation to existing Widget Blueprints.

Run in a dedicated Unreal Editor after saving and closing the user's Editor.
No map, texture, font, gameplay Blueprint or new package is created.
"""
import hashlib
import json
import runpy
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/CoopUIFaithful20261001'
OUT.mkdir(parents=True, exist_ok=True)
# Reuse the existing Editor operations without invoking its old layout pass.
helper_source = (ROOT/'ProjectResources/Scripts/Editor/apply_catalogue_ui.py').read_text(encoding='utf-8')
H = {'QA_FOLDER': 'CoopUIFaithful20261001'}
exec(compile(helper_source[:helper_source.index('paths=sorted(')], 'catalogue_helpers', 'exec'), H)
TOOLS = H['TOOLS']
def css_color(value, alpha=1):
    value = value.lstrip('#')
    def linear(channel):
        channel = channel / 255.0
        return channel / 12.92 if channel <= .04045 else ((channel + .055) / 1.055) ** 2.4
    return unreal.LinearColor(*(linear(int(value[i:i+2], 16)) for i in (0, 2, 4)), alpha)


WHITE = css_color('#edece7')
MUTED = css_color('#a4aaa9')
INK = css_color('#101416')
SURFACE = css_color('#101416', .96)
EDGE = css_color('#313536')
TRANSPARENT = unreal.LinearColor(0, 0, 0, 0)
H.update(IVORY=WHITE, GOLD=WHITE, MUTED=MUTED, INK=INK, SURFACE=SURFACE, EDGE=EDGE)
H.update(WHITE=WHITE, css_color=css_color)
old_brush = H['brush']


def brush(key=None, color=WHITE, size=(4, 4), outline=None):
    b = old_brush(key, color, size, outline)
    if key is None:
        s = b.get_editor_property('outline_settings')
        s.set_editor_property('corner_radii', unreal.Vector4(0, 0, 0, 0))
        s.set_editor_property('width', 0)
        b.set_editor_property('outline_settings', s)
    return b


H['brush'] = brush
margin, font, label, box, pos = (H[n] for n in ['margin', 'font', 'label', 'box', 'pos'])
flat, fixed, add = (H[n] for n in ['flat', 'fixed_bounds', 'add_widget'])


def flat(w, color=SURFACE, outline=None):
    w.set_brush(brush(None, color, (4, 4), outline))
    w.set_brush_color(unreal.LinearColor(1, 1, 1, 1))


H['flat'] = flat


def label(w, size=None, heading=False, color=WHITE):
    font(w, size, H['BOLD'] if heading else H['BODY'])
    w.set_color_and_opacity(H['sc'](color))


H['label'] = label


def button_style(existing=None, primary=False):
    s = existing or unreal.ButtonStyle()
    # Slate blends in linear light; authored menu colors match the approved
    # browser's sRGB compositing over #101416 instead of becoming bright gray.
    for key, color in [('normal', WHITE if primary else css_color('#212526')),
                       ('hovered', css_color('#d6d9d6') if primary else css_color('#323537')),
                       ('pressed', css_color('#c4cac6') if primary else css_color('#323537')),
                       ('disabled', css_color('#212526' if primary else '#171b1d'))]:
        s.set_editor_property(key, brush(color=color))
    for key in ['normal_foreground', 'hovered_foreground', 'pressed_foreground']:
        s.set_editor_property(key, H['sc'](INK if primary else WHITE))
    s.set_editor_property('disabled_foreground', H['sc'](MUTED))
    s.set_editor_property('normal_padding', margin(24, 12, 24, 12))
    s.set_editor_property('pressed_padding', margin(24, 12, 24, 12))
    return s


def style_button(w, primary=False):
    w.set_style(button_style(w.get_editor_property('widget_style'), primary))
    w.set_background_color(unreal.LinearColor(1, 1, 1, 1))
    w.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    def inherit(child):
        if isinstance(child, unreal.TextBlock):
            font(child, 16, H['BODY'])
            child.set_editor_property('min_desired_width', 0)
            child.set_editor_property('justification', unreal.TextJustify.CENTER)
            child.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
        if isinstance(child, unreal.PanelWidget):
            for c in child.get_all_children():
                inherit(c)
    for c in w.get_all_children():
        c.slot.set_padding(margin(0))
        c.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        c.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        inherit(c)


H.update(button_style=button_style, style_button=style_button)


def fit_authored_screen(bp):
    widgets = {str(i.widget_name): i.widget for i in TOOLS.call_method('GetWidgets', (bp,)).widgets if i.widget}
    if 'HeistConceptDesignSize' in widgets:
        return
    original = TOOLS.call_method('GetWidgets', (bp,)).widgets[0].widget
    design = TOOLS.call_method('WrapWidgets', (bp, [original], unreal.SizeBox.static_class()))[0].widget
    design = TOOLS.call_method('RenameWidget', (bp, design, 'HeistConceptDesignSize')).widget
    box(design, 1280, 720)
    fit = TOOLS.call_method('WrapWidgets', (bp, [design], unreal.ScaleBox.static_class()))[0].widget
    fit = TOOLS.call_method('RenameWidget', (bp, fit, 'HeistConceptViewportFit')).widget
    fit.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    fit.set_stretch_direction(unreal.StretchDirection.BOTH)


def neutral_defaults(bp, ws):
    H['CURRENT_ALREADY_FIXED'] = True
    for n, w in ws.items():
        w.modify()
        if isinstance(w, unreal.TextBlock):
            H['label'](w, heading=('Title' in n), color=WHITE)
        elif isinstance(w, unreal.Border) and n not in ['DrawingSurface', 'CrewStatusBadge']:
            flat(w, TRANSPARENT)
        elif isinstance(w, unreal.Button) and not n.startswith('PaletteButton'):
            H['style_button'](w)
        if isinstance(w, (unreal.Slider, unreal.ComboBoxString)):
            H['controls'](w)


def other_screens(bp, key, ws):
    # All coordinates below are the accepted HTML's 1280 x 720 design space.
    # The entry point adds one screen ScaleBox; bound controls and gameplay data
    # stay in their existing classes and WidgetTree.
    W = lambda n: ws[n]
    def refresh():
        ws.update({str(i.widget_name): i.widget for i in TOOLS.call_method('GetWidgets', (bp,)).widgets if i.widget})

    def node(cls, name, parent):
        if name not in ws:
            ws[name] = TOOLS.call_method('AddWidget', (bp, cls.static_class(), name, parent, -1)).widget
        return ws[name]

    def move(w, parent, index=-1):
        if w.get_parent() != parent:
            w = TOOLS.call_method('MoveWidget', (bp, w, parent, index)).widget
            refresh()
        return w

    def place(w, parent, x, y, width, height):
        w = move(w, parent)
        pos(w, x, y, width, height)
        return w

    def wrap(w, cls, name):
        if name in ws:
            return W(name)
        wrapper = TOOLS.call_method('WrapWidgets', (bp, [w], cls.static_class()))[0].widget
        wrapper = TOOLS.call_method('RenameWidget', (bp, wrapper, name)).widget
        refresh()
        return wrapper

    def text(w, size, color=WHITE, justify=unreal.TextJustify.LEFT):
        label(w, size, color=color)
        w.set_editor_property('justification', justify)
        w.set_editor_property('min_desired_width', 0)
        w.set_editor_property('auto_wrap_text', False)
        w.set_editor_property('wrap_text_at', 0)
        w.set_shadow_offset(unreal.Vector2D(0, 0))
        w.set_shadow_color_and_opacity(TRANSPARENT)

    def caption(parent, name, content, x, y, width, height, size=12, color=MUTED,
                justify=unreal.TextJustify.LEFT):
        w = node(unreal.TextBlock, name, parent)
        w.set_text(content)
        text(w, size, color, justify)
        place(w, parent, x, y, width, height)
        return w

    def rule(parent, name, x, y, width):
        # A 4px layout box carries the one-pixel divider used by the mockup.
        w = node(unreal.Image, name, parent)
        w.set_brush(brush(None, EDGE, (4, 4)))
        w.set_render_scale(unreal.Vector2D(1, .25))
        place(w, parent, x, y, width, 4)
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    def button(w, primary=False, quiet=False, size=16):
        H['style_button'](w, primary)
        s = w.get_editor_property('widget_style')
        if quiet:
            s.set_editor_property('normal', brush(None, TRANSPARENT, (4, 4)))
            s.set_editor_property('hovered', brush(None, css_color('#ffffff', .08), (4, 4)))
            s.set_editor_property('pressed', brush(None, css_color('#ffffff', .14), (4, 4)))
            w.set_style(s)
        for child in w.get_all_children():
            child.slot.set_padding(margin(0))
            child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            child.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        def inherit(child):
            if isinstance(child, unreal.TextBlock):
                text(child, size, WHITE, unreal.TextJustify.CENTER)
                child.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
            elif isinstance(child, unreal.PanelWidget):
                for c in child.get_all_children():
                    inherit(c)
        inherit(w)

    def fullscreen_background(w):
        flat(w, css_color('#101416'))
        w.set_padding(margin(0))
        w.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        w.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        if isinstance(w.slot, unreal.CanvasPanelSlot):
            pos(w, 0, 0, 1280, 720)

    if key == 'WBP_Settings':
        box(W('SettingsPanelSize'), 1280, 720)
        fullscreen_background(W('SettingsPanel'))
        canvas = wrap(W('SettingsColumn_ImageBounds'), unreal.CanvasPanel, 'CoopSettingsCanvas')
        canvas.slot.set_padding(margin(0))
        canvas.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        canvas.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('SettingsColumn_ImageBounds').set_visibility(unreal.SlateVisibility.COLLAPSED)
        caption(canvas, 'CoopSettingsKicker', 'MUSEUM HEIST', 32, 32, 480, 16)
        W('SettingsTitleText').set_text('환경설정')
        text(W('SettingsTitleText'), 28)
        place(W('SettingsTitleText'), canvas, 32, 52, 640, 36)
        rule(canvas, 'CoopSettingsHeaderRule', 32, 104, 1216)
        place(W('SettingsCloseButton'), canvas, 1144, 40, 104, 48)
        W('SettingsCloseButtonText').set_text('×  닫기')
        button(W('SettingsCloseButton'), quiet=True)
        sliders = {'FOV': ('FOVSlider', 'FOVValueText'),
                   'Sensitivity': ('MouseSensitivitySlider', 'MouseSensitivityValueText'),
                   'Volume': ('MasterVolumeSlider', 'MasterVolumeValueText')}
        for i, prefix in enumerate(['FOV', 'Sensitivity', 'Volume', 'Resolution', 'WindowMode']):
            y = 128 + i * 64
            row_button = node(unreal.Button, 'CoopSettings'+prefix+'Hover', canvas)
            place(row_button, canvas, 160, y, 960, 64)
            button(row_button, quiet=True)
            row_button.set_editor_property('is_focusable', False)
            s = row_button.get_editor_property('widget_style')
            s.set_editor_property('hovered', brush(None, WHITE, (4, 4)))
            s.set_editor_property('normal_padding', margin(0))
            s.set_editor_property('pressed_padding', margin(0))
            s.set_editor_property('hovered_foreground', unreal.SlateColor(specified_color=INK))
            row_button.set_style(s)
            row_canvas = node(unreal.CanvasPanel, 'CoopSettings'+prefix+'Canvas', row_button)
            row_canvas.slot.set_padding(margin(0))
            row_canvas.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            row_canvas.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            text(W(prefix+'RowLabel'), 16)
            W(prefix+'RowLabel').set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
            place(W(prefix+'RowLabel'), row_canvas, 24, 20, 208, 24)
            if prefix in sliders:
                control, value = sliders[prefix]
                fill_name = {'FOV': 'FOVValueFill', 'Sensitivity': 'MouseSensitivityValueFill',
                             'Volume': 'MasterVolumeValueFill'}[prefix]
                fill = node(unreal.ProgressBar, fill_name, row_canvas)
                fill_style = fill.get_editor_property('widget_style')
                track = brush(None, css_color('#3b3b3b'), (4, 4))
                track_outline = track.get_editor_property('outline_settings')
                track_outline.set_editor_property('corner_radii', unreal.Vector4(4, 4, 4, 4))
                track_outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
                track_outline.set_editor_property('color', unreal.SlateColor(specified_color=css_color('#8b8e8f')))
                track_outline.set_editor_property('width', 1)
                track_outline.set_editor_property('use_brush_transparency', False)
                track.set_editor_property('outline_settings', track_outline)
                fill_style.set_editor_property('background_image', track)
                fill_style.set_editor_property('fill_image', brush(None, unreal.LinearColor(1, 1, 1, 1), (4, 4)))
                fill.set_editor_property('widget_style', fill_style)
                fill.set_fill_color_and_opacity(css_color('#b6babb'))
                fill.set_editor_property('border_padding', unreal.Vector2D(0, 0))
                fill.set_editor_property('bar_fill_type', unreal.ProgressBarFillType.LEFT_TO_RIGHT)
                fill.set_editor_property('bar_fill_style', unreal.ProgressBarFillStyle.SCALE)
                fill.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
                place(fill, row_canvas, 264, 28, 528, 8)
                fill.slot.set_z_order(0)
                place(W(control), row_canvas, 256, 8, 544, 48)
                W(control).slot.set_z_order(1)
                W(control).set_indent_handle(False)
                slider_style = W(control).get_editor_property('widget_style')
                for property_name, color in [('normal_thumb_image', css_color('#b6babb')),
                                             ('hovered_thumb_image', css_color('#1c232a')),
                                             ('disabled_thumb_image', MUTED)]:
                    thumb = brush(None, color, (16, 16))
                    thumb_outline = thumb.get_editor_property('outline_settings')
                    thumb_outline.set_editor_property('corner_radii', unreal.Vector4(8, 8, 8, 8))
                    thumb_outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
                    thumb.set_editor_property('outline_settings', thumb_outline)
                    slider_style.set_editor_property(property_name, thumb)
                for property_name in ['normal_bar_image', 'hovered_bar_image', 'disabled_bar_image']:
                    slider_style.set_editor_property(property_name, brush(None, TRANSPARENT, (4, 4)))
                W(control).set_editor_property('widget_style', slider_style)
                W(control).set_slider_handle_color(unreal.LinearColor(1, 1, 1, 1))
                W(control).set_slider_bar_color(unreal.LinearColor(1, 1, 1, 1))
                text(W(value), 16, WHITE, unreal.TextJustify.RIGHT)
                W(value).set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
                place(W(value), row_canvas, 824, 20, 112, 24)
            else:
                control = W(prefix+'ComboBox')
                place(control, row_canvas, 256, 8, 680, 48)
                H['controls'](control)
                font(control, 16)
                control.set_editor_property('content_padding', margin(16, 0, 16, 0))
                combo_style = control.get_editor_property('widget_style')
                combo_button_style = combo_style.get_editor_property('combo_button_style')
                combo_button = combo_button_style.get_editor_property('button_style')
                for state in ['normal', 'hovered', 'pressed', 'disabled']:
                    combo_button.set_editor_property(state, brush(None, TRANSPARENT, (4, 4)))
                for state in ['normal_foreground', 'hovered_foreground', 'pressed_foreground']:
                    combo_button.set_editor_property(state, unreal.SlateColor(
                        color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
                combo_button_style.set_editor_property('button_style', combo_button)
                combo_style.set_editor_property('combo_button_style', combo_button_style)
                control.set_editor_property('widget_style', combo_style)
                control.set_editor_property('foreground_color', unreal.SlateColor(
                    color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
            rule(canvas, 'CoopSettings'+prefix+'Rule', 160, y+60, 960)
        place(W('RestoreDefaultSettingsButton'), canvas, 900, 480, 104, 48)
        W('RestoreDefaultSettingsButtonText').set_text('기본값')
        button(W('RestoreDefaultSettingsButton'))
        place(W('ApplySettingsButton'), canvas, 1020, 480, 100, 48)
        button(W('ApplySettingsButton'), primary=True)
        text(W('SettingsStatusText'), 12, MUTED, unreal.TextJustify.RIGHT)
        place(W('SettingsStatusText'), canvas, 160, 544, 960, 24)
        rule(canvas, 'CoopSettingsFooterRule', 32, 644, 1216)
        key_box = node(unreal.Border, 'CoopSettingsEscKey', canvas)
        key_brush = brush(None, TRANSPARENT, (28, 28))
        key_outline = key_brush.get_editor_property('outline_settings')
        key_outline.set_editor_property('width', 1)
        key_outline.set_editor_property('color', unreal.SlateColor(specified_color=MUTED))
        key_outline.set_editor_property('use_brush_transparency', False)
        key_brush.set_editor_property('outline_settings', key_outline)
        key_box.set_brush(key_brush)
        key_box.set_brush_color(unreal.LinearColor(1, 1, 1, 1))
        key_box.set_padding(margin(0))
        key_box.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        key_box.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        place(key_box, canvas, 32, 660, 36, 28)
        key_text = node(unreal.TextBlock, 'CoopSettingsEscText', key_box)
        text(key_text, 16, MUTED, unreal.TextJustify.CENTER)
        key_text.set_text('ESC')
        key_text.slot.set_padding(margin(0))
        key_text.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        key_text.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        caption(canvas, 'CoopSettingsKeys', '닫기', 76, 664, 64, 24, 16, MUTED)
    elif key == 'WBP_HeistForgery':
        canvas = W('RootCanvas')
        fullscreen_background(W('FullScreenBackground'))
        W('VerticalBox_0').set_visibility(unreal.SlateVisibility.COLLAPSED)
        caption(canvas, 'CoopForgeryKicker', 'MUSEUM HEIST', 32, 32, 480, 16)
        W('TitleText').set_text('그림 위조')
        text(W('TitleText'), 28)
        place(W('TitleText'), canvas, 32, 52, 640, 36)
        text(W('DrawingTimeRemainingText'), 24, WHITE, unreal.TextJustify.RIGHT)
        place(W('DrawingTimeRemainingText'), canvas, 1152, 48, 96, 32)
        clock = node(unreal.Border, 'CoopForgeryTimerIcon', canvas)
        clock_brush = brush(None, TRANSPARENT, (24, 24))
        outline = clock_brush.get_editor_property('outline_settings')
        outline.set_editor_property('corner_radii', unreal.Vector4(12, 12, 12, 12))
        outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
        outline.set_editor_property('width', 1)
        outline.set_editor_property('use_brush_transparency', False)
        outline.set_editor_property('color', unreal.SlateColor(specified_color=WHITE))
        clock_brush.set_editor_property('outline_settings', outline)
        clock.set_brush(clock_brush)
        clock.set_brush_color(WHITE)
        clock.set_padding(margin(0))
        place(clock, canvas, 1116, 52, 24, 24)
        for name, x, y, width, height, scale in [('CoopForgeryClockVertical', 1128, 56, 4, 8, (.25, 1)),
                                                ('CoopForgeryClockHorizontal', 1128, 64, 8, 4, (1, .25))]:
            hand = node(unreal.Image, name, canvas)
            hand.set_brush(brush(None, WHITE, (4, 4)))
            hand.set_render_scale(unreal.Vector2D(*scale))
            place(hand, canvas, x, y, width, height)
            hand.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        rule(canvas, 'CoopForgeryHeaderRule', 32, 100, 1216)
        text(W('PreviewScoreText'), 16)
        W('PreviewScoreText').set_text('작품 유사도')
        place(W('PreviewScoreText'), canvas, 856, 116, 256, 24)
        place(W('PreviewQualityBar_ImageBounds'), canvas, 856, 148, 256, 8)
        box(W('PreviewQualityBar_ImageBounds'), 256, 8)
        quality_style = W('PreviewQualityBar').get_editor_property('widget_style')
        quality_style.set_editor_property('background_image', brush(None, css_color('#323537'), (4, 4)))
        quality_style.set_editor_property('fill_image', brush(None, unreal.LinearColor(1, 1, 1, 1), (4, 4)))
        W('PreviewQualityBar').set_editor_property('widget_style', quality_style)
        for n, x in [('HeistCanvasLabel', 168), ('HeistReferenceLabel', 700)]:
            text(W(n), 12, MUTED)
            place(W(n), canvas, x, 172, 412, 16)
        W('HeistCanvasLabel').set_text('복제 작업')
        W('HeistReferenceLabel').set_text('관찰한 원본')
        # Input normalizes DrawingSurface's cached geometry; raster resolution
        # stays native. It is not tied to the old 800px display SizeBox.
        flat(W('DrawingContainer'), css_color('#11100d'))
        W('DrawingContainer').set_padding(margin(0))
        W('DrawingContainer').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('DrawingContainer').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        box(W('DrawingSurfaceSizeBox'), 412, 412)
        place(W('DrawingContainer'), canvas, 168, 196, 412, 412)
        place(W('ReferenceImage_AspectFit'), canvas, 700, 196, 412, 412)
        W('ReferenceImage_AspectFit').set_stretch(unreal.Stretch.SCALE_TO_FIT)
        pos(W('DrawingContent'), 604, 196, 72, 412)
        # Put the SizeBox inside each bound button. Native palette availability
        # can then collapse the entire row, instead of leaving fixed blank slots.
        for w in list(ws.values()):
            if isinstance(w, (unreal.Spacer, unreal.SizeBox)) and w.get_parent() == W('DrawingContent'):
                w.set_visibility(unreal.SlateVisibility.COLLAPSED)
        W('BrushSizeLabel_1').set_visibility(unreal.SlateVisibility.COLLAPSED)
        for i in range(1, 9):
            palette = move(W('PaletteButton'+str(i)), W('DrawingContent'), i-1)
            sized_label = wrap(W('PaletteButtonText'+str(i)), unreal.SizeBox, 'CoopPaletteLabelSize'+str(i))
            box(sized_label, 72, 40)
            text(W('PaletteButtonText'+str(i)), 16, WHITE, unreal.TextJustify.CENTER)
            palette.slot.set_padding(margin(0, 0, 0, 8))
            palette.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            palette.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            s = palette.get_editor_property('widget_style')
            s.set_editor_property('normal_padding', margin(0))
            s.set_editor_property('pressed_padding', margin(0))
            palette.set_style(s)
            sized_label.slot.set_padding(margin(0))
            sized_label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            sized_label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            W('PaletteButtonText'+str(i)).slot.set_padding(margin(0))
            W('PaletteButtonText'+str(i)).slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            W('PaletteButtonText'+str(i)).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        brush_label = move(W('BrushSizeLabel'), W('DrawingContent'))
        text(brush_label, 12, MUTED)
        brush_label.set_text('붓 크기')
        brush_label.slot.set_padding(margin(0, 12, 0, 8))
        brush_row = node(unreal.HorizontalBox, 'CoopForgeryBrushRow', W('DrawingContent'))
        for i, suffix in enumerate(['Small', 'Medium', 'Large']):
            brush_button = move(W('Brush'+suffix+'Button'), brush_row, i)
            sized_label = wrap(W('Brush'+suffix+'Label'), unreal.SizeBox, 'CoopBrush'+suffix+'Size')
            box(sized_label, 24, 32)
            button(brush_button, size=12)
            W('Brush'+suffix+'Label').slot.set_padding(margin(0))
            W('Brush'+suffix+'Label').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            W('Brush'+suffix+'Label').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            s = brush_button.get_editor_property('widget_style')
            s.set_editor_property('normal_padding', margin(0))
            s.set_editor_property('pressed_padding', margin(0))
            brush_button.set_style(s)
            brush_button.slot.set_padding(margin(0))
        W('FooterActionRow').set_visibility(unreal.SlateVisibility.COLLAPSED)
        place(W('CancelButton'), canvas, 1040, 640, 96, 48)
        place(W('SubmitButton'), canvas, 1152, 640, 96, 48)
        button(W('CancelButton'))
        button(W('SubmitButton'), primary=True)
        W('FooterHint').set_visibility(unreal.SlateVisibility.COLLAPSED)
        rule(canvas, 'CoopForgeryFooterRule', 32, 620, 1216)
        caption(canvas, 'CoopForgeryDrawKey', '좌클릭 그리기', 32, 652, 100, 24, 12)
        caption(canvas, 'CoopForgeryEraseKey', '우클릭 지우기', 148, 652, 100, 24, 12)
        if 'CoopForgeryResetKey' in ws:
            W('CoopForgeryResetKey').set_visibility(unreal.SlateVisibility.COLLAPSED)
        reset = node(unreal.Button, 'ResetDrawingButton', canvas)
        reset_text = node(unreal.TextBlock, 'CoopForgeryResetText', reset)
        reset_text.set_text('초기화')
        place(reset, canvas, 248, 640, 96, 48)
        button(reset, quiet=True, size=12)
    elif key == 'WBP_Result':
        canvas = W('CanvasPanel_38')
        fullscreen_background(W('ResultBackdrop'))
        caption(canvas, 'CoopResultKicker', 'MUSEUM HEIST / CONTRACT RESULT', 32, 32, 640, 16)
        text(W('OutcomeTextBlock'), 28)
        pos(W('OutcomeTextBlock'), 32, 52, 1056, 36)
        if 'CoopResultOutcomeIcon' in ws and not isinstance(W('CoopResultOutcomeIcon'), unreal.CanvasPanel):
            assert TOOLS.call_method('RemoveWidget', (bp, W('CoopResultOutcomeIcon')))
            del ws['CoopResultOutcomeIcon']
            refresh()
        outcome_icon = node(unreal.CanvasPanel, 'CoopResultOutcomeIcon', canvas)
        place(outcome_icon, canvas, 1224, 52, 24, 28)
        for name, failed, strokes in [
                ('CoopResultSuccessCheck', False, [('Short', 0, 12, 12, 45), ('Long', 8, 20, 20, -45)]),
                ('CoopResultFailedCross', True, [('Down', 4, 4, 24, 45), ('Up', 4, 20, 24, -45)])]:
            shape = node(unreal.CanvasPanel, name, outcome_icon)
            place(shape, outcome_icon, 0, 0, 24, 28)
            shape.set_visibility(unreal.SlateVisibility.COLLAPSED if failed else unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            for suffix, x, y, width, angle in strokes:
                stroke = node(unreal.Image, name+suffix, shape)
                stroke.set_brush(brush(None, WHITE, (4, 4)))
                stroke.set_render_transform_pivot(unreal.Vector2D(0, .5))
                stroke.set_render_scale(unreal.Vector2D(1, .5))
                stroke.set_render_transform_angle(angle)
                place(stroke, shape, x, y, width, 4)
                stroke.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        rule(canvas, 'CoopResultHeaderRule', 32, 104, 1216)
        caption(canvas, 'CoopResultRecapTitle', '확보한 작품', 32, 128, 720, 28, 20, WHITE)
        text(W('OutcomeReasonTextBlock'), 16, MUTED)
        pos(W('OutcomeReasonTextBlock'), 32, 164, 856, 24)
        caption(canvas, 'CoopResultTeamValueLabel', '팀 확보 가치', 984, 128, 264, 16,
                12, MUTED, unreal.TextJustify.RIGHT)
        text(W('TeamRewardTextBlock'), 32, WHITE, unreal.TextJustify.RIGHT)
        pos(W('TeamRewardTextBlock'), 984, 148, 264, 40)
        pos(W('ReplicaRecapVisualPanel'), 32, 204, 1216, 212)
        W('ReplicaRecapVisualPanel').set_padding(margin(0))
        flat(W('ReplicaRecapVisualPanel'), TRANSPARENT)
        W('ReplicaRecapVisualPanel').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaRecapVisualPanel').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('ReplicaRecapScrollBox').set_scroll_bar_visibility(unreal.SlateVisibility.COLLAPSED)
        W('ReplicaRecapScrollBox').set_editor_property('allow_overscroll', False)
        W('ReplicaRecapScrollBox').slot.set_padding(margin(0))
        W('ReplicaRecapViewportWidth').clear_width_override()
        W('ReplicaRecapViewportWidth').set_min_desired_width(1216)
        flat(W('ReplicaRecapCenter'), TRANSPARENT)
        W('ReplicaRecapCenter').set_padding(margin(0))
        W('ReplicaRecapCenter').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaRecapCenter').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        W('ReplicaRecapVisualContainer').slot.set_padding(margin(0))
        W('ReplicaRecapVisualContainer').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        W('ReplicaRecapVisualContainer').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        table_layout = node(unreal.VerticalBox, 'CoopResultTableLayout', canvas)
        pos(table_layout, 32, 440, 1216, 188)
        header_size = node(unreal.SizeBox, 'CoopResultCompactHeaderSize', table_layout)
        box(header_size, 1216, 28)
        header = node(unreal.CanvasPanel, 'CoopResultCompactHeader', header_size)
        W('ContributionTableHeader').set_visibility(unreal.SlateVisibility.COLLAPSED)
        # Header and runtime rows use identical four column boundaries.
        columns = [('HeaderPlayer', 0, 768, '플레이어'), ('HeaderState', 784, 112, '상태'),
                   ('HeaderSecured', 912, 144, '확보 가치'), ('HeaderBestQuality', 1072, 144, '최고 품질')]
        for n, x, width, title in columns:
            text(W(n), 12, MUTED, unreal.TextJustify.LEFT if n == 'HeaderPlayer' else unreal.TextJustify.RIGHT)
            W(n).set_text(title)
            place(W(n), header, x, 4, width, 20)
        rule(header, 'CoopResultHeaderBottomRule', 0, 24, 1216)
        detail_header_size = node(unreal.SizeBox, 'CoopResultDetailHeaderSize', table_layout)
        box(detail_header_size, 1216, 24)
        detail_header = node(unreal.CanvasPanel, 'CoopResultDetailHeader', detail_header_size)
        detail_header_size.set_visibility(unreal.SlateVisibility.COLLAPSED)
        for i, (n, title) in enumerate([('HeaderDrawing', '위조'), ('HeaderOriginals', '원본 회수'),
                                       ('HeaderGuards', '경비 유인'), ('HeaderRescues', '동료 구출'), ('HeaderAlarms', '경보 발생')]):
            text(W(n), 12, MUTED, unreal.TextJustify.RIGHT)
            W(n).set_text(title)
            place(W(n), detail_header, 576+i*128, 0, 120, 20)
        move(W('ContributionTablePanel'), table_layout)
        W('ContributionTablePanel').slot.set_size(unreal.SlateChildSize(1, unreal.SlateSizeRule.FILL))
        W('ContributionTablePanel').slot.set_padding(margin(0))
        W('ContributionTablePanel').set_padding(margin(0))
        W('ContributionTablePanel').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ContributionTablePanel').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        flat(W('ContributionTablePanel'), TRANSPARENT)
        table_scroll = wrap(W('ContributionTableContainer'), unreal.ScrollBox, 'CoopResultTableScroll')
        table_scroll.set_editor_property('allow_overscroll', False)
        table_scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(4, 4))
        table_scroll.set_scroll_bar_visibility(unreal.SlateVisibility.COLLAPSED)
        table_scroll.slot.set_padding(margin(0))
        table_scroll.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        table_scroll.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('ContributionTableContainer').slot.set_padding(margin(0))
        W('ContributionTableContainer').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        text(W('ReplicaRecapEmptyTextBlock'), 16, MUTED)
        pos(W('ReplicaRecapEmptyTextBlock'), 32, 204, 1216, 212)
        text(W('ContributionEmptyTextBlock'), 16, MUTED)
        pos(W('ContributionEmptyTextBlock'), 32, 468, 1216, 160)
        rule(canvas, 'CoopResultFooterRule', 32, 628, 1216)
        pos(W('RewardDetailsButton'), 32, 644, 128, 48)
        button(W('RewardDetailsButton'), quiet=True)
        pos(W('ReturnToLobbyButton'), 1072, 644, 176, 48)
        button(W('ReturnToLobbyButton'), primary=True)
        pos(W('RewardDetailWidget'), 32, 128, 1216, 288)
        W('RewardDetailWidget').slot.set_z_order(8)
    elif key == 'WBP_ResultPlayerRow':
        row_size = wrap(W('PlayerResultRowRoot'), unreal.SizeBox, 'CoopResultRowSize')
        box(row_size, 1216)
        row_size.clear_height_override()
        flat(W('PlayerResultRowRoot'), TRANSPARENT)
        W('PlayerResultRowRoot').set_padding(margin(0))
        W('PlayerResultRowRoot').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('PlayerResultRowRoot').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        layout = wrap(W('PlayerResultRowLayout'), unreal.VerticalBox, 'CoopResultRowColumn')
        layout.slot.set_padding(margin(0))
        layout.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        layout.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('PlayerResultRowLayout').set_visibility(unreal.SlateVisibility.COLLAPSED)
        compact_size = node(unreal.SizeBox, 'CoopResultCompactFieldsSize', layout)
        box(compact_size, 1216, 36)
        canvas = node(unreal.CanvasPanel, 'CoopResultRowCanvas', compact_size)
        for n, x, width in [('PlayerNameText', 0, 768), ('PlayerStateText', 784, 112),
                            ('SecuredLootValueText', 912, 144), ('BestSurfaceQualityText', 1072, 144)]:
            text(W(n), 16, WHITE, unreal.TextJustify.LEFT if n == 'PlayerNameText' else unreal.TextJustify.RIGHT)
            place(W(n), canvas, x, 4, width, 24)
        W('PlayerNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        rule(canvas, 'CoopResultRowRule', 0, 32, 1216)
        details_size = node(unreal.SizeBox, 'CoopResultDetailFieldsSize', layout)
        box(details_size, 1216, 24)
        details = node(unreal.CanvasPanel, 'CoopResultDetailFields', details_size)
        details_size.set_visibility(unreal.SlateVisibility.COLLAPSED)
        for i, n in enumerate(['SurfaceForgeryCountText', 'ArtifactsRecoveredText', 'GuardsDistractedText',
                               'TeammatesRescuedText', 'AlarmsTriggeredText']):
            text(W(n), 12, MUTED, unreal.TextJustify.RIGHT)
            place(W(n), details, 576+i*128, 0, 120, 20)
    elif key == 'WBP_ResultReplicaCard':
        card_size = wrap(W('ReplicaCardRoot'), unreal.SizeBox, 'CoopReplicaCardSize')
        box(card_size, 284, 212)
        flat(W('ReplicaCardRoot'), TRANSPARENT)
        W('ReplicaCardRoot').set_padding(margin(0))
        W('ReplicaCardRoot').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaCardRoot').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        W('ReplicaCardColumn').slot.set_padding(margin(0))
        W('ReplicaCardColumn').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaCardColumn').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        box(W('ReplicaImageSize'), 284, 160)
        image_backdrop = wrap(W('ReplicaImage_AspectFit'), unreal.Border, 'CoopReplicaImageBackdrop')
        image_backdrop.set_brush(brush(None, css_color('#070909'), (4, 4)))
        image_backdrop.set_brush_color(unreal.LinearColor(1, 1, 1, 1))
        image_backdrop.set_padding(margin(0))
        image_backdrop.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        image_backdrop.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        image_backdrop.slot.set_padding(margin(0))
        image_backdrop.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        image_backdrop.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('ReplicaImage_AspectFit').set_stretch(unreal.Stretch.SCALE_TO_FIT)
        W('ReplicaImage_AspectFit').slot.set_padding(margin(0))
        W('ReplicaImage_AspectFit').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaImage_AspectFit').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('ReplicaImageSize').slot.set_padding(margin(0))
        W('ReplicaImageSize').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ReplicaImageSize').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        W('Overlay_0').slot.set_padding(margin(0, 0, 0, 12))
        W('Overlay_0').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('Overlay_0').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        caption_size = node(unreal.SizeBox, 'CoopReplicaCaptionSize', W('ReplicaCardColumn'))
        box(caption_size, 284, 20)
        TOOLS.call_method('MoveWidget', (bp, caption_size, W('ReplicaCardColumn'), 1))
        caption_row = node(unreal.HorizontalBox, 'CoopReplicaCaptionRow', caption_size)
        caption_row.slot.set_padding(margin(0))
        caption_row.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        caption_row.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        move(W('RequiredTargetBadge'), caption_row, 0)
        move(W('ArtifactNameText'), caption_row, 1)
        text(W('ArtifactNameText'), 16)
        W('ArtifactNameText').set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
        W('ArtifactNameText').slot.set_padding(margin(0))
        W('ArtifactNameText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('ArtifactNameText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        W('ArtifactNameText').slot.set_size(unreal.SlateChildSize(1, unreal.SlateSizeRule.FILL))
        text(W('QualityText'), 12, MUTED)
        W('QualityText').slot.set_padding(margin(0))
        W('QualityText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        W('QualityText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        text(W('RequiredTargetBadge'), 12)
        W('RequiredTargetBadge').slot.set_padding(margin(0, 0, 8, 0))
        W('RequiredTargetBadge').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        W('RequiredTargetBadge').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    elif key == 'WBP_ResultRewardDetail':
        box(W('SizeBox_0'), 1216, 288)
        fullscreen_background(W('RewardDetailRoot'))
        canvas = wrap(W('RewardDetailLayout'), unreal.CanvasPanel, 'CoopRewardDetailCanvas')
        canvas.slot.set_padding(margin(0))
        canvas.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        canvas.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('RewardDetailLayout').set_visibility(unreal.SlateVisibility.COLLAPSED)
        text(W('RewardDetailTitle'), 20)
        place(W('RewardDetailTitle'), canvas, 24, 16, 960, 28)
        button(W('RewardDetailsCloseButton'), quiet=True)
        place(W('RewardDetailsCloseButton'), canvas, 1104, 8, 88, 48)
        rows = [('DetailRequiredTargetRowLabel', 'DetailRequiredTargetValueTextBlock'),
                ('DetailTargetStatusRowLabel', 'DetailTargetStatusValueTextBlock'),
                ('DetailQuotaRowLabel', 'DetailQuotaValueTextBlock'),
                ('DetailSecuredValueRowLabel', 'DetailSecuredValueTextBlock'),
                ('DetailExtraValueRowLabel', 'DetailExtraValueTextBlock'),
                ('DetailRequiredTargetRewardRowLabel', 'DetailRequiredTargetRewardValueTextBlock'),
                ('DetailLooseLootRowLabel', 'DetailLooseLootValueTextBlock'),
                ('DetailForgeryMultiplierRowLabel', 'DetailForgeryMultiplierValueTextBlock'),
                ('DetailStealthMultiplierRowLabel', 'DetailStealthMultiplierValueTextBlock'),
                ('DetailArrestPenaltyRowLabel', 'DetailArrestPenaltyValueTextBlock')]
        for i, (name, value) in enumerate(rows):
            x = 24 if i < 5 else 640
            y = 64+(i % 5)*40
            text(W(name), 16)
            text(W(value), 16, WHITE, unreal.TextJustify.RIGHT)
            W(value).set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
            place(W(name), canvas, x, y+8, 280, 24)
            place(W(value), canvas, x+288, y+8, 280, 24)
            rule(canvas, 'CoopRewardDetailRule'+str(i), x, y+36, 568)


def hash_packages():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (ROOT/'Content').rglob('*') if p.suffix in ['.uasset', '.umap']}


if __name__ == '__main__':
    before = hash_packages()
    results, descriptions = [], {}
    try:
        module = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/apply_coop_hud_lobby.py'))
        wanted = set(module['ASSET_NAMES']) | {'WBP_Settings', 'WBP_HeistForgery', 'WBP_Result',
                  'WBP_ResultPlayerRow', 'WBP_ResultReplicaCard', 'WBP_ResultRewardDetail'}
        paths = unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI', recursive=True, include_folder=False)
        leaf_names = {'WBP_TeamCard', 'WBP_QuickSlot', 'WBP_InteractionPrompt', 'WBP_ActionProgress', 'WBP_HeistPopupFeedback', 'WBP_LobbyMapCard', 'WBP_LobbyPlayerCard',
                      'WBP_ResultPlayerRow', 'WBP_ResultReplicaCard', 'WBP_ResultRewardDetail'}
        for path in sorted(paths, key=lambda p: (0 if p.rsplit('/', 1)[-1].split('.')[0] in leaf_names else 1, p)):
            bp = unreal.load_asset(path)
            if not isinstance(bp, unreal.WidgetBlueprint) or bp.get_name() not in wanted:
                continue
            bp.modify()
            key = bp.get_name()
            ws = {str(i.widget_name): i.widget for i in TOOLS.call_method('GetWidgets', (bp,)).widgets if i.widget}
            neutral_defaults(bp, ws)
            if key in module['ASSET_NAMES']:
                module['apply'](bp, key, ws, H)
            else:
                other_screens(bp, key, ws)
            if key in {'WBP_HeistHUD', 'WBP_Lobby', 'WBP_Settings', 'WBP_HeistForgery', 'WBP_Result'}:
                fit_authored_screen(bp)
            final = {str(i.widget_name): i.widget for i in TOOLS.call_method('GetWidgets', (bp,)).widgets if i.widget}
            for w in final.values():
                H['quantize'](w)
            tree = TOOLS.call_method('GetWidgets', (bp,)).widgets[0].widget.get_outer()
            assert all(w.get_outer() == tree for w in final.values()), key+' foreign widget tree'
            assert TOOLS.call_method('CompileWidgetBlueprint', (bp,)), key+' compile failed'
            unreal.EditorAssetLibrary.set_metadata_tag(bp, 'HeistUIDesign', 'CoopConcept1280Faithful')
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
            results.append({'asset': path, 'compiled': True, 'widgets': len(final)})
            descriptions[path] = TOOLS.call_method('GetWidgetDescription', (bp, None, -1)).description
        after = hash_packages()
        changed = [p for p in before if before[p] != after.get(p)]
        assert set(before) == set(after), 'New/deleted packages'
        assert all(p.startswith('Content/Blueprints/UI/') and p.endswith('.uasset') for p in changed), changed
        (OUT/'widget-descriptions.json').write_text(json.dumps(descriptions, ensure_ascii=False, indent=2), encoding='utf-8')
        (OUT/'apply-complete.json').write_text(json.dumps({'success': True, 'assets': results, 'changed_packages': changed,
            'new_packages': 0, 'maps_changed': 0}, ensure_ascii=False, indent=2), encoding='utf-8')
    except Exception:
        error = traceback.format_exc()
        unreal.log_error(error)
        (OUT/'apply-complete.json').write_text(json.dumps({'success': False, 'error': error, 'applied': results}, indent=2), encoding='utf-8')
    finally:
        unreal.SystemLibrary.quit_editor()
