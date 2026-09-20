"""Read-only saved-map geometry/navigation audit and optional eye-level captures."""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Automation/DetentionDoor/Maps'
OUT.mkdir(parents=True,exist_ok=True)
PLANS=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
MAPS={'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
STATE=dict(index=0,phase='load',time=0,busy=False,callback=None)
REPORT=dict(maps=[],captures=[])

def audit(plan):
    code=plan['id']; world=EDITOR.get_editor_world(); actors=ACTORS.get_all_level_actors()
    doors=[a for a in actors if isinstance(a,unreal.HeistDetentionDoorActor)]
    assert len(doors)==1, 'Exactly one cell door required'
    door=doors[0]; bounds=door.get_editor_property('cell_bounds')
    extent=bounds.get_unscaled_box_extent(); center=bounds.get_world_location()
    def contained(p):return all(abs(a-b)<=e for a,b,e in zip((p.x,p.y,p.z),(center.x,center.y,center.z),(extent.x,extent.y,extent.z)))
    spawns=[a for a in actors if unreal.Name('HeistDetentionSpawn') in a.tags]
    evidence=[a for a in actors if a.get_actor_label().startswith('LDV2_'+code+'_Evidence_')]
    assert len(spawns)==4 and all(contained(a.get_actor_location()) for a in spawns)
    assert len(evidence)==25 and all(not contained(a.get_actor_location()) for a in evidence)
    assert door.get_editor_property('door_blocker').get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    parts=door.get_components_by_class(unreal.StaticMeshComponent)
    assert len([p for p in parts if p.static_mesh])==16, 'Door shell bars/rails/lock incomplete'
    guard=[a for a in actors if unreal.Name('HeistDetentionPatrol') in a.tags]
    assert len(guard)==1
    route=guard[0].get_component_by_class(unreal.HeistPatrolPathComponent).get_editor_property('patrol_route_id')
    points=sorted([a for a in actors if isinstance(a,unreal.HeistGuardWaypoint) and a.get_editor_property('patrol_route_id')==route],key=lambda a:a.get_editor_property('patrol_order'))
    assert all(not contained(p.get_actor_location()) for p in points), 'Guard path enters cell'
    assert points[-1].get_editor_property('wait_duration_override')==2.5
    lengths=[]
    for a,b in zip(points,points[1:]):
        path=unreal.NavigationSystemV1.find_path_to_location_synchronously(world,a.get_actor_location(),b.get_actor_location())
        assert path and path.is_valid() and not path.is_partial(), 'Broken patrol navigation: '+a.get_actor_label()
        poly=path.path_points
        assert all(not contained(p) for p in poly), 'Navigation detours through locked cell'
        lengths.append(sum((v-u).length() for u,v in zip(poly,poly[1:]))/100)
    result=dict(map=code,detention_spawns=len(spawns),evidence_anchors=len(evidence),door_mesh_parts=16,guard=guard[0].get_actor_label(),waypoints=len(points),navigation_round_trip_m=round(sum(lengths)*2,2),estimated_seconds=round(sum(lengths)*2/2.5+2.5+.4*(2*len(points)-3),2),cell_pause_seconds=2.5)
    REPORT['maps'].append(result)
    unreal.log_warning('MH_DETENTION_AUDIT='+json.dumps(result))

def finish(error=None):
    REPORT.update(status='FAIL' if error else 'PASS',error=error)
    (OUT/'audit.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    unreal.log_warning('MH_DETENTION_MAPS='+REPORT['status'])
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(_):
    if STATE['busy'] or time.monotonic()-STATE['time']<4:return
    STATE['busy']=True
    try:
        if STATE['index']==len(PLANS):finish();return
        plan=PLANS[STATE['index']]; code=plan['id']
        if STATE['phase']=='load':
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+MAPS[code])
            STATE.update(phase='audit',time=time.monotonic());return
        if STATE['phase']=='audit':
            audit(plan)
            x,y=plan['security_wing']['opening']
            position=unreal.Vector((x-1.2)*100,(y+1.1)*100,170)
            rotation=unreal.Rotator(yaw=-42)
            EDITOR.set_level_viewport_camera_info(position,rotation)
            camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,position,rotation)
            camera.set_actor_location(position,False,False)
            camera.set_actor_rotation(rotation,False)
            camera.camera_component.set_editor_property('field_of_view',90)
            path=OUT/(code+'_cell.png')
            task=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(path),camera=camera,delay=3)
            assert task.is_valid_task()
            STATE.update(phase='capture',camera=camera,task=task,path=path,time=time.monotonic());return
        if not STATE['task'].is_task_done():
            if time.monotonic()-STATE['time']>60:raise RuntimeError('Screenshot timeout')
            return
        assert STATE['path'].is_file()
        REPORT['captures'].append(str(STATE['path']))
        ACTORS.destroy_actor(STATE['camera'])
        STATE.update(index=STATE['index']+1,phase='load',time=time.monotonic())
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
