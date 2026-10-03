"""Rebuild derived navigation after existing exhibition supports are moved.

Wait for loaded bounds to register, rebuild through the actual Editor, preserve
every authored actor snapshot, then save the existing map. No map generator runs.
"""
import hashlib
import json
import runpy
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Automation/LooseLootExhibition20261003'
PLAN=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json').read_text(encoding='utf-8'))
snapshot=runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/apply_loot_exhibition_layout.py'))['snapshot']
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
STATE=dict(index=0,phase='load',stamp=0,callback=None,busy=False)
REPORT=dict(maps=[],scope='Derived Recast navigation rebuild only; authored actors preserved',user_pie='NOT_TESTED')

def finish(error=None):
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    REPORT.update(status='ERROR' if error else 'PASS',error=error)
    (OUT/'navigation_rebuild.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.log('LOOT_EXHIBITION_NAV_REBUILD_'+REPORT['status'])

def tick(_):
    if STATE['busy']:return
    STATE['busy']=True
    try:
        if STATE['index']==len(PLAN['maps']):finish();return
        m=PLAN['maps'][STATE['index']]
        if STATE['phase']=='load':
            assert E.get_game_world() is None
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+m['map'])
            STATE['before']={a.get_name():snapshot(a) for a in A.get_all_level_actors()}
            STATE.update(phase='initialize',stamp=time.monotonic());return
        world=E.get_editor_world();elapsed=time.monotonic()-STATE['stamp']
        if elapsed>120:raise RuntimeError('Nav timeout '+m['code'])
        if STATE['phase']=='initialize':
            if elapsed<6:return
            system=unreal.NavigationSystemV1.get_navigation_system(world)
            volumes=[a for a in A.get_all_level_actors() if isinstance(a,unreal.NavMeshBoundsVolume)]
            assert system and volumes
            for volume in volumes:system.on_navigation_bounds_updated(volume)
            unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation')
            STATE.update(phase='wait',stamp=time.monotonic());return
        if elapsed<8 or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):return
        starts=[a for a in A.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
        projections=[unreal.NavigationSystemV1.project_point_to_navigation(world,a.get_actor_location(),None,None,unreal.Vector(100,100,200)) for a in starts]
        assert len(starts)==4 and all(projections), 'Nav unavailable at player starts'
        assert STATE['before']=={a.get_name():snapshot(a) for a in A.get_all_level_actors()}, 'Authored actor changed during rebuild'
        assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/'+m['map'])
        assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+m['map'])
        after={a.get_name():snapshot(a) for a in A.get_all_level_actors()}
        assert STATE['before']==after, 'Saved nav changed authored actor'
        sha=hashlib.sha256((ROOT/'Content/Maps'/(m['map']+'.umap')).read_bytes()).hexdigest()
        (OUT/(m['code']+'_after.json')).write_text(json.dumps(dict(code=m['code'],map=m['map'],sha256=sha,actors=list(after.values())),indent=2),encoding='utf-8')
        REPORT['maps'].append(dict(code=m['code'],authored_actors_preserved=len(after),projected_player_starts=len(projections),after_sha256=sha))
        STATE.update(index=STATE['index']+1,phase='load')
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False

assert E.get_game_world() is None
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
