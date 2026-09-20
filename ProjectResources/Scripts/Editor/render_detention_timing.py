"""Transient populated timing prompt render; no runtime or input PASS is inferred."""
import json
import time
import traceback
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/Automation/DetentionDoor/UI'
OUT.mkdir(parents=True,exist_ok=True)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE=dict(phase='start',time=time.monotonic(),callback=None,items=[])
klass=unreal.load_class(None,'/Game/Blueprints/UI/HUD/WBP_InteractionPrompt.WBP_InteractionPrompt_C')
cdo=unreal.get_default_object(klass); original_tick=cdo.get_editor_property('tick_frequency')
cdo.set_editor_property('tick_frequency',unreal.WidgetTickFrequency.NEVER)

def finish(error=None):
    cdo.set_editor_property('tick_frequency',original_tick)
    (OUT/'render.json').write_text(json.dumps(dict(status='FAIL' if error else 'PASS',error=error,visual_fixture=True)),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(_):
    if time.monotonic()-STATE['time']<4:return
    try:
        if STATE['phase']=='start':
            level.editor_play_simulate(); STATE.update(phase='create',time=time.monotonic());return
        if STATE['phase']=='create':
            world=unreal.EditorLevelLibrary.get_game_world()
            gameplay=unreal.get_default_object(unreal.GameplayStatics)
            for index,width in enumerate((88,64,40)):
                actor=gameplay.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.Actor.static_class(),unreal.Transform(),unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                gameplay.call_method('FinishSpawningActor',(actor,unreal.Transform(),unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                component=actor.call_method('AddComponentByClass',(unreal.WidgetComponent.static_class(),False,unreal.Transform(),False))
                component.set_tick_when_offscreen(True);component.set_draw_size(unreal.Vector2D(1024,256))
                component.set_background_color(unreal.LinearColor(.016,.02,.025,1))
                widget=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,klass,None))
                component.set_widget(widget)
                widgets={w.get_name():w for w in unreal.ObjectIterator(unreal.Widget) if w.get_path_name().startswith(widget.get_path_name()+'.') or w.get_path_name().startswith(widget.get_path_name()+':')}
                # Static visual fixture: detach only this transient presentation binding.
                widget.set_editor_property('detention_timing_container',None)
                widgets['TargetText'].set_text('구금실 철창문')
                widgets['AvailabilityText'].set_text('걸쇠 %d/3 · 초록 구간에서 [E]'%index)
                widgets['InteractionPromptContainer'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
                widgets['DetentionTimingContainer'].set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
                widgets['DetentionTimingWindow'].slot.set_size(unreal.Vector2D(width,16))
                widgets['DetentionTimingWindow'].slot.set_position(unreal.Vector2D(224-width/2,4))
                widgets['DetentionTimingCursor'].slot.set_position(unreal.Vector2D(212,0))
                component.request_render_update();STATE['items'].append((component,index,widget,widgets))
            STATE.update(phase='export',time=time.monotonic());return
        if STATE['phase']=='export':
            world=unreal.EditorLevelLibrary.get_game_world()
            for component,index,widget,widgets in STATE['items']:
                (OUT/('state%d.json'%index)).write_text(json.dumps({n:dict(visibility=str(w.get_visibility()),size=str(w.get_cached_geometry()),parent=str(w.get_parent()),brush=str(w.get_editor_property('brush')) if isinstance(w,unreal.Image) else '') for n,w in widgets.items() if 'Detention' in n},indent=2),encoding='utf-8')
                target=component.get_render_target();assert target
                unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'Latch%d.exr'%(index+1))
            unreal.EditorLevelLibrary.editor_end_play()
            STATE.update(phase='finish',time=time.monotonic());return
        finish()
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.EditorLevelLibrary.editor_end_play();finish(traceback.format_exc())

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
