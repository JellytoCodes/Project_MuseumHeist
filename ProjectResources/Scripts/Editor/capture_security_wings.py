"""Read-only eye-level viewport captures of the saved security branches."""
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Screenshots/SecurityWings'
OUT.mkdir(parents=True,exist_ok=True)
PLANS=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
MAPS={'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
VIEWS=[(m['id'],str(i),p,yaw) for m in PLANS for i,(p,yaw) in enumerate(m['security_wing']['views'])]
STATE=dict(index=0,phase='load',time=0,map=None,busy=False,callback=None)
REPORT=dict(captures=[])

def finish(error=None):
    REPORT.update(status='FAIL' if error else 'PASS',error=error)
    (OUT/'audit.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    unreal.log_warning('MH_SECURITY_CAPTURE='+REPORT['status'])
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_):
    if STATE['busy'] or time.monotonic()-STATE['time']<4:return
    STATE['busy']=True
    try:
        if STATE['index']==len(VIEWS):finish();return
        code,name,p,yaw=VIEWS[STATE['index']]
        path=OUT/(code+'_'+name+'.png')
        if STATE['phase']=='load':
            if STATE['map']!=code:
                if not unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+MAPS[code]):raise RuntimeError('Load failed: '+code)
                STATE['map']=code
            position=unreal.Vector(*[v*100 for v in p]);rotation=unreal.Rotator(yaw=yaw)
            EDITOR.set_level_viewport_camera_info(position,rotation)
            camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,position,rotation)
            camera.camera_component.set_editor_property('field_of_view',90.0)
            task=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(path),camera=camera,delay=3.0)
            if not task.is_valid_task():raise RuntimeError('Capture not scheduled')
            STATE.update(camera=camera,task=task,phase='capture',time=time.monotonic())
        else:
            if not STATE['task'].is_task_done():
                if time.monotonic()-STATE['time']>60:raise RuntimeError('Capture timeout')
                return
            if not path.is_file():raise RuntimeError('Missing '+str(path))
            ACTORS.destroy_actor(STATE['camera'])
            REPORT['captures'].append(str(path))
            STATE.update(index=STATE['index']+1,phase='load',time=time.monotonic())
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
