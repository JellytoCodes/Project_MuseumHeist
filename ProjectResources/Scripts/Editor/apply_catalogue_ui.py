"""Apply the approved catalogue presentation to existing WBP shells in the Editor."""
import json
import math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
TOOLS=unreal.get_default_object(unreal.UMGToolSet)
WHITE=unreal.LinearColor(1,1,1,1)
IVORY=unreal.LinearColor(0.82,0.76,0.64,1)
INK=unreal.LinearColor(0.016,0.014,0.012,1)
MUTED=unreal.LinearColor(0.42,0.40,0.35,1)
BODY=unreal.load_asset('/Game/Assets/UI/Fonts/Catalogue/F_NanumGothic_Regular_Font')
BOLD=unreal.load_asset('/Game/Assets/UI/Fonts/Catalogue/F_NanumGothic_Bold_Font')
TITLE=unreal.load_asset('/Game/Assets/UI/Fonts/Catalogue/F_NanumMyeongjo_Bold_Font')
assert BODY and BOLD and TITLE
TEX={p.stem.removeprefix('T_Catalogue_'):unreal.load_asset('/Game/Assets/UI/Catalogue/'+p.stem)
     for p in (ROOT/'ProjectResources/SourceArt/UI/Catalogue').glob('T_Catalogue_*.png')}
assert all(TEX.values()),'Import textures first'
FONT_NATIVE_PER_DISPLAY=72.0/96.0
CURRENT_ALREADY_FIXED=False

def q(v):
    return float(math.floor(float(v)/4+0.5)*4)

def margin(l=0,t=None,r=None,b=None):
    return unreal.Margin(l,l if t is None else t,l if r is None else r,l if b is None else b)

def sc(c): return unreal.SlateColor(specified_color=c)

def brush(key=None,color=WHITE,size=(256,64)):
    b=unreal.WidgetLibrary.make_brush_from_texture(TEX.get(key),int(size[0]),int(size[1]))
    b.set_editor_property('tint_color',sc(color))
    b.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
    b.set_editor_property('margin',margin(0))
    return b

def font(w,size=None,kind=None):
    f=w.get_editor_property('font')
    f.set_editor_property('font_object',kind or BODY)
    f.set_editor_property('typeface_font_name','Default')
    displayed=f.size/FONT_NATIVE_PER_DISPLAY if CURRENT_ALREADY_FIXED else f.size
    f.set_editor_property('size',q(size if size is not None else max(16,displayed))*FONT_NATIVE_PER_DISPLAY)
    outline=f.get_editor_property('outline_settings');outline.set_editor_property('outline_size',0);f.set_editor_property('outline_settings',outline)
    w.set_editor_property('font',f)

def label(w,size=None,heading=False,color=IVORY):
    font(w,size,TITLE if heading else BODY)
    w.set_color_and_opacity(sc(color))

def button_style(existing=None,primary=False):
    s=existing or unreal.ButtonStyle()
    s.set_editor_property('normal',brush('Button_Primary' if primary else 'Button_Normal'))
    s.set_editor_property('hovered',brush('Button_Primary'))
    s.set_editor_property('pressed',brush('Button_Pressed'))
    s.set_editor_property('disabled',brush('Button_Normal',unreal.LinearColor(.5,.5,.5,.72)))
    for key,col in [('normal_foreground',INK if primary else IVORY),('hovered_foreground',INK),('pressed_foreground',IVORY),('disabled_foreground',MUTED)]:
        s.set_editor_property(key,sc(col))
    s.set_editor_property('normal_padding',margin(20,8,20,8))
    s.set_editor_property('pressed_padding',margin(20,12,20,4))
    return s

def style_button(w,primary=False):
    s=button_style(w.get_editor_property('widget_style'),primary)
    if 'Brush' in w.get_name() or w.get_name()=='CloseButton':
        s.set_editor_property('normal_padding',margin(4));s.set_editor_property('pressed_padding',margin(4,8,4,0))
    w.set_style(s)
    w.set_background_color(WHITE)
    w.set_color_and_opacity(WHITE)
    # Let the authored button state choose the text colour on ivory hover surfaces.
    def inherit(child):
        if isinstance(child,unreal.TextBlock):
            child.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
        if isinstance(child,unreal.PanelWidget):
            for c in child.get_all_children(): inherit(c)
    for c in w.get_all_children():
        c.slot.set_padding(margin(0));c.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);c.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        inherit(c)

def panel(w,light=False):
    w.set_brush(brush('Header' if light else 'Panel',size=(256,256) if not light else (256,64)))
    w.set_brush_color(WHITE)

def flat(w,color=unreal.LinearColor(.014,.013,.011,.92)):
    w.set_brush(brush(None,color,size=(4,4)))
    w.set_brush_color(WHITE)

def box(w,width,height=None):
    w.set_width_override(width)
    if height is not None:w.set_height_override(height)

