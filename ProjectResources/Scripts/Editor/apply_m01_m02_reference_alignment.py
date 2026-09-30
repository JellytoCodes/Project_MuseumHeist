"""One-shot Editor application of the approved M03 reference sizes/count/height.

Uses existing architecture, canvas meshes, print MIs and four fixture Blueprints.
No rooms, source meshes, Blueprint defaults or new content assets are authored.
Plans are fingerprinted and post-load geometry checks gate each map save.
"""
from pathlib import Path
import hashlib,json,math,itertools,time,traceback,shutil
import unreal
R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/M01M02Alignment20260930'
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);EL=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
META=json.loads((R/'ProjectResources/SourceArt/Canvas/canvas_uv.json').read_text())
MAT='/Game/Assets/Art/SurfaceForgery/Materials/Canvas'
PLANS=[json.loads((O/f'{code}_plan.json').read_text()) for code in ['M01','M02']]
REPORT={'saved':[],'errors':[],'maps':[]};S={'phase':'load','index':0,'busy':False,'stamp':0}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def v(p):return [p.x,p.y,p.z]
def integer(p):return unreal.Vector(*(math.trunc(x) for x in v(p)))
def move(a,p):a.modify();a.set_actor_location(integer(p),False,True)
def bounds(c):
 b=c.static_mesh.get_bounding_box();t=c.get_world_transform();pts=[v(unreal.MathLibrary.transform_location(t,unreal.Vector(*p))) for p in itertools.product(*zip(v(b.min),v(b.max)))]
 return [[min(p[i] for p in pts) for i in range(3)],[max(p[i] for p in pts) for i in range(3)]]
