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
TEX={key:unreal.load_asset('/Game/Assets/UI/Catalogue/T_Catalogue_'+key) for key in
     ['Button_Normal','Button_Primary','Button_Pressed','Panel','Header','Logo','Thumb','Chevron','Backdrop']}
assert all(TEX.values()),'Import textures first'

def q(v):
    return float(math.floor(float(v)/4+0.5)*4)

def margin(l=0,t=None,r=None,b=None):
    return unreal.Margin(l,l if t is None else t,l if r is None else r,l if b is None else b)

def sc(c): return unreal.SlateColor(specified_color=c)

def brush(key=None,color=WHITE,size=(256,64),slices=True):
    b=unreal.WidgetLibrary.make_brush_from_texture(TEX.get(key),int(size[0]),int(size[1]))
    b.set_editor_property('tint_color',sc(color))
    b.set_editor_property('draw_as',unreal.SlateBrushDrawType.BOX if slices else unreal.SlateBrushDrawType.IMAGE)
    if slices: b.set_editor_property('margin',margin(.125,.25,.125,.25) if key and key.startswith('Button') else margin(.125))
    return b

def font(w,size=None,kind=None):
    f=w.get_editor_property('font')
    f.set_editor_property('font_object',kind or BODY)
    f.set_editor_property('typeface_font_name','Default')
    f.set_editor_property('size',q(size if size is not None else max(16,f.size)))
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
    w.set_brush(brush(None,color,slices=False,size=(4,4)))
    w.set_brush_color(WHITE)

def box(w,width,height=None):
    w.set_width_override(width)
    if height is not None:w.set_height_override(height)

def pos(w,x,y,width,height,anchor=(0,0),align=(0,0)):
    s=w.slot
    assert isinstance(s,unreal.CanvasPanelSlot),w.get_name()
    s.set_anchors(unreal.Anchors(unreal.Vector2D(*anchor),unreal.Vector2D(*anchor)))
    s.set_alignment(unreal.Vector2D(*align))
    s.set_auto_size(False)
    s.set_position(unreal.Vector2D(x,y))
    s.set_size(unreal.Vector2D(width,height))

def image_size(w,width,height):
    w.set_brush_size(unreal.Vector2D(width,height))

def tab_title(bp,w,width=240,height=64):
    """Wrap an existing bound title, preserving its identity and outer slot."""
    title_name=w.get_name()+'_CatalogueTab'
    current={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets}
    if title_name in current:
        wrapper=current[title_name]
    else:
        wrapper=TOOLS.call_method('WrapWidgets',(bp,[w],unreal.Border.static_class()))[0].widget
        wrapper=TOOLS.call_method('RenameWidget',(bp,wrapper,title_name)).widget
    panel(wrapper,True);wrapper.set_padding(margin(24,8,24,8))
    w.set_editor_property('justification',unreal.TextJustify.CENTER)
    w.set_editor_property('min_desired_width',160)
    label(w,32,True,INK)
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
            s.set_editor_property(key,brush('Thumb',size=(24,24),slices=False))
        for key in ['normal_bar_image','hovered_bar_image','disabled_bar_image']:
            s.set_editor_property(key,brush(None,unreal.LinearColor(.34,.31,.25,1),size=(4,4),slices=False))
        s.set_editor_property('bar_thickness',4)
        w.set_editor_property('widget_style',s)
        w.set_slider_handle_color(WHITE);w.set_slider_bar_color(WHITE)
    elif isinstance(w,unreal.ComboBoxString):
        font(w,20)
        s=w.get_editor_property('widget_style');cb=s.combo_button_style
        bs=button_style(cb.button_style);bs.set_editor_property('normal_padding',margin(0));bs.set_editor_property('pressed_padding',margin(0,4,0,0));cb.set_editor_property('button_style',bs)
        cb.set_editor_property('down_arrow_image',brush('Chevron',size=(24,24),slices=False))
        cb.set_editor_property('menu_border_brush',brush('Panel',size=(256,256)))
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
            row.set_editor_property(key,brush(None,INK,size=(4,4),slices=False))
        for key in ['even_row_background_hovered_brush','odd_row_background_hovered_brush','active_brush','active_hovered_brush']:
            row.set_editor_property(key,brush('Button_Normal'))
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
        fs=s.text_style.font;fs.set_editor_property('font_object',BODY);fs.set_editor_property('size',24);fs.set_editor_property('typeface_font_name','Default')
        ts=s.text_style;ts.set_editor_property('font',fs);ts.set_editor_property('color_and_opacity',sc(IVORY));s.set_editor_property('text_style',ts)
        w.set_editor_property('widget_style',s)

