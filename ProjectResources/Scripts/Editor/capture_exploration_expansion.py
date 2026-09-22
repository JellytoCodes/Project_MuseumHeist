"""Read-only eye-level captures of the expanded first exhibition rooms."""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
_, SWITCHES, PARAMS=unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
PARAMS={str(k).lower():str(v) for k,v in PARAMS.items()}
CIRCULATION='museumcapturecirculation' in {str(s).lower() for s in SWITCHES}
OUT=Path(PARAMS.get('museumcapturedirectory',str(ROOT/'Saved/Automation/ExplorationExpansion/Views')))
OUT.mkdir(parents=True,exist_ok=True)
PLANS=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
MAPS={'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
ROOMS={'M01':'N1','M02':'W1','M03':'N1'}
EDITOR=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STATE=dict(index=0,phase='load',time=0,busy=False,callback=None)
REPORT=dict(captures=[])


def finish(error=None):
    REPORT.update(status='FAIL' if error else 'PASS',error=error)
    (OUT/'capture.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(_):
    if STATE['busy'] or time.monotonic()-STATE['time']<4:return
    STATE['busy']=True
    try:
        if STATE['index']==len(PLANS):finish();return
        plan=PLANS[STATE['index']];code=plan['id'];path=OUT/(code+'.png')
        if STATE['phase']=='load':
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+MAPS[code])
            if CIRCULATION:
                view={'M01':([-34,-7],[-34,6]),'M02':([-18,-10],[-18,19]),'M03':([-43,-5.7],[-43,8])}[code]
                position=unreal.Vector(view[0][0]*100,view[0][1]*100,170)
                target=view[1]
            else:
                room=plan['rooms'][ROOMS[code]];x0,y0,x1,y1=room['bounds']
                position=unreal.Vector((x1-4)*100,(y0+4)*100,170)
                target=next(p for p in plan['paintings'] if p['active'] and p['room']==room['id'])['xy']
            yaw=math.degrees(math.atan2(target[1]*100-position.y,target[0]*100-position.x))
            rotation=unreal.Rotator(yaw=yaw)
            EDITOR.set_level_viewport_camera_info(position,rotation)
            camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,position,rotation)
            camera.camera_component.set_editor_property('field_of_view',90)
            task=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(path),camera=camera,delay=4)
            assert task.is_valid_task()
            STATE.update(camera=camera,task=task,phase='capture',time=time.monotonic())
        else:
            if not STATE['task'].is_task_done():
                if time.monotonic()-STATE['time']>90:raise RuntimeError('Capture timeout')
                return
            assert path.exists()
            ACTORS.destroy_actor(STATE['camera'])
            REPORT['captures'].append(str(path))
            STATE.update(index=STATE['index']+1,phase='load',time=time.monotonic())
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