def pos(w,x,y,width,height,anchor=(0,0),align=(0,0)):
    if isinstance(w.get_parent(),unreal.ScaleBox):w=w.get_parent()
    s=w.slot
    assert isinstance(s,unreal.CanvasPanelSlot),w.get_name()
    s.set_anchors(unreal.Anchors(unreal.Vector2D(*anchor),unreal.Vector2D(*anchor)))
    s.set_alignment(unreal.Vector2D(*align))
    s.set_auto_size(False)
    s.set_position(unreal.Vector2D(x,y))
    s.set_size(unreal.Vector2D(width,height))

def image_size(w,width,height):
    w.set_brush_size(unreal.Vector2D(width,height))

def tab_title(bp,w,width=240,height=68):
    """Wrap an existing bound title, preserving its identity and outer slot."""
    title_name=w.get_name()+'_CatalogueTab'
    current={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets}
    if title_name in current:
        wrapper=current[title_name]
    else:
        wrapper=TOOLS.call_method('WrapWidgets',(bp,[w],unreal.Border.static_class()))[0].widget
        wrapper=TOOLS.call_method('RenameWidget',(bp,wrapper,title_name)).widget
    wrapper.set_brush(brush(f'Header_{width}x{height}',size=(width,height)))
    wrapper.set_brush_color(WHITE);wrapper.set_padding(margin(24,8,24,8))
    w.set_editor_property('justification',unreal.TextJustify.CENTER)
    w.set_editor_property('min_desired_width',160)
    label(w,32,True,INK)
    fixed_bounds(bp,wrapper,width,height)
    if isinstance(wrapper.get_parent(),unreal.SizeBox):wrapper.get_parent().slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    if isinstance(wrapper.slot,unreal.VerticalBoxSlot):wrapper.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    elif isinstance(wrapper.slot,unreal.CanvasPanelSlot):wrapper.slot.set_size(unreal.Vector2D(width,height))
    return wrapper

def quantize(w):
    for p in ['padding','content_padding']:
        try:
            m=w.get_editor_property(p)
            if isinstance(m,unreal.Margin):w.set_editor_property(p,margin(q(m.left),q(m.top),q(m.right),q(m.bottom)))
        except Exception:pass
    for p in ['width_override','height_override','min_desired_width','min_desired_height','max_desired_width','max_desired_height','wrap_text_at']:
        try:
            v=w.get_editor_property(p)
            if isinstance(v,(float,int)):w.set_editor_property(p,q(v))
        except Exception:pass
    if isinstance(w,unreal.Spacer):
        v=w.get_editor_property('size');w.set_size(unreal.Vector2D(max(4,q(v.x)),max(4,q(v.y))))
    if isinstance(w,unreal.TextBlock):
        v=w.get_editor_property('shadow_offset');w.set_shadow_offset(unreal.Vector2D(q(v.x),q(v.y)))
    if isinstance(w,unreal.ScrollBox):
        v=w.get_editor_property('scrollbar_thickness');w.set_editor_property('scrollbar_thickness',unreal.Vector2D(q(v.x),q(v.y)))
    if w.slot:
        s=w.slot
        try:
            if not isinstance(s,unreal.BorderSlot):
                m=s.get_editor_property('padding');s.set_padding(margin(q(m.left),q(m.top),q(m.right),q(m.bottom)))
        except Exception:pass
        if isinstance(s,unreal.CanvasPanelSlot):
            m=s.get_offsets();s.set_offsets(margin(q(m.left),q(m.top),q(m.right),q(m.bottom)))

