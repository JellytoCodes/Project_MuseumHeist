"""Apply exhibition-label presentation to existing WBP shells, without new assets."""
import hashlib
import json
import math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
TOOLS=unreal.get_default_object(unreal.UMGToolSet)
WHITE=unreal.LinearColor(1,1,1,1)
IVORY=unreal.LinearColor(.90,.88,.81,1)
INK=unreal.LinearColor(.012,.016,.020,1)
MUTED=unreal.LinearColor(.52,.56,.58,1)
GOLD=unreal.LinearColor(.58,.43,.22,1)
EDGE=unreal.LinearColor(.12,.15,.17,1)
SURFACE=unreal.LinearColor(.025,.031,.038,.96)
OUT=ROOT/'Saved/Automation'/globals().get('QA_FOLDER','UIUX20260930')/'Apply'
OUT.mkdir(parents=True,exist_ok=True)
MAP_PATHS=sorted((ROOT/'Content/Maps').rglob('*.umap'))
MAP_BEFORE={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in MAP_PATHS}
ASSETS_BEFORE=sorted(str(p.relative_to(ROOT)) for p in (ROOT/'Content').rglob('*.uasset'))
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
    return int(math.floor(float(v)/4+0.5)*4)

def margin(l=0,t=None,r=None,b=None):
    return unreal.Margin(l,l if t is None else t,l if r is None else r,l if b is None else b)

def sc(c): return unreal.SlateColor(specified_color=c)

def brush(key=None,color=WHITE,size=(256,64),outline=None):
    assert all(int(v)==v and int(v)%4==0 for v in size)
    b=unreal.WidgetLibrary.make_brush_from_texture(TEX.get(key),int(size[0]),int(size[1]))
    # MakeBrushFromTexture returns a zero-size brush when no texture is given.
    # Native controls such as Slider use ImageSize to arrange their handle.
    image_size=b.get_editor_property('image_size')
    image_size.set_editor_property('x',size[0]);image_size.set_editor_property('y',size[1])
    b.set_editor_property('image_size',image_size)
    b.set_editor_property('tint_color',sc(color))
    b.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
    b.set_editor_property('margin',margin(0))
    if key is None:
        b.set_editor_property('draw_as',unreal.SlateBrushDrawType.ROUNDED_BOX)
        s=b.get_editor_property('outline_settings')
        s.set_editor_property('corner_radii',unreal.Vector4(4,4,4,4))
        s.set_editor_property('rounding_type',unreal.SlateBrushRoundingType.FIXED_RADIUS)
        s.set_editor_property('width',4 if outline else 0)
        s.set_editor_property('color',sc(outline or unreal.LinearColor(0,0,0,0)))
        b.set_editor_property('outline_settings',s)
    return b

def font(w,size=None,kind=None):
    f=w.get_editor_property('font')
    f.set_editor_property('font_object',kind or BODY)
    f.set_editor_property('typeface_font_name','Default')
    f.set_editor_property('letter_spacing',0)
    displayed=f.size/FONT_NATIVE_PER_DISPLAY if CURRENT_ALREADY_FIXED else f.size
    f.set_editor_property('size',int(q(size if size is not None else max(20,displayed))*FONT_NATIVE_PER_DISPLAY))
    outline=f.get_editor_property('outline_settings');outline.set_editor_property('outline_size',0);f.set_editor_property('outline_settings',outline)
    w.set_editor_property('font',f)

def label(w,size=None,heading=False,color=IVORY):
    font(w,size,BOLD if heading else BODY)
    w.set_color_and_opacity(sc(color))

def button_style(existing=None,primary=False):
    s=existing or unreal.ButtonStyle()
    s.set_editor_property('normal',brush(color=GOLD if primary else SURFACE,outline=None if primary else EDGE))
    s.set_editor_property('hovered',brush(color=unreal.LinearColor(.74,.57,.30,1) if primary else unreal.LinearColor(.09,.12,.14,1),outline=IVORY))
    s.set_editor_property('pressed',brush(color=unreal.LinearColor(.40,.29,.14,1) if primary else INK,outline=GOLD))
    s.set_editor_property('disabled',brush(color=unreal.LinearColor(.03,.035,.04,.60)))
    for key,col in [('normal_foreground',INK if primary else IVORY),('hovered_foreground',INK if primary else IVORY),('pressed_foreground',IVORY),('disabled_foreground',MUTED)]:
        s.set_editor_property(key,sc(col))
    s.set_editor_property('normal_padding',margin(20,12,20,12))
    s.set_editor_property('pressed_padding',margin(20,12,20,12))
    return s

