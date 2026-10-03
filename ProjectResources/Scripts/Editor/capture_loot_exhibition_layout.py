"""Capture saved exhibition stations with temporary representative loot previews.

Editor Lit + Lumen SceneCapture, existing night exposure and player flashlight
template. Preview actors are destroyed; no map/package is saved. This is visual
placement evidence, not a random-spawn or natural multiplayer gameplay test.
"""
import hashlib
import json
import math
import os
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
BASE=ROOT/'Saved/Automation/LooseLootExhibition20261003'
OUT=BASE/'Captures';OUT.mkdir(parents=True,exist_ok=True)
PLAN=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json').read_text(encoding='utf-8'))
MODE=os.environ.get('MH_LOOT_CAPTURE_MODE','night')
TABLE=unreal.load_asset('/Game/Data/DataTable/DT_LootData')
ROWS={r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(TABLE))}
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
VIEWS=[(m,m['stations'][i],row) for m,i,row in zip(PLAN['maps'],[5,0,8],['Loot_Painting','Loot_GoldenVase','Loot_AncientSword'])]
VIEWS += [(m,m['stations'][10],'Loot_RoyalCrown') for m in PLAN['maps']]
STATE=dict(index=0,phase='load',time=0,callback=None,actors=[],rt=None)
REPORT=dict(context='Editor Lit / Lumen SceneCapture2D; temporary representative row preview, not natural PIE',captures=[],user_pie='NOT_TESTED')
if MODE=='base_color':
    REPORT['context']='Editor Base Color inspection; lighting ignored for placement review, temporary row preview, not gameplay brightness'
HASHES={m['code']:hashlib.sha256((ROOT/'Content/Maps'/(m['map']+'.umap')).read_bytes()).hexdigest() for m in PLAN['maps']}

def xyz(d):return unreal.Vector(d['X'],d['Y'],d['Z'])

def clean():
    for a in STATE['actors']:
        ACTORS.destroy_actor(a)
    STATE['actors']=[]
    if STATE['rt']:
        unreal.RenderingLibrary.release_render_target2d(STATE['rt']);STATE['rt']=None

def finish(error=None):
    clean()
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    REPORT['saved_maps_unchanged']=all(HASHES[m['code']]==hashlib.sha256((ROOT/'Content/Maps'/(m['map']+'.umap')).read_bytes()).hexdigest() for m in PLAN['maps'])
    REPORT['status']='ERROR' if error else 'PASS';REPORT['error']=error
    (OUT/('base-color-captures.json' if MODE=='base_color' else 'captures.json')).write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.log('LOOT_EXHIBITION_CAPTURE_'+REPORT['status'])

def tick(dt):
    if time.monotonic()-STATE['time']<4:return
    try:
        if STATE['index']==len(VIEWS):finish();return
        m,s,rowname=VIEWS[STATE['index']]
        name=m['code']+'_'+s['id']+('_BaseColor' if MODE=='base_color' else '')+'.png'
        if STATE['phase']=='load':
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+m['map'])
            world=EDITOR.get_editor_world()
            anchor=next(a for a in ACTORS.get_all_level_actors() if a.get_name()==s['spawn_actor'])
            preview=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,anchor.get_actor_location(),anchor.get_actor_rotation())
            STATE['actors'].append(preview)
            c=preview.static_mesh_component;r=ROWS[rowname];t=r['WorldVisualRelativeTransform'];q=t['Rotation']
            c.set_static_mesh(unreal.load_asset(r['WorldMesh']))
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            local=unreal.Transform(xyz(t['Translation']),unreal.Quat(q['X'],q['Y'],q['Z'],q['W']).rotator(),xyz(t['Scale3D']))
            preview.set_actor_transform(unreal.MathLibrary.compose_transforms(local,anchor.get_actor_transform()),False,False)
            for i,path in enumerate(r['WorldMaterials']):c.set_material(i,unreal.load_asset(path))
            n=s['approach_normal'];p=anchor.get_actor_location()
            location=unreal.Vector(p.x+n[0]*400,p.y+n[1]*400,s['floor_z']+160)
            rotation=unreal.Rotator(pitch=9,yaw=math.degrees(math.atan2(-n[1],-n[0])))
            capture=ACTORS.spawn_actor_from_class(unreal.SceneCapture2D,location,rotation);STATE['actors'].append(capture)
            cc=capture.capture_component2d
            rt=unreal.RenderingLibrary.create_render_target2d(world,1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
            STATE['rt']=rt
            capture_source=unreal.SceneCaptureSource.SCS_BASE_COLOR if MODE=='base_color' else unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
            for key,value in [('texture_target',rt),('capture_source',capture_source),('capture_every_frame',True),('always_persist_rendering_state',True),('fov_angle',90.0)]:cc.set_editor_property(key,value)
            pp=cc.get_editor_property('post_process_settings')
            pp.set_editor_property('override_dynamic_global_illumination_method',True);pp.set_editor_property('dynamic_global_illumination_method',unreal.DynamicGlobalIlluminationMethod.LUMEN)
            pp.set_editor_property('override_reflection_method',True);pp.set_editor_property('reflection_method',unreal.ReflectionMethod.LUMEN)
            cc.set_editor_property('post_process_settings',pp)
            torch=ACTORS.spawn_actor_from_class(unreal.SpotLight,location,rotation);STATE['actors'].append(torch)
            lib=unreal.SubobjectDataBlueprintFunctionLibrary;sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
            bp=unreal.load_asset('/Game/Blueprints/Player/BP_HeistPlayerCharacter')
            lights=[lib.get_object(lib.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
            source=next(c for c in lights if isinstance(c,unreal.SpotLightComponent) and c.component_has_tag('Flashlight'))
            light=torch.spot_light_component
            for k in ['intensity_units','intensity','attenuation_radius','inner_cone_angle','outer_cone_angle','indirect_lighting_intensity','use_temperature','temperature','cast_shadows','volumetric_scattering_intensity','use_inverse_squared_falloff']:
                light.set_editor_property(k,source.get_editor_property(k))
            light.set_visibility(True)
            cc.capture_scene()
            STATE.update(phase='capture',time=time.monotonic(),flashlight_cd=float(light.intensity),location=[location.x,location.y,location.z])
        else:
            unreal.RenderingLibrary.export_render_target(EDITOR.get_editor_world(),STATE['rt'],str(OUT),name)
            REPORT['captures'].append(dict(path=name,map=m['code'],spawn_actor=s['spawn_actor'],station=s['id'],row=rowname,context=REPORT['context'],flashlight_cd=0 if MODE=='base_color' else STATE['flashlight_cd'],camera=STATE['location']))
            clean();STATE.update(index=STATE['index']+1,phase='load',time=time.monotonic())
    except Exception:finish(traceback.format_exc())

STATE['callback']=unreal.register_slate_post_tick_callback(tick)
