"""M03-only lighting correction. Reuse existing Blueprint assets; no asset writes."""
import unreal,json,time,hashlib,shutil,traceback
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/M03LightingFix';P=json.loads((R/'ProjectResources/SourceArt/Gallery/M03/M03LightingLayout.json').read_text());MAP='/Game/Maps/M03_GlasshousePrototype';A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
S={'phase':0,'stamp':0,'busy':False};OUT={'saved':False,'errors':[],'fixtures':[]}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def apply():
 file=R/'Content/Maps/M03_GlasshousePrototype.umap';assert sha(file)==P['source_map_sha256'],'Source changed; inspect before applying'
 O.mkdir(parents=True,exist_ok=True);shutil.copy2(file,O/'M03_before.umap');OUT['protected']={str(p):sha(p) for p in (R/'Content').rglob('*') if p.is_file() and p.suffix in ['.uasset','.umap'] and p!=file}
 S['world']=unreal.EditorLoadingAndSavingUtils.load_map(MAP);actors={a.get_name():a for a in A.get_all_level_actors()};S['art']=[]
 for i,r in enumerate(P['fixtures']):
  a=actors[r['fixture_actor']] if r['fixture_actor'] else A.spawn_actor_from_class(unreal.load_class(None,r['blueprint']+'.'+r['blueprint'].split('/')[-1]+'_C'),unreal.Vector(*r['actor_location']))
  if not r['fixture_actor']:a.set_actor_label('M03L_Light_Art_%02d'%i);a.set_folder_path('M03Gallery/Lights/Art')
  a.modify();a.set_actor_location(unreal.Vector(*r['actor_location']),False,True);a.set_actor_rotation(unreal.Rotator(yaw=r['yaw']),False);a.tags=list(a.tags)+[unreal.Name('M03ArtLight_'+r['painting'])]
  S['art'].append((a,r));OUT['fixtures'].append(dict(r,actor=a.get_name()))
 for a in A.get_all_level_actors():
  if a.get_class().get_name()!='BP_HeistLight_DropCeiling_01a_C':continue
  c=a.get_component_by_class(unreal.SpotLightComponent);a.modify();c.modify();c.set_editor_properties({'temperature':P['temperature'],'use_temperature':True,'light_color':unreal.Color(255,255,255,255),'use_inverse_squared_falloff':True,'intensity_units':unreal.LightUnits.CANDELAS,'intensity':P['passage_candela'],'attenuation_radius':1000.,'inner_cone_angle':25.,'outer_cone_angle':44.})

def configure():
 # Match the approved M03 fixture geometry before configuring the regenerated light component.
 # Editor property changes may reconstruct SCS components; apply light properties as one batch on a later tick.
 for a,r in S['art']:
  cm=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh);mr=r['mesh_relative'];cm.modify();cm.set_relative_scale3d(unreal.Vector(*mr['scale']));cm.set_relative_location(unreal.Vector(*mr['location']),False,False)
def finish(error=None):
 if error:OUT['errors'].append(error)
 (O/'applied.json').write_text(json.dumps(OUT,indent=2));unreal.unregister_slate_post_tick_callback(S['cb']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(dt):
 if S['busy']:return
 S['busy']=True
 try:
  if S['phase']==0:apply();S.update(phase=1,stamp=time.monotonic());return
  if S['phase']==1 and time.monotonic()-S['stamp']>4:configure();S.update(phase=2,stamp=time.monotonic());return
  if S['phase']==2 and time.monotonic()-S['stamp']>5:
   for a,r in S['art']:
    c=a.get_component_by_class(unreal.SpotLightComponent);parent=c.get_attach_parent();assert parent;c.modify();pt=parent.get_world_transform();source=unreal.Vector(*r['source']);rot=unreal.MathLibrary.find_look_at_rotation(source,unreal.Vector(*r['aim']));c.set_editor_properties({'relative_location':unreal.MathLibrary.inverse_transform_location(pt,source),'relative_rotation':unreal.MathLibrary.inverse_transform_rotation(pt,rot),'temperature':P['temperature'],'use_temperature':True,'light_color':unreal.Color(255,255,255,255),'use_inverse_squared_falloff':True,'intensity_units':unreal.LightUnits.CANDELAS,'intensity':r['candela'],'attenuation_radius':r['radius'],'inner_cone_angle':r['inner'],'outer_cone_angle':r['outer']})
   for name,pos in P.get('passage_moves',{}).items():
    a=next(a for a in A.get_all_level_actors() if a.get_name()==name);a.modify();a.set_actor_location(unreal.Vector(*pos),False,True)
   S.update(phase=3,stamp=time.monotonic());return
  if S['phase']==3 and time.monotonic()-S['stamp']>5:
   for a,r in S['art']:
    c=a.get_component_by_class(unreal.SpotLightComponent);source=unreal.Vector(*r['source']);delta=unreal.Vector(*r['aim'])-source;assert (c.get_world_location()-source).length()<1;assert unreal.MathLibrary.dot_vector_vector(c.get_forward_vector(),delta/delta.length())>.99999
   unreal.SystemLibrary.execute_console_command(E.get_editor_world(),'BUILDPATHS');S.update(phase=4,stamp=time.monotonic());return
  if S['phase']==4 and time.monotonic()-S['stamp']>25:
   OUT['saved']=unreal.EditorLoadingAndSavingUtils.save_map(E.get_editor_world(),MAP);assert OUT['saved'];OUT['map_sha256']=sha(R/'Content/Maps/M03_GlasshousePrototype.umap');assert all(sha(Path(p))==h for p,h in OUT['protected'].items());finish()
 except:finish(traceback.format_exc())
 finally:S['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['cb']=unreal.register_slate_post_tick_callback(tick)
