"""Reviewed canvas adjustments and four fixed ceiling fixture shells.

Run through Unreal Editor. The authored architecture and gameplay references are
preserved. Input is a reviewed, absolute placement delta, not a layout generator.
"""
import json,math,shutil
from pathlib import Path
import unreal
from refine_canvas_and_light_fixtures import painting_geometry,MAPS,LIGHT_PROPERTIES,data_object

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/ArtDetail';OUT.mkdir(exist_ok=True)
PLAN=json.loads((ROOT/'ProjectResources/SourceArt/Canvas/art_detail_adjustment.json').read_text())
ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SUB=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
EL=unreal.EditorAssetLibrary
OLD='/Game/Blueprints/Environment/BP_HeistLightFixture'
MESH_DIR='/Game/Assets/MapAssets/Showcase/Meshes/'
TYPES={
 'SM_Drop_Ceiling_01a':('BP_HeistLight_DropCeiling_01a',152),
 **{'SM_F_Ceiling_Droplights_01'+c:('BP_HeistLight_Droplights_01'+c,100) for c in 'abc'},
}
TAG='ArtDetailV2'
NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER

def vec(v):return [v.x,v.y,v.z]
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=NEVER)
def backup(package,extension):
 src=ROOT/'Content'/(package.removeprefix('/Game/')+extension)
 dst=OUT/'Backup'/(package.removeprefix('/Game/')+extension)
 if src.exists() and not dst.exists():
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)

def prepare_types():
 result={}
 for mesh_name,(bp_name,hang) in TYPES.items():
  path='/Game/Blueprints/Environment/'+bp_name
  bp=unreal.load_asset(path) if EL.does_asset_exist(path) else EL.duplicate_asset(OLD,path)
  assert bp,path
  bp.modify();handles=SUB.k2_gather_subobject_data_for_blueprint(bp)
  root=next(h for h in handles if data_object(h).get_name()=='DefaultSceneRoot_GEN_VARIABLE')
  mh=next(h for h in handles if isinstance(data_object(h),unreal.StaticMeshComponent))
  mesh=data_object(mh);spot=next(data_object(h) for h in handles if isinstance(data_object(h),unreal.SpotLightComponent))
  scale=next((h for h in handles if data_object(h).get_name()=='FixtureVisualScale_GEN_VARIABLE'),None)
  if scale is not None:
   assert SUB.attach_subobject(root,mh)
   assert SUB.delete_subobject(handles[0],scale,bp)==1
  asset=unreal.load_asset(MESH_DIR+mesh_name);bounds=asset.get_bounding_box();s=max(.1,round(hang/(bounds.max.z-bounds.min.z),1))
  # Reuse the source mesh at 0.1-step scales. Mount its top at the ceiling and
  # place the light below the resulting suspension, without baking a size copy.
  width_scale=max(.1,round((152 if 'Drop_Ceiling' in mesh_name else 180)/(bounds.max.z-bounds.min.z),1))
  hang=(bounds.max.z-bounds.min.z)*s
  mesh.set_static_mesh(asset);mesh.set_relative_scale3d(unreal.Vector(width_scale,width_scale,s))
  mesh.set_relative_rotation(unreal.Rotator(),False,True)
  mesh.set_relative_location(unreal.Vector(-(bounds.min.x+bounds.max.x)*width_scale/2,-(bounds.min.y+bounds.max.y)*width_scale/2,-bounds.max.z*s),False,True)
  mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);setp(mesh,'cast_shadow',False)
  spot.set_relative_location(unreal.Vector(0,0,-hang-4),False,True)
  spot.set_relative_rotation(unreal.Rotator(pitch=-90 if 'Drop_Ceiling' in mesh_name else -45),False,True)
  unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert EL.save_loaded_asset(bp)
  result[mesh_name]=bp
 return result

