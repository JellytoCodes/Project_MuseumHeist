"""Apply the approved M03 night balance once, then reload and verify the saved map."""
import unreal,json,time,hashlib,traceback
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/M03NightReadability';P=json.loads((R/'ProjectResources/SourceArt/Gallery/M03/M03NightLighting.json').read_text());B=json.loads((O/'before.json').read_text());MAP='/Game/Maps/M03_GlasshousePrototype';A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);S={'phase':0,'stamp':0,'busy':False};OUT={'errors':[],'saved':False}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def vec(v):return [v.x,v.y,v.z]
def snapshot():
 return {a.get_name():{'location':vec(a.get_actor_location()),'scale':vec(a.get_actor_scale3d()),'rotation':[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll]} for a in A.get_all_level_actors()}
def lights():
 return {(a.get_name(),c.get_name()):(a,c) for a in A.get_all_level_actors() for c in a.get_components_by_class(unreal.LightComponentBase)}
def finish(error=None):
 if error:OUT['errors'].append(error)
 (O/'applied.json').write_text(json.dumps(OUT,indent=2));unreal.unregister_slate_post_tick_callback(S['cb']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(dt):
 if S['busy']:return
 S['busy']=True
 try:
  if S['phase']==0:
   assert sha(R/'Content/Maps/M03_GlasshousePrototype.umap')==P['source_map_sha256']==B['map_sha256'],'Map changed: inspect before applying';S['world']=unreal.EditorLoadingAndSavingUtils.load_map(MAP);S.update(phase=1,stamp=time.monotonic());return
  if time.monotonic()-S['stamp']<6:return
  if S['phase']==1:
   S['poses']=snapshot();ls=lights();assert len(ls)==P['expected_lights']
   for row in B['lights']:
    a,c=ls[row['actor'],row['component']];assert abs(c.intensity-row['intensity'])<.001;a.modify();c.modify();c.set_editor_property('intensity',row['intensity']*P['intensity_multiplier'])
   pp=next(a for a in A.get_all_level_actors() if a.get_name()==P['postprocess_actor']);pp.modify();st=pp.settings
   for k,v in P['postprocess'].items():st.set_editor_property(k,unreal.Vector4(*v) if isinstance(v,list) else v);st.set_editor_property('override_'+k,True)
   pp.set_editor_property('settings',st);S.update(phase=2,stamp=time.monotonic());return
  if S['phase']==2:
   for row in B['lights']:
    a,c=lights()[row['actor'],row['component']];assert abs(c.intensity-row['intensity']*.5)<.001,(row['label'],c.intensity)
   OUT['saved']=unreal.EditorLoadingAndSavingUtils.save_map(S['world'],MAP);assert OUT['saved'];S['world']=unreal.EditorLoadingAndSavingUtils.load_map(MAP);S.update(phase=3,stamp=time.monotonic());return
  if S['phase']==3:
   OUT['poses_unchanged']=snapshot()==S['poses'];assert OUT['poses_unchanged'];ls=lights();OUT['lights']=[]
   for row in B['lights']:
    a,c=ls[row['actor'],row['component']];assert abs(c.intensity-row['intensity']*.5)<.001;OUT['lights'].append({'actor':row['actor'],'component':row['component'],'before':row['intensity'],'after':c.intensity})
   pp=next(a for a in A.get_all_level_actors() if a.get_name()==P['postprocess_actor']);st=pp.settings;OUT['postprocess']={}
   for k,v in P['postprocess'].items():
    actual=st.get_editor_property(k);actual=[getattr(actual,n) for n in 'xyzw'] if isinstance(v,list) else actual;assert st.get_editor_property('override_'+k);assert all(abs(x-y)<.00001 for x,y in zip(actual,v)) if isinstance(v,list) else abs(actual-v)<.00001;OUT['postprocess'][k]=actual
   OUT['exposure']={k:st.get_editor_property(k) for k in ['auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias']};assert OUT['exposure']=={k:B['postprocess'][0][k] for k in OUT['exposure']};OUT['map_sha256']=sha(R/'Content/Maps/M03_GlasshousePrototype.umap');OUT['protected_unchanged']=all(sha(Path(p))==h for p,h in B['protected'].items());assert OUT['protected_unchanged'];expected={Path(p).resolve() for p in B['protected']}|{(R/'Content/Maps/M03_GlasshousePrototype.umap').resolve()};current={p.resolve() for p in (R/'Content').rglob('*') if p.suffix in ['.umap','.uasset']};assert expected==current;OUT['asset_set_unchanged']=True;finish()
 except:finish(traceback.format_exc())
 finally:S['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['cb']=unreal.register_slate_post_tick_callback(tick)
