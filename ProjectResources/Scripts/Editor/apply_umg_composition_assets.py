"""Wire the approved reusable art and apply layouts in the live Unreal Editor."""
import hashlib
import json
import runpy
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation' / globals().get('QA_FOLDER', 'UIComposition20261003')
OUT.mkdir(parents=True, exist_ok=True)
H = runpy.run_path(str(ROOT / 'ProjectResources/Scripts/Editor/apply_coop_ui.py'))
H.update(H['H'])
TOOLS = H['TOOLS']
TARGET = '/Game/Assets/UI/Heist'
COMMON = '/Game/Blueprints/UI/Common'
TRANSPARENT = unreal.LinearColor(0, 0, 0, 0)
WHITE = unreal.LinearColor(1, 1, 1, 1)

def material_brush(glyph, width=24, height=24):
    material = unreal.load_asset(TARGET + '/MI_HeistIcon_' + glyph)
    assert material, glyph
    brush = unreal.WidgetLibrary.make_brush_from_material(material, width, height)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('margin', H['margin'](0))
    brush.set_editor_property('tint_color', unreal.SlateColor(specified_color=WHITE))
    return brush

def icon(image, glyph, width=24, height=24):
    image.set_brush(material_brush(glyph, width, height))

def texture_brush(name, width=256, height=64):
    texture = unreal.load_asset(TARGET + '/T_Heist_' + name)
    assert texture
    brush = unreal.WidgetLibrary.make_brush_from_texture(texture, width, height)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('margin', H['margin'](0))
    return brush

def button(widget, primary=False, quiet=False):
    H['style_button'](widget, primary)
    style = widget.get_editor_property('widget_style')
    if not quiet:
        for state, name in [('normal', 'ButtonPrimary' if primary else 'Button'),
                            ('hovered', 'ButtonPrimary' if primary else 'ButtonHover'),
                            ('pressed', 'ButtonPrimary' if primary else 'ButtonPressed'), ('disabled', 'Button')]:
            style.set_editor_property(state, texture_brush(name))
    else:
        for state, alpha in [('normal',0),('disabled',0),('hovered',.08),('pressed',.14)]:
            style.set_editor_property(state,H['brush'](None,unreal.LinearColor(1,1,1,alpha),(4,4)))
    padding = H['margin'](0) if quiet else H['margin'](24, 12, 24, 12)
    style.set_editor_property('normal_padding', padding)
    style.set_editor_property('pressed_padding', padding)
    widget.set_style(style)

def slot_values(slot):
    if isinstance(slot, unreal.CanvasPanelSlot):
        keys = ['layout_data', 'auto_size', 'z_order']
    elif isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot)):
        keys = ['padding', 'size', 'horizontal_alignment', 'vertical_alignment']
    else:
        keys = ['padding', 'horizontal_alignment', 'vertical_alignment']
    return {key:slot.get_editor_property(key) for key in keys}

def named_wrapper(bp, child, name, class_name, padding=0, width=None, height=None):
    widgets = {str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
    if name in widgets:
        wrapper = widgets[name]
        inset = widgets[name+'Content']
    else:
        parent = child.get_parent()
        assert parent, (bp, child, name)
        index = parent.get_child_index(child)
        properties = slot_values(child.slot)
        # Editor structural operations can compile immediately. Keep the existing
        # bound subtree attached until the complete NamedSlot host is in the tree.
        single_content = isinstance(parent,(unreal.Border,unreal.SizeBox,unreal.Button,unreal.ScaleBox))
        if single_content:
            parent = TOOLS.call_method('WrapWidgets',(bp,[child],unreal.Overlay.static_class()))[0].widget
            index = 0
        klass = unreal.load_class(None, COMMON + '/' + class_name + '.' + class_name + '_C')
        wrapper = TOOLS.call_method('AddWidget',(bp,klass,name,parent,index)).widget
        assert wrapper
        if single_content:
            wrapper.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            wrapper.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        else:
            for key,value in properties.items():
                wrapper.slot.set_editor_property(key,value)
        inset = TOOLS.call_method('SetNamedSlotContent',(bp,wrapper,'ContentSlot',unreal.SizeBox.static_class(),name+'Content')).widget
        assert inset
    if child.get_parent() != inset:
        child.remove_from_parent()
        inset.add_child(child)
    child.slot.set_padding(H['margin'](padding))
    child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER if width else unreal.HorizontalAlignment.H_ALIGN_FILL)
    child.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER if height else unreal.VerticalAlignment.V_ALIGN_FILL)
    if width:
        inset.set_width_override(width)
    if height:
        inset.set_height_override(height)
    return wrapper