def paintings(actors,name):
 changed=[];lookup={}
 for a in actors:
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if not c.static_mesh or not c.static_mesh.get_name().startswith('SM_Canvas_Painting_'):continue
   label=a.get_actor_label();assert label in PLAN['maps'][name],label
   target=PLAN['maps'][name][label];old=painting_geometry(c)
   if TAG not in [str(t) for t in a.tags]:
    assert (old['center']-unreal.Vector(*target['old_center'])).length()<2,(name,label,'placement changed since review')
    a.modify();c.modify()
    center=unreal.Vector(*target['center'])
    if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
     # Keep the interaction volume with the artwork horizontally, at floor Z.
     delta=center-old['center'];pos=a.get_actor_location()
     a.set_actor_location(unreal.Vector(math.trunc(pos.x+delta.x),math.trunc(pos.y+delta.y),pos.z),False,True)
    c.set_relative_scale3d(unreal.Vector(*target['scale']))
    now=painting_geometry(c);c.set_world_location(c.get_world_location()+center-now['center'],False,True)
    if isinstance(a,unreal.StaticMeshActor):
     pos=a.get_actor_location();a.set_actor_location(unreal.Vector(*(math.trunc(v) for v in vec(pos))),False,True)
    a.set_editor_property('tags',list(a.tags)+[TAG]);changed.append(label)
   lookup[label]=painting_geometry(c)
 return changed,lookup

def replace_lights(actors,types,lookup):
 rows=[]
 for old in actors:
  replace=old.get_class().get_name()=='BP_HeistLightFixture_C'
  if not replace and not old.get_class().get_name().startswith('BP_HeistLight_'):continue
  mesh=old.get_component_by_class(unreal.StaticMeshComponent);spot=old.get_component_by_class(unreal.SpotLightComponent)
  mesh_name=mesh.static_mesh.get_name();assert mesh_name in types
  tags=list(old.tags);names=[str(t)[4:] for t in tags if str(t).startswith('Art_')]
  group=[lookup[n] for n in names];properties={}
  for k in LIGHT_PROPERTIES:
   try:properties[k]=spot.get_editor_property(k)
   except Exception:pass
  pos=old.get_actor_location();yaw=mesh.get_world_rotation().yaw
  a=ACT.spawn_actor_from_class(types[mesh_name].generated_class(),pos,unreal.Rotator(yaw=yaw)) if replace else old
  a.modify();a.set_actor_label(old.get_actor_label());a.set_folder_path(old.get_folder_path())
  a.set_editor_property('tags',tags+([TAG] if TAG not in [str(t) for t in tags] else []))
  light=a.get_component_by_class(unreal.SpotLightComponent)
  light.modify()
  for k,v in properties.items():setp(light,k,v)
  light.set_editor_property('component_tags',list(spot.component_tags))
  # Freshly spawned SCS components may still report an identity ComponentToWorld
  # until the next Editor tick. Use the authored ceiling anchor, never that cache.
  source=pos+unreal.Vector(0,0,-TYPES[mesh_name][1]-4)
  if group:
   n=group[0]['normal'];h=unreal.Vector(-n.y,n.x,0)
   xs=[unreal.MathLibrary.dot_vector_vector(p['center'],h) for p in group]
   xmin=min(x-p['width']/2 for x,p in zip(xs,group));xmax=max(x+p['width']/2 for x,p in zip(xs,group))
   zmin=min(p['center'].z-p['height']/2 for p in group);zmax=max(p['center'].z+p['height']/2 for p in group)
   target=group[0]['center']+h*((xmin+xmax)/2-xs[0]);target.z=(zmin+zmax)/2
   # Keep wide/tall groups inside a useful cone rather than clipping their upper
   # corners with a near-180-degree spotlight. Move along the same ceiling plane.
   for step in range(17):
    direction=target-source;distance=direction.length();angles=[]
    for x in (xmin,xmax):
     for z in (zmin,zmax):
      ray=target+h*(x-(xmin+xmax)/2)+unreal.Vector(0,0,z-target.z)-source
      angles.append(math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(ray/ray.length(),direction/distance))))))
    if max(angles)<=62 or step==16:break
    pos=pos+n*16;pos=unreal.Vector(*(math.trunc(v) for v in vec(pos)))
    source=pos+unreal.Vector(0,0,-TYPES[mesh_name][1]-4)
   assert max(angles)<75,(a.get_actor_label(),'insufficient light coverage')
   a.set_actor_location(pos,False,True)
   rotation=unreal.MathLibrary.make_rot_from_x(direction)
   light.set_relative_rotation(unreal.Rotator(pitch=rotation.pitch,yaw=rotation.yaw-yaw,roll=rotation.roll),False,True)
   outer=min(78,max(20,max(angles)+3));setp(light,'outer_cone_angle',outer);setp(light,'inner_cone_angle',outer*.7)
   setp(light,'attenuation_radius',max(600,distance+250))
  else:
   light.set_relative_rotation(unreal.Rotator(pitch=-90),False,True)
  assert len(a.get_components_by_class(unreal.LightComponent))==1
  rows.append(dict(label=a.get_actor_label(),type=a.get_class().get_name(),intensity=light.intensity,source=vec(source),hang=TYPES[mesh_name][1]))
  if replace:assert ACT.destroy_actor(old)
 return rows

