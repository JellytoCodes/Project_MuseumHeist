"""Physics overlap checks with the real player capsule and saved painting shells.

Uses a transient capsule probe in SIE. No player input or gameplay success is inferred.
"""
import json
import time
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
MAPS=['M01_ClassicalPrototype','M02_MoonlitPrototype','M03_GlasshousePrototype']
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state={'i':0,'phase':'load','time':0,'callback':None,'results':[]}


def spawn(world,klass):
    gs=unreal.get_default_object(unreal.GameplayStatics)
    a=gs.call_method('BeginDeferredActorSpawnFromClass',(world,klass,unreal.Transform(),unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    return gs.call_method('FinishSpawningActor',(a,unreal.Transform(),unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))


def tick(dt):
    if time.monotonic()-state['time']<3:return
    if state['i']==len(MAPS):
        unreal.unregister_slate_post_tick_callback(state['callback'])
        result={'status':'PASS' if all(r['pass'] for r in state['results']) else 'FAIL','checks':state['results'],'user_pie':'NOT_TESTED'}
        (ROOT/'Saved/Logs/PaintingBoxPhysics.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        return
    if state['phase']=='load':
        unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+MAPS[state['i']]);levels.editor_play_simulate()
        state.update(phase='check',time=time.monotonic());return
    if state['phase']=='check':
        world=editor.get_game_world()
        mode=unreal.GameplayStatics.get_game_mode(world)
        template=unreal.get_default_object(mode.get_editor_property('default_pawn_class')).get_component_by_class(unreal.CapsuleComponent)
        radius=template.get_scaled_capsule_radius();half=template.get_scaled_capsule_half_height()
        cases=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistPaintingDisplayCaseActor)
        probe=spawn(world,unreal.Actor.static_class())
        cap=probe.call_method('AddComponentByClass',(unreal.CapsuleComponent.static_class(),False,unreal.Transform(),False))
        cap.set_capsule_size(radius,half,True)
        cap.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        cap.set_collision_object_type(unreal.CollisionChannel.ECC_HEIST_PLAYER)
        overlap_response=cases[0].get_component_by_class(unreal.BoxComponent).get_collision_response_to_channel(unreal.CollisionChannel.ECC_HEIST_PLAYER)
        cap.set_collision_response_to_all_channels(overlap_response)
        cap.set_editor_property('generate_overlap_events',True)
        for a in cases:
            box=a.get_component_by_class(unreal.BoxComponent)
            normal=-a.get_actor_forward_vector();tangent=a.get_actor_right_vector()
            for name,depth,side,expected in [('front',70,0,True),('front_edge',130,0,True),('far',180,0,False),('side',70,120,False),('behind_wall',-60,0,False),('corner',148,78,True)]:
                pos=a.get_actor_location()+normal*depth+tangent*side;pos.z=half+3
                probe.set_actor_location(pos,False,True)
                overlap=box.is_overlapping_component(cap)
                state['results'].append(dict(map=MAPS[state['i']],case=a.get_name(),sample=name,expected=expected,overlap=overlap,pass_=overlap==expected))
                state['results'][-1]['pass']=state['results'][-1].pop('pass_')
        probe.destroy_actor();levels.editor_request_end_play();state.update(phase='end',time=time.monotonic());return
    if state['phase']=='end' and editor.get_game_world() is None:
        state.update(i=state['i']+1,phase='load',time=time.monotonic())


def safe_tick(dt):
    try:
        tick(dt)
    except Exception:
        import traceback
        unreal.unregister_slate_post_tick_callback(state['callback'])
        levels.editor_request_end_play()
        (ROOT/'Saved/Logs/PaintingBoxPhysics.json').write_text(json.dumps({'status':'ERROR','error':traceback.format_exc()}),encoding='utf-8')
        unreal.log_error(traceback.format_exc())


state['callback']=unreal.register_slate_post_tick_callback(safe_tick)