def controls(w):
    if isinstance(w,unreal.Slider):
        s=w.get_editor_property('widget_style')
        for key in ['normal_thumb_image','hovered_thumb_image','disabled_thumb_image']:
            s.set_editor_property(key,brush('Thumb',size=(24,24)))
        for key in ['normal_bar_image','hovered_bar_image','disabled_bar_image']:
            s.set_editor_property(key,brush(None,unreal.LinearColor(.34,.31,.25,1),size=(4,4)))
        s.set_editor_property('bar_thickness',4)
        w.set_editor_property('widget_style',s)
        w.set_slider_handle_color(WHITE);w.set_slider_bar_color(WHITE)
    elif isinstance(w,unreal.ComboBoxString):
        font(w,20)
        s=w.get_editor_property('widget_style');cb=s.combo_button_style
        bs=button_style(cb.button_style);bs.set_editor_property('normal_padding',margin(0));bs.set_editor_property('pressed_padding',margin(0,4,0,0));cb.set_editor_property('button_style',bs)
        cb.set_editor_property('down_arrow_image',brush('Chevron',size=(24,24)))
        cb.set_editor_property('menu_border_brush',brush(None,INK,size=(4,4)))
        cb.set_editor_property('menu_border_padding',margin(8))
        cb.set_editor_property('content_padding',margin(0))
        cb.set_editor_property('down_arrow_padding',margin(8,0,24,0))
        arrow_shadow=cb.get_editor_property('shadow_offset');arrow_shadow.set_editor_property('x',0);arrow_shadow.set_editor_property('y',0);cb.set_editor_property('shadow_offset',arrow_shadow)
        s.set_editor_property('combo_button_style',cb)
        s.set_editor_property('content_padding',margin(0))
        s.set_editor_property('menu_row_padding',margin(8))
        w.set_editor_property('widget_style',s)
        w.set_editor_property('foreground_color',sc(IVORY))
        w.set_editor_property('content_padding',margin(48,0,24,0))
        w.set_editor_property('max_list_height',320)
        row=w.get_editor_property('item_style')
        for key in ['even_row_background_brush','odd_row_background_brush']:
            row.set_editor_property(key,brush(None,INK,size=(4,4)))
        for key in ['even_row_background_hovered_brush','odd_row_background_hovered_brush','active_brush','active_hovered_brush']:
            row.set_editor_property(key,brush(None,unreal.LinearColor(.12,.10,.07,1),size=(4,4)))
        row.set_editor_property('text_color',sc(IVORY));row.set_editor_property('selected_text_color',sc(IVORY))
        w.set_editor_property('item_style',row)
    elif isinstance(w,unreal.EditableTextBox):
        s=w.get_editor_property('widget_style')
        for key in ['background_image_normal','background_image_hovered','background_image_focused','background_image_read_only']:
            s.set_editor_property(key,brush('Button_Normal'))
        s.set_editor_property('padding',margin(48,8,32,8))
        s.set_editor_property('foreground_color',sc(IVORY))
        s.set_editor_property('focused_foreground_color',sc(IVORY))
        s.set_editor_property('read_only_foreground_color',sc(IVORY))
        fs=s.text_style.font;fs.set_editor_property('font_object',BODY);fs.set_editor_property('size',24*FONT_NATIVE_PER_DISPLAY);fs.set_editor_property('typeface_font_name','Default')
        ts=s.text_style;ts.set_editor_property('font',fs);ts.set_editor_property('color_and_opacity',sc(IVORY));s.set_editor_property('text_style',ts)
        w.set_editor_property('widget_style',s)

