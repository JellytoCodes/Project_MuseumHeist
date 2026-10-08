"""Hash-gated security instance migration; environment and asset definitions are preserved.

Dedicated rendered Editor only. -HeistSecurityDryRun validates without saving.
The source JSON records approved placement, not a procedural museum generator.
"""
import copy,hashlib,json,math,re,runpy,time,traceback
from pathlib import Path
import unreal
R=Path(unreal.Paths.project_dir()).resolve();O=R/'Saved/Automation/SecurityLayout20261008'
SOURCE=R/'ProjectResources/SourceArt/Gallery/MuseumSecurityLayout.json'
PLAN=json.loads(SOURCE.read_text(encoding='utf-8'))
DRY='-heistsecuritydryrun' in unreal.SystemLibrary.get_command_line().lower()
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
V=runpy.run_path(str(R/'ProjectResources/Scripts/Editor/verify_approved_exhibition.py'))
B=runpy.run_path(str(R/'ProjectResources/Scripts/Editor/audit_common_map_rules.py'))
prop=B['prop'];xyz=B['vec'];tx=B['transform'];bounds=B['bounds']
D=dict(status='RUNNING',dry_run=DRY,maps=[],source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),error=None,
       scope='Guard/CCTV/Waypoint instances only; saved/reloaded native strict Nav and physical sweeps. Not natural multiplayer balance.',user_pie='NOT_TESTED',multiplayer='NOT_TESTED')
S=dict(i=0,phase='load',time=0,stable=None,busy=False,done=False,row=None)
RESUME='-heistsecurityresume' in unreal.SystemLibrary.get_command_line().lower()
PREVIOUS=json.loads((O/'security_apply.json').read_text(encoding='utf-8')) if RESUME else None
if PREVIOUS:
    assert not DRY and PREVIOUS['source_sha256']==D['source_sha256']
    history=O/('security_apply_attempt_'+str(time.time_ns())+'.json')
    history.write_text(json.dumps(PREVIOUS,ensure_ascii=False,indent=2),encoding='utf-8')
def normalize_colors(rows):
    rows=copy.deepcopy(rows)
    for row in rows.values():
        # Unreal's UObject repr contains a process address. Retain the asset
        # identity and all property values, excluding that non-authored address.
        for key,value in row.get('postprocess',{}).items():
            if isinstance(value,str):row['postprocess'][key]=re.sub(r' \(0x[0-9A-Fa-f]+\)','',value)
        for c in row.get('scene',[]):
            if 'light' not in c:continue
            color=c['light'].get('light_color')
            if isinstance(color,str) and color.startswith("<Struct 'Color'"):
                values=dict((key,int(value)) for key,value in re.findall(r'([rgba]): (\d+)',color));assert len(values)==4
                c['light']['light_color']=[values[k] for k in ('r','g','b','a')]
    return rows
def write(name,data):(O/name).write_text(json.dumps(data,ensure_ascii=False,indent=2,default=str,allow_nan=False)+'\n',encoding='utf-8')
def flush():write('security_dry_run.json' if DRY else 'security_apply.json',D)
def sha(code):return hashlib.sha256((R/'Content/Maps'/(PLAN['maps'][code]['map']+'.umap')).read_bytes()).hexdigest()
def snapshot():
    base=V['authored_snapshot']()
    for a in A.get_all_level_actors():
        row=base[a.get_name()];row['scene']=[]
        for c in a.get_components_by_class(unreal.SceneComponent):
            if isinstance(c,unreal.BillboardComponent):continue
            p=dict(name=c.get_name(),klass=c.get_class().get_name(),relative=tx(c.get_relative_transform()),world=tx(c.get_world_transform()))
            if isinstance(c,unreal.LightComponentBase):p['light']={k:str(prop(c,k)) for k in ('intensity','attenuation_radius','inner_cone_angle','outer_cone_angle','specular_scale','cast_shadows','use_inverse_squared_falloff','indirect_lighting_intensity','volumetric_scattering_intensity','light_color','intensity_units')}
            row['scene'].append(p)
        row['scene'].sort(key=lambda x:x['name'])
        if isinstance(a,unreal.HeistGuardCharacter):
            p=a.get_component_by_class(unreal.HeistPatrolPathComponent);cap=a.get_component_by_class(unreal.CapsuleComponent)
            row['guard']=dict(profile=str(prop(a,'guard_profile_id')),patrol={k:str(prop(p,k)) for k in ('patrol_route_id','loop_patrol','waypoint_wait_duration','acceptance_radius','look_around_at_waypoints','look_around_turn_rate','look_around_yaw_angle')},capsule=[cap.get_scaled_capsule_radius(),cap.get_scaled_capsule_half_height()])
        if isinstance(a,unreal.HeistSecurityCameraActor):row['camera']={k:prop(a,k) for k in ('detection_range','detection_half_angle_degrees','sweep_half_angle_degrees','sweep_period_seconds')}
        if isinstance(a,unreal.PostProcessVolume):row['postprocess']={k:str(prop(a.settings,k)) for k in ('ambient_cubemap','ambient_cubemap_intensity','auto_exposure_method','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','lumen_final_gather_screen_traces')}
    return normalize_colors(base)
