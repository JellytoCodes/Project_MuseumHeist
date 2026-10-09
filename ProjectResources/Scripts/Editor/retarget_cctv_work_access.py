"""Apply only approved CCTV transforms to current authored museum maps.

Reads the approved historical baseline and per-map orientation plan from
MuseumSecurityLayout.json. The baseline guard rejects already-applied or newer
maps; a new placement revision requires a new approved geometry-derived plan.
Validates each exhibit independently before save,
then reloads and repeats the native-contract collision / continuous-sweep check.
Guard and exhibit actors, camera optics, materials and component scale are fixed.
"""
import copy
import hashlib
import json
import runpy
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/CCTVWorkAccess20261009'
SOURCE = ROOT / 'ProjectResources/SourceArt/Gallery/MuseumSecurityLayout.json'
STAGE = json.loads(SOURCE.read_text(encoding='utf-8-sig'))['applied']['cctv_work_access_stage']
assert STAGE['status'] == 'PASS', 'CCTV source plan is not approved and verified'
BASE = dict(maps={code: dict(sha256=row['before_sha256']) for code, row in STAGE['maps'].items()})
OUT.mkdir(parents=True, exist_ok=True)
H = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/apply_museum_security_layout.py'))
W = runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/verify_cctv_work_access.py'))
A, E = H['A'], H['E']
MAPS = {'M01':'M01_ClassicalPrototype','M02':'M02_MoonlitPrototype','M03':'M03_GlasshousePrototype'}
COUNTS = {'M01':14,'M02':12,'M03':16}
S = dict(index=0, phase='load', stamp=0, busy=False, done=False)
REPORT = dict(status='RUNNING', maps=[], error=None, guard_placement_changed=False,
    scope='Only existing CCTV actor transforms; per-exhibit work geometry and full continuous sweep, saved/reloaded strict guard navigation',
    human_play='NOT_TESTED', multiplayer='NOT_TESTED')

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def write(name, value):
    (OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
def flush(): write('apply.json',REPORT)
def phase(name): S.update(phase=name,stamp=time.monotonic())
def finish(error=None):
    S['done']=True; REPORT.update(status='FAIL' if error else 'PASS',error=error);flush()
    unreal.log('CCTV_WORK_APPLY_'+REPORT['status'])
    unreal.unregister_slate_post_tick_callback(S['handle'])
    # The Python editor executor queues QUIT_EDITOR on its next tick.
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def camera_invariants(row):
    row=copy.deepcopy(row)
    row['transform'].pop('rotation',None)
    for c in row.get('components',[]):
        c.pop('bounds',None)
        for k in ('location','rotation'):c['transform'].pop(k,None)
    for c in row.get('scene',[]):
        for k in ('location','rotation'):c['world'].pop(k,None)
        # Unattached root relative transform is the actor transform itself.
        if c['name']=='SceneRootComponent':
            c['relative'].pop('rotation',None)
    return row
def preservation(before,after,camera_names):
    assert set(before)==set(after),'Actors created/deleted'
    changed=[]
    for name,row in before.items():
        if name in camera_names:
            assert camera_invariants(row)==camera_invariants(after[name]),'Camera properties changed '+name
            if row!=after[name]:changed.append(name)
        else: assert row==after[name],'Unrelated authored actor changed '+name
    return dict(status='PASS',changed_cameras=changed,preserved_other_actors=len(before)-len(camera_names))
def check(world,code):
    geometry=W['export_map'](world)
    evaluation=W['evaluate_map'](world,geometry=geometry)
    assert geometry['counts']['paintings']==60 and geometry['counts']['loot']==12
    assert evaluation['status']=='PASS','Blocked work exhibits '+str(evaluation['counts'])
    write(code+'_geometry_after.json',geometry);write(code+'_evaluation_after.json',evaluation)
    actors=list(A.get_all_level_actors())
    cameras=[a for a in actors if isinstance(a,unreal.HeistSecurityCameraActor)]
    assert len(cameras)==COUNTS[code]
    for c in cameras:
        light=c.get_editor_property('sight_light_component');sensor=c.get_editor_property('sensor_origin_component')
        assert light.get_editor_property('intensity')==2.0
        assert light.get_editor_property('specular_scale')==0.0
        assert (light.get_world_location()-sensor.get_world_location()).length()<.001
        assert (light.get_forward_vector()-sensor.get_forward_vector()).length()<.001
        assert abs(light.get_editor_property('outer_cone_angle')-c.get_editor_property('detection_half_angle_degrees'))<.001
    guards=[a for a in actors if isinstance(a,unreal.HeistGuardCharacter)];routes={}
    for a in actors:
        if isinstance(a,unreal.HeistGuardWaypoint):routes.setdefault(str(a.get_editor_property('patrol_route_id')),[]).append(a)
    for route in routes.values():route.sort(key=lambda a:(int(a.get_editor_property('patrol_order')),a.get_name()))
    nav=H['V']['verify_guard_navigation'](world,guards,routes,'strict')
    assert nav['status']=='PASS','Existing guard navigation failed'
    sweeps=H['sweeps'](world);assert sweeps['status']=='PASS','Existing patrol capsule path failed'
    # The fixed-origin candidate plan supplies actual route points and LOS;
    # independently verify the chosen center view in the saved Editor world.
    positives=[]
    for camera in geometry['cameras']:
        ci=next(i for i,c in enumerate(geometry['cameras']) if c['actor']==camera['actor'])
        options=[p for p in geometry['corridor_probes'] if p['transit_goal_eligible'] and (p['native_exposure_mask']&(1<<ci))]
        assert options,'CCTV lost usable passage coverage '+camera['actor']
        positives.append(dict(camera=camera['actor'],center=options[0]['location']))
    stances=[]
    for obj,ev in zip(geometry['objects'],evaluation['objects']):
        patch=obj['patches'][ev['robust_safe_patch_indices'][0]]
        point=obj['points'][patch['point_indices'][len(patch['point_indices'])//2]]
        stances.append(dict(actor=obj['actor'],kind=obj['kind'],center=point['center']))
    return dict(status='PASS',counts=evaluation['counts'],strict_navigation=nav,patrol_capsule_sweeps=sweeps,
        work_stances=stances,positive_controls=positives,cameras=geometry['cameras'])
def tick(_):
    if S['busy'] or S['done']:return
    S['busy']=True
    try:
        if S['index']==3:finish();return
        code=list(MAPS)[S['index']];path='/Game/Maps/'+MAPS[code]
        if S['phase']=='load':
            assert sha(ROOT/'Content/Maps'/(MAPS[code]+'.umap'))==BASE['maps'][code]['sha256'],'Map changed after baseline '+code
            source_map=STAGE['maps'][code]
            assert source_map['map']==MAPS[code], 'CCTV source map mismatch '+code
            assert source_map['camera_count']==COUNTS[code], 'CCTV source camera count mismatch '+code
            plan=dict(plans=source_map['all_camera_plans'])
            assert len(plan['plans'])==COUNTS[code]
            assert unreal.EditorLoadingAndSavingUtils.load_map(path)
            S['plan']=plan;S['row']=dict(code=code,status='RUNNING',before_sha256=BASE['maps'][code]['sha256'],packages_saved=False)
            REPORT['maps'].append(S['row']);phase('apply');flush();return
        world=E.get_editor_world()
        assert time.monotonic()-S['stamp']<300,'Phase timeout '+S['phase']
        if time.monotonic()-S['stamp']<6 or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):return
        if S['phase']=='apply':
            S['before']=H['snapshot']();actors={a.get_name():a for a in A.get_all_level_actors()}
            camera_names={p['actor'] for p in S['plan']['plans']};S['camera_names']=camera_names
            for p in S['plan']['plans']:
                camera=actors[p['actor']];assert isinstance(camera,unreal.HeistSecurityCameraActor)
                assert all(abs(x-y)<.01 for x,y in zip(H['xyz'](camera.get_actor_location()),p['location_cm']))
                camera.modify();camera.set_actor_rotation(unreal.Rotator(pitch=p['rotation_pyr_deg'][0],yaw=p['rotation_pyr_deg'][1],roll=p['rotation_pyr_deg'][2]),False)
            S['after']=H['snapshot']();S['row']['preservation']=preservation(S['before'],S['after'],camera_names)
            S['row']['before_save_checks']=check(world,code)
            write(code+'_after.json',S['after'])
            assert unreal.EditorLoadingAndSavingUtils.save_map(world,path)
            S['row'].update(packages_saved=True,after_sha256=sha(ROOT/'Content/Maps'/(MAPS[code]+'.umap')));flush()
            assert unreal.EditorLoadingAndSavingUtils.load_map(path);phase('reload');return
        if S['phase']=='reload':
            assert H['snapshot']()==S['after'],'Saved/reloaded authored state changed'
            S['row']['saved_reload_checks']=check(world,code)
            S['row']['status']='PASS';flush();S['index']+=1;phase('load')
    except Exception:finish(traceback.format_exc())
    finally:S['busy']=False

assert '-unattended' in unreal.SystemLibrary.get_command_line().lower() and E.get_game_world() is None
OUT.mkdir(parents=True,exist_ok=True);flush()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
S['handle']=unreal.register_slate_post_tick_callback(tick)
