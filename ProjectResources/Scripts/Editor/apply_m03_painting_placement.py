"""Apply the reviewed M03-only 60-frame / 12-case placement through Unreal Editor.

Uses existing meshes and materials only. The source map fingerprint prevents this
focused change from overwriting later user edits or multiplying scales on rerun.
"""
import hashlib
import itertools
import json
import math
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/Automation/M03PaintingRedistribution'
MAP='/Game/Maps/M03_GlasshousePrototype'
PLAN=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/M03/M03PaintingPlacement.json').read_text())
META=json.loads((ROOT/'ProjectResources/SourceArt/Canvas/canvas_uv.json').read_text())
LAYOUT=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/M03/M03GalleryLayout.json').read_text())
CEILINGS={r['name']:r['ceiling'] or 595 for r in LAYOUT['rooms']}
MAT='/Game/Assets/Art/SurfaceForgery/Materials/Canvas'
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EL=unreal.EditorAssetLibrary
ML=unreal.MaterialEditingLibrary

def vec(q):return [float(getattr(q,k)) for k in 'xyz']
def integer(q):return unreal.Vector(*(math.trunc(v) for v in vec(q)))
def canvas(a):return next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_'))
def bounds(c):
 b=c.static_mesh.get_bounding_box();t=c.get_world_transform()
 pts=[vec(unreal.MathLibrary.transform_location(t,unreal.Vector(*p))) for p in itertools.product(*zip(vec(b.min),vec(b.max)))]
 return [[min(p[i] for p in pts) for i in range(3)],[max(p[i] for p in pts) for i in range(3)]]