def ready(w,delay):
    if time.monotonic()-S['time']<delay:return False
    if not unreal.NavigationSystemV1.get_navigation_system(w) or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(w):S['stable']=None;return False
    if S['stable'] is None:S['stable']=time.monotonic()
    return time.monotonic()-S['stable']>=2
def phase(p):S.update(phase=p,time=time.monotonic(),stable=None)
def ray(w,p,q,ignore):
    h=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(*p),unreal.Vector(*q),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
    f=h.to_dict() if h else {};a=f.get('hit_actor')
    return dict(blocked=bool(f.get('blocking_hit')),actor=a.get_name() if a else None,impact=xyz(f['impact_point']) if f.get('blocking_hit') else None)
def clear_point(w,p,guards):
    nv=unreal.NavigationSystemV1.project_point_to_navigation(w,unreal.Vector(*p),None,None,unreal.Vector(35,35,200))
    if nv is None or math.dist(p[:2],[nv.x,nv.y])>35:return False
    g=ray(w,[p[0],p[1],70],[p[0],p[1],-40],guards)
    if not g['blocked'] or g['impact'][2]>30:return False
    c=unreal.Vector(p[0],p[1],math.ceil(g['impact'][2]+89-1e-5))
    h=unreal.SystemLibrary.capsule_trace_single_by_profile(w,c,c,33,87,'Pawn',False,guards,unreal.DrawDebugTrace.NONE,True)
    return not(h and h.to_dict().get('blocking_hit'))
def camera_visible(w,a,p):
    sensor=prop(a,'sensor_origin_component');org=sensor.get_world_location();direction=(unreal.Vector(*p)-org);dist=direction.length()
    if not 0<dist<=prop(a,'detection_range'):return False
    direction/=dist;sweep=prop(a,'sweep_half_angle_degrees');half=prop(a,'detection_half_angle_degrees');base=sensor.get_forward_vector();up=sensor.get_up_vector()
    # Exact maximum over the continuous authored local-up sweep, using the
    # same Rodrigues rotation as ResolveSensorForward rather than phase samples.
    cc=direction.dot(up)*base.dot(up);aa=direction.dot(base)-cc
    bb=direction.x*(up.y*base.z-up.z*base.y)+direction.y*(up.z*base.x-up.x*base.z)+direction.z*(up.x*base.y-up.y*base.x)
    peak=math.degrees(math.atan2(bb,aa));angles=[-sweep,sweep]
    if -sweep<=peak<=sweep:angles.append(peak)
    inside=max(aa*math.cos(math.radians(y))+bb*math.sin(math.radians(y))+cc for y in angles)>=math.cos(math.radians(half))
    return inside and not ray(w,xyz(org),p,[a])['blocked']
