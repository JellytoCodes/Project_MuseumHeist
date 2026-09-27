"""Apply reviewed artwork/passage fixtures through Unreal Editor only.

The JSON is an explicit placement and pre-change intensity baseline, not a
randomized room generator. Authored architecture, paintings and PCG are untouched.
"""
import json, math, shutil, sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from refine_canvas_and_light_fixtures import LIGHT_PROPERTIES, data_object, BP_PATH

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/PurposeLighting';OUT.mkdir(exist_ok=True)
PLAN=json.loads((ROOT/'ProjectResources/SourceArt/Lighting/purpose_lighting.json').read_text())
ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SUB=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER

def backup(package,extension):
 src=ROOT/'Content'/(package.removeprefix('/Game/')+extension)
 dst=OUT/'Backup'/(package.removeprefix('/Game/')+extension)
 if not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)

def setp(c,key,value):c.set_editor_property(key,value,notify_mode=NEVER)

def prepare_blueprint():
 backup(BP_PATH,'.uasset');bp=unreal.load_asset(BP_PATH)
 handles=SUB.k2_gather_subobject_data_for_blueprint(bp)
 for h in handles:
  c=data_object(h)
  if isinstance(c,unreal.PointLightComponent) and not isinstance(c,unreal.SpotLightComponent):
   assert SUB.delete_subobject(handles[0],h,bp)==1
 handles=SUB.k2_gather_subobject_data_for_blueprint(bp)
 mesh=next(data_object(h) for h in handles if isinstance(data_object(h),unreal.StaticMeshComponent))
 spot=next(data_object(h) for h in handles if isinstance(data_object(h),unreal.SpotLightComponent))
 mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);setp(mesh,'cast_shadow',False)
 setp(spot,'intensity',22.0);setp(spot,'attenuation_radius',650.0)
 setp(spot,'inner_cone_angle',30.0);setp(spot,'outer_cone_angle',44.0)
 spot.set_relative_location(unreal.Vector(0,0,-80),False,True)
 spot.set_relative_rotation(unreal.Rotator(pitch=-90),False,True)
 bp.modify();unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
 return bp

def configure(a,s,source):
 a.modify();a.set_folder_path('Lighting/'+s['purpose'])
 a.set_editor_property('tags',['PurposeLightingV1',s['purpose'],'CeilingConfirmed' if s['ceiling_confirmed'] else 'PendingM03Ceiling']+[unreal.Name('Art_'+p) for p in s['paintings']])
 mesh=a.get_component_by_class(unreal.StaticMeshComponent);spot=a.get_component_by_class(unreal.SpotLightComponent)
 mesh.modify();spot.modify()
 for key,value in source.items():setp(spot,key,value)
 mesh.set_mobility(unreal.ComponentMobility.MOVABLE);spot.set_mobility(unreal.ComponentMobility.MOVABLE)
 m=unreal.load_asset('/Game/Assets/MapAssets/Showcase/Meshes/'+s['mesh']);b=m.get_bounding_box()
 scale=s['hang']/(b.max.z-b.min.z)
 mesh.set_static_mesh(m);mesh.set_relative_scale3d(unreal.Vector(scale,scale,scale))
 yaw=math.radians(s['yaw']);cx=(b.min.x+b.max.x)*scale/2;cy=(b.min.y+b.max.y)*scale/2
 mesh.set_relative_rotation(unreal.Rotator(yaw=s['yaw']),False,True)
 mesh.set_relative_location(unreal.Vector(-cx*math.cos(yaw)+cy*math.sin(yaw),-cx*math.sin(yaw)-cy*math.cos(yaw),-b.max.z*scale),False,True)
 mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);setp(mesh,'cast_shadow',False)
 pos=unreal.Vector(*s['location'])+unreal.Vector(0,0,-s['hang']-4);target=unreal.Vector(*s['target'])
 spot.set_world_location(pos,False,True);spot.set_world_rotation(unreal.MathLibrary.make_rot_from_x(target-pos),False,True)
 distance=(target-pos).length();outer=44.0
 if s['purpose']=='Artwork':
  n=unreal.Vector(*s['normal']);h=unreal.Vector(-n.y,n.x,0);direction=(target-pos)/distance;angles=[]
  for x in (-.5,.5):
   for z in (-.5,.5):
    ray=target+h*s['width']*x+unreal.Vector(0,0,s['height']*z)-pos
    angles.append(math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(ray/ray.length(),direction))))))
  outer=min(75,max(20,max(angles)+3))
 setp(spot,'inner_cone_angle',outer*.7);setp(spot,'outer_cone_angle',outer)
 setp(spot,'attenuation_radius',max(600,distance+250))
 setp(spot,'intensity',s['baseline_intensity']*PLAN['intensity_multiplier'])
 setp(spot,'cast_shadows',True);setp(spot,'volumetric_scattering_intensity',.15)
 setp(spot,'component_tags',['PurposeLightingV1',s['source_tag']])
 return dict(actor=s['label'],purpose=s['purpose'],mesh=s['mesh'],baseline=s['baseline_intensity'],intensity=spot.intensity,outer_cone=outer,location=s['location'],target=s['target'],paintings=s['paintings'],ceiling_confirmed=s['ceiling_confirmed'])

def run():
 for name in PLAN['maps']:backup('/Game/Maps/'+name,'.umap')
 bp=prepare_blueprint();report={}
 for name,row in PLAN['maps'].items():
  package='/Game/Maps/'+name;world=unreal.EditorLoadingAndSavingUtils.load_map(package);world.modify()
  actors=list(ACT.get_all_level_actors());old=[a for a in actors if a.get_class().get_name()=='BP_HeistLightFixture_C']
  sources={}
  for a in old:
   for c in a.get_components_by_class(unreal.LightComponent):
    tags=[str(t) for t in c.component_tags]
    for tag in tags:
     if tag.startswith('Source_'):
      values={}
      for key in LIGHT_PROPERTIES:
       try:values[key]=c.get_editor_property(key)
       except Exception:pass
      sources[tag]=values
  assert all(s['source_tag'] in sources for s in row['fixtures']),name
  result=[]
  for s in row['fixtures']:
   a=ACT.spawn_actor_from_class(bp.generated_class(),unreal.Vector(*s['location']))
   a.set_actor_label(s['label']);result.append(configure(a,s,sources[s['source_tag']]))
   assert len(a.get_components_by_class(unreal.LightComponent))==1
  for a in old:assert ACT.destroy_actor(a)
  ambient=[]
  for s in row['ambient']:
   a=next(a for a in actors if a.get_actor_label()==s['actor']);a.modify();c=a.light_component;c.modify()
   c.set_intensity(s['intensity']*.5);ambient.append(dict(actor=s['actor'],baseline=s['intensity'],intensity=c.intensity))
  assert unreal.EditorLoadingAndSavingUtils.save_map(world,package)
  report[name]=dict(fixtures=result,removed_groups=len(old),ambient=ambient)
  (OUT/'applied.json').write_text(json.dumps(report,indent=2))
 print('PURPOSE_LIGHTING_APPLIED')
 return report

if __name__=='__main__':run()