def canvas(a):return next((c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_')),None)
def reset_height(a,c,zscale):
 b=bounds(c);a.modify();c.modify();scale=a.get_actor_scale3d();scale.z=zscale;a.set_actor_scale3d(scale);q=bounds(c);move(a,a.get_actor_location()+unreal.Vector(0,0,b[0][2]-q[0][2]))
def architecture(P,actors,report):
 code=P['code'];ceil=P['ceiling'];changed=[]
 if code=='M01':
  roof=actors['StaticMeshActor_1180'];assert roof.static_mesh_component.static_mesh.get_name()=='SM_Wall_1500_01a';p=roof.get_actor_location();p.z=800;move(roof,p)
  for a in actors.values():
   if not isinstance(a,unreal.StaticMeshActor):continue
   c=a.static_mesh_component
   if not c.static_mesh:continue
   name=c.static_mesh.get_name();b=bounds(c)
   if name.startswith('SM_Display_Wall_') and b[0][2]<40 and b[1][2]>400:
    reset_height(a,c,2.0);changed.append(a.get_name())
   elif a.get_actor_label().startswith('LDV2_M01_Gallery_Door_') and name=='Shape_Cube' and b[0][2]>=350 and b[1][2]<=481:
    reset_height(a,c,4.4);changed.append(a.get_name())
 else:
  roof=next(a for a in actors.values() if a.get_actor_label()=='SM_Floor_2');assert roof.static_mesh_component.static_mesh.get_name()=='SM_Floor_01';p=roof.get_actor_location();p.z=820;move(roof,p)
  wallactors=[a for a in actors.values() if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_WALL_B_') and 'DOOR' not in a.static_mesh_component.static_mesh.get_name()]
  before={a.get_name():bounds(a.static_mesh_component) for a in wallactors}
  uppers=[a for a in wallactors if before[a.get_name()][0][2]>250]
  for a in wallactors:
   b=before[a.get_name()];c=a.static_mesh_component
   if b[0][2]>250:reset_height(a,c,1.7);changed.append(a.get_name())
   else:
    matches=[q for q in uppers if all(abs(before[q.get_name()][j][k]-b[j][k])<2 for j in [0,1] for k in [0,1])]
    if not matches:reset_height(a,c,2.7);changed.append(a.get_name())
  for a in actors.values():
   if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and 'TOP' in a.static_mesh_component.static_mesh.get_name().upper() and a.get_actor_location().z>500:
    move(a,a.get_actor_location()+unreal.Vector(0,0,210));changed.append(a.get_name())
 report['architecture']={'ceiling_bounds':bounds(roof.static_mesh_component),'wall_top_changes':changed}
 assert abs(bounds(roof.static_mesh_component)[0][2]-ceil)<1
def paintings(P,actors,report):
 materials={};changed=[]
 for p in P['paintings']:
  a=actors[p['name']];c=canvas(a);mi=c.get_material(0);assert mi.get_path_name()==p['material'] and isinstance(mi,unreal.MaterialInstanceConstant)
  refs=[str(r) for r in EL.find_package_referencers_for_asset(mi.get_path_name(),False)]
  assert all(r=='/Game/Maps/'+P['map'] for r in refs),(mi.get_path_name(),refs)
  assert mi.get_path_name() not in materials,'Shared print material needs explicit mapping';materials[mi.get_path_name()]=mi
 for p in P['paintings']:
  a=actors[p['name']];c=canvas(a);a.modify();c.modify();m=META[p['variant']-1];scale=p['scale'];t=p['surface'];normal=unreal.Vector(t['sign'] if t['axis']==0 else 0,t['sign'] if t['axis']==1 else 0,0);rot=unreal.Rotator(yaw=math.degrees(math.atan2(normal.y,normal.x)));rear=unreal.Vector(*p['center'])
  local=unreal.Vector(m['min'][0],(m['min'][1]+m['max'][1])/2,(m['min'][2]+m['max'][2])/2)
  offset=unreal.MathLibrary.transform_direction(unreal.Transform(rotation=rot),local*scale)
  c.set_static_mesh(unreal.load_asset(f'/Game/Assets/MapAssets/Showcase/Meshes/SM_Canvas_Painting_{p["variant"]:02}a'))
  if p['case_id']:
   assert str(a.get_display_case_id())==p['case_id'] and str(a.target_artifact_id)==p['artifact_id']
   a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_rotation(unreal.Rotator(yaw=rot.yaw+180),False);move(a,unreal.Vector(rear.x,rear.y,P['floor']))
   # Relative offsets may carry geometry precision; scale remains exact integer.
   c.set_world_transform(unreal.Transform(location=rear-offset,rotation=rot,scale=unreal.Vector(scale,scale,scale)),False,True)
   box=a.get_component_by_class(unreal.BoxComponent);box.modify();box.set_world_scale3d(unreal.Vector(1,1,1));box.set_box_extent(unreal.Vector(60,60,80),False);box.set_world_location(integer(unreal.Vector(rear.x,rear.y,P['floor']+100)+normal*90),False,True)
   base=unreal.load_asset(MAT+f'/MI_HeistCanvas_{p["variant"]:02}');a.set_editor_property('original_painting_material',base);a.set_editor_property('replica_painting_material',base)
  else:a.set_actor_transform(unreal.Transform(location=integer(rear-offset),rotation=rot,scale=unreal.Vector(scale,scale,scale)),False,True)
  c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_editor_property('generate_overlap_events',False)
  mi=materials[p['material']];mi.modify();texture=ML.get_material_instance_texture_parameter_value(mi,'PaintingTexture');assert texture
  ML.clear_all_material_instance_parameters(mi);ML.set_material_instance_parent(mi,unreal.load_asset(MAT+f'/MI_HeistCanvas_{p["variant"]:02}'))
  ML.set_material_instance_texture_parameter_value(mi,'PaintingTexture',texture);ML.set_material_instance_scalar_parameter_value(mi,'PaintingAspect',texture.blueprint_get_size_x()/texture.blueprint_get_size_y())
  ML.set_material_instance_scalar_parameter_value(mi,'CanvasAspect',(m['front_max'][1]-m['front_min'][1])/(m['front_max'][2]-m['front_min'][2]));ML.set_material_instance_scalar_parameter_value(mi,'CanvasEllipse',0)
  for k,x in [('CanvasURow',m['u_row']),('CanvasVRow',m['v_row'])]:ML.set_material_instance_vector_parameter_value(mi,k,unreal.LinearColor(*x,0))
  c.set_material(0,mi)
  if p['case_id']:
   panel=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('SecurityPanelVisual'));panel.modify();b=bounds(c);pc=unreal.Vector(*[(l+u)/2 for l,u in zip(*b)]);horizontal=c.get_right_vector();target=pc-unreal.Vector(0,0,p['height']/2+16)+normal*2
   if target.z<P['floor']+12:target=pc+horizontal*(p['width']/2+16)+normal*2;target.z=P['floor']+36
   panel.set_world_location(target,False,True);panel.set_world_rotation(unreal.MathLibrary.make_rot_from_z(normal),False,True)
  a.tags=[q for q in a.tags if not str(q).startswith(('MuseumHangingPattern_','MuseumHangingGroup_','MuseumHangingSlot_'))]+['ReferenceM03Alignment20260930',f'PaintingTier{p["tier"]}']
  changed.append({'name':p['name'],'case_id':p['case_id'],'variant':p['variant'],'tier':p['tier'],'scale':scale,'bounds':bounds(c)})
 for a in actors.values():
  if isinstance(a,unreal.HeistLaserBarrierActor):assert a.protected_painting_case.get_name() in P['protected']
 for name in P['removed']:assert name not in P['protected'];assert A.destroy_actor(actors[name])
 S['materials']=materials;report['paintings']=changed;report['removed']=P['removed']
def lighting(P,actors,report):
 fixtures=[a for a in actors.values() if a.get_class().get_name().startswith('BP_HeistLight_')];art=[a for a in fixtures if 'Droplights_' in a.get_class().get_name()];passages=[a for a in fixtures if 'DropCeiling_' in a.get_class().get_name()];lights=[]
 for p in P['paintings']:
  c=canvas(actors[p['name']]);b=bounds(c);target=unreal.Vector(*[(l+u)/2 for l,u in zip(*b)]);n=c.get_forward_vector();position=target+n*250;position.z=P['ceiling']
  if art:a=min(art,key=lambda a:(a.get_actor_location()-position).length());art.remove(a)
  else:
   suffix='01c' if p['width']>560 else '01b' if p['width']>400 else '01a';path=f'/Game/Blueprints/Environment/BP_HeistLight_Droplights_{suffix}';a=A.spawn_actor_from_class(unreal.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C'),position);a.set_actor_label(P['code']+'_ArtworkLight_'+p['name']);a.set_folder_path(P['code']+'/Lights/Artwork')
  move(a,position);a.set_actor_rotation(unreal.Rotator(yaw=(math.degrees(math.atan2(n.y,n.x))+90)%180),False)
  spot=a.get_component_by_class(unreal.SpotLightComponent);spot.modify();origin=spot.get_world_location();d=target-origin;spot.set_world_rotation(unreal.MathLibrary.make_rot_from_x(d),False,True)
  corners=[unreal.Vector(*q) for q in itertools.product(*zip(*b))];angles=[math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(d/d.length(),(q-origin)/(q-origin).length()))))) for q in corners];outer=min(75,max(38,max(angles)+5))
  spot.set_editor_properties(dict(intensity=4.,intensity_units=unreal.LightUnits.CANDELAS,use_inverse_squared_falloff=True,indirect_lighting_intensity=0.,volumetric_scattering_intensity=0.,outer_cone_angle=outer,inner_cone_angle=min(30,outer*.65),attenuation_radius=math.ceil(max((q-origin).length() for q in corners)*1.4/50)*50))
  fixture=next(q for q in a.get_components_by_class(unreal.StaticMeshComponent) if q.static_mesh);fixture.set_editor_property('affect_dynamic_indirect_lighting',False);assert bounds(fixture)[0][2]>b[1][2]+40,(a.get_name(),'Fixture intersects artwork')
  lights.append({'name':a.get_name(),'painting':p['name'],'mount':v(a.get_actor_location()),'emitter':v(origin),'target':v(target),'cone':outer})
 assert not art
 for a in passages:
  p=a.get_actor_location();p.z=P['ceiling'];move(a,p);a.set_actor_rotation(unreal.Rotator(),False);spot=a.get_component_by_class(unreal.SpotLightComponent);spot.modify();spot.set_world_rotation(unreal.Rotator(pitch=-90),False,True);spot.set_attenuation_radius(max(900,spot.attenuation_radius))
 report['lights']=lights;report['passage_lights']=len(passages)