def keycap(bp, text_widget, name, wide=False):
    text_widget.set_text(str(text_widget.get_text()).strip('[]'))
    H['font'](text_widget, 12, H['BODY'])
    text_widget.set_editor_property('justification', unreal.TextJustify.CENTER)
    return named_wrapper(bp,text_widget,name,'WBP_HeistKeycap',0,64 if wide else 32,32)

def frame(bp, child, name, padding=16):
    if isinstance(child,unreal.Border):
        H['flat'](child,TRANSPARENT)
        child.set_padding(H['margin'](0))
    return named_wrapper(bp,child,name,'WBP_HeistPanelFrame',padding+8)

H.update(frame=frame,keycap=keycap,icon=icon,button=button)

def defaults(bp):
    cdo = unreal.get_default_object(bp.generated_class())
    name = bp.get_name()
    properties = {}
    if name in ['WBP_TeamCard','WBP_HeistNameplate','WBP_HeistHUD']:
        properties.update(stunned_status_icon='stunned',arrested_status_icon='detention',
                          carrying_original_status_icon='painting',heavy_status_icon='heavy')
    if name == 'WBP_TeamCard':
        properties.update(forging_status_icon='painting',escaped_status_icon='ready')
    if name in ['WBP_TeamCard','WBP_LobbyPlayerCard','WBP_ResultPlayerRow']:
        properties['default_profile_texture'] = 'profile'
    if name == 'WBP_HeistHUD':
        properties['coin_quick_slot_icon'] = 'coin'
        cdo.set_editor_property('flashlight_on_brush', material_brush('flashlight_on',56,56))
        cdo.set_editor_property('flashlight_off_brush', material_brush('flashlight_off',56,56))
    for property_name,glyph in properties.items():
        cdo.set_editor_property(property_name, unreal.load_asset(TARGET+'/MI_HeistIcon_'+glyph))