def normal(p):
 n=[0,0,0];n[p['surface']['axis']]=p['surface']['sign'];return unreal.Vector(*n)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def apply():
 source=ROOT/'Content/Maps/M03_GlasshousePrototype.umap'
 assert sha(source)==PLAN['source_map_sha256'],'M03 changed since placement review; inspect again.'
 assert not PLAN['failed'] and len(PLAN['paintings'])==60
 assert sum(bool(p['case_id']) for p in PLAN['paintings'])==12
 protected=[ROOT/'Content/Maps'/f'{name}.umap' for name in ('M01_ClassicalPrototype','M02_MoonlitPrototype')]
 protected += list((ROOT/'Content/Assets/MapAssets/Showcase/Meshes').glob('SM_Canvas_Painting_*.uasset'))
 before={str(p):sha(p) for p in protected}
 assets_before={str(p.relative_to(ROOT)) for p in (ROOT/'Content').rglob('*.uasset')}
 world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
 actors={a.get_name():a for a in A.get_all_level_actors()}
 old_case_ids=sorted(str(a.get_display_case_id()) for a in actors.values() if isinstance(a,unreal.HeistPaintingDisplayCaseActor))
 removed=set(PLAN['removed'])
 assert len(removed)==18
 for a in actors.values():
  if isinstance(a,unreal.HeistLaserBarrierActor):assert a.protected_painting_case.get_name() not in removed
 # Existing per-print MIs must not affect another map. No MI is duplicated.
 materials={}
 for p in PLAN['paintings']:
  a=actors[p['name']];c=canvas(a);mi=c.get_material(0)
  assert mi.get_path_name()==p['material'] and isinstance(mi,unreal.MaterialInstanceConstant)
  refs=[str(x) for x in EL.find_package_referencers_for_asset(mi.get_path_name(),False)]
  assert all(x==MAP for x in refs),(mi.get_path_name(),refs)
  assert mi.get_path_name() not in materials,'A shared print requires a separate reviewed mapping.'
  materials[mi.get_path_name()]=mi
 report={'saved':False,'changed':[],'deleted_actors':[],'lights':[],'protected_before':before}
 for p in PLAN['paintings']:
  a=actors[p['name']];c=canvas(a);a.modify();c.modify()
  index=p['variant'];meta=META[index-1];s=p['scale'];n=normal(p)
  yaw=math.degrees(math.atan2(n.y,n.x));rot=unreal.Rotator(yaw=yaw)
  rear=unreal.Vector(*p['center'])
  local=unreal.Vector(meta['min'][0],(meta['min'][1]+meta['max'][1])/2,(meta['min'][2]+meta['max'][2])/2)
  offset=unreal.MathLibrary.transform_direction(unreal.Transform(rotation=rot),local*s)
  mesh=unreal.load_asset(f'/Game/Assets/MapAssets/Showcase/Meshes/SM_Canvas_Painting_{index:02}a')
  c.set_static_mesh(mesh)
  if p['case_id']:
   assert str(a.get_display_case_id())==p['case_id']
   a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_rotation(unreal.Rotator(yaw=yaw+180),False)
   a.set_actor_location(integer(unreal.Vector(rear.x,rear.y,0)),False,True)
   c.set_world_transform(unreal.Transform(location=rear-offset,rotation=rot,scale=unreal.Vector(s,s,s)),False,True)
   box=a.get_component_by_class(unreal.BoxComponent);box.modify()
   box.set_world_scale3d(unreal.Vector(1,1,1));box.set_box_extent(unreal.Vector(60,60,80),False)
   box.set_world_location(integer(unreal.Vector(rear.x,rear.y,100)+n*90),False,True)
   base=unreal.load_asset(MAT+f'/MI_HeistCanvas_{index:02}')
   a.set_editor_property('original_painting_material',base);a.set_editor_property('replica_painting_material',base)
  else:
   a.set_actor_transform(unreal.Transform(location=integer(rear-offset),rotation=rot,scale=unreal.Vector(s,s,s)),False,True)
  c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  c.set_editor_property('generate_overlap_events',False)
  mi=materials[p['material']];mi.modify()
  texture=ML.get_material_instance_texture_parameter_value(mi,'PaintingTexture')
  assert texture,'Missing painting texture'
  ML.clear_all_material_instance_parameters(mi)
  ML.set_material_instance_parent(mi,unreal.load_asset(MAT+f'/MI_HeistCanvas_{index:02}'))
  ML.set_material_instance_texture_parameter_value(mi,'PaintingTexture',texture)
  ML.set_material_instance_scalar_parameter_value(mi,'PaintingAspect',texture.blueprint_get_size_x()/max(1,texture.blueprint_get_size_y()))
  for name,values in [('CanvasURow',meta['u_row']),('CanvasVRow',meta['v_row'])]:
   ML.set_material_instance_vector_parameter_value(mi,name,unreal.LinearColor(*values,0))
  ML.set_material_instance_scalar_parameter_value(mi,'CanvasAspect',(meta['front_max'][1]-meta['front_min'][1])/(meta['front_max'][2]-meta['front_min'][2]))
  ML.set_material_instance_scalar_parameter_value(mi,'CanvasEllipse',0)
  c.set_material(0,mi)
  a.set_folder_path('M03Gallery/Paintings/'+p['surface']['room'])
  # Old group/pattern tags described the retired dense arrangement.
  a.tags=[t for t in a.tags if not str(t).startswith(('MuseumHangingPattern_','MuseumHangingGroup_','MuseumHangingSlot_'))]+['M03PaintingPlacement60']
  report['changed'].append({'name':p['name'],'case_id':p['case_id'],'variant':index,'scale':s,'box':bounds(c),'room':p['surface']['room']})
 # Retarget the existing 20 art fixtures. Passage fixtures keep their placement.
 lights=[a for a in actors.values() if a.get_actor_label().startswith('M03G_Light_Art_')]
 assert len(lights)==20
 targets=[p for p in PLAN['paintings'] if p['case_id']]
 remaining=[p for p in PLAN['paintings'] if not p['case_id'] and p['surface']['room'] in ('Hall','A','B','C','Vault','Entry')]
 while len(targets)<len(lights):
  p=max(remaining,key=lambda p:min(math.dist(p['center'][:2],q['center'][:2]) for q in targets))
  targets.append(p);remaining.remove(p)
 for p in targets:
  n=normal(p);position=unreal.Vector(*p['center'])+n*260
  position.z=CEILINGS[p['surface']['room']]
  a=min(lights,key=lambda a:(a.get_actor_location()-position).length());lights.remove(a)
  a.modify();a.set_actor_location(integer(position),False,True)
  spot=a.get_component_by_class(unreal.SpotLightComponent);spot.modify()
  source=spot.get_world_location();target=unreal.Vector(*p['center']);direction=target-source
  spot.set_world_rotation(unreal.MathLibrary.make_rot_from_x(direction),False,True)
  spot.set_editor_property('attenuation_radius',max(600,direction.length()+300))
  spot.set_editor_property('outer_cone_angle',70.0);spot.set_editor_property('inner_cone_angle',45.0)
  report['lights'].append({'name':a.get_name(),'painting':p['name']})
 # The second protected work moves from a tiny alcove to the north vault wall.
 # Span the existing vault walls so the beam still separates its interaction area.
 laser=actors['BP_LaserBarrier_C_1'];laser.modify()
 laser.set_actor_location(unreal.Vector(2200,-50,0),False,True);laser.set_actor_rotation(unreal.Rotator(yaw=90),False)
 box=next(c for c in laser.get_components_by_class(unreal.BoxComponent) if c.get_name()=='BeamTriggerComponent');box.modify();box.set_box_extent(unreal.Vector(10,488,120),False)
 for i,c in enumerate(laser.get_components_by_class(unreal.NiagaraComponent)):
  c.set_variable_vec3('User.BeamStart',unreal.Vector(0,-488,0));c.set_variable_vec3('User.BeamEnd',unreal.Vector(0,488,0))
 button=actors['BP_SecurityHoldButton_C_1'];button.modify();button.set_actor_location(unreal.Vector(1900,-220,105),False,True)
 for name in PLAN['removed']:
  a=actors[name];canvas(a);report['deleted_actors'].append({'name':name,'label':a.get_actor_label()});assert A.destroy_actor(a)
 assert len([a for a in A.get_all_level_actors() if isinstance(a,unreal.HeistPaintingDisplayCaseActor)])==12
 dt=unreal.load_asset('/Game/Data/DataTable/DT_ContractData')
 assert dt,'Contract table missing'
 dt.modify();assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(dt,str(ROOT/'ProjectResources/DataTableImports/DT_ContractDataRow.json'))
 for mi in materials.values():assert EL.save_loaded_asset(mi)
 assert EL.save_loaded_asset(dt)
 presentation=unreal.load_asset('/Game/Data/DataTable/DT_MapPresentation')
 rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(presentation))
 for row in rows:
  if row['Name']=='M03':row['ContractTargetGalleryZoneId']='Zone_H'
 presentation.modify()
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(presentation,json.dumps(rows,ensure_ascii=False))
 assert EL.save_loaded_asset(presentation)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
 report['saved']=True
 report['protected_unchanged']=all(sha(Path(p))==h for p,h in before.items())
 report['new_assets']=sorted({str(p.relative_to(ROOT)) for p in (ROOT/'Content').rglob('*.uasset')}-assets_before)
 report['old_case_ids']=old_case_ids
 assert report['protected_unchanged'] and not report['new_assets']
 (OUT/'applied.json').write_text(json.dumps(report,indent=2))

if __name__=='__main__':
 try:apply()
 except Exception:(OUT/'apply_error.txt').write_text(traceback.format_exc());raise
 finally:unreal.SystemLibrary.quit_editor()
