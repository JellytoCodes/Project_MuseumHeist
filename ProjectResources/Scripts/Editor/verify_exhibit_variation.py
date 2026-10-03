"""Two transient SIE assignments per authored map. No saving or player E input.

Checks fixed world transforms, visible inactive artwork, active template
uniqueness, regional selection, laser links and active/decorative loot counts.
Authority/race/timing paths are checked separately by C++ Automation.
"""
import hashlib
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Automation/ExhibitVariation20261003'
MAPS=['M01_ClassicalPrototype','M02_MoonlitPrototype','M03_GlasshousePrototype']
EDITOR=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LEVELS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACT=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STATE=dict(index=0,phase='load',since=time.monotonic(),start=time.monotonic(),callback=None,saved=None)
REPORT=dict(status='RUNNING',scope='TransientSIE_ServerAssignment_TwoRunsPerMap',user_pie='NOT_TESTED',multiplayer='NOT_TESTED',runs=[])
HASHES={name:hashlib.sha256((ROOT/'Content/Maps'/(name+'.umap')).read_bytes()).hexdigest() for name in MAPS}

def xyz(v): return [float(getattr(v,k)) for k in 'xyz']

def transform(t):
    q=t.rotation
    return [t.translation.x,t.translation.y,t.translation.z,t.scale3d.x,t.scale3d.y,t.scale3d.z,q.x,q.y,q.z,q.w]

def close(a,b):
    return max(abs(x-y) for x,y in zip(a[:6],b[:6]))<.002 and abs(abs(sum(x*y for x,y in zip(a[6:],b[6:])))-1)<.00001

def painting(a):
    return next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OriginalVisualComponent')

def signature(a):
    return dict(actor=transform(a.get_actor_transform()),canvas=transform(painting(a).get_world_transform()),mesh=painting(a).static_mesh.get_path_name(),artifact=str(a.get_editor_property('target_artifact_id')))

