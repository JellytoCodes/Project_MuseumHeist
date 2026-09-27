"""Read-only verification of saved authored maps and their painting/floor assets."""
import unreal,json
from pathlib import Path
out={};saved=Path(unreal.Paths.project_saved_dir())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in ['M01_ClassicalPrototype','M02_MoonlitPrototype','M03_GlasshousePrototype']:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+name)
 aa=list(actors.get_all_level_actors());cases=[a for a in aa if isinstance(a,unreal.HeistPaintingDisplayCaseActor)]
 row={'actors':len(aa),'cases':len(cases),'fractional_positions':[],'case_errors':[],'canvas_variants':{},'case_samples':[],'pcg':[]}
 for a in aa:
  p=a.get_actor_location()
  if any(abs(v-round(v))>.0001 for v in [p.x,p.y,p.z]):row['fractional_positions'].append((a.get_actor_label(),str(p)))
  if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
   meshes=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
   if len(meshes)!=1 or not meshes[0].static_mesh.get_name().startswith('SM_Canvas_Painting_'):row['case_errors'].append(a.get_actor_label())
   else:
    c=meshes[0];m=c.static_mesh.get_name();row['canvas_variants'][m]=row['canvas_variants'].get(m,0)+1
    row['case_samples'].append({'actor':a.get_actor_label(),'location':str(c.get_world_location()),'rotation':str(c.get_world_rotation()),'scale':str(c.get_world_scale()),'material':str(c.get_material(0)),'original':str(a.get_editor_property('original_painting_material')),'replica':str(a.get_editor_property('replica_painting_material'))})
  pcg=a.get_component_by_class(unreal.PCGComponent)
  if pcg:
   instances=a.get_components_by_class(unreal.InstancedStaticMeshComponent)
   row['pcg'].append({'actor':a.get_actor_label(),'graph':str(pcg.get_graph()),'generated':pcg.get_editor_property('generated'),'count':sum(c.get_instance_count() for c in instances),'collision':[(c.static_mesh.get_name(),str(c.get_collision_enabled()),str(c.get_collision_profile_name())) for c in instances]})
 out[name]=row
(saved/'CanvasFloorVerification.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print('CANVAS_FLOOR_RELOAD',json.dumps({k:{n:v[n] for n in ['actors','cases','fractional_positions','case_errors','canvas_variants','pcg']} for k,v in out.items()}))
