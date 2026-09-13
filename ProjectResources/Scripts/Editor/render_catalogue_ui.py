import unreal,json,time,traceback,hashlib
from pathlib import Path
out=Path('D:/Dev/UE5.8/Project_MuseumHeist/Saved/Screenshots/CatalogueUI');out.mkdir(exist_ok=True)
root=Path('D:/Dev/UE5.8/Project_MuseumHeist')
paths=sorted((root/'Content/Blueprints/UI').rglob('WBP_*.uasset'))
before={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets=['Title/WBP_TitleMenu','Lobby/WBP_Lobby','HUD/WBP_HeistHUD','Inventory/WBP_Inventory','HUD/WBP_HeistFloorPlanMap','Forgery/WBP_HeistForgery','Result/WBP_Result','Title/WBP_Settings','Title/WBP_SessionJoin']
items=[]
state={'t':time.time(),'cb':None,'phase':'start','errors':[]}
widget_cache={}
populated=set()

def make_widget(asset):
    path='/Game/Blueprints/UI/'+asset
    klass=unreal.load_class(None,path+'.'+asset.split('/')[-1]+'_C')
    return unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(unreal.EditorLevelLibrary.get_game_world(),klass,None))

def widgets(widget):
    path=widget.get_path_name()
    if path not in widget_cache:
        widget_cache[path]={w.get_name():w for w in unreal.ObjectIterator(unreal.Widget) if w.get_path_name().startswith((path+'.',path+':'))}
    return widget_cache[path]

def fixture(key,widget):
    # Transient visual fixtures only. No gameplay/replication PASS is inferred.
    ws=widgets(widget)
    def txt(name,text):
        if name in ws:ws[name].set_text(text)
    if key.endswith('WBP_Settings'):
        for n,t in [('FOVValueText','90'),('MouseSensitivityValueText','1.0'),('MasterVolumeValueText','80%')]:txt(n,t)
        for n,v in [('FOVSlider',.5),('MouseSensitivitySlider',.4),('MasterVolumeSlider',.8)]:
            if n in ws:ws[n].set_value(v)
        for n,text in [('ResolutionComboBox','1920 × 1080'),('WindowModeComboBox','전체 화면')]:
            if n in ws:ws[n].add_option(text);ws[n].set_selected_option(text)
    elif key.endswith('WBP_SessionJoin'):
        ws['JoinCodeInput'].set_text('MUSE24')
        for n in ['RetrySessionSize','CancelSessionSize']:ws[n].set_visibility(unreal.SlateVisibility.COLLAPSED)
    elif key.endswith('WBP_Lobby'):
        txt('JoinCodeText','MUSE24');txt('PlayerCountText','2 / 4')
        for i in range(1,5):
            n='PlayerCard'+str(i)
            if n in ws:
                for name,w in widgets(ws[n]).items():
                    if name=='PlayerNameText':w.set_text(['PLAYER 1','PLAYER 2','참가 대기','참가 대기'][i-1])
    elif key.endswith('WBP_HeistHUD'):
        txt('MissionTimeText','14:32');txt('RequiredTargetNameText','황금빛 초상');txt('InteractionPromptText','[E] 작품 관찰')
    elif key.endswith('WBP_Inventory'):
        txt('InventorySummaryText','배낭 상태: 가벼움\n확보 가치 $8,400  /  계약 할당량 $24,000')
        if key not in populated:
            grid=ws['InventoryGrid'];grid.clear_children()
            grid.set_editor_property('min_desired_slot_width',128);grid.set_editor_property('min_desired_slot_height',128)
            for i in range(25):
                slot=grid.add_child_to_uniform_grid(make_widget('Inventory/WBP_InventorySlot'),i//5,i%5)
                slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            populated.add(key)
    elif key.endswith('WBP_HeistForgery'):
        for n in ['DrawingContainer','DrawingTimeRemainingText','SubmitButton','CancelButton']:
            if n in ws:ws[n].set_visibility(unreal.SlateVisibility.VISIBLE)
        txt('DrawingTimeRemainingText','남은 시간  00:32')
        for i in range(1,10):
            n='PaletteButton'+str(i)
            if n in ws:ws[n].set_visibility(unreal.SlateVisibility.VISIBLE if i<=5 else unreal.SlateVisibility.COLLAPSED)
        for i,c in enumerate([(0.82,0.74,0.57),(0.74,0.45,0.11),(0.10,0.24,0.28),(0.025,0.06,0.06),(0.42,0.10,0.065)],1):
            ws['PaletteButton'+str(i)].set_background_color(unreal.LinearColor(*c,1))
            ws['PaletteButtonText'+str(i)].set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(0,0,0,1) if i<=2 else unreal.LinearColor(1,1,1,1)))
        ws['DrawingSurface'].set_brush_color(unreal.LinearColor(.82,.74,.57,1))
        texture=unreal.load_asset('/Game/Data/Forgery/Textures/T_Forgery_SunArchWave')
        ws['ReferenceImage'].set_brush_from_texture(texture)
    elif key.endswith('WBP_Result'):
        txt('OutcomeTextBlock','계약 성공');txt('OutcomeReasonTextBlock','필수 목표를 반출하고 계약 할당량을 달성했습니다.');txt('TeamRewardTextBlock','팀 확보 가치  $24,800')
        ws['ReturnToLobbyButton'].set_visibility(unreal.SlateVisibility.VISIBLE)
        if key not in populated:
            for i in range(4):
                row=make_widget('Result/WBP_ResultPlayerRow');slot=ws['ContributionTableContainer'].add_child_to_vertical_box(row);slot.set_padding(unreal.Margin(0,0,0,4))
                for n,w in widgets(row).items():
                    if isinstance(w,unreal.TextBlock):w.set_text('PLAYER '+str(i+1) if n=='PlayerNameText' else '탈출' if n=='PlayerStateText' else '6,200' if n=='SecuredLootValueText' else '84' if n=='BestSurfaceQualityText' else '2')
                card=make_widget('Result/WBP_ResultReplicaCard');slot=ws['ReplicaRecapVisualContainer'].add_child_to_horizontal_box(card);slot.set_padding(unreal.Margin(0,0,12,0));slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
                cw=widgets(card)
                cw['ArtifactNameText'].set_text('황금빛 초상' if i==0 else '전시 작품 '+str(i+1));cw['QualityText'].set_text('품질 '+str(84+i*3))
                cw['RequiredTargetBadge'].set_visibility(unreal.SlateVisibility.VISIBLE if i==0 else unreal.SlateVisibility.HIDDEN)
                cw['ReplicaImage'].set_brush_from_texture(unreal.load_asset('/Game/Data/Forgery/Textures/T_Forgery_SunArchWave'));cw['ReplicaImage'].set_visibility(unreal.SlateVisibility.VISIBLE)
            populated.add(key)