def apply_screen(bp,key,ws):
    W=lambda name:ws[name]
    if key=='WBP_TitleMenu':
        W('Image_113').set_brush(brush('Backdrop',size=(1920,1080)))
        W('LogoImage').set_brush(brush('Logo',size=(512,288)));pos(W('LogoImage'),88,88,512,288)
        W('GameSubtitleText').set_text('잠입 · 위조 · 탈출');label(W('GameSubtitleText'),20);W('GameSubtitleText').set_editor_property('justification',unreal.TextJustify.LEFT)
        pos(W('GameSubtitleText'),128,400,448,40)
        pos(W('TitleMenuColumn'),96,464,480,480)
        for name in ['HostSessionSize','JoinSessionSize','SettingsSize','QuitGameSize']:box(W(name),384,108)
        W('Spacer_185').set_size(unreal.Vector2D(4,4))
        for name in ['HostSessionButtonText','JoinSessionButtonText','SettingsButtonText','QuitGameButtonText']:font(W(name),28,TITLE)
        style_button(W('HostSessionButton'),True)
    elif key=='WBP_Settings':
        box(W('SettingsPanelSize'),1360,752);W('SettingsPanel').set_padding(margin(64,32,64,32))
        tab_title(bp,W('SettingsTitleText'),256,72)
        for prefix in ['FOV','Sensitivity','Volume','Resolution','WindowMode']:
            box(W(prefix+'RowLabelSize'),208,56)
            if prefix in ['FOV','Sensitivity','Volume']:
                box(W(prefix+'RowSliderSize'),608,56);box(W(prefix+'RowValueSize'),80,56)
            else:box(W(prefix+'RowComboSize'),688,80)
        for name in ['FOVRowLabel','SensitivityRowLabel','VolumeRowLabel','ResolutionRowLabel','WindowModeRowLabel']:label(W(name),24,True)
        for name in ['FOVValueText','MouseSensitivityValueText','MasterVolumeValueText']:label(W(name),24)
        for name in ['RestoreDefaultSettingsButtonSize','ApplySettingsButtonSize','SettingsCloseButtonSize']:box(W(name),224,64)
        style_button(W('ApplySettingsButton'),True)
        W('SettingsActionRow').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    elif key=='WBP_SessionJoin':
        box(W('SessionJoinPanelSize'),1008,560);W('SessionJoinPanel').set_padding(margin(48,32,48,32))
        tab_title(bp,W('SessionJoinTitleText'),256,72)
        box(W('SubmitJoinSessionSize'),256,72)
        for n in ['JoinCloseSize','RetrySessionSize','CancelSessionSize']:box(W(n),224,64)
        W('Spacer_1').set_size(unreal.Vector2D(4,32));style_button(W('SubmitJoinSessionButton'),True)
        label(W('JoinCodeLabelText'),20);W('JoinCodeInput').set_editor_property('minimum_desired_width',576)
    elif key=='WBP_HeistHUD':
        pos(W('MissionPanel'),24,24,304,208);W('MissionPanel').set_padding(margin(20,16,20,16));flat(W('MissionPanel'))
        for n,size in [('MissionTitleText',20),('MissionTimeText',32),('RequiredTargetLabelText',16),('RequiredTargetNameText',20)]:label(W(n),size)
        W('Spacer').set_size(unreal.Vector2D(4,12))
        pos(W('AlertMeterPanel'),0,24,344,96,anchor=(.5,0),align=(.5,0));W('AlertMeterPanel').set_padding(margin(12,8,12,8));flat(W('AlertMeterPanel'))
        label(W('AlertTitleText'),16);label(W('AlertEventText'),16)
        for i in range(1,11):image_size(W('AlertStar%02d'%i),24,24)
        pos(W('TeamCardsPanel'),-24,24,304,352,anchor=(1,0),align=(1,0))
        for i in range(1,5):W('TeamCard'+str(i)).slot.set_padding(margin(0,0,0,8))
        pos(W('HUDQuickSlotRow'),-24,-24,440,96,anchor=(1,1),align=(1,1))
        W('Spacer_1').set_size(unreal.Vector2D(24,4));box(W('SizeBox_0'),88,88);flat(W('Border_0'))
        label(W('InventoryShortcutKeyText'),16)
        W('InventoryShortcutKeyText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        W('InventoryShortcutKeyText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('InventoryShortcutIcon').slot.set_padding(margin(24,36,24,12))
        pos(W('TutorialCardContainer'),0,180,800,200,anchor=(.5,0),align=(.5,0));W('TutorialCardContainer').set_padding(margin(24,16,24,16))
        W('TutorialBodyText').set_editor_property('wrap_text_at',752)
        label(W('TutorialTitleText'),28,True);label(W('TutorialBodyText'),20);label(W('TutorialProgressText'),16)
        label(W('CrosshairIdleIndicator'),20)
    elif key=='WBP_TeamCard':
        box(W('SizeBox_0'),304,80);W('TeamCardBorder').set_padding(margin(12,8,12,8));flat(W('TeamCardBorder'))
        image_size(W('ProfileImage'),56,56);image_size(W('StatusIcon'),20,20);image_size(W('MicStatusImage'),20,20)
        label(W('PlayerNameText'),20);label(W('StatusText'),16);W('Spacer_143').set_size(unreal.Vector2D(4,4))
    elif key=='WBP_QuickSlot':
        for w in ws.values():
            if isinstance(w,unreal.SizeBox) and not w.slot:box(w,88,88)
            if isinstance(w,unreal.Border):flat(w)
            if isinstance(w,unreal.TextBlock):label(w,16)
        W('KeyLabelText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        W('KeyLabelText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        W('PlaceholderIcon').slot.set_padding(margin(24,28,24,20))
    elif key=='WBP_LobbyPlayerCard':
        box(W('PlayerCardSizeBox'),400,300);W('PlayerCardBorder').set_padding(margin(24))
        box(W('ProfileImageSizeBox'),96,96);box(W('SizeBox_0'),256,72)
        label(W('PlayerSlotText'),24,True);label(W('PlayerNameText'),20)
        label(W('ReadyButtonLabel'),20);image_size(W('ReadyCheckImage'),24,24)
        W('ReadyButtonLabel').slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
        W('ReadyButtonLabel').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('ReadyCheckImage').slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
        W('ReadyCheckImage').slot.set_padding(margin(8,0,0,0))
        for n in ['Spacer_133','Spacer_1','Spacer']:W(n).set_size(unreal.Vector2D(4,8))
    elif key=='WBP_LobbyMapCard':
        box(W('MapCardSizeBox'),384,216);label(W('MapNameText'),24,True)
        W('MapNameText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        W('MapNameText').slot.set_padding(margin(16))
        W('MapNameText').set_shadow_color_and_opacity(unreal.LinearColor(0,0,0,1));W('MapNameText').set_shadow_offset(unreal.Vector2D(0,4))
        image_size(W('SelectedCheckImage'),40,40)
    elif key=='WBP_Lobby':
        W('LobbyRootBorder').set_brush(brush('Backdrop',color=unreal.LinearColor(.28,.28,.28,1),size=(1920,1080)));W('LobbyRootBorder').set_brush_color(WHITE)
        W('LobbyRootBorder').set_padding(margin(48,32,48,32))
        W('LobbyLogoImage').set_brush(brush('Logo',size=(128,72)))
        W('LobbyTitleText').set_text('작전 대기실');label(W('LobbyTitleText'),32,True)
        label(W('JoinCodeLabel'),20);label(W('JoinCodeText'),28)
        copy_button=W('CopyJoinCodeButton')
        if not copy_button.get_children_count():
            copy_label=TOOLS.call_method('AddWidget',(bp,unreal.TextBlock.static_class(),'CopyJoinCodeLabel',copy_button,-1)).widget
        else:copy_label=copy_button.get_child_at(0)
        copy_label.set_text('복사');label(copy_label,20)
        copy_parent=copy_button.get_parent()
        if not isinstance(copy_parent,unreal.SizeBox):
            copy_parent=TOOLS.call_method('WrapWidgets',(bp,[copy_button],unreal.SizeBox.static_class()))[0].widget
        box(copy_parent,128,36)
        copy_parent.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        box(W('LeaveSessionSize'),200,56);box(W('PlayerCountSize'),112,56)
        W('Spacer_1').set_size(unreal.Vector2D(32,4));W('Spacer_3').set_size(unreal.Vector2D(24,4))
        for n in ['Spacer_2','Spacer_4','Spacer_5']:W(n).set_size(unreal.Vector2D(24,4))
        W('MapSection').slot.set_padding(margin(0,24,0,0));label(W('MapSectionTitle'),28,True)
        box(W('SizeBox_0'),256,72);style_button(W('StartGameButton'),True)
        for n in ['MapRandomCard','MapM01Card','MapM02Card','MapM03Card']:W(n).slot.set_padding(margin(8))
        for code in ['M01','M02','M03']:
            thumb=unreal.load_asset('/Game/Assets/UI/Catalogue/T_Catalogue_Map_'+code)
            if thumb:W('Map'+code+'Card').set_editor_property('MapThumbnail',thumb)
        W('MapRandomCard').set_editor_property('MapThumbnail',TEX['Backdrop'])
    elif key=='WBP_Inventory':
        pos(W('InventoryPanel'),0,0,768,960,anchor=(.5,.5),align=(.5,.5));W('InventoryPanel').set_padding(margin(32))
        title_tab=tab_title(bp,W('InventoryTitleText'),240,68);pos(title_tab,0,16,240,68)
        pos(W('CloseButton'),-16,24,128,36,anchor=(1,0),align=(1,0))
        pos(W('InventoryFrameWidget'),0,48,640,640,anchor=(.5,.5),align=(.5,.5))
    elif key=='WBP_InventoryFrame':
        flat(W('InventoryFrame'),unreal.LinearColor(0,0,0,0));W('InventoryFrame').set_padding(margin(0));box(W('SizeBox_0'),640,640)
        W('InventoryGrid').set_editor_property('slot_padding',margin(4))
        for n,w in ws.items():
            if n.startswith('GridCell') and isinstance(w,unreal.Border):flat(w,unreal.LinearColor(.035,.03,.024,1))
    elif key=='WBP_InventorySlot':
        flat(W('SlotBackground'),WHITE);W('SlotBackground').set_brush_color(unreal.LinearColor(.035,.03,.024,1))
        W('CoordinateText').set_visibility(unreal.SlateVisibility.COLLAPSED)
        W('OccupancyText').set_visibility(unreal.SlateVisibility.COLLAPSED)
    elif key=='WBP_HeistForgery':
        flat(W('FullScreenBackground'),unreal.LinearColor(.009,.008,.007,1))
        panel(W('DrawingContainer'));W('DrawingContainer').set_padding(margin(32))
        # DrawingSurface stays 800 square to preserve pointer/raster aspect and brush scale.
        box(W('DrawingSurfaceSizeBox'),800,800)
        label(W('TitleText'),32,True);label(W('DrawingTimeRemainingText'),24);label(W('PreviewScoreText'),20)
        pos(W('VerticalBox_0'),0,72,1400,128,anchor=(.5,0),align=(.5,.5))
        pos(W('DrawingContent'),0,0,144,768,anchor=(.5,.5),align=(.5,.5))
        pos(W('DrawingContainer'),448,0,864,864,anchor=(0,.5),align=(.5,.5))
        pos(W('ReferenceImage'),-448,0,800,800,anchor=(1,.5),align=(.5,.5))
        W('BrushSizeLabel_1').set_text('팔레트')
        for n,t in [('BrushSmallLabel','소'),('BrushMediumLabel','중'),('BrushLargeLabel','대')]:
            W(n).set_text(t);label(W(n),20)
        for n,w in ws.items():
            if n.startswith('PaletteButton') and isinstance(w,unreal.Button):
                # Palette backgrounds are the actual paint colours, not decorative paper.
                s=w.get_editor_property('widget_style')
                for k in ['normal','hovered','pressed']:s.set_editor_property(k,brush(None,WHITE,size=(80,40)))
                w.set_style(s)
        label(W('FooterHint'),16,color=MUTED);pos(W('FooterHint'),0,-12,1728,32,anchor=(.5,1),align=(.5,1))
        style_button(W('SubmitButton'),True)
    elif key=='WBP_HeistFloorPlanMap':
        W('MapBackdropImage').set_brush(brush('MapPanel',size=(1920,1080)))
        W('MapSurfaceBackdrop').set_brush(brush('Backdrop',size=(1920,1080)))
        W('MapLayout').slot.set_padding(margin(240,148,240,140))
        W('MapLayout').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('MapLayout').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('MapSurfaceBackdrop').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('MapSurfaceBackdrop').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        W('MapBackdropImage').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('MapBackdropImage').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        box(W('MapAspectSize'),1600,1000)
        W('MapAspectFit').set_stretch(unreal.Stretch.SCALE_TO_FIT)
        label(W('MapTitleText'),32,True);label(W('LegendText'),20);label(W('MapHintText'),16,color=MUTED)
        W('LegendText').set_text('● 나/팀원   ↗ 출구   ◆ 목표 전시관   ★ 발견한 목표   ◇ 떨어진 원본   ○ 탈출   ! 체포')
    elif key=='WBP_ActionProgress':
        box(W('SizeBox_0'),516,60)
        W('ActionProgressContainer').set_padding(margin(24,8,24,8))
        W('ActionProgressBar').set_fill_color_and_opacity(IVORY)
        progress_style=W('ActionProgressBar').get_editor_property('widget_style')
        progress_style.set_editor_property('background_image',brush(None,INK,(4,4)))
        progress_style.set_editor_property('fill_image',brush(None,WHITE,(4,4)))
        W('ActionProgressBar').set_editor_property('widget_style',progress_style)
    elif key=='WBP_HeistPopupFeedback':
        fixed_bounds(bp,W('PopupContainer'),688,80)
        W('PopupContainer').set_padding(margin(24,16,24,16))
        W('PopupText').set_editor_property('wrap_text_at',640)
    elif key=='WBP_InteractionPrompt':
        fixed_bounds(bp,W('InteractionPromptContainer'),688,80)
        W('InteractionPromptContainer').set_padding(margin(24,12,24,12))
        W('TargetText').set_editor_property('wrap_text_at',640)
        W('AvailabilityText').set_editor_property('justification',unreal.TextJustify.CENTER)
    elif key=='WBP_ResultReplicaCard':
        flat(W('ReplicaCardRoot'));W('ReplicaCardRoot').set_padding(margin(16,12,16,12))
        box(W('ReplicaImageSize'),176,176)
        W('ArtifactNameText').set_editor_property('min_desired_width',224);W('ArtifactNameText').set_editor_property('wrap_text_at',224)
        label(W('ArtifactNameText'),20);label(W('QualityText'),16)
    elif key=='WBP_ResultPlayerRow':
        flat(W('PlayerResultRowRoot'),unreal.LinearColor(.028,.025,.020,1));W('PlayerResultRowRoot').set_padding(margin(8,4,8,4))
        box(W('ProfileImageSize'),32,32)
    elif key=='WBP_Result':
        for n,w in ws.items():
            if isinstance(w,unreal.Border):panel(w)
            if isinstance(w,unreal.TextBlock) and n.startswith('Header'):label(w,16)
        # Keep the established recap/contribution responsibility and populated row widths.
        for n,w in ws.items():
            if isinstance(w,unreal.TextBlock) and ('Title' in n or 'Outcome' in n):label(w,32,True)
        flat(W('ResultBackdrop'),unreal.LinearColor(.009,.008,.007,1))
        label(W('OutcomeReasonTextBlock'),20)
        W('ReplicaRecapVisualPanel').set_padding(margin(24))
        W('ContributionTablePanel').set_padding(margin(24,40,24,16))
        pos(W('ReplicaRecapVisualPanel'),0,-152,1400,352,anchor=(.5,.5),align=(.5,.5))
        pos(W('ContributionTablePanel'),0,204,1400,328,anchor=(.5,.5),align=(.5,.5))
        W('HeaderProfile').set_editor_property('min_desired_width',36)
        pos(W('ContributionTableHeader'),0,92,1092,36,anchor=(.5,.5),align=(.5,.5))
        pos(W('TeamRewardTextBlock'),0,-136,672,32,anchor=(.5,1),align=(.5,1))
        W('ReplicaRecapScrollBox').set_editor_property('scrollbar_thickness',unreal.Vector2D(8,8))

def fixed_bounds(bp,w,width,height):
    if isinstance(w.slot,unreal.CanvasPanelSlot):
        w.slot.set_auto_size(False);w.slot.set_size(unreal.Vector2D(width,height));return
    parent=w.get_parent()
    if not isinstance(parent,unreal.SizeBox):
        parent=TOOLS.call_method('WrapWidgets',(bp,[w],unreal.SizeBox.static_class()))[0].widget
        parent=TOOLS.call_method('RenameWidget',(bp,parent,w.get_name()+'_ImageBounds')).widget
    box(parent,width,height)
    if isinstance(parent.slot,(unreal.HorizontalBoxSlot,unreal.VerticalBoxSlot)):
        parent.slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
    if parent.slot:
        parent.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        parent.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

def fit_image(bp,w):
    parent=w.get_parent()
    if not isinstance(parent,unreal.ScaleBox):
        parent=TOOLS.call_method('WrapWidgets',(bp,[w],unreal.ScaleBox.static_class()))[0].widget
        parent=TOOLS.call_method('RenameWidget',(bp,parent,w.get_name()+'_AspectFit')).widget
    parent.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    parent.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    b=w.get_editor_property('brush');texture=b.get_editor_property('resource_object')
    if isinstance(texture,unreal.Texture2D):w.set_brush_from_texture(texture,True)

def image_presentation(bp,key,ws):
    panels={'SettingsPanel':('SettingsPanel',(1360,752)), 'SessionJoinPanel':('JoinPanel',(1008,560)),
        'MapRootBorder':('MapPanel',(1920,1080)), 'InventoryPanel':('InventoryPanel',(768,960)),
        'RewardDetailRoot':('RewardPanel',(540,700)), 'ReplicaRecapVisualPanel':('RecapPanel',(1400,352)),
        'ContributionTablePanel':('ContributionPanel',(1400,328)), 'PlayerCardBorder':('PlayerPanel',(400,300)),
        'MissionPanel':('HUDMissionPanel',(304,208)), 'AlertMeterPanel':('HUDAlertPanel',(344,112)),
        'TeamCardBorder':('HUDTeamPanel',(304,80)), 'TutorialCardContainer':('HUDTutorialPanel',(800,200))}
    panels.update({'ActionProgressContainer':('Field',(516,60)), 'PopupContainer':('Field',(688,80)),
                   'InteractionPromptContainer':('Field',(688,80))})
    if key=='WBP_HeistHUD':panels['Border_0']=('HUDSlotPanel',(88,88))
    if key=='WBP_QuickSlot':
        for name,w in ws.items():
            if isinstance(w,unreal.Border):panels[name]=('HUDSlotPanel',(88,88))
    for name,w in ws.items():
        if isinstance(w,unreal.Border):
            if name in panels:
                texture,size=panels[name];w.set_brush(brush(texture,size=size));w.set_brush_color(WHITE)
            elif name not in ['DrawingContainer','DrawingSurface','CrewStatusBadge','LobbyRootBorder'] and not name.endswith('_CatalogueTab'):
                # Content-sized utility surfaces have no decorative frame to deform.
                b=w.get_editor_property('background')
                if b.get_editor_property('resource_object'):flat(w)
        if isinstance(w,unreal.Button) and not name.startswith('PaletteButton') and name!='SelectMapButton':
            if key=='WBP_TitleMenu':size=(384,108)
            elif name in ['CloseButton','CopyJoinCodeButton']:size=(128,36)
            elif name=='LeaveSessionButton':size=(200,56)
            elif name.startswith('Brush'):size=(144,40)
            elif name in ['SubmitJoinSessionButton','StartGameButton','ReadyButton','SubmitButton','CancelButton','ReturnToLobbyButton','RewardDetailsButton']:size=(256,72)
            else:size=(224,64)
            fixed_bounds(bp,w,*size)
            s=w.get_editor_property('widget_style')
            primary=name in ['HostSessionButton','ApplySettingsButton','SubmitJoinSessionButton','StartGameButton','SubmitButton']
            for state,art in [('normal','Primary' if primary else 'Normal'),('hovered','Primary'),('pressed','Pressed'),('disabled','Normal')]:
                tint=unreal.LinearColor(.5,.5,.5,.72) if state=='disabled' else WHITE
                s.set_editor_property(state,brush(f'Button_{art}_{size[0]}x{size[1]}',tint,size))
            w.set_style(s)
        if isinstance(w,unreal.ComboBoxString):
            s=w.get_editor_property('widget_style');cb=s.combo_button_style;bs=cb.button_style
            for state in ['normal','hovered','pressed','disabled']:bs.set_editor_property(state,brush('Field',size=(688,80)))
            cb.set_editor_property('button_style',bs);s.set_editor_property('combo_button_style',cb);w.set_editor_property('widget_style',s)
        if isinstance(w,unreal.EditableTextBox):
            fixed_bounds(bp,w,688,80)
            s=w.get_editor_property('widget_style')
            for state in ['normal','hovered','focused','read_only']:s.set_editor_property('background_image_'+state,brush('Field',size=(688,80)))
            w.set_editor_property('widget_style',s)
        if isinstance(w,unreal.Image) and name in ['PlaceholderIcon','ProfileImage','MapThumbnailImage','ReplicaImage','ReferenceImage']:
            fit_image(bp,w)
        if isinstance(w,unreal.TextBlock) and name in ['PlayerNameText','NameText']:
            w.set_editor_property('text_overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)
            w.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    if key=='WBP_HeistNameplate':
        ws['CrewStatusBadge'].set_padding(margin(4))
        for name in ['PlayerNameText','CrewStatusText']:
            ws[name].set_editor_property('justification',unreal.TextJustify.LEFT)
            ws[name].set_editor_property('auto_wrap_text',False)
            ws[name].set_editor_property('wrap_text_at',0)
    if key=='WBP_HeistHUD':
        ws['AlertMeterPanel'].slot.set_size(unreal.Vector2D(344,112))
        pos(ws['InteractionPromptWidget'],0,-224,688,80,anchor=(.5,1),align=(.5,.5))
        pos(ws['ActionProgressWidget'],0,-136,516,60,anchor=(.5,1),align=(.5,.5))
        pos(ws['PopupFeedbackLayer'],0,0,688,0,anchor=(.5,.5),align=(.5,0))
    if key=='WBP_Settings':fixed_bounds(bp,ws['SettingsColumn'],944,688)

paths=sorted(unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI',recursive=True,include_folder=False))
results=[]
descriptions={}
for path in paths:
    if not path.split('/')[-1].startswith('WBP_'):continue
    bp=unreal.load_asset(path)
    if not isinstance(bp,unreal.WidgetBlueprint):continue
    key=bp.get_name()
    if 'ObjectAssembly' in key:continue
    CURRENT_ALREADY_FIXED=unreal.EditorAssetLibrary.get_metadata_tag(bp,'CatalogueFontDisplayDPI')=='72'
    bp.modify()
    ws={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
    # Remove obsolete presentation nodes before styling so reruns cannot restore them.
    if key=='WBP_Inventory' and 'InventorySummaryText' in ws:
        assert TOOLS.call_method('RemoveWidget',(bp,ws['InventorySummaryText']))
    if key=='WBP_HeistNameplate' and 'NameplateBorder' in ws:
        background=ws['NameplateBorder']
        content=background.get_content()
        assert content and content.get_name()=='NameplateContentRow'
        assert TOOLS.call_method('ReplaceWidgetWithChild',(bp,background))
    if key=='WBP_HeistFloorPlanMap' and 'MapRootBorder' in ws:
        old=ws['MapRootBorder']
        overlay=TOOLS.call_method('WrapWidgets',(bp,[old],unreal.Overlay.static_class()))[0].widget
        overlay=TOOLS.call_method('RenameWidget',(bp,overlay,'MapRootOverlay')).widget
        bg=TOOLS.call_method('AddWidget',(bp,unreal.Image.static_class(),'MapBackdropImage',overlay,0)).widget
        bg.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        assert TOOLS.call_method('ReplaceWidgetWithChild',(bp,old))
        size=TOOLS.call_method('WrapWidgets',(bp,[ws['MapOverlay']],unreal.SizeBox.static_class()))[0].widget
        size=TOOLS.call_method('RenameWidget',(bp,size,'MapAspectSize')).widget
        fit=TOOLS.call_method('WrapWidgets',(bp,[size],unreal.ScaleBox.static_class()))[0].widget
        TOOLS.call_method('RenameWidget',(bp,fit,'MapAspectFit'))
    if key=='WBP_InteractionPrompt' and 'InteractionPromptContainer' not in ws:
        border=TOOLS.call_method('WrapWidgets',(bp,[ws['PromptColumn']],unreal.Border.static_class()))[0].widget
        TOOLS.call_method('RenameWidget',(bp,border,'InteractionPromptContainer'))
    if key=='WBP_HeistFloorPlanMap':
        current={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
        if 'MapSurfaceBackdrop' not in current:
            bg=TOOLS.call_method('AddWidget',(bp,unreal.Image.static_class(),'MapSurfaceBackdrop',current['MapRootOverlay'],0)).widget
            bg.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    ws={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
    for name,w in ws.items():
        w.modify();quantize(w)
        if isinstance(w,unreal.TextBlock):
            label(w,heading=('Title' in name or name=='MapNameText'))
        elif isinstance(w,unreal.Border):
            if name not in ['DrawingSurface','CrewStatusBadge'] and not name.startswith('GridCell'):
                if 'Background' in name and 'Slot' not in name:flat(w)
                else:panel(w)
        elif isinstance(w,unreal.Button):
            if not name.startswith('PaletteButton') and name!='SelectMapButton':style_button(w)
        controls(w)
    apply_screen(bp,key,ws)
    # Button children may have been styled after their parent in the depth-first walk.
    final_ws={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
    for name,w in final_ws.items():
        quantize(w)
        if isinstance(w,unreal.Button) and not name.startswith('PaletteButton') and name!='SelectMapButton':
            style_button(w,name in ['HostSessionButton','ApplySettingsButton','SubmitJoinSessionButton','StartGameButton','SubmitButton'])
    image_presentation(bp,key,final_ws)
    unreal.EditorAssetLibrary.set_metadata_tag(bp,'CatalogueFontDisplayDPI','72')
    ok=TOOLS.call_method('CompileWidgetBlueprint',(bp,))
    if not ok:raise RuntimeError('Compile failed '+path)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    descriptions[path]=TOOLS.call_method('GetWidgetDescription',(bp,None,-1)).description
    results.append({'asset':path,'widgets':len(final_ws),'compiled':bool(ok)})
    unreal.log_warning('CATALOGUE APPLIED '+key)
(ROOT/'Saved/Logs/CatalogueApply.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
(ROOT/'Saved/Logs/CatalogueWidgetDescriptions.json').write_text(json.dumps(descriptions,ensure_ascii=False,indent=2),encoding='utf-8')