def redundant_frames(actors,lookup,remove=False):
 rows=[]
 for a in actors:
  if not isinstance(a,unreal.StaticMeshActor):continue
  label=a.get_actor_label();stem,sep,side=label.rpartition('_')
  if side not in ('Top','Bottom','Left','Right'):continue
  owner=stem if stem in lookup else stem.replace('_Gallery_Secure_','_Painting_')
  if owner not in lookup:continue
  c=a.static_mesh_component
  if not c.static_mesh or c.static_mesh.get_path_name() not in (
   '/Game/Assets/StarterContent/Shapes/Shape_Cube.Shape_Cube','/Engine/BasicShapes/Cube.Cube'):continue
  # Label alone is not evidence: confirm a thin blockout bar on the canvas wall.
  origin,extent=a.get_actor_bounds(False);p=lookup[owner];delta=origin-p['center']
  if delta.length()>320 or abs(unreal.MathLibrary.dot_vector_vector(delta,p['normal']))>40:continue
  dims=[x*2 for x in vec(extent)]
  if min(dims)>16 or max(dims)>400:continue
  rows.append(dict(label=label,canvas=owner,mesh=c.static_mesh.get_path_name(),dimensions=dims))
  if remove:assert ACT.destroy_actor(a)
 return rows

def run():
 backup(OLD,'.uasset')
 for name in MAPS:backup('/Game/Maps/'+name,'.umap')
 types=prepare_types();report={}
 for name in MAPS:
  world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+name);world.modify()
  actors=list(ACT.get_all_level_actors());changed,lookup=paintings(actors,name)
  removed=redundant_frames(actors,lookup,True)
  lights=replace_lights(list(ACT.get_all_level_actors()),types,lookup)
  assert len([a for a in ACT.get_all_level_actors() if isinstance(a,unreal.HeistPaintingDisplayCaseActor)])==20
  assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/'+name)
  report[name]=dict(paintings_changed=changed,fixtures=lights,redundant_frames_removed=removed)
  (OUT/'applied.json').write_text(json.dumps(report,indent=2))
 laser=unreal.load_asset('/Game/Blueprints/World/Actors/Security/BP_LaserBarrier');laser.modify()
 unreal.get_default_object(laser.generated_class()).set_editor_property('beam_width',3.6)
 unreal.BlueprintEditorLibrary.compile_blueprint(laser);assert EL.save_loaded_asset(laser)
 if EL.does_asset_exist(OLD):
  refs=EL.find_package_referencers_for_asset(OLD,True)
  assert not refs,('old fixture still referenced',refs)
  assert EL.delete_asset(OLD)
 print('ART_DETAIL_APPLIED')

if __name__=='__main__':run()
