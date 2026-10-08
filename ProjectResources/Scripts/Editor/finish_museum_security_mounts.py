"""Complete the approved security instance pass with reused fixed CCTV supports.

Dedicated rendered unattended Editor. Hash-gated; no asset definitions change.
Two Showcase mesh instances per changed camera; no collision/navigation effect.
"""
import copy, json, math, runpy, time, traceback
from pathlib import Path
import unreal

R = Path(unreal.Paths.project_dir()).resolve()
H = runpy.run_path(str(R/'ProjectResources/Scripts/Editor/apply_museum_security_layout.py'))
O = H['O']; A = H['A']; E = H['E']; prop = H['prop']; bounds = H['bounds']
PRIOR = json.loads((O/'security_apply.json').read_text(encoding='utf-8')) if (O/'security_apply.json').is_file() else dict(status='PASS',maps=H['PLAN']['applied']['guard_camera_stage']['maps'])
assert PRIOR['status'] == 'PASS'
REPORT = dict(status='RUNNING', maps=[], scope='Fixed CCTV support instances only; prior security actors preserved exactly', error=None)
S = dict(i=0, phase='load', stamp=0, busy=False, done=False)
RESUME = '-heistsecuritymountresume' in unreal.SystemLibrary.get_command_line().lower()
PREVIOUS = json.loads((O/'security_mounts.json').read_text(encoding='utf-8')) if RESUME else None
if PREVIOUS:
    (O/('security_mounts_attempt_'+str(time.time_ns())+'.json')).write_text(json.dumps(PREVIOUS,ensure_ascii=False,indent=2),encoding='utf-8')

