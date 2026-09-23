"""Read-only M01 light preview; colors are visual fixtures, not player-input evidence."""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()).resolve() / 'Screenshots/CCTVLight'
out.mkdir(parents=True, exist_ok=True)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/M01_ClassicalPrototype')
cameras = sorted([a for a in actors.get_all_level_actors() if isinstance(a, unreal.HeistSecurityCameraActor)],
                 key=lambda a: a.get_actor_label())
cctv = cameras[0]
sensor = cctv.get_editor_property('sensor_origin_component')
light = cctv.get_component_by_class(unreal.SpotLightComponent)
origin, forward, right = sensor.get_world_location(), sensor.get_forward_vector(), sensor.get_right_vector()
position = origin + forward * 180 + right * 180
position.z = 170
target = origin + forward * 900
target.z = 40
rotation = unreal.MathLibrary.find_look_at_rotation(position, target)
view = actors.spawn_actor_from_class(unreal.CameraActor, position, rotation)
view.camera_component.set_editor_property('field_of_view', 90.0)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position, rotation)
state = dict(index=0, phase='prepare', time=time.monotonic(), busy=False)
report = dict(camera=cctv.get_actor_label(), intensity=light.get_editor_property('intensity'),
              radius=light.get_editor_property('attenuation_radius'),
              outer_angle=light.get_editor_property('outer_cone_angle'), visual_fixture=True, captures=[])
colors = [('idle', cctv.get_editor_property('idle_light_color')),
          ('detected', cctv.get_editor_property('alert_light_color'))]

def finish(error=None):
    report['error'] = error
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(state['handle'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(_):
    if state['busy'] or time.monotonic()-state['time'] < 8:
        return
    state['busy'] = True
    try:
        if state['index'] == len(colors):
            finish()
            return
        name, color = colors[state['index']]
        path = out/(name+'.png')
        if state['phase'] == 'prepare':
            light.set_light_color(color)
            state.update(phase='capture', time=time.monotonic())
        elif state['phase'] == 'capture':
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, str(path), camera=view, delay=2.0)
            state.update(phase='wait', time=time.monotonic())
        elif state['task'].is_task_done():
            assert path.is_file(), str(path)
            report['captures'].append(str(path))
            state.update(index=state['index']+1, phase='prepare', time=time.monotonic())
        elif time.monotonic()-state['time'] > 90:
            raise RuntimeError('Screenshot timed out')
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['handle'] = unreal.register_slate_post_tick_callback(tick)