def apply_screen(bp,key,ws):
    W=lambda name:ws[name]
    if key=='WBP_TitleMenu':
        W('Image_113').set_brush(brush('Backdrop',size=(1920,1080),slices=False))
        W('LogoImage').set_brush(brush('Logo',size=(512,288),slices=False));pos(W('LogoImage'),88,88,512,288)
        W('GameSubtitleText').set_text('잠입 · 위조 · 탈출');label(W('GameSubtitleText'),20);W('GameSubtitleText').set_editor_property('justification',unreal.TextJustify.LEFT)
        pos(W('GameSubtitleText'),128,400,448,40)
        pos(W('TitleMenuColumn'),96,464,480,456)
        for name in ['HostSessionSize','JoinSessionSize','SettingsSize','QuitGameSize']:box(W(name),448,80)
        W('Spacer_185').set_size(unreal.Vector2D(4,4))
        for name in ['HostSessionButtonText','JoinSessionButtonText','SettingsButtonText','QuitGameButtonText']:font(W(name),28,TITLE)
        style_button(W('HostSessionButton'),True)
    elif key=='WBP_Settings':
        box(W('SettingsPanelSize'),1120,752);W('SettingsPanel').set_padding(margin(48,32,48,32))
        tab_title(bp,W('SettingsTitleText'),256,64)
        for prefix in ['FOV','Sensitivity','Volume','Resolution','WindowMode']:
            box(W(prefix+'RowLabelSize'),208,56)
            if prefix in ['FOV','Sensitivity','Volume']:
                box(W(prefix+'RowSliderSize'),608,56);box(W(prefix+'RowValueSize'),80,56)
            else:box(W(prefix+'RowComboSize'),688,56)
        for name in ['FOVRowLabel','SensitivityRowLabel','VolumeRowLabel','ResolutionRowLabel','WindowModeRowLabel']:label(W(name),24,True)
        for name in ['FOVValueText','MouseSensitivityValueText','MasterVolumeValueText']:label(W(name),24)
        for name in ['RestoreDefaultSettingsButtonSize','ApplySettingsButtonSize','SettingsCloseButtonSize']:box(W(name),224,64)
        style_button(W('ApplySettingsButton'),True)
        W('SettingsActionRow').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    elif key=='WBP_SessionJoin':
        box(W('SessionJoinPanelSize'),960,560);W('SessionJoinPanel').set_padding(margin(48,32,48,32))
        tab_title(bp,W('SessionJoinTitleText'),256,64)
        box(W('SubmitJoinSessionSize'),320,64)
        for n in ['JoinCloseSize','RetrySessionSize','CancelSessionSize']:box(W(n),224,56)
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
        box(W('PlayerCardSizeBox'),400,336);W('PlayerCardBorder').set_padding(margin(24))
        box(W('ProfileImageSizeBox'),112,112);box(W('SizeBox_0'),288,56)
        label(W('PlayerSlotText'),24,True);label(W('PlayerNameText'),20)
        label(W('ReadyButtonLabel'),20);image_size(W('ReadyCheckImage'),24,24)
        W('ReadyButtonLabel').slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
        W('ReadyButtonLabel').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('ReadyCheckImage').slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
        W('ReadyCheckImage').slot.set_padding(margin(8,0,0,0))
        for n in ['Spacer_133','Spacer_1','Spacer']:W(n).set_size(unreal.Vector2D(4,16))
    elif key=='WBP_LobbyMapCard':
        box(W('MapCardSizeBox'),400,248);label(W('MapNameText'),24,True)
        W('MapNameText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
        W('MapNameText').slot.set_padding(margin(16))
        W('MapNameText').set_shadow_color_and_opacity(unreal.LinearColor(0,0,0,1));W('MapNameText').set_shadow_offset(unreal.Vector2D(0,4))
        image_size(W('SelectedCheckImage'),40,40)
    elif key=='WBP_Lobby':
        W('LobbyRootBorder').set_brush(brush('Backdrop',color=unreal.LinearColor(.28,.28,.28,1),size=(1920,1080),slices=False));W('LobbyRootBorder').set_brush_color(WHITE)
        W('LobbyRootBorder').set_padding(margin(48,32,48,32))
        W('LobbyLogoImage').set_brush(brush('Logo',size=(144,80),slices=False))
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
        box(copy_parent,144,56)
        copy_parent.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        box(W('LeaveSessionSize'),200,56);box(W('PlayerCountSize'),112,56)
        W('Spacer_1').set_size(unreal.Vector2D(32,4));W('Spacer_3').set_size(unreal.Vector2D(24,4))
        for n in ['Spacer_2','Spacer_4','Spacer_5']:W(n).set_size(unreal.Vector2D(24,4))
        W('MapSection').slot.set_padding(margin(0,24,0,0));label(W('MapSectionTitle'),28,True)
        box(W('SizeBox_0'),400,72);style_button(W('StartGameButton'),True)
        for n in ['MapRandomCard','MapM01Card','MapM02Card','MapM03Card']:W(n).slot.set_padding(margin(8))
        for code in ['M01','M02','M03']:
            thumb=unreal.load_asset('/Game/Assets/UI/Catalogue/T_Catalogue_Map_'+code)
            if thumb:W('Map'+code+'Card').set_editor_property('MapThumbnail',thumb)
        W('MapRandomCard').set_editor_property('MapThumbnail',TEX['Backdrop'])
    elif key=='WBP_Inventory':
        pos(W('InventoryPanel'),0,0,768,944,anchor=(.5,.5),align=(.5,.5));W('InventoryPanel').set_padding(margin(32))
        title_tab=tab_title(bp,W('InventoryTitleText'),240,64);pos(title_tab,0,16,240,64)
        pos(W('CloseButton'),-16,24,128,48,anchor=(1,0),align=(1,0))
        pos(W('InventorySummaryText'),16,112,672,104);label(W('InventorySummaryText'),20)
        W('InventorySummaryText').set_editor_property('wrap_text_at',672)
        pos(W('InventoryFrameWidget'),0,112,640,640,anchor=(.5,.5),align=(.5,.5))
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
        panel(W('DrawingContainer'));W('DrawingContainer').set_padding(margin(12))
        # DrawingSurface stays 800 square to preserve pointer/raster aspect and brush scale.
        box(W('DrawingSurfaceSizeBox'),800,800)
        label(W('TitleText'),32,True);label(W('DrawingTimeRemainingText'),24);label(W('PreviewScoreText'),20)
        pos(W('VerticalBox_0'),0,72,1400,128,anchor=(.5,0),align=(.5,.5))
        pos(W('DrawingContent'),0,0,144,768,anchor=(.5,.5),align=(.5,.5))
        pos(W('DrawingContainer'),440,0,824,824,anchor=(0,.5),align=(.5,.5))
        pos(W('ReferenceImage'),-440,0,800,800,anchor=(1,.5),align=(.5,.5))
        W('BrushSizeLabel_1').set_text('팔레트')
        for n,t in [('BrushSmallLabel','소'),('BrushMediumLabel','중'),('BrushLargeLabel','대')]:
            W(n).set_text(t);label(W(n),20)
        for n,w in ws.items():
            if n.startswith('PaletteButton') and isinstance(w,unreal.Button):
                # Palette backgrounds are the actual paint colours, not decorative paper.
                s=w.get_editor_property('widget_style')
                for k in ['normal','hovered','pressed']:s.set_editor_property(k,brush(None,WHITE,size=(80,40),slices=False))
                w.set_style(s)
        label(W('FooterHint'),16,color=MUTED);pos(W('FooterHint'),0,-12,1728,32,anchor=(.5,1),align=(.5,1))
        style_button(W('SubmitButton'),True)
    elif key=='WBP_HeistFloorPlanMap':
        W('MapRootBorder').set_padding(margin(48,32,48,32));panel(W('MapRootBorder'))
        label(W('MapTitleText'),32,True);label(W('LegendText'),20);label(W('MapHintText'),16,color=MUTED)
        W('LegendText').set_text('● 나/팀원   ↗ 출구   ◆ 목표 전시관   ★ 발견한 목표   ◇ 떨어진 원본   ○ 탈출   ! 체포')
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
        pos(W('ReplicaRecapVisualPanel'),0,-152,1400,360,anchor=(.5,.5),align=(.5,.5))
        pos(W('ContributionTablePanel'),0,180,1400,272,anchor=(.5,.5),align=(.5,.5))
        W('HeaderProfile').set_editor_property('min_desired_width',36)
        pos(W('ContributionTableHeader'),0,92,1092,36,anchor=(.5,.5),align=(.5,.5))
        W('ReplicaRecapScrollBox').set_editor_property('scrollbar_thickness',unreal.Vector2D(8,8))

paths=sorted(unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI',recursive=True,include_folder=False))
results=[]
descriptions={}
for path in paths:
    if not path.split('/')[-1].startswith('WBP_'):continue
    bp=unreal.load_asset(path)
    if not isinstance(bp,unreal.WidgetBlueprint):continue
    key=bp.get_name()
    if 'ObjectAssembly' in key:continue
    bp.modify()
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
    ok=TOOLS.call_method('CompileWidgetBlueprint',(bp,))
    if not ok:raise RuntimeError('Compile failed '+path)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    descriptions[path]=TOOLS.call_method('GetWidgetDescription',(bp,None,-1)).description
    results.append({'asset':path,'widgets':len(final_ws),'compiled':bool(ok)})
    unreal.log_warning('CATALOGUE APPLIED '+key)
(ROOT/'Saved/Logs/CatalogueApply.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
(ROOT/'Saved/Logs/CatalogueWidgetDescriptions.json').write_text(json.dumps(descriptions,ensure_ascii=False,indent=2),encoding='utf-8')
