"""Canvas geometry and material helpers. Room light consolidation is retired.

Run through Unreal Editor Python. Keeps a pre-change backup and JSON evidence in
Saved/CanvasLightRefinement. Does not regenerate architecture or floor PCG.
"""
import json, math, shutil
from pathlib import Path
import unreal
from apply_showcase_canvas import META, ASSET_DIR, MAPS

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/CanvasLightRefinement'
OUT.mkdir(exist_ok=True)
ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SUB=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
EL=unreal.EditorAssetLibrary
ML=unreal.MaterialEditingLibrary
BL=unreal.BlueprintEditorLibrary
BP_PATH='/Game/Blueprints/Environment/BP_HeistLightFixture'
FIXTURES=['SM_Drop_Ceiling_01a']+['SM_F_Ceiling_Droplights_01'+c for c in 'abc']
LIGHT_PROPERTIES=('intensity','light_color','attenuation_radius','intensity_units',
 'use_inverse_squared_falloff','light_falloff_exponent','inner_cone_angle','outer_cone_angle',
 'source_radius','soft_source_radius','source_length','cast_shadows','cast_static_shadows',
 'cast_dynamic_shadows','cast_translucent_shadows','affect_translucent_lighting',
 'affect_dynamic_indirect_lighting','affect_global_illumination','indirect_lighting_intensity',
 'volumetric_scattering_intensity','use_temperature','temperature','specular_scale',
 'diffuse_scale','shadow_bias','shadow_slope_bias','shadow_sharpen','shadow_resolution_scale',
 'contact_shadow_length','lighting_channels','ies_texture','use_ies_brightness',
 'ies_brightness_scale','light_function_material','light_function_scale',
 'light_function_fade_distance','disabled_brightness','visible','hidden_in_game')

def backup(package):
 for ext in ('.umap','.uasset'):
  rel=package.removeprefix('/Game/')+ext;src=ROOT/'Content'/rel;dst=OUT/'Backup'/rel
  if src.exists() and not dst.exists():
   dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)

def data_object(handle):
 return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))

def component_handle(handles,typ):
 return next(h for h in handles if isinstance(data_object(h),typ))

def material_contain():
 master=unreal.load_asset(ASSET_DIR+'/M_HeistCanvas');backup(ASSET_DIR+'/M_HeistCanvas')
 visited=set();changed=0
 def walk(node):
  nonlocal changed
  if not node or node.get_path_name() in visited:return
  visited.add(node.get_path_name())
  if isinstance(node,unreal.MaterialExpressionCustom) and 'CanvasAspect/max(PaintingAspect' in node.get_editor_property('code'):
   node.modify();node.set_editor_property('code','float a=CanvasAspect/max(PaintingAspect,0.001); return (P-0.5)*float2(max(a,1),max(1/a,1))+0.5;');changed+=1
  if isinstance(node,unreal.MaterialExpressionConstant3Vector) and abs(node.constant.r-.94)<.001 and abs(node.constant.g-.92)<.001:
   node.modify();node.set_editor_property('constant',unreal.LinearColor(0,0,0,1))
  for child in ML.get_inputs_for_material_expression(master,node):walk(child)
 walk(ML.get_material_property_input_node(master,unreal.MaterialProperty.MP_BASE_COLOR))
 assert changed==1,changed
 master.modify();ML.recompile_material(master);EL.save_loaded_asset(master)
 default_tex=unreal.load_asset('/Game/Data/Forgery/Textures/M01/T_Forgery_M01_Portrait_01')
 repaired=[]
 for p in EL.list_assets(ASSET_DIR,True,False):
  mi=unreal.load_asset(p)
  if not isinstance(mi,unreal.MaterialInstanceConstant):continue
  backup(p.split('.')[0]);mi.modify()
  ML.set_material_instance_scalar_parameter_value(mi,'CanvasEllipse',0.0)
  tex=ML.get_material_instance_texture_parameter_value(mi,'PaintingTexture')
  if not tex or 'WhiteSquare' in tex.get_name():
   ML.set_material_instance_texture_parameter_value(mi,'PaintingTexture',default_tex);repaired.append(p)
  EL.save_loaded_asset(mi)
 return repaired

def painting_geometry(c):
 i=int(c.static_mesh.get_name().split('_')[-1][:2])-1;m=META[i];axis=m['thin_axis'];wide=1-axis
 t=c.get_world_transform();s=c.get_world_scale()
 local=unreal.Vector(*[(x+y)/2 for x,y in zip(m['front_min'],m['front_max'])])
 center=unreal.MathLibrary.transform_location(t,local)
 normal=c.get_forward_vector() if axis==0 else c.get_right_vector()
 horizontal=c.get_right_vector() if axis==0 else c.get_forward_vector()
 scales=[s.x,s.y,s.z]
 return dict(comp=c,center=center,normal=normal,horizontal=horizontal,width=(m['max'][wide]-m['min'][wide])*abs(scales[wide]),height=(m['max'][2]-m['min'][2])*abs(s.z),local=local)

def enlarge_paintings(actors):
 items=[]
 for a in actors:
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_'):
    item=painting_geometry(c);item['actor']=a;items.append(item)
 changed=[];limited=[]
 for p in items:
  if 'CanvasSizeRefined' in [str(t) for t in p['actor'].tags]:continue
  w,h=p['width'],p['height'];factor=min(1.65,max(1,160/max(w,h),100/min(w,h)))
  requested=factor
  # Keep existing centers, wall plane and at least 12 cm between neighboring frames.
  for q in items:
   if p is q:continue
   d=q['center']-p['center']
   if abs(unreal.MathLibrary.dot_vector_vector(p['normal'],q['normal']))<.99 or abs(unreal.MathLibrary.dot_vector_vector(d,p['normal']))>20:continue
   horizontal=abs(unreal.MathLibrary.dot_vector_vector(d,p['horizontal']));vertical=abs(d.z)
   if vertical<(h*factor+q['height'])/2+12:
    limit=(2*(horizontal-12)-q['width'])/w
    if limit>=1:factor=min(factor,limit)
    elif horizontal<(w+q['width'])/2+12:factor=1
  factor=max(1,min(factor,2*max(p['center'].z-60,0)/h))
  a=p['actor'];c=p['comp'];a.modify();c.modify()
  if factor>1.001:
   t=c.get_world_transform();t.scale3d=t.scale3d*factor
   t.translation=t.translation+(p['center']-unreal.MathLibrary.transform_location(t,p['local']))
   c.set_world_transform(t,False,True)
   if isinstance(a,unreal.StaticMeshActor):
    v=a.get_actor_location();a.set_actor_location(unreal.Vector(math.trunc(v.x),math.trunc(v.y),math.trunc(v.z)),False,True)
   changed.append({'actor':a.get_actor_label(),'factor':factor,'old_size':[w,h],'new_size':[w*factor,h*factor]})
   p['width']*=factor;p['height']*=factor
  if factor+0.001<requested:limited.append(a.get_actor_label())
  a.set_editor_property('tags',list(a.tags)+['CanvasSizeRefined'])
 return {'enlarged':changed,'spacing_limited':limited,'total':len(items)}

def run():
 raise RuntimeError("Retired room light grouping. Use apply_purpose_lighting.py; canvas changes are already applied.")

if __name__=='__main__':run()
