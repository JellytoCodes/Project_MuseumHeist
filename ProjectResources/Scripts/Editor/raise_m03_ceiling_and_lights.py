"""One-shot M03 ceiling and fixture correction; never duplicates content assets."""
import unreal,json,math,hashlib,shutil,time,traceback,itertools
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/M03CeilingLights';MAP='/Game/Maps/M03_GlasshousePrototype'
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
B=json.loads((O/'before.json').read_text());P=json.loads((R/'Saved/Automation/M03Expansion/applied.json').read_text())
OUT={'errors':[],'saved':False,'changes':[],'lights':[]};S={'phase':0,'stamp':0,'busy':False}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def v(p):return [p.x,p.y,p.z]
def box(c):
 b=c.static_mesh.get_bounding_box();t=c.get_world_transform();q=[v(unreal.MathLibrary.transform_location(t,unreal.Vector(*p))) for p in itertools.product(*zip(v(b.min),v(b.max)))];return [[min(p[k] for p in q) for k in range(3)],[max(p[k] for p in q) for k in range(3)]]
def loc(a,p):a.modify();a.set_actor_location(unreal.Vector(*(round(x) for x in p)),False,True)
def mesh(a):return next((c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh),None)
def apply():
 file=R/'Content/Maps/M03_GlasshousePrototype.umap';assert B['source_sha256']=='b23b151c9d9d219d5334cd7720c57218eca5dea22f9d532ab7473bdaa81a7f42' and sha(file)==B['source_sha256'],'One-shot source map changed; do not rerun'
 shutil.copy2(file,O/'M03_before.umap')
 OUT['protected']={str(p):sha(p) for base in ['Content/Maps','Content/Blueprints/Environment','Content/AIUE5_vol10_01','Content/Assets/Environment/M03Gallery','Content/Assets/MapAssets/Showcase'] for p in (R/base).rglob('*') if p.is_file() and p!=file}
 S['world']=unreal.EditorLoadingAndSavingUtils.load_map(MAP);actors={a.get_name():a for a in A.get_all_level_actors()};before={a['name']:a for a in B['actors']}
 ceilings=[]
 for a in actors.values():
  c=mesh(a);label=a.get_actor_label()
  if not c:continue
  mn=c.static_mesh.get_name();b=box(c);old=v(a.get_actor_location())
  if label.startswith(('M03G_Ceiling_','M03X_Ceiling_')):
   assert 'stone_tile_' in mn and b[0][2]>350
   ceilings.append((a,c,b));continue
  if label=='M03X_NorthGrand_UpperCap':A.destroy_actor(a);OUT['changes'].append([label,'redundant upper cap removed']);continue
  if '_walls_' in mn:
   if any(t in label for t in ['HallPartition','HallEndNorth','HallEndSouth','SpecialExhibition','IndependentExhibit','VaultAlcove']):continue
   s=a.get_actor_scale3d();height=(b[1][2]-b[0][2])/s.z;s.z=math.ceil((820-b[0][2])/height*10)/10;a.modify();a.set_actor_scale3d(s)
   nb=box(c);loc(a,[old[0],old[1],old[2]+b[0][2]-nb[0][2]]);OUT['changes'].append([label,'wall',b[1][2],box(c)[1][2]]);continue
  if label=='M03G_UpperFacade' or (mn.startswith('SM_AI_vol10_01_') and any(t in mn for t in ['_roof_','_cable_','_ventilation_','_window_'])):
   loc(a,[old[0],old[1],old[2]+205]);OUT['changes'].append([label,'roof assembly +205'])
 # Overlapping imported ceiling slabs occupy separate 2 cm planes, preventing coplanar flicker.
 placed=[]
 for a,c,b in sorted(ceilings,key=lambda q:q[0].get_actor_label()):
  neighbors={color for _,q,color in placed if all(min(b[1][k],q[1][k])-max(b[0][k],q[0][k])>1 for k in (0,1))}
  color=next(i for i in range(20) if i not in neighbors);old=v(a.get_actor_location());loc(a,[old[0],old[1],old[2]+800+color*2-b[0][2]]);placed.append((a,box(c),color));OUT['changes'].append([a.get_actor_label(),'ceiling underside',box(c)[0][2]])
 links={l['name']:l['painting'] for l in P['lights'] if l.get('painting')}
 for a in actors.values():
  if not unreal.SystemLibrary.is_valid(a) or not a.get_class().get_name().startswith('BP_HeistLight_'):continue
  c=mesh(a);spot=a.get_component_by_class(unreal.SpotLightComponent);assert c and spot
  pos=v(a.get_actor_location());support=[b[0][2] for _,b,_ in placed if all(b[0][k]<=pos[k]<=b[1][k] for k in (0,1))];mount=min(support) if support else 800
  loc(a,[pos[0],pos[1],mount]);a.set_actor_rotation(unreal.Rotator(),False)
  target=None
  if a.get_name() in links:
   painting=actors[links[a.get_name()]];pc=next(c for c in painting.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and 'SM_Canvas_Painting_' in c.static_mesh.get_name());pb=box(pc);target=unreal.Vector(*[(lo+hi)/2 for lo,hi in zip(*pb)])
   n=pc.get_forward_vector();yaw=round(math.degrees(math.atan2(n.y,n.x))+90)%180;a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
   origin=unreal.MathLibrary.transform_location(a.get_actor_transform(),spot.get_relative_transform().translation)
   aim=unreal.MathLibrary.find_look_at_rotation(origin,target);spot.modify();spot.set_world_rotation(aim,False,False)
   corners=[unreal.Vector(*p) for p in itertools.product(*zip(*pb))];d=(target-origin);dn=d/d.length();angles=[]
   for q in corners:
    z=q-origin;angles.append(math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(dn,z/z.length()))))))
   spot.set_outer_cone_angle(min(75,max(38,max(angles)+5)));spot.set_inner_cone_angle(min(30,spot.outer_cone_angle*.65));spot.set_attenuation_radius(max(spot.attenuation_radius,math.ceil(max((q-origin).length() for q in corners)*1.4/50)*50))
   assert box(c)[0][2]-pb[1][2]>70,(a.get_actor_label(),'insufficient painting clearance')
  else:
   spot.modify();spot.set_world_rotation(unreal.Rotator(pitch=-90),False,False);spot.set_attenuation_radius(max(spot.attenuation_radius,900))
  OUT['lights'].append({'name':a.get_name(),'label':a.get_actor_label(),'painting':links.get(a.get_name()),'mount':mount,'box':box(c),'target':v(target) if target else None,'intensity':spot.intensity})
 assert len(OUT['lights'])==63

def finish(error=None):
 if error:OUT['errors'].append(error)
 (O/'applied.json').write_text(json.dumps(OUT,indent=2));unreal.unregister_slate_post_tick_callback(S['cb']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(dt):
 if S['busy']:return
 S['busy']=True
 try:
  if S['phase']==0:apply();S.update(phase=1,stamp=time.monotonic());return
  if S['phase']==1 and time.monotonic()-S['stamp']>8:
   S['world']=E.get_editor_world();unreal.SystemLibrary.execute_console_command(S['world'],'BUILDPATHS');S.update(phase=2,stamp=time.monotonic());return
  if S['phase']==2 and time.monotonic()-S['stamp']>30:
   OUT['saved']=unreal.EditorLoadingAndSavingUtils.save_map(S['world'],MAP);assert OUT['saved'];OUT['map_sha256']=sha(R/'Content/Maps/M03_GlasshousePrototype.umap');assert all(sha(Path(p))==h for p,h in OUT['protected'].items());finish()
 except:finish(traceback.format_exc())
 finally:S['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['cb']=unreal.register_slate_post_tick_callback(tick)
