"""Add one approved existing BP_Guard resident and one waypoint per saved map.

Dedicated unattended Editor. Other authored actors and assets stay unchanged.
Single-point waiting is the existing AI behavior, not a new gameplay system.
"""
import copy,hashlib,json,math,runpy,time,traceback
from pathlib import Path
import unreal

R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/DetentionSentry20261008';O.mkdir(parents=True,exist_ok=True)
H=runpy.run_path(str(R/'ProjectResources/Scripts/Editor/apply_museum_security_layout.py'))
A=H['A'];E=H['E'];P=H['PLAN'];prop=H['prop']
CORE=P['applied']['guard_camera_stage']['maps']
D=dict(status='RUNNING',maps=[],error=None,source_sha256=hashlib.sha256(H['SOURCE'].read_bytes()).hexdigest(),
       scope='One BP_Guard Medium resident and one single-point route per map; no prior actor/asset definition changed',user_pie='NOT_TESTED',multiplayer='NOT_TESTED')
S=dict(i=0,phase='load',time=0,busy=False,done=False,row=None)
RESUME='-heistdetentionsentryresume' in unreal.SystemLibrary.get_command_line().lower()
PREVIOUS=json.loads((O/'apply.json').read_text(encoding='utf-8')) if RESUME else None
if PREVIOUS:
    assert PREVIOUS['source_sha256']==D['source_sha256']
    (O/('apply_attempt_'+str(time.time_ns())+'.json')).write_text(json.dumps(PREVIOUS,ensure_ascii=False,indent=2),encoding='utf-8')
def dump(name,value):(O/name).write_text(json.dumps(value,ensure_ascii=False,indent=2,default=str)+'\n',encoding='utf-8')
def flush():dump('apply.json',D)
def phase(p):S.update(phase=p,time=time.monotonic());H['S'].update(time=time.monotonic(),stable=None)
def rebuild(w):
    system=unreal.NavigationSystemV1.get_navigation_system(w);assert system
    for a in A.get_all_level_actors():
        if isinstance(a,unreal.NavMeshBoundsVolume):system.on_navigation_bounds_updated(a)
    unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')
def add(code):
    spec=P['maps'][code]['detention_sentry'];before=H['snapshot']();actors={a.get_name():a for a in A.get_all_level_actors()}
    assert not any(a.actor_has_tag('HeistDetentionSentry') for a in actors.values())
    assert not any(isinstance(a,unreal.HeistGuardWaypoint) and str(prop(a,'patrol_route_id'))==spec['route_id'] for a in actors.values())
    loc=unreal.Vector(*spec['guard_start']);g=A.spawn_actor_from_class(unreal.load_class(None,'/Game/Blueprints/Guard/BP_Guard.BP_Guard_C'),loc)
    assert g;g.set_actor_label('Security_'+code+'_DetentionSentry');g.set_folder_path('Security/Guards');g.set_actor_rotation(unreal.Rotator(yaw=spec['yaw_degrees']),False)
    g.set_actor_scale3d(unreal.Vector(1,1,1));g.set_editor_property('guard_profile_id',spec['profile_id']);g.set_editor_property('tags',['HeistDetentionSentry'])
    patrol=g.get_component_by_class(unreal.HeistPatrolPathComponent)
    for k,v in spec['patrol_component'].items():patrol.set_editor_property(k,v)
    patrol.set_editor_property('patrol_route_id',spec['route_id'])
    point=A.spawn_actor_from_class(unreal.HeistGuardWaypoint,loc);assert point
    point.set_actor_label('Security_'+code+'_DetentionSentry_P00');point.set_folder_path('Security/Patrol/'+spec['route_id']);point.set_editor_property('patrol_route_id',spec['route_id'])
    point.set_editor_property('patrol_order',0);point.set_editor_property('wait_duration_override',8)
    after=H['snapshot']();created={g.get_name(),point.get_name()}
    assert set(after)-set(before)==created and not set(before)-set(after)
    assert all(before[n]==after[n] for n in before),'Unrelated prior actor changed'
    S['after']=after;dump(code+'_before.json',before);dump(code+'_modified.json',after)
    S['row'].update(resident_actor=g.get_name(),waypoint_actor=point.get_name(),created=sorted(created),preserved_actor_count=len(before),outside_allowlist_changes=[],resident_spec=spec)
