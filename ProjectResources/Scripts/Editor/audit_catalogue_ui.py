"""Read-only authoring audit for the current UI; does not save packages."""
import hashlib,json,re
from pathlib import Path
import unreal
out=Path(unreal.Paths.project_saved_dir()).resolve()/'Automation'/globals().get('QA_FOLDER','UIUX20260930')/'CatalogueAudit.json'
out.parent.mkdir(parents=True,exist_ok=True)
root=Path(unreal.Paths.project_dir()).resolve()
files=sorted(set(list((root/'Content/Blueprints/UI').rglob('*.uasset'))+list((root/'Content/Assets/UI').rglob('*.uasset'))+list((root/'Content/Maps').rglob('*.umap'))))
before={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
ts=unreal.get_default_object(unreal.UMGToolSet)
report={'widgets':0,'values_checked':0,'brushes_checked':0,'native_brushes_checked':0,'fonts_checked':0,'render_scales_checked':0,'violations':[],'assets':[],'contracts':{},'font_display_dpi':72,'font_render_dpi':96,'read_only':True,'package_save_requested':False}
result_columns={}
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
        elif value.get_editor_property('draw_as')!=unreal.SlateBrushDrawType.NO_DRAW_TYPE:
            report['native_brushes_checked']+=1
            if value.get_editor_property('draw_as')==unreal.SlateBrushDrawType.ROUNDED_BOX:
                outline=value.get_editor_property('outline_settings')
                check(asset,name,prop+'.outline.width',outline.get_editor_property('width'))
                radii=outline.get_editor_property('corner_radii')
                for axis in ['x','y','z','w']:check(asset,name,prop+'.corner_radii.'+axis,getattr(radii,axis))
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
        transform=w.get_editor_property('render_transform')
        report['render_scales_checked']+=1
        if abs(transform.scale.x-1)>0.0001 or abs(transform.scale.y-1)>0.0001:
            report['violations'].append({'asset':path,'widget':name,'property':'render_transform.scale','value':[transform.scale.x,transform.scale.y]})
        dimensions(path,name,'render_transform.translation',transform.translation)
        for p in ['padding','content_padding','width_override','height_override','min_desired_width','min_desired_height','max_desired_width','max_desired_height','wrap_text_at','minimum_desired_width','max_list_height','min_desired_slot_width','min_desired_slot_height','shadow_offset','scrollbar_thickness','size']:
            try:dimensions(path,name,p,w.get_editor_property(p))
            except Exception:pass
        if isinstance(w,unreal.TextBlock) or isinstance(w,unreal.ComboBoxString):
            f=w.get_editor_property('font');style(path,name,'font',f)
            if f.get_editor_property('letter_spacing')!=0:
                report['violations'].append({'asset':path,'widget':name,'property':'font.letter_spacing','value':f.get_editor_property('letter_spacing')})
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
        report['contracts']['flashlight_icon_bound']=isinstance(ws.get('FlashlightIcon'),unreal.Image) and ws['FlashlightIcon'].get_editor_property('brush').get_editor_property('resource_object').get_name()=='T_Catalogue_Flashlight'
        report['contracts']['flashlight_key_separate']=str(ws['FlashlightKeyText'].get_text())=='[F]'
        report['contracts']['team_cards']=sum('TeamCard'+str(i) in ws for i in range(1,5))==4
        report['contracts']['alert_stars']=sum('AlertStar%02d'%i in ws for i in range(1,11))==10
        report['contracts']['hud_panels_native']=all(ws[n].get_editor_property('background').get_editor_property('resource_object') is None for n in ['MissionPanel','AlertMeterPanel','TutorialCardContainer','Border_0'])
    if bp.get_name()=='WBP_HeistForgery':
        report['contracts']['preview_completion_bar']=isinstance(ws.get('PreviewQualityBar'),unreal.ProgressBar)
        report['contracts']['preview_fixed_similarity_label']=str(ws['PreviewScoreText'].get_text())=='작품 유사도'
        report['contracts']['no_preview_numeric_clutter']='70+' not in str(ws['PreviewScoreText'].get_text()) and '/100' not in str(ws['PreviewScoreText'].get_text())
        w=ws['DrawingSurfaceSizeBox'];report['contracts']['drawing_800_square']=w.get_editor_property('width_override')==800 and w.get_editor_property('height_override')==800
    if bp.get_name()=='WBP_Settings':
        report['contracts']['settings_label_value_vertical_center']=all(ws[n].slot.get_editor_property('vertical_alignment')==unreal.VerticalAlignment.V_ALIGN_CENTER for n in ['FOVRowLabel','SensitivityRowLabel','VolumeRowLabel','ResolutionRowLabel','WindowModeRowLabel','FOVValueText','MouseSensitivityValueText','MasterVolumeValueText'])
        report['contracts']['settings_common_row_width']=all(ws[p+'RowLabelSize'].get_editor_property('width_override')==240 and ws[p+'RowSliderSize'].get_editor_property('width_override')==688 and ws[p+'RowValueSize'].get_editor_property('width_override')==112 for p in ['FOV','Sensitivity','Volume']) and all(ws[p+'RowComboSize'].get_editor_property('width_override')==824 for p in ['Resolution','WindowMode'])
    if bp.get_name()=='WBP_SessionJoin':
        report['contracts']['join_submit_symmetric_padding']=ws['SubmitJoinSessionSize'].slot.get_editor_property('padding').left==ws['SubmitJoinSessionSize'].slot.get_editor_property('padding').right==0
    if bp.get_name()=='WBP_Lobby':
        report['contracts']['lobby_common_width']=ws['LobbyContent'].get_parent().get_editor_property('width_override')==1352 and all(ws[n].get_editor_property('size').x==24 for n in ['Spacer_2','Spacer_4','Spacer_5'])
    if bp.get_name()=='WBP_LobbyPlayerCard':
        report['contracts']['lobby_square_card']=ws['PlayerCardSizeBox'].get_editor_property('width_override')==ws['PlayerCardSizeBox'].get_editor_property('height_override')==320
        report['contracts']['lobby_square_profile']=ws['ProfileImageSizeBox'].get_editor_property('width_override')==ws['ProfileImageSizeBox'].get_editor_property('height_override')==112 and ws['ProfileImage_AspectFit'].get_editor_property('stretch')==unreal.Stretch.SCALE_TO_FIT
        report['contracts']['lobby_symmetric_content_padding']=all(getattr(ws['PlayerCardContent'].slot.get_editor_property('padding'),p)==24 for p in ['left','top','right','bottom'])
        report['contracts']['lobby_ready_check_24']=ws['ReadyCheckImage'].get_parent().get_editor_property('width_override')==ws['ReadyCheckImage'].get_parent().get_editor_property('height_override')==24
        report['contracts']['lobby_name_ellipsis']=ws['PlayerNameText'].get_editor_property('text_overflow_policy')==unreal.TextOverflowPolicy.ELLIPSIS and ws['PlayerNameText'].get_editor_property('justification')==unreal.TextJustify.LEFT and ws['PlayerNameText'].slot.get_editor_property('horizontal_alignment')==unreal.HorizontalAlignment.H_ALIGN_CENTER
    if bp.get_name()=='WBP_LobbyMapCard':
        report['contracts']['lobby_map_aspect_16_9']=ws['MapThumbnailSize'].get_editor_property('width_override')==320 and ws['MapThumbnailSize'].get_editor_property('height_override')==180
    if bp.get_name() in ['WBP_ResultPlayerRow','WBP_Result']:
        names=['PlayerNameText','PlayerStateText','SurfaceForgeryCountText','BestSurfaceQualityText','ArtifactsRecoveredText','SecuredLootValueText','GuardsDistractedText','TeammatesRescuedText','AlarmsTriggeredText'] if bp.get_name()=='WBP_ResultPlayerRow' else ['HeaderPlayer','HeaderState','HeaderDrawing','HeaderBestQuality','HeaderOriginals','HeaderSecured','HeaderGuards','HeaderRescues','HeaderAlarms']
        result_columns[bp.get_name()]=[ws[n].get_parent().get_editor_property('width_override') for n in names]
        report['contracts'][bp.get_name()+'_name_left']=ws[names[0]].get_editor_property('justification')==unreal.TextJustify.LEFT
    if bp.get_name()=='WBP_HeistFloorPlanMap':
        report['contracts']['map_backdrop_image']=isinstance(ws.get('MapBackdropImage'),unreal.Image)
        report['contracts']['map_aspect_fit']=ws['MapAspectFit'].get_editor_property('stretch')==unreal.Stretch.SCALE_TO_FIT
    for asset,container in [('WBP_ActionProgress','ActionProgressContainer'),('WBP_HeistPopupFeedback','PopupContainer'),('WBP_InteractionPrompt','InteractionPromptContainer')]:
        if bp.get_name()==asset:
            report['contracts'][container+'_native']=ws[container].get_editor_property('background').get_editor_property('resource_object') is None
    if bp.get_name()=='WBP_TeamCard':
        report['contracts']['team_panel_native']=ws['TeamCardBorder'].get_editor_property('background').get_editor_property('resource_object') is None
    if bp.get_name()=='WBP_Inventory':
        report['contracts']['inventory_summary_absent']='InventorySummaryText' not in ws
    if bp.get_name()=='WBP_HeistNameplate':
        report['contracts']['nameplate_background_absent']='NameplateBorder' not in ws and not ws['NameplateContentRow'].get_parent()
        report['contracts']['nameplate_status_preserved']=all(n in ws for n in ['PlayerNameText','CrewStatusText','CrewStatusBadge','CrewStatusIconText'])
    if bp.get_name()=='WBP_InventoryFrame':
        w=ws['SizeBox_0'];report['contracts']['inventory_frame_640_square']=w.get_editor_property('width_override')==640 and w.get_editor_property('height_override')==640
font_dir=Path(unreal.Paths.project_content_dir()).resolve()/'Assets/UI/Fonts/Catalogue'
report['contracts']['font_packages_saved']=all((font_dir/(n+s+'.uasset')).is_file() for n in ['F_NanumGothic_Regular','F_NanumGothic_Bold','F_NanumMyeongjo_Bold'] for s in ['', '_Font'])
report['contracts']['textured_brushes_audited']=report['brushes_checked']>0
report['contracts']['native_brushes_audited']=report['native_brushes_checked']>0
report['contracts']['result_columns_match']=result_columns.get('WBP_ResultPlayerRow')==result_columns.get('WBP_Result')==[448,96,96,112,112,160,112,112,112]
report['layout_metrics']={'result_column_widths':result_columns,'lobby_group_width':1352,'lobby_card_width':320,'lobby_profile_size':112,'lobby_gap':24,'settings_row_width':1088,'geometry_source':'saved authored slots; raster alignment reviewed separately'}
after={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
report['contracts']['assets_unchanged']=before==after
report['asset_hashes']={'before':before,'after':after}
report['pass']=len(report['assets'])==23 and report['widgets']>=400 and not report['violations'] and all(report['contracts'].values())
out.write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log_warning('CATALOGUE AUDIT '+str(report['pass'])+' values='+str(report['values_checked'])+' violations='+str(len(report['violations'])))