def write():
    (O/'security_mounts.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, default=str)+'\n', encoding='utf-8')

def phase(name):
    S.update(phase=name, stamp=time.monotonic())
    H['S'].update(time=time.monotonic(), stable=None)

def overlap(a, b):
    return all(min(a[1][i], b[1][i])-max(a[0][i], b[0][i]) > .001 for i in range(3))

def add_supports(row):
    actors = {a.get_name(): a for a in A.get_all_level_actors()}
    assert not any(a.actor_has_tag('HeistCameraMount') for a in actors.values()), 'Supports already exist'
    result = []
    for camrow in row['cameras']:
        cam = actors[camrow['actor']]; host = actors[camrow['host']]
        hc = next(c for c in host.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh)
        hb = bounds(hc); pos = H['xyz'](cam.get_actor_location())
        axis = 0 if hb[1][0]-hb[0][0] < hb[1][1]-hb[0][1] else 1
        side = 1 if pos[axis] > (hb[1][axis]+hb[0][axis])/2 else -1
        face = hb[1 if side > 0 else 0][axis]
        yaw = (0 if side > 0 else 180) if axis == 0 else (90 if side > 0 else -90)
        rotation = unreal.Rotator(pitch=-90, yaw=yaw)
        parts = {}
        for part, meshname, offset, zscale in (('Arm','01a',-1,.2), ('Plate','01b',2,1)):
            location = [round(v) for v in pos]; location[axis] = round(face+side*offset); location[2] += 12
            a = A.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
            assert a
            a.set_actor_label(cam.get_actor_label()+'_Mount'+part)
            a.set_folder_path('Security/CCTV/Mounts')
            a.set_editor_property('tags', ['HeistCameraMount', cam.get_name()])
            c = a.static_mesh_component
            mesh = unreal.load_asset('/Game/Assets/MapAssets/Showcase/Meshes/SM_LightSupport_'+meshname)
            assert mesh and c.set_static_mesh(mesh)
            a.set_actor_scale3d(unreal.Vector(1,1,zscale))
            c.set_collision_profile_name('NoCollision', True)
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            c.set_editor_property('can_ever_affect_navigation', False)
            c.set_editor_property('affect_dynamic_indirect_lighting', False)
            parts[part] = dict(actor=a.get_name(), mesh=mesh.get_path_name(), bounds=bounds(c), location=location, scale=[1,1,zscale])
        mb = bounds(prop(cam,'visual_mesh_component'))
        assert overlap(parts['Arm']['bounds'], hb), 'Arm/wall gap '+cam.get_name()
        assert overlap(parts['Plate']['bounds'], hb), 'Plate/wall gap '+cam.get_name()
        assert overlap(parts['Arm']['bounds'], parts['Plate']['bounds']), 'Arm/plate gap'
        assert overlap(parts['Arm']['bounds'], mb), 'Arm/camera body gap '+cam.get_name()+str(mb)
        result.append(dict(camera=cam.get_name(), host=host.get_name(), parts=parts, contact_bounds='PASS'))
    return result

def checks(row):
    names = [p['actor'] for r in row['supports'] for p in r['parts'].values()]
    actors = {a.get_name(): a for a in A.get_all_level_actors()}
    assert len(names) == 18 and len(set(names)) == 18
    for name in names:
        c = actors[name].static_mesh_component
        assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, str((name,c.get_collision_enabled(),c.get_collision_profile_name()))
        assert not prop(c,'can_ever_affect_navigation')
    H['S']['row'] = row
    result = H['validate'](E.get_editor_world(), row['code'])
    sweep = H['sweeps'](E.get_editor_world()); assert sweep['status'] == 'PASS'
    return dict(checks=result, sweeps=sweep, collision_navigation='DISABLED', camera_work_candidate_geometry='PASS_CONTINUOUS_SENSOR_SWEEP', support_contact='STATIC_AABB_CURRENT_POSE_ONLY')

def finish(error=None):
    if S['done']: return
    S['done'] = True; REPORT.update(status='FAIL' if error else 'PASS', error=error); write()
    unreal.log('MUSEUM_SECURITY_MOUNTS_'+REPORT['status'])
    unreal.unregister_slate_post_tick_callback(S['handle'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(_):
    if S['busy'] or S['done']: return
    S['busy'] = True
    try:
        if S['i'] == 3: finish(); return
        prior = PRIOR['maps'][S['i']]; code = prior['code']
        path = '/Game/Maps/'+H['PLAN']['maps'][code]['map']
        if S['phase'] == 'load':
            saved = next((r for r in PREVIOUS['maps'] if r['code']==code and r.get('packages_saved')),None) if PREVIOUS else None
            if saved:
                assert H['sha'](code)==saved['after_sha256'], 'Resume mounted hash changed'
                assert unreal.EditorLoadingAndSavingUtils.load_map(path)
                S['row']=copy.deepcopy(saved); S['row']['status']='RUNNING'; REPORT['maps'].append(S['row'])
                S['after']=json.loads((O/(code+'_mounted_modified.json')).read_text(encoding='utf-8'))
                phase('reload_warm'); write(); return
            assert H['sha'](code) == prior['after_sha256'], 'Security stage map hash changed '+code
            assert unreal.EditorLoadingAndSavingUtils.load_map(path)
            S['row'] = copy.deepcopy(prior)
            S['row'].update(status='RUNNING', before_sha256=H['sha'](code), after_sha256=None, packages_saved=False)
            REPORT['maps'].append(S['row']); phase('prepare'); write(); return
        assert time.monotonic()-S['stamp'] < 180, 'Phase timeout'
        if S['phase']=='reload_warm':
            if time.monotonic()-S['stamp']<6:return
            system=unreal.NavigationSystemV1.get_navigation_system(E.get_editor_world());assert system
            for v in A.get_all_level_actors():
                if isinstance(v,unreal.NavMeshBoundsVolume):system.on_navigation_bounds_updated(v)
            unreal.SystemLibrary.execute_console_command(E.get_editor_world(),'RebuildNavigation')
            phase('reload');return
        if not H['ready'](E.get_editor_world(),8): return
        if S['phase'] == 'prepare':
            before = H['snapshot'](); S['row']['supports'] = add_supports(S['row']); after = H['snapshot']()
            created = {p['actor'] for r in S['row']['supports'] for p in r['parts'].values()}
            assert set(after)-set(before) == created and not set(before)-set(after)
            assert all(before[k] == after[k] for k in before), 'Prior security/environment changed'
            S['after'] = after; S['row']['preserved_actor_count'] = len(before)
            (O/(code+'_mounted_modified.json')).write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')
            system=unreal.NavigationSystemV1.get_navigation_system(E.get_editor_world())
            for v in A.get_all_level_actors():
                if isinstance(v,unreal.NavMeshBoundsVolume):system.on_navigation_bounds_updated(v)
            unreal.SystemLibrary.execute_console_command(E.get_editor_world(),'RebuildNavigation')
            phase('rebuilt');return
        if S['phase']=='rebuilt':
            S['row']['mounted_checks'] = checks(S['row'])
            assert unreal.EditorLoadingAndSavingUtils.save_map(E.get_editor_world(),path)
            S['row'].update(packages_saved=True,after_sha256=H['sha'](code)); write()
            assert unreal.EditorLoadingAndSavingUtils.load_map(path); phase('reload_warm'); return
        if S['phase'] == 'reload':
            actual = H['snapshot'](); assert actual == S['after'], 'Saved support snapshot drift'
            (O/(code+'_mounted_reloaded.json')).write_text(json.dumps(actual,ensure_ascii=False,indent=2),encoding='utf-8')
            S['row']['mounted_reload_checks'] = checks(S['row'])
            S['row'].update(status='PASS',saved_reload_preservation='PASS',after_sha256=H['sha'](code)); write()
            S['i'] += 1; phase('load')
    except Exception: finish(traceback.format_exc())
    finally: S['busy'] = False

def main():
    assert '-unattended' in unreal.SystemLibrary.get_command_line().lower() and E.get_game_world() is None
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    write(); unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    S['handle'] = unreal.register_slate_post_tick_callback(tick)

if __name__ == '__main__': main()
