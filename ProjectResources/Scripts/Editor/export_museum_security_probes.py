"""Read-only static Visibility traces on saved maps for security exposure analysis.

Run UnrealEditor -ExecutePythonScript=<this file> -RenderOffscreen.
Uses initialized SIE collision, ignoring pawns. Never saves assets.
"""
import collections
import hashlib
import json
import math
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Automation/SecuritySimulation'
probes=json.loads((OUT/'probes.json').read_text(encoding='utf-8'))
native=json.loads((OUT/'native.json').read_text(encoding='utf-8'))
plans=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
names=['M01_ClassicalPrototype','M02_MoonlitPrototype','M03_GlasshousePrototype']


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def hashes():return {n:sha(ROOT/'Content/Maps'/(n+'.umap')) for n in names}


before=hashes()
assert before==native['map_sha256_after']
assert probes['native_sha256']==sha(OUT/'native.json')
report=dict(status='RUNNING',probes_sha256=sha(OUT/'probes.json'),map_sha256_before=before,
    maps=[],packages_saved=False,scope='Static ECC_Visibility traces only, pawns ignored; not detection gameplay')
started=time.monotonic()
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state=dict(index=0,phase='load',since=started,busy=False)


def query(world):
    index=state['index']
    name,m,p,plan=names[index],native['maps'][index],probes['maps'][index],plans[index]
    ignored=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Pawn))
    cameras=sorted(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistSecurityCameraActor),key=lambda a:a.get_actor_label())
    counts=collections.Counter()
    def clear(a,b,extra=None):
        hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*[v*100 for v in a]),
            unreal.Vector(*[v*100 for v in b]),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False,ignored+(extra or []),unreal.DrawDebugTrace.NONE,True)
        blocked=bool(hit and hit.to_dict().get('blocking_hit'))
        counts['blocked' if blocked else 'clear']+=1
        return not blocked
    # Known exterior wall and open spawn-air checks prevent empty collision worlds passing silently.
    wall=plan['walls'][0];f=wall['fixed'];mid=(wall['start']+wall['end'])/2
    a,b=([mid,f-.7,1.65],[mid,f+.7,1.65]) if wall['axis']=='h' else ([f-.7,mid,1.65],[f+.7,mid,1.65])
    wall_blocks=not clear(a,b)
    spawn=m['nodes']['SP1']['nav'];air=[spawn[0],spawn[1],spawn[2]+1.65]
    air_clear=clear(air,[air[0],air[1],air[2]+.05])
    assert wall_blocks and air_clear,(name,'Trace controls failed',wall_blocks,air_clear)
    targets=[[q[0],q[1],q[2]+1.65] for q in p['points']]
    buckets=collections.defaultdict(list);cell=6.0
    for i,q in enumerate(targets):buckets[(math.floor(q[0]/cell),math.floor(q[1]/cell))].append(i)
    guard_clear=[]
    for observer in p['observers']:
        a=observer['xyz'];radius=observer['radius'];cx,cy=math.floor(a[0]/cell),math.floor(a[1]/cell)
        found=[];reach=math.ceil(radius/cell)
        for x in range(cx-reach,cx+reach+1):
            for y in range(cy-reach,cy+reach+1):
                for i in buckets.get((x,y),[]):
                    if math.dist(a,targets[i])<=radius and clear(a,targets[i]):found.append(i)
        guard_clear.append(found)
    masks=[]
    for q in targets:
        mask=0
        for i,c in enumerate(m['cameras']):
            if math.dist(c['origin'],q)<=c['range_m']+1.5 and clear(c['origin'],q,[cameras[i]]):mask|=1<<i
        masks.append(mask)
    report['maps'].append(dict(id=m['id'],guard_clear_points=guard_clear,camera_los_masks=masks,
        counts=dict(counts),controls=dict(wall_blocks=wall_blocks,air_clear=air_clear)))
    (OUT/'los.json').write_text(json.dumps(report,separators=(',',':'))+'\n',encoding='utf-8')
    unreal.log_warning('MH_SECURITY_LOS_MAP='+json.dumps(dict(map=m['id'],counts=dict(counts))))


def finish(error=None):
    report.update(map_sha256_after=hashes(),elapsed_seconds=time.monotonic()-started,error=error)
    report['map_files_unchanged']=report['map_sha256_after']==before
    report['status']='PASS' if not error and report['map_files_unchanged'] else 'FAIL'
    (OUT/'los.json').write_text(json.dumps(report,separators=(',',':'))+'\n',encoding='utf-8')
    unreal.log_warning('MH_SECURITY_LOS_DONE='+report['status']+' '+str(error))
    unreal.unregister_slate_post_tick_callback(state['callback'])
    if editor.get_game_world():levels.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def change(phase):state.update(phase=phase,since=time.monotonic())


def tick(_):
    if state['busy']:return
    state['busy']=True
    try:
        if time.monotonic()-started>600:raise TimeoutError(state['phase'])
        if state['phase']=='load':
            if state['index']==len(names):
                finish();return
            change('loading')
            assert levels.load_level('/Game/Maps/'+names[state['index']])
            change('wait_editor')
        elif state['phase']=='wait_editor' and time.monotonic()-state['since']>5:
            levels.editor_play_simulate();change('wait_runtime')
        elif state['phase']=='wait_runtime':
            world=editor.get_game_world()
            if world and unreal.GameplayStatics.get_time_seconds(world)>2:
                query(world);levels.editor_request_end_play();change('wait_end')
        elif state['phase']=='wait_end' and editor.get_game_world() is None:
            state['index']+=1;change('load')
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['callback']=unreal.register_slate_post_tick_callback(tick)