def style_button(w,primary=False):
    s=button_style(w.get_editor_property('widget_style'),primary)
    if 'Brush' in w.get_name() or w.get_name()=='CloseButton':
        s.set_editor_property('normal_padding',margin(4));s.set_editor_property('pressed_padding',margin(4))
    w.set_style(s)
    w.set_background_color(WHITE)
    w.set_color_and_opacity(WHITE)
    # Let the authored button state choose the text colour on ivory hover surfaces.
    def inherit(child):
        if isinstance(child,unreal.TextBlock):
            # Old minimum widths and fill slots survive a font/style change.
            # Centre the actual text, rather than only the enclosing button.
            child.set_editor_property('min_desired_width',0)
            child.set_editor_property('justification',unreal.TextJustify.CENTER)
            child.set_editor_property('auto_wrap_text',False)
            child.set_editor_property('wrap_text_at',0)
            font(child,kind=BOLD)
            child.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
        if isinstance(child,unreal.PanelWidget):
            for c in child.get_all_children(): inherit(c)
    for c in w.get_all_children():
        c.slot.set_padding(margin(0));c.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);c.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        inherit(c)

def panel(w,light=False):
    flat(w,SURFACE)

def flat(w,color=SURFACE,outline=None):
    w.set_brush(brush(None,color,size=(4,4),outline=outline))
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
    flat(wrapper,unreal.LinearColor(0,0,0,0));wrapper.set_padding(margin(0))
    w.set_editor_property('justification',unreal.TextJustify.LEFT)
    w.set_editor_property('min_desired_width',160)
    label(w,36,True,IVORY)
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
        for key,color in [('normal_thumb_image',IVORY),('hovered_thumb_image',GOLD),('disabled_thumb_image',MUTED)]:
            s.set_editor_property(key,brush(color=color,size=(16,24)))
        for key in ['normal_bar_image','hovered_bar_image','disabled_bar_image']:
            s.set_editor_property(key,brush(color=EDGE,size=(4,4)))
        s.set_editor_property('bar_thickness',4)
        w.set_editor_property('widget_style',s)
        w.set_slider_handle_color(WHITE);w.set_slider_bar_color(WHITE)
    elif isinstance(w,unreal.ComboBoxString):
        font(w,24)
        s=w.get_editor_property('widget_style');cb=s.combo_button_style
        bs=button_style(cb.button_style);bs.set_editor_property('normal_padding',margin(0));bs.set_editor_property('pressed_padding',margin(0));cb.set_editor_property('button_style',bs)
        cb.set_editor_property('down_arrow_image',brush('Chevron',size=(24,24)))
        cb.set_editor_property('menu_border_brush',brush(color=SURFACE,size=(4,4),outline=EDGE))
        cb.set_editor_property('menu_border_padding',margin(8))
        cb.set_editor_property('content_padding',margin(0))
        cb.set_editor_property('down_arrow_padding',margin(8,0,24,0))
        arrow_shadow=cb.get_editor_property('shadow_offset');arrow_shadow.set_editor_property('x',0);arrow_shadow.set_editor_property('y',0);cb.set_editor_property('shadow_offset',arrow_shadow)
        s.set_editor_property('combo_button_style',cb)
        s.set_editor_property('content_padding',margin(0))
        s.set_editor_property('menu_row_padding',margin(8))
        w.set_editor_property('widget_style',s)
        w.set_editor_property('foreground_color',sc(IVORY))
        w.set_editor_property('content_padding',margin(24,0,24,0))
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
        for key,color in [('normal',EDGE),('hovered',GOLD),('focused',IVORY),('read_only',EDGE)]:
            s.set_editor_property('background_image_'+key,brush(color=INK,outline=color))
        s.set_editor_property('padding',margin(24,12,24,12))
        s.set_editor_property('foreground_color',sc(IVORY))
        s.set_editor_property('focused_foreground_color',sc(IVORY))
        s.set_editor_property('read_only_foreground_color',sc(IVORY))
        fs=s.text_style.font;fs.set_editor_property('font_object',BODY);fs.set_editor_property('size',24*FONT_NATIVE_PER_DISPLAY);fs.set_editor_property('typeface_font_name','Default')
        ts=s.text_style;ts.set_editor_property('font',fs);ts.set_editor_property('color_and_opacity',sc(IVORY));s.set_editor_property('text_style',ts)
        w.set_editor_property('widget_style',s)

def apply_existing_layout(bp,key,ws):
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
        check=W('ReadyCheckImage')
        check_slot=check.get_parent().slot if isinstance(check.get_parent(),unreal.SizeBox) else check.slot
        check_slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
        check_slot.set_padding(margin(8,0,0,0))
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