def mount_camera(w,a,spec,actors):
    pos=spec['position_cm'];rot=spec['rotation_pyr_deg'];a.modify();a.set_actor_location(unreal.Vector(*pos),False,False);a.set_actor_rotation(unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]),False);a.set_actor_scale3d(unreal.Vector(1,1,1))
    for k in ('detection_range','detection_half_angle_degrees','sweep_half_angle_degrees','sweep_period_seconds'):a.set_editor_property(k,spec[k])
    light=prop(a,'sight_light_component');light.set_intensity(2);light.set_attenuation_radius(spec['detection_range']);light.set_outer_cone_angle(spec['detection_half_angle_degrees']);light.set_inner_cone_angle(spec['detection_half_angle_degrees']*.9);light.set_specular_scale(0)
    host=actors[spec['host_actor']];hc=[c for c in host.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh];assert hc,'Missing host mesh'
    hb=bounds(hc[0]);axis=0 if hb[1][0]-hb[0][0]<hb[1][1]-hb[0][1] else 1;side=1 if pos[axis]>(hb[0][axis]+hb[1][axis])/2 else -1
    edge=hb[1][axis] if side>0 else hb[0][axis];mesh=prop(a,'visual_mesh_component');initial=mesh.get_editor_property('relative_rotation');overlap=0
    for ang in (-35,-17.5,0,17.5,35):
        mesh.set_relative_rotation(unreal.Rotator(pitch=initial.pitch,yaw=initial.yaw+ang,roll=initial.roll),False,False)
        mb=bounds(mesh);overlap=max(overlap,(edge-mb[0][axis]) if side>0 else (mb[1][axis]-edge))
    mesh.set_relative_rotation(initial,False,False)
    if overlap>0:
        shift=math.ceil(overlap+2);pos=list(pos);pos[axis]+=side*shift;a.set_actor_location(unreal.Vector(*pos),False,False)
    final=xyz(a.get_actor_location());target=list(spec['target_body_cm']);guards=[x for x in actors.values() if isinstance(x,unreal.HeistGuardCharacter)]
    candidates=[(0,0)]+sorted([(dx,dy) for dx in (-150,-100,-50,0,50,100,150) for dy in (-150,-100,-50,0,50,100,150) if (dx,dy)!=(0,0)],key=lambda q:math.hypot(*q))
    chosen=None
    for dx,dy in candidates:
        p=[target[0]+dx,target[1]+dy,target[2]]
        if clear_point(w,p,guards) and camera_visible(w,a,p):chosen=p;break
    assert chosen,'No visible clear approach '+a.get_actor_label()+' target='+str(target)+' lens='+str(final)
    backing=ray(w,final,[final[0]-(side*80 if axis==0 else 0),final[1]-(side*80 if axis==1 else 0),final[2]],[a])
    assert backing['blocked'] and backing['actor']==host.get_name(),'Camera not backed by nominated wall '+a.get_actor_label()+str(backing)
    return dict(actor=a.get_name(),label=a.get_actor_label(),region=spec['region'],host=host.get_name(),position=final,rotation=rot,approach=chosen,target_ray=ray(w,final,chosen,[a]),backing=backing,mesh_axis_clearance_adjustment_cm=math.ceil(overlap+2) if overlap>0 else 0,safe_work_pockets=spec['safe_work_pockets'])
