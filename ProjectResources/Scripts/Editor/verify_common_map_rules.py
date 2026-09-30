"""Read back the saved common-rule migration and verify layout/material contracts."""
import json,math,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from audit_common_map_rules import snapshot,MAPS,OUT,ROOT
from refine_canvas_and_light_fixtures import painting_geometry

ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SUB=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB=unreal.SubobjectDataBlueprintFunctionLibrary


def verify_defaults():
    paths=['/Game/Blueprints/Environment/BP_HeistLight_'+n for n in ['DropCeiling_01a','Droplights_01a','Droplights_01b','Droplights_01c']]
    paths+=['/Game/Blueprints/World/Actors/Security/BP_SecurityCamera']
    for path in paths:
        bp=unreal.load_asset(path)
        objects=[LIB.get_object(LIB.get_data(h)) for h in SUB.k2_gather_subobject_data_for_blueprint(bp)]
        c=next(c for c in objects if isinstance(c,unreal.SpotLightComponent))
        assert c.intensity==(2. if 'SecurityCamera' in path else 4.)
        assert c.intensity_units==unreal.LightUnits.CANDELAS and c.use_inverse_squared_falloff
        if 'HeistLight' in path:
            assert all(not c.get_editor_property('affect_dynamic_indirect_lighting') for c in objects if isinstance(c,unreal.StaticMeshComponent))
    return paths


def near(a,b,tolerance=.002):
    return all(abs(x-y)<=tolerance for x,y in zip(a,b))


