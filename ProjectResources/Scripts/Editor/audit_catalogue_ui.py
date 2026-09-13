"""Read back explicit UMG authoring dimensions and required layout contracts."""
import json,re
from pathlib import Path
import unreal
out=Path(unreal.Paths.project_saved_dir()).resolve()/'Logs/CatalogueAudit.json'
ts=unreal.get_default_object(unreal.UMGToolSet)
report={'widgets':0,'values_checked':0,'brushes_checked':0,'fonts_checked':0,'violations':[],'assets':[],'contracts':{},'font_display_dpi':72,'font_render_dpi':96}
def check(asset,widget,prop,number):
    report['values_checked']+=1
    if abs(float(number)/4-round(float(number)/4))>0.0001:
        report['violations'].append({'asset':asset,'widget':widget,'property':prop,'value':float(number)})
def dimensions(asset,name,p,value):
    if isinstance(value,unreal.Margin):
        for k in ['left','top','right','bottom']:check(asset,name,p+'.'+k,getattr(value,k))
    elif isinstance(value,unreal.Vector2D):
        check(asset,name,p+'.x',value.x);check(asset,name,p+'.y',value.y)
    elif isinstance(value,(float,int)):check(asset,name,p,value)

def style(asset,name,prop,value):
    if isinstance(value,unreal.SlateBrush):
        if isinstance(value.get_editor_property('resource_object'),unreal.Texture2D):
            report['brushes_checked']+=1
            m=value.get_editor_property('margin')
            if value.get_editor_property('draw_as')!=unreal.SlateBrushDrawType.IMAGE or any(abs(v)>0.00001 for v in [m.left,m.top,m.right,m.bottom]):
                report['violations'].append({'asset':asset,'widget':name,'property':prop,'value':'Textured brush must use Image and zero margin'})
        return
    if isinstance(value,unreal.SlateFontInfo):
        report['fonts_checked']+=1
        check(asset,name,prop+'.display_size',value.size*96/72)
        return
    if isinstance(value,unreal.StructBase):
        for p in dir(value):
            if p.startswith('_'):continue
            try:child=value.get_editor_property(p)
            except Exception:continue
            if isinstance(child,unreal.StructBase):style(asset,name,prop+'.'+p,child)
for path in unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/UI',recursive=True,include_folder=False):
    if 'WBP_' not in path or 'ObjectAssembly' in path:continue
    bp=unreal.load_asset(path)
    if not isinstance(bp,unreal.WidgetBlueprint):continue
    ws={str(i.widget_name):i.widget for i in ts.call_method('GetWidgets',(bp,)).widgets if i.widget}
    report['assets'].append(path)
    for name,w in ws.items():
        report['widgets']+=1
        for p in ['padding','content_padding','width_override','height_override','min_desired_width','min_desired_height','max_desired_width','max_desired_height','wrap_text_at','minimum_desired_width','max_list_height','min_desired_slot_width','min_desired_slot_height','shadow_offset','scrollbar_thickness','size']:
            try:dimensions(path,name,p,w.get_editor_property(p))
            except Exception:pass
        if isinstance(w,unreal.TextBlock) or isinstance(w,unreal.ComboBoxString):
            f=w.get_editor_property('font');style(path,name,'font',f)
            if not f.font_object or '/Catalogue/' not in f.font_object.get_path_name():
                report['violations'].append({'asset':path,'widget':name,'property':'font_object','value':str(f.font_object)})
        for p in ['brush','background','widget_style','item_style']:
            try:value=w.get_editor_property(p)
            except Exception:continue
            style(path,name,p,value)
        if w.slot:
            try:dimensions(path,name,'slot.padding',w.slot.get_editor_property('padding'))
            except Exception:pass
            if isinstance(w.slot,unreal.CanvasPanelSlot):dimensions(path,name,'slot.offsets',w.slot.get_offsets())
    if bp.get_name()=='WBP_HeistHUD':
        report['contracts']['team_cards']=sum('TeamCard'+str(i) in ws for i in range(1,5))==4
        report['contracts']['alert_stars']=sum('AlertStar%02d'%i in ws for i in range(1,11))==10
        report['contracts']['hud_panels_textured']=all(isinstance(ws[n].get_editor_property('background').get_editor_property('resource_object'),unreal.Texture2D) for n in ['MissionPanel','AlertMeterPanel','TutorialCardContainer','Border_0'])
    if bp.get_name()=='WBP_HeistForgery':
        w=ws['DrawingSurfaceSizeBox'];report['contracts']['drawing_800_square']=w.get_editor_property('width_override')==800 and w.get_editor_property('height_override')==800
    if bp.get_name()=='WBP_HeistFloorPlanMap':
        report['contracts']['map_backdrop_image']=isinstance(ws.get('MapBackdropImage'),unreal.Image)
        report['contracts']['map_aspect_fit']=ws['MapAspectFit'].get_editor_property('stretch')==unreal.Stretch.SCALE_TO_FIT
    for asset,container in [('WBP_ActionProgress','ActionProgressContainer'),('WBP_HeistPopupFeedback','PopupContainer'),('WBP_InteractionPrompt','InteractionPromptContainer')]:
        if bp.get_name()==asset:
            report['contracts'][container+'_textured']=isinstance(ws[container].get_editor_property('background').get_editor_property('resource_object'),unreal.Texture2D)
    if bp.get_name()=='WBP_TeamCard':
        report['contracts']['team_panel_textured']=isinstance(ws['TeamCardBorder'].get_editor_property('background').get_editor_property('resource_object'),unreal.Texture2D)
    if bp.get_name()=='WBP_Inventory':
        report['contracts']['inventory_summary_absent']='InventorySummaryText' not in ws
    if bp.get_name()=='WBP_HeistNameplate':
        report['contracts']['nameplate_background_absent']='NameplateBorder' not in ws and not ws['NameplateContentRow'].get_parent()
        report['contracts']['nameplate_status_preserved']=all(n in ws for n in ['PlayerNameText','CrewStatusText','CrewStatusBadge','CrewStatusIconText'])
    if bp.get_name()=='WBP_InventoryFrame':
        w=ws['SizeBox_0'];report['contracts']['inventory_frame_640_square']=w.get_editor_property('width_override')==640 and w.get_editor_property('height_override')==640
font_dir=Path(unreal.Paths.project_content_dir()).resolve()/'Assets/UI/Fonts/Catalogue'
report['contracts']['font_packages_saved']=all((font_dir/(n+s+'.uasset')).is_file() for n in ['F_NanumGothic_Regular','F_NanumGothic_Bold','F_NanumMyeongjo_Bold'] for s in ['', '_Font'])
report['contracts']['textured_brushes_audited']=report['brushes_checked']>=100
report['pass']=len(report['assets'])==23 and report['widgets']>=400 and not report['violations'] and all(report['contracts'].values())
out.write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log_warning('CATALOGUE AUDIT '+str(report['pass'])+' values='+str(report['values_checked'])+' violations='+str(len(report['violations'])))