def apply(w,code):
    spec=PLAN['maps'][code];actors={a.get_name():a for a in A.get_all_level_actors()};before=snapshot();S['before']=before;write(code+'_security_before.json',before)
    allowed=set();removed=[];created=[];guard_rows=[]
    for g in spec['guards']+([spec['detention_sentry']] if spec.get('detention_sentry') else []):
        rid=g['reuse_current_route_id'] or g['route_id'];old=sorted([a for a in actors.values() if isinstance(a,unreal.HeistGuardWaypoint) and str(prop(a,'patrol_route_id'))==rid],key=lambda a:(int(prop(a,'patrol_order')),a.get_name()))
        for i,p in enumerate(g['points']):
            a=old[i] if i<len(old) else A.spawn_actor_from_class(unreal.HeistGuardWaypoint,unreal.Vector(*p['location']))
            assert a;a.modify();a.set_actor_label('Security_'+code+'_'+g['route_id']+'_P%02d'%i);a.set_folder_path('Security/Patrol/'+rid);a.set_actor_location(unreal.Vector(*p['location']),False,False)
            a.set_editor_property('patrol_route_id',rid);a.set_editor_property('patrol_order',i);a.set_editor_property('wait_duration_override',p['wait_duration_override']);allowed.add(a.get_name())
            if a.get_name() not in before:created.append(a.get_name())
        for a in old[len(g['points']):]:allowed.add(a.get_name());removed.append(a.get_name());assert A.destroy_actor(a)
        guard=actors[g['existing_guard_actor']] if g['existing_guard_actor'] else A.spawn_actor_from_class(unreal.load_class(None,'/Game/Blueprints/Guard/BP_Guard.BP_Guard_C'),unreal.Vector(*g['guard_start']))
        assert guard;guard.modify();guard.set_actor_location(unreal.Vector(*g['guard_start']),False,False);guard.set_actor_scale3d(unreal.Vector(1,1,1));guard.set_editor_property('guard_profile_id',g['profile_id'])
        if len(g['points'])>1:
            dx=g['points'][1]['location'][0]-g['points'][0]['location'][0];dy=g['points'][1]['location'][1]-g['points'][0]['location'][1];yaw=math.degrees(math.atan2(dy,dx))
        else:yaw=g['yaw_degrees']
        guard.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
        if g.get('role')=='detention_sentry':guard.set_editor_property('tags',['HeistDetentionSentry'])
        guard.set_actor_label('Security_'+code+'_Guard_'+g['route_id'].rsplit('_',1)[-1]);guard.set_folder_path('Security/Guards')
        patrol=guard.get_component_by_class(unreal.HeistPatrolPathComponent)
        for k,val in g['patrol_component'].items():patrol.set_editor_property(k,val)
        patrol.set_editor_property('patrol_route_id',rid);allowed.add(guard.get_name())
        if guard.get_name() not in before:created.append(guard.get_name())
        guard_rows.append(dict(actor=guard.get_name(),route=rid,profile=g['profile_id'],regions=g['regions'],start=xyz(guard.get_actor_location()),points=g['points']))
    camera_rows=[]
    for c in spec['cameras']['existing_adjustments']+spec['cameras']['additions']:
        name=c.get('actor');cam=actors[name] if name else A.spawn_actor_from_class(unreal.load_class(None,'/Game/Blueprints/World/Actors/Security/BP_SecurityCamera.BP_SecurityCamera_C'),unreal.Vector(*c['position_cm']))
        assert cam
        if not name:cam.set_actor_label(c['label']);cam.set_folder_path('Security/CCTV');created.append(cam.get_name())
        camera_rows.append(mount_camera(w,cam,c,actors));allowed.add(cam.get_name())
    after=snapshot();S['after']=after;S['allowed']=allowed
    differences=[name for name in before if name not in allowed and before[name]!=after.get(name)]
    assert not differences,'Unrelated actors changed '+str(differences)
    assert set(after)-set(before)==set(created) and set(before)-set(after)==set(removed),'Unexpected creation/deletion'
    assert before[spec['detention_preserved']['actor']]==after[spec['detention_preserved']['actor']],'Detention guard changed'
    S['row'].update(guards=guard_rows,cameras=camera_rows,changed_actor_allowlist=sorted(allowed),created=created,removed=removed,unrelated_preservation='PASS',preserved_actor_count=len(before)-len(allowed&before.keys()))
    write(code+'_security_modified.json',after)