def apply(P):
 source=R/f'Content/Maps/{P["map"]}.umap';assert sha(source)==P['source_sha256'],'Source map changed; review before applying'
 shutil.copy2(source,O/(P['code']+'_before.umap'));world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+P['map']);actors={a.get_name():a for a in A.get_all_level_actors()};report={'map':P['map'],'saved':False};REPORT['maps'].append(report)
 architecture(P,actors,report);paintings(P,actors,report);lighting(P,actors,report);S.update(world=world,actors=actors,report=report)
def validate(P):
 world=S['world'];report=S['report'];actors=S['actors'];errors=[];boxes=[];wallchecks=[]
 for p in P['paintings']:
  a=actors[p['name']];c=canvas(a);b=bounds(c);n=c.get_forward_vector();horizontal=c.get_right_vector();m=META[p['variant']-1];front=unreal.MathLibrary.transform_location(c.get_world_transform(),unreal.Vector(m['front_max'][0],(m['front_min'][1]+m['front_max'][1])/2,(m['front_min'][2]+m['front_max'][2])/2));support=0
  for x,z in itertools.product([-.45,-.225,0,.225,.45],repeat=2):
   point=front+horizontal*(p['width']*x)+unreal.Vector(0,0,p['height']*z);hit=unreal.SystemLibrary.line_trace_single(world,point+n*2,point-n*85,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[a],unreal.DrawDebugTrace.NONE,True)
   if hit:
    h=hit.to_tuple();other=h[9]
    if isinstance(other,unreal.StaticMeshActor) and other.static_mesh_component.static_mesh and 'wall' in other.static_mesh_component.static_mesh.get_name().lower() and unreal.MathLibrary.dot_vector_vector(h[6],n)>.8 and unreal.MathLibrary.dot_vector_vector(h[4]-point,n)<-1:support+=1
  wallchecks.append({'name':p['name'],'wall_support_samples':support,'total':25})
  if support<25:errors.append([p['name'],'wall support',support])
  if b[0][2]<P['floor']+10 or b[1][2]>P['ceiling']-55:errors.append([p['name'],'floor/ceiling clearance',b])
  for name,q in boxes:
   if all(min(b[1][k],q[1][k])-max(b[0][k],q[0][k])>1 for k in range(3)):errors.append([p['name'],'painting overlap',name])
  boxes.append((p['name'],b))
  for other in A.get_all_level_actors():
   if not isinstance(other,unreal.StaticMeshActor) or canvas(other):continue
   oc=other.static_mesh_component
   if not oc.static_mesh or 'wall' not in oc.static_mesh.get_name().lower():continue
   q=bounds(oc)
   if all(min(b[1][k],q[1][k])-max(b[0][k],q[0][k])>1 for k in range(3)):errors.append([p['name'],'wall intersection',other.get_name()])
 report['wall_checks']=wallchecks;report['geometry_errors']=errors
 (O/(P['code']+'_preflight.json')).write_text(json.dumps(report,indent=2),encoding='utf-8');assert not errors,errors[:12]
 live=A.get_all_level_actors();assert sum(bool(canvas(a)) for a in live)==60;assert sum(isinstance(a,unreal.HeistPaintingDisplayCaseActor) for a in live)==12
 for mi in S['materials'].values():assert EL.save_loaded_asset(mi)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/'+P['map']);report['saved']=True;report['sha256']=sha(R/f'Content/Maps/{P["map"]}.umap');REPORT['saved'].append(P['code'])