def checks(code):
    row=S['row'];w=E.get_editor_world();actors={a.get_name():a for a in A.get_all_level_actors()};g=actors[row['resident_actor']]
    guards=[a for a in actors.values() if isinstance(a,unreal.HeistGuardCharacter)];p=g.get_component_by_class(unreal.HeistPatrolPathComponent)
    assert len(guards)==9 and sum(a.actor_has_tag('HeistDetentionSentry') for a in guards)==1
    assert not g.actor_has_tag('HeistDetentionPatrol');assert str(prop(g,'guard_profile_id'))=='Guard_Alert_Medium'
    rid=str(prop(p,'patrol_route_id'));points=[a for a in actors.values() if isinstance(a,unreal.HeistGuardWaypoint) and str(prop(a,'patrol_route_id'))==rid]
    assert len(points)==1 and not prop(p,'loop_patrol') and prop(p,'waypoint_wait_duration')==8
    assert H['xyz'](g.get_actor_location())==H['xyz'](points[0].get_actor_location())
    door=actors[row['resident_spec']['door_actor']];cell=next(c for c in door.get_components_by_class(unreal.BoxComponent) if c.get_name()=='CellBounds')
    local=unreal.MathLibrary.inverse_transform_location(cell.get_world_transform(),g.get_actor_location());extent=cell.get_unscaled_box_extent()
    assert abs(local.x)>extent.x+34 or abs(local.y)>extent.y+34,'Sentry inside cell'
    assert min((g.get_actor_location()-a.get_actor_location()).length() for a in guards if a!=g)>120,'Guard starts overlap'
    old=next(a for a in guards if a.actor_has_tag('HeistDetentionPatrol'))
    nv=[unreal.NavigationSystemV1.project_point_to_navigation(w,a.get_actor_location(),None,None,unreal.Vector(50,50,200)) for a in (g,old)];assert all(nv)
    path=unreal.NavigationSystemV1.find_path_to_location_synchronously(w,*nv,g);assert path and path.is_valid() and not path.is_partial(),'Sentry disconnected from patrol region'
    H['S']['row']=row;result=H['validate'](w,code);sweep=H['sweeps'](w);assert sweep['status']=='PASS'
    return dict(strict=result,sweeps=sweep,resident_outside_cell=True,resident_point_count=1,resident_connected_to_existing_patrol=True,profile='Guard_Alert_Medium',normal_behavior='Existing waiting/look-around/noise/chase/alert reactions; no absolute immobilization')
def finish(err=None):
    if S['done']:return
    S['done']=True;D.update(status='FAIL' if err else 'PASS',error=err);flush();unreal.log('DETENTION_SENTRY_APPLY_'+D['status'])
    unreal.unregister_slate_post_tick_callback(S['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(_):
    if S['busy'] or S['done']:return
    S['busy']=True
    try:
        if S['i']==3:finish();return
        code=list(P['maps'])[S['i']];spec=P['maps'][code];path='/Game/Maps/'+spec['map']
        if S['phase']=='load':
            previous=next((r for r in PREVIOUS['maps'] if r['code']==code and r.get('packages_saved')),None) if PREVIOUS else None
            if previous:
                assert H['sha'](code)==previous['after_sha256'];S['row']=copy.deepcopy(previous);S['row']['status']='RUNNING';S['after']=json.loads((O/(code+'_modified.json')).read_text(encoding='utf-8'))
                assert unreal.EditorLoadingAndSavingUtils.load_map(path);D['maps'].append(S['row']);phase('reload_warm');return
            assert H['sha'](code)==spec['detention_sentry']['baseline_sha256'],'Current baseline changed '+code
            assert unreal.EditorLoadingAndSavingUtils.load_map(path)
            S['row']=dict(code=code,status='RUNNING',before_sha256=H['sha'](code),packages_saved=False,cameras=copy.deepcopy(next(r for r in CORE if r['code']==code)['cameras']))
            D['maps'].append(S['row']);phase('warm');flush();return
        assert time.monotonic()-S['time']<150,'Phase timeout '+S['phase']
        w=E.get_editor_world()
        if S['phase'] in ('warm','reload_warm'):
            if time.monotonic()-S['time']<6:return
            rebuild(w);phase('prepare' if S['phase']=='warm' else 'reload');return
        if not H['ready'](w,8):return
        if S['phase']=='prepare':add(code);rebuild(w);phase('modified');flush();return
        if S['phase']=='modified':
            S['row']['checks']=checks(code)
            assert H['snapshot']()==S['after'];assert unreal.EditorLoadingAndSavingUtils.save_map(w,path)
            S['row'].update(packages_saved=True,after_sha256=H['sha'](code));flush();assert unreal.EditorLoadingAndSavingUtils.load_map(path);phase('reload_warm');return
        if S['phase']=='reload':
            actual=H['snapshot']();assert actual==S['after'],'Saved/reloaded authored drift';dump(code+'_reloaded.json',actual)
            S['row']['reload_checks']=checks(code);S['row'].update(status='PASS',saved_reload_preservation='PASS',after_sha256=H['sha'](code));flush();S['i']+=1;phase('load')
    except Exception:finish(traceback.format_exc())
    finally:S['busy']=False
def main():
    assert '-unattended' in unreal.SystemLibrary.get_command_line().lower() and E.get_game_world() is None
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    flush();unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['handle']=unreal.register_slate_post_tick_callback(tick)
if __name__=='__main__':main()