def add_widget(bp,cls,name,parent,index=-1):
    current={str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
    return current.get(name) or TOOLS.call_method('AddWidget',(bp,cls.static_class(),name,parent,index)).widget

def apply_screen(bp,key,ws):
    if key=='WBP_TitleMenu' and 'GameSubtitleText' not in ws:
        ws['GameSubtitleText']=add_widget(bp,unreal.TextBlock,'GameSubtitleText',ws['TitleRoot'])
    apply_existing_layout(bp,key,ws)
    W=lambda name:ws[name]
    if key=='WBP_TitleMenu':
        W('Image_113').set_brush(brush('Backdrop',unreal.LinearColor(.48,.48,.48,1),(1920,1080)))
        bg=add_widget(bp,unreal.Border,'HeistMenuShade',W('TitleRoot'),1)
        flat(bg,unreal.LinearColor(.012,.016,.020,.94));bg.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        pos(bg,0,0,640,1080)
        W('LogoImage').set_brush(brush('Logo',size=(448,252)));pos(W('LogoImage'),80,64,448,252)
        label(W('GameSubtitleText'),24,color=MUTED);pos(W('GameSubtitleText'),96,328,448,40)
        rule=add_widget(bp,unreal.Image,'HeistMenuRule',W('TitleRoot'))
        rule.set_brush(brush(color=GOLD));rule.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);pos(rule,96,396,80,4)
        pos(W('TitleMenuColumn'),96,444,448,448)
        for n in ['HostSessionSize','JoinSessionSize','SettingsSize','QuitGameSize']:box(W(n),448,72)
        for n in ['HostSessionButtonText','JoinSessionButtonText','SettingsButtonText','QuitGameButtonText']:font(W(n),28,BOLD)
        for n,w in ws.items():
            if isinstance(w,unreal.Spacer):w.set_size(unreal.Vector2D(4,24 if n!='Spacer_185' else 4))
    elif key=='WBP_Settings':
        box(W('SettingsPanelSize'),1184,752);W('SettingsPanel').set_padding(margin(48,32,48,32))
        label(W('SettingsTitleText'),36,True);fixed_bounds(bp,W('SettingsColumn'),1088,688)
        W('SettingsColumn').get_parent().slot.set_padding(margin(48,32,48,32))
        for prefix in ['FOV','Sensitivity','Volume','Resolution','WindowMode']:
            label_box=W(prefix+'RowLabelSize');box(label_box,240,64)
            label_box.slot.set_padding(margin(0,0,24,0))
            label_box.get_child_at(0).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            label_box.get_child_at(0).slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
            if prefix in ['FOV','Sensitivity','Volume']:
                box(W(prefix+'RowSliderSize'),688,64);box(W(prefix+'RowValueSize'),112,64)
                W(prefix+'RowSliderSize').slot.set_padding(margin(0,0,24,0))
                W(prefix+'RowValueSize').slot.set_padding(margin(0))
                value=W(prefix+'RowValueSize').get_child_at(0)
                value.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
                value.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
                value.set_editor_property('justification',unreal.TextJustify.RIGHT)
            else:
                box(W(prefix+'RowComboSize'),824,64);W(prefix+'RowComboSize').slot.set_padding(margin(0))
        for n in ['FOVRowLabel','SensitivityRowLabel','VolumeRowLabel','ResolutionRowLabel','WindowModeRowLabel']:label(W(n),24)
        for n in ['FOVValueText','MouseSensitivityValueText','MasterVolumeValueText']:label(W(n),24,True)
        for n in ['RestoreDefaultSettingsButtonSize','ApplySettingsButtonSize','SettingsCloseButtonSize']:
            W(n).slot.set_padding(margin(16 if n!='RestoreDefaultSettingsButtonSize' else 0,0,0,0))
    elif key=='WBP_SessionJoin':
        box(W('SessionJoinPanelSize'),832,560);W('SessionJoinPanel').set_padding(margin(48,32,48,32))
        W('SessionJoinColumn').slot.set_padding(margin(48,32,48,32))
        label(W('SessionJoinTitleText'),36,True);fixed_bounds(bp,W('JoinCodeInput'),736,64)
        W('JoinCodeInput').get_parent().slot.set_padding(margin(0))
        W('JoinCodeLabelText').slot.set_padding(margin(0))
        W('JoinCodeLabelText').set_editor_property('justification',unreal.TextJustify.LEFT)
        W('JoinCodeInput').set_editor_property('minimum_desired_width',640)
        for n in ['JoinCloseSize','RetrySessionSize','CancelSessionSize']:box(W(n),208,56)
        box(W('SubmitJoinSessionSize'),256,64)
        W('SubmitJoinSessionSize').slot.set_padding(margin(0))
        W('SubmitJoinSessionSize').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    elif key=='WBP_HeistHUD':
        pos(W('MissionPanel'),32,32,304,208);W('MissionPanel').set_padding(margin(16,12,16,12))
        flat(W('MissionPanel'),unreal.LinearColor(.012,.016,.020,.60))
        label(W('MissionTitleText'),20,True,MUTED);label(W('MissionTimeText'),40,True)
        label(W('RequiredTargetLabelText'),16,color=MUTED);label(W('RequiredTargetNameText'),24,True)
        pos(W('AlertMeterPanel'),0,32,448,128,anchor=(.5,0),align=(.5,0));flat(W('AlertMeterPanel'),unreal.LinearColor(0,0,0,0))
        W('AlertMeterPanel').set_padding(margin(16,8,16,8));label(W('AlertTitleText'),20,True,MUTED);label(W('AlertEventText'),20)
        W('AlertEventText').set_editor_property('auto_wrap_text',True);W('AlertEventText').set_editor_property('wrap_text_at',416)
        W('AlertStarRow').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        pos(W('TeamCardsPanel'),-32,32,272,304,anchor=(1,0),align=(1,0))
        pos(W('HUDQuickSlotRow'),-32,-32,152,64,anchor=(1,1),align=(1,1));W('Spacer_1').set_size(unreal.Vector2D(16,4))
        box(W('SizeBox_0'),64,64);flat(W('Border_0'),unreal.LinearColor(.012,.016,.020,.60))
        W('InventoryShortcutIcon').slot.set_padding(margin(20,24,20,12))
        if 'FlashlightStatusText' in ws:
            # One original atlas, two visually distinct states; keep the bound
            # status node and add a separate static key cue.
            row=add_widget(bp,unreal.HorizontalBox,'FlashlightControlRow',W('HUDCanvas'))
            pos(row,-32,-112,160,48,anchor=(1,1),align=(1,1))
            icon=add_widget(bp,unreal.Image,'FlashlightIcon',row,0)
            icon.set_brush(brush('Flashlight',size=(48,48)));fixed_bounds(bp,icon,48,48)
            icon.get_parent().slot.set_padding(margin(0,0,12,0))
            key_text=add_widget(bp,unreal.TextBlock,'FlashlightKeyText',row)
            key_text.set_text('[F]');label(key_text,20,True);fixed_bounds(bp,key_text,40,48)
            key_text.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            key_text.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            status=W('FlashlightStatusText')
            if status.get_parent()!=row and status.get_parent().get_parent()!=row:
                status=TOOLS.call_method('MoveWidget',(bp,status,row,-1)).widget
            status.set_text('OFF');label(status,20,color=MUTED);fixed_bounds(bp,status,48,48)
            status.set_editor_property('justification',unreal.TextJustify.CENTER)
            status.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            status.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        pos(W('InteractionPromptWidget'),0,-112,448,80,anchor=(.5,1),align=(.5,.5))
        pos(W('ActionProgressWidget'),0,-208,384,64,anchor=(.5,1),align=(.5,.5))
        pos(W('PopupFeedbackLayer'),0,-24,640,0,anchor=(.5,.5),align=(.5,0))
        label(W('TutorialBodyText'),24);label(W('TutorialProgressText'),20,color=MUTED)
    elif key=='WBP_TeamCard':
        box(W('SizeBox_0'),272,64);W('TeamCardBorder').set_padding(margin(12,8,12,8));flat(W('TeamCardBorder'),unreal.LinearColor(.012,.016,.020,.60))
        fixed_bounds(bp,W('ProfileImage_AspectFit'),40,40);image_size(W('ProfileImage'),40,40)
        marker=add_widget(bp,unreal.Image,'PlayerColorMarker',W('TeamCardRow'),0)
        marker.set_brush(brush(color=WHITE,size=(12,12)));fixed_bounds(bp,marker,12,12)
        marker.get_parent().slot.set_padding(margin(0,0,8,0));marker.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        for n in ['StatusIcon','MicStatusImage']:image_size(W(n),16,16)
        label(W('PlayerNameText'),20,True);label(W('StatusText'),20);W('Spacer').set_size(unreal.Vector2D(8,4))
    elif key=='WBP_HeistNameplate':
        marker=add_widget(bp,unreal.Image,'PlayerColorMarker',W('NameplateContentRow'),1)
        marker.set_brush(brush(color=WHITE,size=(12,12)));fixed_bounds(bp,marker,12,12)
        marker.get_parent().slot.set_padding(margin(4,0,8,0));marker.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        flat(W('CrewStatusBadge'),unreal.LinearColor(.012,.016,.020,.72));W('CrewStatusBadge').set_padding(margin(4))
        label(W('PlayerNameText'),24,True);label(W('CrewStatusText'),20)
    elif key=='WBP_QuickSlot':
        box(W('SizeBox_0'),64,64);W('PlaceholderIcon_AspectFit').slot.set_padding(margin(20,24,20,12));image_size(W('PlaceholderIcon'),24,24)
    elif key=='WBP_LobbyPlayerCard':
        box(W('PlayerCardSizeBox'),320,320)
        W('PlayerCardBorder').set_padding(margin(24))
        # BorderSlot rebuilds the content padding: update the actual slot too.
        W('PlayerCardContent').slot.set_padding(margin(24))
        W('PlayerCardContent').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('PlayerCardContent').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        for child in W('PlayerCardContent').get_all_children():
            child.slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
            child.slot.set_padding(margin(0))
        fixed_bounds(bp,W('PlayerSlotText'),272,24)
        box(W('ProfileImageSizeBox'),112,112)
        W('ProfileImageSizeBox').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('ProfileImageSizeBox').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        W('ProfileImage').set_color_and_opacity(WHITE)
        fixed_bounds(bp,W('PlayerNameText'),272,32)
        box(W('SizeBox_0'),272,56)
        W('SizeBox_0').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        fixed_bounds(bp,W('ReadyCheckImage'),24,24)
        W('ReadyCheckImage').get_parent().slot.set_padding(margin(8,0,0,0))
        image_size(W('ReadyCheckImage'),24,24)
        label(W('PlayerSlotText'),20,True,MUTED);label(W('PlayerNameText'),24,True)
        # Slate disables single-line ellipsis for centered text. Center the
        # widget in its bounds instead, keeping long names readable from left.
        W('PlayerNameText').set_editor_property('justification',unreal.TextJustify.LEFT)
        W('PlayerNameText').set_editor_property('min_desired_width',0)
        W('PlayerNameText').set_editor_property('auto_wrap_text',False)
        W('PlayerNameText').set_editor_property('wrap_text_at',0)
        W('PlayerNameText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('PlayerNameText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        W('PlayerSlotText').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('PlayerSlotText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        for n,height in [('Spacer_133',8),('Spacer_1',12),('Spacer',16)]:W(n).set_size(unreal.Vector2D(4,height))
    elif key=='WBP_LobbyMapCard':
        # Runtime replaces these brushes with the map thumbnail. Keep the
        # authored fallback and outline dimensions on the same four-pixel grid.
        W('SelectMapButton').set_style(button_style())
        box(W('MapCardSizeBox'),320,184)
        # Do not resize the outer card when sizing its thumbnail.
        thumbnail_size=W('SelectMapButton').get_parent()
        if thumbnail_size==W('MapCardSizeBox'):
            thumbnail_size=TOOLS.call_method('WrapWidgets',(bp,[W('SelectMapButton')],unreal.SizeBox.static_class()))[0].widget
            TOOLS.call_method('RenameWidget',(bp,thumbnail_size,'MapThumbnailSize'))
        box(thumbnail_size,320,180)
        thumbnail_size.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        thumbnail_size.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        W('SelectMapButton').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        W('SelectMapButton').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    elif key=='WBP_Lobby':
        W('LobbyRootBorder').set_brush(brush('Backdrop',unreal.LinearColor(.12,.12,.12,1),(1920,1080)))
        W('LobbyRootBorder').set_padding(margin(0));label(W('LobbyTitleText'),36,True)
        W('LobbyRootBorder').set_editor_property('horizontal_alignment',unreal.HorizontalAlignment.H_ALIGN_CENTER)
        W('LobbyRootBorder').set_editor_property('vertical_alignment',unreal.VerticalAlignment.V_ALIGN_CENTER)
        fixed_bounds(bp,W('LobbyContent'),1352,816)
        for child in W('LobbyContent').get_all_children():
            child.slot.set_size(unreal.SlateChildSize(0,unreal.SlateSizeRule.AUTOMATIC))
            child.slot.set_padding(margin(0))
            child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        fixed_bounds(bp,W('LobbyHeaderRow'),1352,72)
        W('Spacer_7').set_size(unreal.Vector2D(4,32))
        for n in ['Spacer_2','Spacer_4','Spacer_5']:W(n).set_size(unreal.Vector2D(24,4))
        W('PlayerCardsRow').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        label(W('JoinCodeLabel'),20,color=MUTED);label(W('JoinCodeText'),28,True)
        box(W('LeaveSessionSize'),192,56);box(W('PlayerCountSize'),96,56)
        W('MapSection').slot.set_padding(margin(0,40,0,0))
        title=W('MapSectionTitle');fixed_bounds(bp,title,1352,40)
        title.get_parent().slot.set_padding(margin(0,0,0,24));title.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        title.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        for n in ['MapRandomCard','MapM01Card','MapM02Card','MapM03Card']:W(n).slot.set_padding(margin(0,0,0 if n=='MapM03Card' else 24,0))
        W('MapHorizontalScrollBox').set_editor_property('always_show_scrollbar',False)
        W('MapHorizontalScrollBox').set_scroll_bar_visibility(unreal.SlateVisibility.COLLAPSED)
        W('Spacer_6').set_size(unreal.Vector2D(4,32));box(W('SizeBox_0'),320,72)
        W('SizeBox_0').slot.set_padding(margin(0));W('SizeBox_0').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    elif key=='WBP_Inventory':
        pos(W('InventoryPanel'),0,0,768,848,anchor=(.5,.5),align=(.5,.5));W('InventoryPanel').set_padding(margin(32))
        pos(W('InventoryFrameWidget'),0,32,640,640,anchor=(.5,.5),align=(.5,.5))
        label(W('InventoryTitleText'),36,True)
        if 'BackgroundBlur_73' in ws:W('BackgroundBlur_73').set_editor_property('blur_strength',4)
    elif key=='WBP_InventoryFrame':
        for n,w in ws.items():
            if n.startswith('GridCell') and isinstance(w,unreal.Border):flat(w,unreal.LinearColor(.035,.044,.052,1),EDGE)
    elif key=='WBP_InventorySlot':
        flat(W('SlotBackground'),WHITE,EDGE)
        W('SlotBackground').set_brush_color(unreal.LinearColor(.04,.05,.06,.96))
    elif key=='WBP_InventoryItem':
        flat(W('ItemBackground'),unreal.LinearColor(.018,.023,.029,.96),GOLD);W('ItemBackground').set_padding(margin(8))
        W('PlaceholderIcon').set_color_and_opacity(WHITE)
    elif key=='WBP_HeistForgery':
        flat(W('FullScreenBackground'),INK);flat(W('DrawingContainer'),unreal.LinearColor(.035,.044,.052,1),EDGE);W('DrawingContainer').set_padding(margin(16))
        label(W('TitleText'),32,True);label(W('DrawingTimeRemainingText'),28,True,GOLD)
        pos(W('VerticalBox_0'),0,20,960,108,anchor=(.5,0),align=(.5,0))
        pos(W('VerticalBox_0'),0,20,960,128,anchor=(.5,0),align=(.5,0))
        label(W('PreviewScoreText'),20,True,unreal.LinearColor(.72,.76,.82,1));W('PreviewScoreText').set_text('작품 유사도')
        W('PreviewScoreText').set_editor_property('justification',unreal.TextJustify.CENTER)
        bar=add_widget(bp,unreal.ProgressBar,'PreviewQualityBar',W('VerticalBox_0'))
        fixed_bounds(bp,bar,400,12);bar.get_parent().slot.set_padding(margin(0))
        bar_style=bar.get_editor_property('widget_style')
        bar_style.set_editor_property('background_image',brush(color=EDGE,size=(4,4)))
        bar_style.set_editor_property('fill_image',brush(color=WHITE,size=(4,4)))
        bar.set_editor_property('widget_style',bar_style);bar.set_percent(0)
        pos(W('DrawingContent'),0,0,144,720,anchor=(.5,.5),align=(.5,.5))
        pos(W('DrawingContainer'),448,28,832,832,anchor=(0,.5),align=(.5,.5))
        pos(W('ReferenceImage'),-448,28,800,800,anchor=(1,.5),align=(.5,.5))
        for name,text,x,anchor in [('HeistCanvasLabel','복제 작업',48,(0,0)),('HeistReferenceLabel','관찰한 원본',-848,(1,0))]:
            w=add_widget(bp,unreal.TextBlock,name,W('RootCanvas'));w.set_text(text);label(w,20,True,MUTED);pos(w,x,128,800,32,anchor=anchor)
        label(W('BrushSizeLabel_1'),20,True,MUTED);label(W('BrushSizeLabel'),20,True,MUTED)
        for n,w in ws.items():
            if n.startswith('PaletteButton') and isinstance(w,unreal.Button):fixed_bounds(bp,w,144,48)
            elif n.startswith('Brush') and isinstance(w,unreal.Button):fixed_bounds(bp,w,144,48)
            elif isinstance(w,unreal.Spacer) and w.get_parent()==W('DrawingContent'):w.set_size(unreal.Vector2D(4,8))
        for name in ['SubmitButton','CancelButton']:fixed_bounds(bp,W(name),224,64)
        pos(W('FooterActionRow'),0,-28,480,64,anchor=(.5,1),align=(.5,1))
        label(W('FooterHint'),20,color=MUTED);pos(W('FooterHint'),0,-4,1808,24,anchor=(.5,1),align=(.5,1))
    elif key=='WBP_HeistFloorPlanMap':
        W('MapBackdropImage').set_brush(brush(color=SURFACE));W('MapSurfaceBackdrop').set_brush(brush(color=INK))
        W('MapLayout').slot.set_padding(margin(96,40,96,32));label(W('MapTitleText'),36,True);label(W('MapHintText'),20,color=MUTED)
    elif key=='WBP_ActionProgress':
        box(W('SizeBox_0'),384,64);W('ActionProgressContainer').set_padding(margin(16,8,16,8));W('ActionProgressBar').set_fill_color_and_opacity(GOLD);label(W('ActionTypeText'),20,True)
    elif key=='WBP_HeistPopupFeedback':
        fixed_bounds(bp,W('PopupContainer'),640,80);label(W('PopupText'),24,True);W('PopupText').set_editor_property('wrap_text_at',592)
    elif key=='WBP_InteractionPrompt':
        fixed_bounds(bp,W('InteractionPromptContainer'),448,80);W('InteractionPromptContainer').set_padding(margin(16,8,16,8))
        label(W('TargetText'),24,True);W('TargetText').set_editor_property('wrap_text_at',416);label(W('AvailabilityText'),20)
    elif key=='WBP_ResultReplicaCard':
        box(W('ReplicaImageSize'),224,224);W('ArtifactNameText').set_editor_property('min_desired_width',272);W('ArtifactNameText').set_editor_property('wrap_text_at',272)
        badge=ws.get('RequiredTargetBadge')
        if badge is None:
            overlay=W('ReplicaImageSize').get_parent()
            if not isinstance(overlay,unreal.Overlay):
                overlay=TOOLS.call_method('WrapWidgets',(bp,[W('ReplicaImageSize')],unreal.Overlay.static_class()))[0].widget
            badge=add_widget(bp,unreal.TextBlock,'RequiredTargetBadge',overlay)
            badge.set_text('필수 목표')
        elif not isinstance(badge.get_parent(),unreal.Overlay):
            overlay=TOOLS.call_method('WrapWidgets',(bp,[W('ReplicaImageSize')],unreal.Overlay.static_class()))[0].widget
            badge=TOOLS.call_method('MoveWidget',(bp,badge,overlay,-1)).widget
        else:
            overlay=badge.get_parent()
            tree=TOOLS.call_method('GetWidgets',(bp,)).widgets[0].widget.get_outer()
            if badge.get_outer()!=tree:
                # Repair the earlier raw AddChild operation through the Editor
                # move API, which also transfers the widget to the correct tree.
                badge=TOOLS.call_method('MoveWidget',(bp,badge,overlay.get_parent(),-1)).widget
                badge=TOOLS.call_method('MoveWidget',(bp,badge,overlay,-1)).widget
        badge.slot.set_padding(margin(8));badge.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        badge.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
        label(W('ArtifactNameText'),24,True);label(W('QualityText'),20);label(badge,20,True,GOLD)
    elif key=='WBP_ResultPlayerRow':
        W('PlayerResultRowRoot').set_padding(margin(8,8,8,8));flat(W('PlayerResultRowRoot'))
        W('PlayerResultRowRoot').set_editor_property('horizontal_alignment',unreal.HorizontalAlignment.H_ALIGN_CENTER)
        box(W('ProfileImageSize'),48,48);W('ProfileImageSize').slot.set_padding(margin(0))
        profile=unreal.load_asset('/Game/Assets/UI/Common/Monochrome/T_UIProfile_EmptyPlayer_Monochrome_2048')
        assert profile,'Missing existing player profile fallback'
        W('ProfileImage').set_brush_from_texture(profile,False)
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.modify();cdo.set_editor_property('default_profile_texture',profile)
        for name,width in [('PlayerNameText',448),('PlayerStateText',96),('SurfaceForgeryCountText',96),
                           ('BestSurfaceQualityText',112),('ArtifactsRecoveredText',112),('SecuredLootValueText',160),
                           ('GuardsDistractedText',112),('TeammatesRescuedText',112),('AlarmsTriggeredText',112)]:
            fixed_bounds(bp,W(name),width,48);W(name).get_parent().slot.set_padding(margin(0))
            W(name).set_editor_property('min_desired_width',0)
            W(name).set_editor_property('justification',unreal.TextJustify.LEFT if name=='PlayerNameText' else unreal.TextJustify.CENTER)
            W(name).slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            W(name).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            if name=='PlayerNameText':W(name).slot.set_padding(margin(16,0,16,0))
        for w in ws.values():
            if isinstance(w,unreal.TextBlock):label(w,20)
    elif key=='WBP_Result':
        flat(W('ResultBackdrop'),INK);label(W('OutcomeTextBlock'),48,True);label(W('OutcomeReasonTextBlock'),24);label(W('TeamRewardTextBlock'),40,True,GOLD)
        pos(W('OutcomeTextBlock'),0,40,1400,64,anchor=(.5,0),align=(.5,0))
        pos(W('OutcomeReasonTextBlock'),0,112,1400,48,anchor=(.5,0),align=(.5,0))
        pos(W('TeamRewardTextBlock'),0,184,1400,64,anchor=(.5,0),align=(.5,0))
        W('ReplicaRecapScrollBox').slot.set_padding(margin(24,8,24,8))
        W('ReplicaRecapScrollBox').set_editor_property('always_show_scrollbar',False)
        W('ContributionTablePanel').set_padding(margin(24,8,24,8))
        recap=W('ReplicaRecapVisualContainer')
        if not isinstance(recap.get_parent(),unreal.Border):
            recap_center=TOOLS.call_method('WrapWidgets',(bp,[recap],unreal.Border.static_class()))[0].widget
            TOOLS.call_method('RenameWidget',(bp,recap_center,'ReplicaRecapCenter'))
            recap_size=TOOLS.call_method('WrapWidgets',(bp,[recap_center],unreal.SizeBox.static_class()))[0].widget
            TOOLS.call_method('RenameWidget',(bp,recap_size,'ReplicaRecapViewportWidth'))
        else:
            recap_center=recap.get_parent();recap_size=recap_center.get_parent()
        flat(recap_center,unreal.LinearColor(0,0,0,0));recap_center.set_padding(margin(0))
        recap_center.set_editor_property('horizontal_alignment',unreal.HorizontalAlignment.H_ALIGN_CENTER)
        recap_center.set_editor_property('vertical_alignment',unreal.VerticalAlignment.V_ALIGN_CENTER)
        recap.slot.set_padding(margin(0));recap_size.set_min_desired_width(1488)
        pos(W('ReplicaRecapVisualPanel'),0,264,1536,352,anchor=(.5,0),align=(.5,0))
        pos(W('ContributionTablePanel'),0,680,1600,288,anchor=(.5,0),align=(.5,0))
        pos(W('ContributionTableHeader'),0,628,1408,40,anchor=(.5,0),align=(.5,0))
        for name,x in [('RewardDetailsButton',-220),('ReturnToLobbyButton',220)]:
            if name in ws:pos(W(name),x,1008,256,64,anchor=(.5,0),align=(.5,.5))
        for name,width in [('HeaderProfile',48),('HeaderPlayer',448),('HeaderState',96),('HeaderDrawing',96),
                           ('HeaderBestQuality',112),('HeaderOriginals',112),('HeaderSecured',160),
                           ('HeaderGuards',112),('HeaderRescues',112),('HeaderAlarms',112)]:
            if name in ws:
                fixed_bounds(bp,W(name),width,40);W(name).get_parent().slot.set_padding(margin(0))
                W(name).set_editor_property('min_desired_width',0)
                W(name).set_editor_property('justification',unreal.TextJustify.LEFT if name=='HeaderPlayer' else unreal.TextJustify.CENTER)
                W(name).slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
                W(name).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
                if name=='HeaderPlayer':W(name).slot.set_padding(margin(16,0,16,0))
        for n,w in ws.items():
            if isinstance(w,unreal.TextBlock) and n.startswith('Header'):label(w,20,True,MUTED)
    elif key=='WBP_ResultRewardDetail':
        box(W('SizeBox_0'),640,816);W('RewardDetailRoot').set_padding(margin(32));label(W('RewardDetailTitle'),32,True)

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
    for name,w in ws.items():
        if isinstance(w,unreal.Image):
            b=w.get_editor_property('brush')
            if isinstance(b.get_editor_property('resource_object'),unreal.Texture2D):
                b.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
                b.set_editor_property('margin',margin(0));w.set_brush(b)
            if name in ['PlaceholderIcon','ProfileImage','MapThumbnailImage','ReplicaImage','ReferenceImage']:fit_image(bp,w)
        if isinstance(w,unreal.TextBlock) and name in ['PlayerNameText','NameText']:
            w.set_editor_property('text_overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)
            w.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    if key=='WBP_HeistNameplate':
        for name in ['PlayerNameText','CrewStatusText']:
            ws[name].set_editor_property('justification',unreal.TextJustify.LEFT)
            ws[name].set_editor_property('auto_wrap_text',False)
            ws[name].set_editor_property('wrap_text_at',0)

paths=sorted(unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI',recursive=True,include_folder=False))
results=[]
descriptions={}
for path in paths:
    if not path.split('/')[-1].startswith('WBP_'):continue
    bp=unreal.load_asset(path)
    if not isinstance(bp,unreal.WidgetBlueprint):continue
    key=bp.get_name()
    if 'UI_ASSET_FILTER' in globals() and key not in UI_ASSET_FILTER:continue
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
        w.modify();quantize(w);w.set_render_scale(unreal.Vector2D(1,1))
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
    unreal.EditorAssetLibrary.set_metadata_tag(bp,'HeistUIDesign','ExhibitionLabelV2')
    owned_tree=TOOLS.call_method('GetWidgets',(bp,)).widgets[0].widget.get_outer()
    assert all(w.get_outer()==owned_tree for w in final_ws.values()),'Foreign widget tree in '+path
    ok=TOOLS.call_method('CompileWidgetBlueprint',(bp,))
    if not ok:raise RuntimeError('Compile failed '+path)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    descriptions[path]=TOOLS.call_method('GetWidgetDescription',(bp,None,-1)).description
    results.append({'asset':path,'widgets':len(final_ws),'compiled':bool(ok)})
    unreal.log_warning('CATALOGUE APPLIED '+key)
(OUT/'apply-assets.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
(OUT/'widget-descriptions.json').write_text(json.dumps(descriptions,ensure_ascii=False,indent=2),encoding='utf-8')

MAP_AFTER={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in MAP_PATHS}
ASSETS_AFTER=sorted(str(p.relative_to(ROOT)) for p in (ROOT/'Content').rglob('*.uasset'))
assert MAP_BEFORE==MAP_AFTER,'UI application changed a map'
assert ASSETS_BEFORE==ASSETS_AFTER,'UI application created an asset'
(OUT/'preservation.json').write_text(json.dumps({'maps_unchanged':True,'new_assets':0,'active_widgets':len(results)},indent=2),encoding='utf-8')