def finish():
    after={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    (out/'asset-hashes.json').write_text(json.dumps({'before':before,'after':after,'unchanged':before==after},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(state['cb'])
    (out/'render-complete.json').write_text(json.dumps({'screens':len(items),'visual_fixture':True,'errors':state['errors']}),encoding='utf-8')
def tick(dt):
    if state['phase']=='export':
        for key,a,wc,widget in items:
            try:fixture(key,widget)
            except Exception as e:
                if str(e) not in state['errors']:state['errors'].append(str(e))
    if time.time()-state['t']<4:return
    try:
        if state['phase']=='start':
            level.editor_play_simulate();state.update(phase='create',t=time.time());return
        if state['phase']=='create':
            world=unreal.EditorLevelLibrary.get_game_world()
            if not world:raise Exception('No simulation world')
            for key in assets:
                path='/Game/Blueprints/UI/'+key
                a=unreal.get_default_object(unreal.GameplayStatics).call_method('BeginDeferredActorSpawnFromClass',(world,unreal.Actor.static_class(),unreal.Transform(),unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                unreal.get_default_object(unreal.GameplayStatics).call_method('FinishSpawningActor',(a,unreal.Transform(),unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                wc=a.call_method('AddComponentByClass',(unreal.WidgetComponent.static_class(),False,unreal.Transform(),False))
                wc.set_tick_when_offscreen(True)
                wc.set_draw_size(unreal.Vector2D(1920,1080))
                wc.set_background_color(unreal.LinearColor(0.08,0.08,0.08,1))
                klass=unreal.load_class(None,path+'.'+key.split('/')[-1]+'_C')
                cdo=unreal.get_default_object(klass);original_tick=cdo.get_editor_property('tick_frequency')
                try:
                    if key.endswith('WBP_HeistForgery'):cdo.set_editor_property('tick_frequency',unreal.WidgetTickFrequency.NEVER)
                    widget=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,klass,None))
                finally:cdo.set_editor_property('tick_frequency',original_tick)
                wc.set_widget(widget);widget.set_visibility(unreal.SlateVisibility.VISIBLE);wc.request_render_update()
                items.append((key,a,wc,widget))
            state.update(phase='export',t=time.time());return
        if state['phase']=='export':
            world=unreal.EditorLevelLibrary.get_game_world()
            for key,a,wc,widget in items:
                if key.endswith('WBP_HeistForgery'):
                    w=widgets(widget)['DrawingTimeRemainingText']
                    (out/'forgery-timer-fixture.json').write_text(json.dumps({'visibility':str(w.get_visibility()),'opacity':w.get_render_opacity(),'text':str(w.get_text()),'geometry':str(w.get_cached_geometry()),'tick':str(widget.get_editor_property('tick_frequency'))}),encoding='utf-8')
                rt=wc.get_render_target();unreal.log_warning('UIREVIEW '+key+' RT '+str(rt))
                if rt:unreal.RenderingLibrary.export_render_target(world,rt,str(out),key.split('/')[-1]+'.png')
            state.update(phase='finish',t=time.time());unreal.EditorLevelLibrary.editor_end_play();return
        finish()
    except:
        state['errors'].append(traceback.format_exc());unreal.log_error(traceback.format_exc());unreal.EditorLevelLibrary.editor_end_play();finish()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['cb']=unreal.register_slate_post_tick_callback(tick)