def finish(error=None):
    if STATE['callback'] is not None: unreal.unregister_slate_post_tick_callback(STATE['callback'])
    if EDITOR.get_game_world(): LEVELS.editor_request_end_play()
    REPORT['map_files_unchanged']=all(h==hashlib.sha256((ROOT/'Content/Maps'/(n+'.umap')).read_bytes()).hexdigest() for n,h in HASHES.items())
    variation=[]
    for name in MAPS:
        runs=[r for r in REPORT['runs'] if r['map']==name]
        variation.append(dict(map=name,checked=len(runs)==2,active_positions_differ=len(runs)==2 and runs[0]['active_ids']!=runs[1]['active_ids'],materials_differ=len(runs)==2 and runs[0]['templates']!=runs[1]['templates']))
    REPORT['variation']=variation
    REPORT['status']='PASS' if not error and REPORT['map_files_unchanged'] and len(REPORT['runs'])==6 and all(r['pass'] for r in REPORT['runs']) and all(v['active_positions_differ'] and v['materials_differ'] for v in variation) else 'FAIL'
    REPORT['error']=error
    (OUT/'assignment-qa.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.log('EXHIBIT_ASSIGNMENT_QA_'+REPORT['status'])

def tick(dt):
    if time.monotonic()-STATE['start']>200: finish('Timeout'); return
    try:
        if STATE['index']==6: finish(); return
        name=MAPS[STATE['index']//2]
        if STATE['phase']=='load':
            assert not EDITOR.get_game_world()
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+name)
            cases=[a for a in ACT.get_all_level_actors() if isinstance(a,unreal.HeistPaintingDisplayCaseActor)]
            STATE['saved']={str(a.get_display_case_id()):signature(a) for a in cases}
            assert len(STATE['saved'])==60
            LEVELS.editor_play_simulate();STATE.update(phase='runtime',since=time.monotonic());return
        if STATE['phase']=='runtime':
            world=EDITOR.get_game_world()
            if not world or unreal.GameplayStatics.get_time_seconds(world)<2: return
            cases=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistPaintingDisplayCaseActor)
            active=[a for a in cases if a.is_contract_exhibit_active()]
            inactive=[a for a in cases if not a.is_contract_exhibit_active()]
            art=[]
            for a in cases:
                id=str(a.get_display_case_id());s=signature(a);before=STATE['saved'][id];c=painting(a)
                p=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('SecurityPanelVisual'))
                b=a.get_component_by_class(unreal.BoxComponent);on=a.is_contract_exhibit_active()
                art.append(dict(case=id,active=on,template=str(a.get_original_visual_template_id()),region=next((str(t) for t in a.tags if str(t).startswith('HeistExhibitRegion_')),None),visible=c.is_visible(),panel_visible=p.is_visible(),collision=str(b.get_collision_enabled()),pass_=bool(close(s['actor'],before['actor']) and close(s['canvas'],before['canvas']) and s['mesh']==before['mesh'] and s['artifact']==before['artifact'] and c.is_visible() and not a.get_editor_property('hidden') and p.is_visible()==on and str(a.get_original_visual_template_id())!='None' and (on or b.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION))))
            loots=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistLootActor) if a.actor_has_tag('HeistMatchSpawnedLooseLoot')]
            loot_rows=[]
            for a in loots:
                visual=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='VisualMeshComponent')
                shell=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='CaseShell')
                panes=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('CaseGlass')]
                active_loot=a.is_exhibition_loot_active();opened=a.is_exhibition_case_open()
                collision=a.get_component_by_class(unreal.SphereComponent).get_collision_enabled()
                pane_checks=[dict(name=c.get_name(),visible=c.is_visible(),scale=xyz(c.get_relative_transform().scale3d),pass_=bool(c.static_mesh and c.static_mesh.get_name()=='SM_GlassWindow' and c.is_visible()==(not opened) and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and all(abs(v*10-round(v*10))<.001 for v in xyz(c.get_relative_transform().scale3d)))) for c in panes]
                loot_rows.append(dict(actor=a.get_name(),row=str(a.get_editor_property('loot_row_id')),active=active_loot,open=opened,visible=visual.is_visible(),shell_visible=shell.is_visible(),panes=pane_checks,pass_=bool(visual.is_visible() and not a.get_editor_property('hidden') and shell.is_visible()==(not opened) and len(panes)==5 and all(p['pass_'] for p in pane_checks) and (active_loot or collision==unreal.CollisionEnabled.NO_COLLISION))))
            ids=sorted(str(a.get_display_case_id()) for a in active)
            templates=sorted(str(a.get_original_visual_template_id()) for a in active)
            targets=[a for a in active if str(a.get_display_case_id()).endswith('_Target')]
            buttons=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistSecurityHoldButtonActor)
            security=[]
            for laser in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistLaserBarrierActor):
                protected=laser.get_protected_painting_case()
                selected=bool(protected and protected.is_contract_exhibit_active())
                linked=[b for b in buttons if b.get_linked_laser_barrier()==laser]
                security.append(dict(laser=laser.get_name(),case=str(protected.get_display_case_id()) if protected else None,active=selected,beam=laser.is_beam_active(),enabled=laser.is_barrier_enabled(),buttons=[b.get_name() for b in linked],pass_=bool(protected and len(linked)==1 and laser.is_barrier_enabled()==selected and laser.is_beam_active()==selected and not laser.is_rearming() and not laser.get_bypass_holder_player_state() and all(not b.is_hold_active() and not b.is_bypass_active() and not b.get_holder_player_state() for b in linked))))
            row=dict(map=name,run=STATE['index']%2+1,active_ids=ids,templates=templates,painting_checks=art,loot_checks=loot_rows,security_checks=security,candidates=len(cases),active=len(active),decorative=len(inactive),loot_active=sum(r['active'] for r in loot_rows),loot_decorative=sum(not r['active'] for r in loot_rows))
            row['pass']=len(cases)==60 and len(active)==12 and len(inactive)==48 and len(set(templates))==12 and len(targets)==1 and all(r['pass_'] for r in art) and len(loots)==12 and row['loot_active']==5 and row['loot_decorative']==7 and all(r['pass_'] for r in loot_rows) and len(security)==2 and len(buttons)==2 and any(r['active'] for r in security) and all(r['pass_'] for r in security)
            REPORT['runs'].append(row)
            (OUT/'assignment-qa.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
            LEVELS.editor_request_end_play();STATE.update(phase='end',since=time.monotonic());return
        if STATE['phase']=='end' and not EDITOR.get_game_world():STATE.update(index=STATE['index']+1,phase='load',since=time.monotonic())
    except Exception: finish(traceback.format_exc())

assert not EDITOR.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