def menu_background(bp, ws):
    roots = {'WBP_Lobby':'CoopLobbyCanvas', 'WBP_Settings':'CoopSettingsCanvas',
             'WBP_Result':'CanvasPanel_38', 'WBP_TitleMenu':'TitleRoot'}
    if bp.get_name() not in roots:
        return
    canvas = ws[roots[bp.get_name()]]
    name = 'CompositionMenuBackdrop'
    image = ws.get(name)
    if image is None:
        image = TOOLS.call_method('AddWidget',(bp,unreal.Image.static_class(),name,canvas,0)).widget
        ws[name] = image
    material = unreal.load_asset(TARGET+'/M_HeistMenuBackdrop')
    assert material
    image.set_brush(unreal.WidgetLibrary.make_brush_from_material(material,1280,720))
    H['pos'](image,0,0,1280,720)
    image.slot.set_z_order(-100)
    # A visible full-screen image is the Settings modal's hit-test surface;
    # empty panel space must not expose the title buttons underneath it.
    image.set_visibility(unreal.SlateVisibility.VISIBLE if bp.get_name() == 'WBP_Settings'
                         else unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    for border_name in ['LobbyRootBorder','SettingsPanel','ResultBackdrop']:
        if border_name in ws:
            H['flat'](ws[border_name],TRANSPARENT)
    for obsolete in ['CoopLobbyShade','Image_113','HeistMenuShade']:
        if obsolete in ws:
            ws[obsolete].set_visibility(unreal.SlateVisibility.COLLAPSED)

def inventory_slot_frame(bp, ws):
    if bp.get_name() != 'WBP_InventorySlot':
        return
    brush = texture_brush('Panel',64,64)
    brush.set_editor_property('draw_as',unreal.SlateBrushDrawType.BOX)
    brush.set_editor_property('margin',H['margin'](.125))
    ws['SlotBackground'].set_brush(brush)

def main():
    maps = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/Maps').rglob('*.umap')}
    module = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/apply_umg_composition.py'))
    results = []
    child_names = {'WBP_InventorySlot': 0, 'WBP_InventoryFrame': 1,
                   'WBP_QuickSlot': 0, 'WBP_TeamCard': 0, 'WBP_LobbyMapCard': 0,
                   'WBP_LobbyPlayerCard': 0, 'WBP_ResultPlayerRow': 0,
                   'WBP_ResultReplicaCard': 0, 'WBP_InteractionPrompt': 0}
    paths = unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI',True,False)
    paths = sorted(paths, key=lambda path: (child_names.get(path.rsplit('/',1)[-1].split('.')[0],10), path))
    for path in paths:
        bp = unreal.load_asset(path)
        if not isinstance(bp,unreal.WidgetBlueprint) or '/Common/' in path:
            continue
        if 'ONLY_ASSETS' in globals() and bp.get_name() not in ONLY_ASSETS:
            continue
        ws = {str(i.widget_name):i.widget for i in TOOLS.call_method('GetWidgets',(bp,)).widgets if i.widget}
        original_names = set(ws)
        defaults(bp)
        applied = module['apply'](bp,bp.get_name(),ws,H)
        menu_background(bp, ws)
        inventory_slot_frame(bp, ws)
        if bp.get_name() in {'WBP_HeistHUD','WBP_Lobby','WBP_Result','WBP_Settings',
                             'WBP_HeistForgery','WBP_TitleMenu','WBP_Inventory'}:
            H['fit_authored_screen'](bp)
            if bp.get_name() == 'WBP_HeistHUD':
                # Project viewport DPI already scales from 1920x1080. The HUD
                # canvas must fill that allocation instead of fitting a second
                # fixed 1280x720 screen and introducing ultrawide side gutters.
                ws['HeistConceptDesignSize'].clear_width_override()
                ws['HeistConceptDesignSize'].clear_height_override()
                ws['HeistConceptViewportFit'].set_stretch(unreal.Stretch.FILL)
                ws['HeistConceptDesignSize'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
                ws['HeistConceptDesignSize'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        final = TOOLS.call_method('GetWidgets',(bp,)).widgets
        assert original_names <= {str(i.widget_name) for i in final}, (path, 'existing widget lost before compile', original_names - {str(i.widget_name) for i in final})
        for entry in final:
            if isinstance(entry.widget,unreal.TextBlock):
                value = str(entry.widget.get_text())
                for old,new in [('작전 대기실','대기실'),('CONTRACT RESULT','미션 결과'),('계약 성공','미션 성공'),('계약 실패','미션 실패'),
                                ('그림 위조','위조품 제작'),('복제 작업','위조품'),('관찰한 원본','원본'),('참가 플레이어','플레이어'),
                                ('HOST','방장'),('준비 대기','준비 안 됨'),('운반·확보','전리품 가치'),('최고 품질','최고 유사도'),
                                ('인벤토리','가방'),('코인','동전')]:
                    value = value.replace(old,new)
                if value == '초기화': value = '전체 지우기'
                entry.widget.set_text(value)
        assert TOOLS.call_method('CompileWidgetBlueprint',(bp,)), path
        compiled_names = {str(i.widget_name) for i in TOOLS.call_method('GetWidgets',(bp,)).widgets}
        assert original_names <= compiled_names, (path, 'existing widget lost during compile', original_names-compiled_names)
        if applied:
            metadata = json.loads((ROOT/'ProjectResources/SourceArt/UI/Heist/ApprovedComposition.json').read_text(encoding='utf-8'))
            unreal.EditorAssetLibrary.set_metadata_tag(bp,'HeistUIDesign','UMGComposition20261003')
            unreal.EditorAssetLibrary.set_metadata_tag(bp,'HeistUIApprovedSHA256',metadata['preview_sha256'])
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp), path
        results.append({'path':path,'widgets':len(final),'compiled':True,'saved':True})
    assert maps == {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/Maps').rglob('*.umap')}
    (OUT/'apply.json').write_text(json.dumps({'assets':results,'maps_unchanged':True},ensure_ascii=False,indent=2),encoding='utf-8')
    print('Composition applied and saved:',len(results),'WBP; maps unchanged.')

if __name__ == '__main__':
    main()