def sweeps(w):
    guards=[a for a in A.get_all_level_actors() if isinstance(a,unreal.HeistGuardCharacter)];routes={}
    for a in A.get_all_level_actors():
        if isinstance(a,unreal.HeistGuardWaypoint):routes.setdefault(str(prop(a,'patrol_route_id')),[]).append(a)
    for rr in routes.values():rr.sort(key=lambda a:(int(prop(a,'patrol_order')),a.get_name()))
    result=dict(status='PASS',checked_segments=0,checked_subsegments=0,failed=[],route_paths=[])
    for g in guards:
        cap=g.get_component_by_class(unreal.CapsuleComponent);route=routes[str(prop(g.get_component_by_class(unreal.HeistPatrolPathComponent),'patrol_route_id'))]
        pairs=[(g,route[0])]+list(zip(route,route[1:]))+list(zip(route[1:],route))
        for aa,bb in pairs:
            nv=[unreal.NavigationSystemV1.project_point_to_navigation(w,a.get_actor_location(),None,None,unreal.Vector(50,50,200)) for a in (aa,bb)];assert all(v is not None for v in nv)
            if (nv[0]-nv[1]).length()<.01:pp=nv[:1]
            else:
                path=unreal.NavigationSystemV1.find_path_to_location_synchronously(w,*nv,g);assert path and path.is_valid() and not path.is_partial();pp=list(path.path_points)
            result['checked_segments']+=1
            result['route_paths'].append(dict(guard=g.get_name(),start=aa.get_name(),end=bb.get_name(),points=[xyz(p) for p in pp]))
            for pa,pb in zip(pp,pp[1:]):
                h=unreal.SystemLibrary.capsule_trace_single_by_profile(w,pa+unreal.Vector(0,0,cap.get_scaled_capsule_half_height()),pb+unreal.Vector(0,0,cap.get_scaled_capsule_half_height()),cap.get_scaled_capsule_radius()-1,cap.get_scaled_capsule_half_height()-1,cap.get_collision_profile_name(),False,guards,unreal.DrawDebugTrace.NONE,True)
                f=h.to_dict() if h else {};result['checked_subsegments']+=1
                if f.get('blocking_hit'):result['failed'].append(dict(guard=g.get_name(),a=aa.get_name(),b=bb.get_name(),blocker=f['hit_actor'].get_name() if f.get('hit_actor') else None))
    result['status']='FAIL' if result['failed'] else 'PASS';return result
def validate(w,code):
    gl=V['validate'].__globals__;gl.update(OUT=O,REPORT_PATH=O/'validation_detail.json',REPORT=D);gl['STATE']['row']={}
    checks=V['validate'](w);actors=list(A.get_all_level_actors());guards=[a for a in actors if isinstance(a,unreal.HeistGuardCharacter)];cams=[a for a in actors if isinstance(a,unreal.HeistSecurityCameraActor)]
    assert len(guards)==PLAN['maps'][code]['expected_guards'] and len(cams)==PLAN['maps'][code]['expected_cameras'];assert sum(a.actor_has_tag('HeistDetentionPatrol') for a in guards)==1
    for a in cams:
        light=prop(a,'sight_light_component');sensor=prop(a,'sensor_origin_component');assert prop(light,'intensity')==2 and prop(light,'specular_scale')==0
        assert abs(prop(light,'attenuation_radius')-prop(a,'detection_range'))<.1 and abs(prop(light,'outer_cone_angle')-prop(a,'detection_half_angle_degrees'))<.001
        assert (light.get_world_location()-sensor.get_world_location()).length()<.001 and (light.get_forward_vector()-sensor.get_forward_vector()).length()<.001
    for a in guards:
        assert [prop(l,'intensity') for l in a.get_components_by_class(unreal.SpotLightComponent)]==[20.]
    checks.update(authored_guards=len(guards),cameras=len(cams),camera_optics='PASS',detention_guard_count=1)
    work=[]
    for c in S['row']['cameras']:
        tested=[]
        for typ,pts in c['safe_work_pockets'].items():
            for p in pts:
                ok=clear_point(w,p,guards);watchers=[a.get_name() for a in cams if camera_visible(w,a,p)] if ok else []
                tested.append(dict(type=typ,position=p,clear_capsule_nav=ok,watching_cameras=watchers))
        # Proposed pockets are seeds. Search a nearby valid stance, rather than
        # treating an AABB proposal or a Nav projection as an interaction proof.
        if not any(x['clear_capsule_nav'] and not x['watching_cameras'] for x in tested):
            for typ,pts in c['safe_work_pockets'].items():
                for seed in pts:
                    for dx,dy in sorted([(x,y) for x in (-100,-50,0,50,100) for y in (-100,-50,0,50,100) if 0<math.hypot(x,y)<=100],key=lambda q:math.hypot(*q)):
                        p=[seed[0]+dx,seed[1]+dy,seed[2]];ok=clear_point(w,p,guards)
                        watchers=[a.get_name() for a in cams if camera_visible(w,a,p)] if ok else []
                        if ok and not watchers:
                            tested.append(dict(type=typ,position=p,seed=seed,clear_capsule_nav=True,watching_cameras=[],nearby_stance_adjustment=[dx,dy]));break
                    if any(x['clear_capsule_nav'] and not x['watching_cameras'] for x in tested):break
                if any(x['clear_capsule_nav'] and not x['watching_cameras'] for x in tested):break
        work.append(dict(camera=c['actor'],region=c['region'],points=tested,at_least_one_unwatched_clear_candidate=any(x['clear_capsule_nav'] and not x['watching_cameras'] for x in tested)))
    checks['work_pocket_candidates']=work
    assert all(r['at_least_one_unwatched_clear_candidate'] for r in work),'No unobserved clear work candidate '+str([r for r in work if not r['at_least_one_unwatched_clear_candidate']])
    return checks