def finish(error=None):
 if error:REPORT['errors'].append(error)
 REPORT['protected_unchanged']=all(sha(Path(p))==h for p,h in S.get('protected',{}).items());REPORT['new_assets']=sorted({str(p.relative_to(R)) for p in (R/'Content').rglob('*.uasset')}-S.get('inventory',set()))
 (O/'applied.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(S['cb']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(dt):
 if S['busy']:return
 S['busy']=True
 try:
  P=PLANS[S['index']]
  if S['phase']=='load':apply(P);S.update(phase='settle',stamp=time.monotonic());return
  if S['phase']=='settle' and time.monotonic()-S['stamp']>8:
   validate(P);S['index']+=1
   if S['index']==len(PLANS):
    dt=unreal.load_asset('/Game/Data/DataTable/DT_ContractData');dt.modify();assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(dt,str(R/'ProjectResources/DataTableImports/DT_ContractDataRow.json'));assert EL.save_loaded_asset(dt);finish()
   else:S['phase']='load'
 except:finish(traceback.format_exc())
 finally:S['busy']=False
if __name__=='__main__':
 S['inventory']={str(p.relative_to(R)) for p in (R/'Content').rglob('*.uasset')}
 S['protected']={str(p):sha(p) for base in ['Content/Blueprints','Content/Assets/MapAssets','Content/AIUE5_vol10_01'] for p in (R/base).rglob('*') if p.is_file()}
 path=R/'Content/Maps/M03_GlasshousePrototype.umap';S['protected'][str(path)]=sha(path)
 unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['cb']=unreal.register_slate_post_tick_callback(tick)