def main():
    result={'maps':{},'errors':[],'defaults':verify_defaults()}
    expected_counts=[(71,20,660),(73,20,1080),(60,12,192)]
    for name,(art_count,case_count,floor_count) in zip(MAPS,expected_counts):
        after=snapshot(name)
        (OUT/(name+'_after.json')).write_text(json.dumps(after,indent=2,default=str))
        before=json.loads((OUT/(name+'_before.json')).read_text())
        original={r['name']:r for r in before['actors']}; current={r['name']:r for r in after['actors']}
        removed=set(original)-set(current)
        allowed={'StaticMeshActor_25'} if name.startswith('M02') else {
            r['name'] for r in before['actors'] if any(c.get('mesh','').startswith('/Game/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_stone_tile_1_1.') for c in r['components']) and r['transform']['location'][2]==-2} if name.startswith('M03') else set()
        assert removed==allowed,(name,removed-allowed)
        for actor_name in set(original)&set(current):
            a,b=original[actor_name],current[actor_name]
            for k in ('location','scale','rotation'):assert near(a['transform'][k],b['transform'][k]),(name,actor_name,k)
            for key in ('case_id','artifact_id','protected_painting_case','laser_barrier','detection_range','detection_half_angle_degrees','detection_time','sweep_angle_degrees','sweep_speed_degrees'):
                assert a.get(key)==b.get(key),(name,actor_name,key)
            ca={c['name']:c for c in a['components']};cb={c['name']:c for c in b['components']}
            for c_name in set(ca)&set(cb):
                x,y=ca[c_name],cb[c_name]
                if 'mesh' in x:
                    assert x['mesh']==y['mesh'],(name,actor_name,c_name,'mesh')
                    for k in ('location','scale','rotation'):assert near(x['relative'][k],y['relative'][k]),(name,actor_name,c_name,k)
                    assert x['materials']==y['materials'],(name,actor_name,c_name,'materials')
                if 'box_extent' in x:assert x['box_extent']==y['box_extent']
        art=[];panels=[];pcgs=[];fixture_count=0;camera_count=0
        for a in ACT.get_all_level_actors():
            assert all(abs(v-math.trunc(v))<.001 for v in (a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z)),(name,a.get_name(),'location')
            for c in a.get_components_by_class(unreal.SceneComponent):
                s=c.get_relative_transform().scale3d
                assert all(abs(v*10-round(v*10))<.001 for v in (s.x,s.y,s.z)),(name,a.get_name(),c.get_name(),'scale')
            cls=a.get_class().get_name()
            if cls.startswith('BP_HeistLight_'):
                fixture_count+=1
                lights=a.get_components_by_class(unreal.SpotLightComponent)
                meshes=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
                assert len(lights)==len(meshes)==1
                assert abs(lights[0].intensity-4)<.001 and lights[0].intensity_units==unreal.LightUnits.CANDELAS
                assert not meshes[0].get_editor_property('affect_dynamic_indirect_lighting')
            elif cls=='BP_SecurityCamera_C':
                camera_count+=1;c=a.get_component_by_class(unreal.SpotLightComponent)
                assert abs(c.intensity-2)<.001 and c.intensity_units==unreal.LightUnits.CANDELAS
            elif isinstance(a,unreal.PostProcessVolume) and a.unbound:
                s=a.settings
                assert abs(s.ambient_cubemap_intensity-.005)<.00001 and s.ambient_cubemap
                assert s.auto_exposure_min_brightness==s.auto_exposure_max_brightness==1
                assert not s.lumen_final_gather_screen_traces
                assert s.auto_exposure_bias==0
                assert near((s.color_offset_shadows.x,s.color_offset_shadows.y,s.color_offset_shadows.z),(.0006,.0009,.0012),.000001)
                assert abs(s.film_toe-.53)<.00001 and abs(s.color_correction_shadows_max-.035)<.00001
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_'):
                    p=painting_geometry(c);art.append(p)
                    if isinstance(a,unreal.HeistPaintingDisplayCaseActor):
                        marker=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('SecurityPanelVisual'))
                        panels.append({'actor':a.get_name(),'center_z':marker.get_world_location().z,'frame_bottom':p['center'].z-p['height']/2,'collision':str(marker.get_collision_enabled())})
                        assert marker.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                        assert marker.get_world_location().z>=12-.001,'Marker intersects floor'
                if isinstance(c,unreal.InstancedStaticMeshComponent):
                    for i in range(c.get_instance_count()):
                        s=c.get_instance_transform(i,False).scale3d
                        assert all(abs(v*10-round(v*10))<.001 for v in (s.x,s.y,s.z))
            if a.get_component_by_class(unreal.PCGComponent):
                comp=a.get_component_by_class(unreal.PCGComponent)
                count=sum(c.get_instance_count() for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent))
                assert comp.generation_trigger==unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND and comp.generated
                pcgs.append({'actor':a.get_actor_label(),'instances':count,'seed':comp.seed})
        assert len(art)==art_count and len(panels)==case_count and sum(p['instances'] for p in pcgs)==floor_count
        if name.startswith('M01'):
            old_instances={c['mesh']:c['instances'] for r in before['actors'] for c in r['components'] if 'instances' in c}
            new_instances={c['mesh']:c['instances'] for r in after['actors'] for c in r['components'] if 'instances' in c}
            assert old_instances==new_instances,'M01 saved random floor changed'
        result['maps'][name]=dict(artworks=len(art),cases=len(panels),fixtures=fixture_count,cctv=camera_count,
            pcg=pcgs,panels=panels,actor_transforms_preserved=True,architecture_meshes_preserved=True,
            identities_preserved=True,canvas_materials_preserved=True)
    baseline=json.loads((OUT/'applied.json').read_text())['asset_inventory_before']
    current=sorted(str(p.relative_to(ROOT/'Content')) for p in (ROOT/'Content').rglob('*') if p.suffix in ('.uasset','.umap'))
    result['new_assets']=sorted(set(current)-set(baseline))
    assert not result['new_assets'],'Unexpected new content asset'
    (OUT/'verified.json').write_text(json.dumps(result,indent=2))
    unreal.log('HEIST_COMMON_RULES_VERIFIED')

if __name__=='__main__':main()