def finish(err=None):
    if S['done']:return
    S['done']=True;D['error']=err;D['status']='FAIL' if err else 'PASS';flush();unreal.log('MUSEUM_SECURITY_'+('DRY_' if DRY else '')+D['status'])
    unreal.unregister_slate_post_tick_callback(S['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
def tick(_):
    if S['busy'] or S['done']:return
    S['busy']=True
    try:
        if S['i']==3:finish();return
        code=list(PLAN['maps'])[S['i']];spec=PLAN['maps'][code];path='/Game/Maps/'+spec['map']
        if S['phase']=='load':
            previous=next((r for r in PREVIOUS['maps'] if r['code']==code and r.get('packages_saved')),None) if PREVIOUS else None
            if previous:
                assert sha(code)==previous['after_sha256'],'Resume saved map hash changed '+code
                assert unreal.EditorLoadingAndSavingUtils.load_map(path);S['row']=copy.deepcopy(previous);S['row']['status']='RUNNING';D['maps'].append(S['row']);S['after']=normalize_colors(json.loads((O/(code+'_security_modified.json')).read_text(encoding='utf-8')));phase('reload');flush();return
            assert sha(code)==spec['baseline_sha256'],'Baseline map hash changed '+code
            assert unreal.EditorLoadingAndSavingUtils.load_map(path);S['row']=dict(code=code,status='RUNNING',before_sha256=sha(code),packages_saved=False);D['maps'].append(S['row']);phase('prepare');flush();return
        w=E.get_editor_world();assert time.monotonic()-S['time']<180,'Phase timeout '+S['phase']
        if S['phase']=='prepare':
            if not ready(w,6):return
            S['row']['baseline_sweeps']=sweeps(w)
            apply(w,code);system=unreal.NavigationSystemV1.get_navigation_system(w)
            for vol in A.get_all_level_actors():
                if isinstance(vol,unreal.NavMeshBoundsVolume):system.on_navigation_bounds_updated(vol)
            unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation');phase('rebuilt');flush();return
        if S['phase']=='rebuilt':
            if not ready(w,8):return
            S['row']['checks']=validate(w,code);S['row']['sweeps']=sweeps(w);assert S['row']['sweeps']['status']=='PASS';assert snapshot()==S['after'],'Rebuild changed authored snapshot'
            if DRY:S['row'].update(status='PASS',after_sha256=sha(code));assert sha(code)==spec['baseline_sha256'];S.update(i=S['i']+1,row=None);phase('load');flush();return
            assert unreal.EditorLoadingAndSavingUtils.save_map(w,path);S['row'].update(packages_saved=True,after_sha256=sha(code));flush();assert unreal.EditorLoadingAndSavingUtils.load_map(path);phase('reload');return
        if S['phase']=='reload':
            if not ready(w,6):return
            actual=snapshot();write(code+'_security_reloaded.json',actual);assert actual==S['after'],'Saved/reloaded actor drift '+str([k for k in actual if actual[k]!=S['after'].get(k)])
            S['row']['reload_checks']=validate(w,code);S['row']['reload_sweeps']=sweeps(w);assert S['row']['reload_sweeps']['status']=='PASS';S['row'].update(status='PASS',saved_reload_preservation='PASS',after_sha256=sha(code));flush();S.update(i=S['i']+1,row=None);phase('load')
    except Exception:finish(traceback.format_exc())
    finally:S['busy']=False
def main():
    assert '-unattended' in unreal.SystemLibrary.get_command_line().lower() and E.get_game_world() is None
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    O.mkdir(parents=True,exist_ok=True);flush();unreal.EditorPythonScripting.set_keep_python_script_alive(True);S['handle']=unreal.register_slate_post_tick_callback(tick)
if __name__=='__main__':main()
