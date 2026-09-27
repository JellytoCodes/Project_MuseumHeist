"""Run in a separate full Editor via -ExecCmds="py ...", allowing PCG to tick.

The old tiles are removed only after generated instances match their exact grid.
"""
import unreal,json,time,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'FloorPCG';OUT.mkdir(exist_ok=True)
ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/M01_ClassicalPrototype')
actors=ACT.get_all_level_actors()
old=[a for a in actors if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Floor_Panel_')]
assert len(old)==660,len(old)
expected={(round(a.get_actor_location().x),round(a.get_actor_location().y),round(a.get_actor_location().z)) for a in old}
actor=ACT.spawn_actor_from_class(unreal.load_class(None,'/Game/Blueprints/Environment/BP_FloorTilePCG.BP_FloorTilePCG_C'),unreal.Vector(10,0,0))
actor.set_actor_label('M01_FloorTiles_PCG');actor.set_folder_path('Environment/Floor')
comp=actor.get_component_by_class(unreal.PCGComponent)
assert comp
assert comp.get_graph(),'Blueprint lost its PCG graph'
actor.modify()
comp.set_editor_property('seed',92701)
# Editing a Blueprint component can rerun construction and replace the object.
comp=actor.get_component_by_class(unreal.PCGComponent)
if not comp.get_graph():raise RuntimeError('Blueprint PCG graph missing after reconstruction')
unreal.SystemLibrary.execute_console_command(actor,'Slate.bAllowThrottling 0')
unreal.SystemLibrary.execute_console_command(actor,'t.MaxFPS 30')
comp.generate_local(True)
(OUT/'started.json').write_text(json.dumps({'graph':str(comp.get_graph()),'actor':actor.get_path_name()}))
start=time.monotonic();phase=0;first=None

def instances():
    rows=[]
    for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,True);p=t.translation
            rows.append((round(p.x,4),round(p.y,4),round(p.z,4),c.static_mesh.get_name()))
    return sorted(rows)

def finish(error=None):
    unreal.unregister_slate_post_tick_callback(handle)
    if error:(OUT/'error.txt').write_text(error)
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    global phase,first,comp
    if phase==2:return
    try:
        comp=actor.get_component_by_class(unreal.PCGComponent)
        if not (OUT/'ticked.txt').exists():(OUT/'ticked.txt').write_text('Slate tick reached')
        if time.monotonic()-start>120:raise RuntimeError('PCG generation timeout')
        if comp.get_editor_property('generation_in_progress') or not comp.get_editor_property('generated'):return
        rows=instances()
        assert len(rows)==660,('Instance count',len(rows))
        actual={(x,y,z) for x,y,z,_ in rows}
        assert actual==expected,('Grid mismatch',list(actual-expected)[:4],list(expected-actual)[:4])
        if phase==0:
            first=rows;phase=1;comp.generate_local(True);return
        assert rows==first,'Seed regeneration changed mesh selection'
        phase=2
        for a in old:assert ACT.destroy_actor(a)
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/M01_ClassicalPrototype'):
            raise RuntimeError('PCG map save failed')
        counts={m:sum(1 for r in rows if r[3]==m) for m in sorted({r[3] for r in rows})}
        (OUT/'result.json').write_text(json.dumps({'count':len(rows),'variants':counts,'seed':92701,'deterministic':True,'grid_matches':True,'rows':rows},indent=2))
        finish()
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
