"""Apply the approved whole-level exhibition plan incrementally in Unreal Editor.

Only allowlisted ordinary paintings, exhibition supports/spawns, their lights,
and the M01 A8 patrol detour may change. No assets are copied or created.
Architecture, PCG floors, protected works and security references are preserved.
"""
import hashlib
import json
import math
import runpy
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/ApprovedExhibition20261007'
PLAN_PATH = ROOT / 'ProjectResources/SourceArt/Gallery/ApprovedExhibitionLayout.json'
PLAN = json.loads(PLAN_PATH.read_text(encoding='utf-8'))
AUDIT = json.loads((OUT / 'audit.json').read_text(encoding='utf-8'))
SIMPLE = runpy.run_path(str(ROOT / 'ProjectResources/Scripts/Editor/apply_loot_exhibition_layout.py'))
FULL = runpy.run_path(str(ROOT / 'ProjectResources/Scripts/Editor/audit_common_map_rules.py'))
A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
MAPS = dict(M01='M01_ClassicalPrototype', M02='M02_MoonlitPrototype', M03='M03_GlasshousePrototype')
TAG = 'ApprovedExhibition20261007'
REPORT = dict(status='RUNNING', maps=[], asset_copies=0, new_assets=0, user_pie='NOT_TESTED')


def v(p):
    return SIMPLE['vec'](p)


def center(b):
    return [(b[i] + b[i + 2]) / 2 for i in range(2)]


def overlap(a, b, margin=0):
    return a[0] < b[2] - margin and a[2] > b[0] + margin and a[1] < b[3] - margin and a[3] > b[1] + margin


def xy_bounds(c):
    b = SIMPLE['component_bounds'](c)
    return [b[0][0], b[0][1], b[1][0], b[1][1]]


def mesh_bounds(path):
    m = unreal.load_asset(path)
    assert isinstance(m, unreal.StaticMesh), path
    b = m.get_bounding_box()
    return m, v(b.min), v(b.max)


def place(a, path, point, bottom, yaw, scales):
    mesh, lo, hi = mesh_bounds(path)
    a.modify()
    c = a.static_mesh_component
    if c.static_mesh != mesh:
        c.set_static_mesh(mesh)
        c.set_editor_property('override_materials', [])
    c.set_collision_profile_name('BlockAll', False)
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    a.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
    a.set_actor_scale3d(unreal.Vector(*scales))
    a.set_actor_location(unreal.Vector(), False, False)
    bounds = SIMPLE['component_bounds'](c)
    p = [math.trunc(point[0] - (bounds[0][0] + bounds[1][0]) / 2),
         math.trunc(point[1] - (bounds[0][1] + bounds[1][1]) / 2),
         math.trunc(bottom - bounds[0][2])]
    a.set_actor_location(unreal.Vector(*p), False, False)
    return c


def new_static(code, gid, role, index):
    a = A.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
    a.set_actor_label(f'AE_{code}_{gid}_{role}_{index:02}')
    a.set_folder_path(f'ApprovedExhibition/{code}/{gid}')
    a.set_editor_property('tags', [TAG, 'Exhibition_' + gid, 'ExhibitionRole_' + role])
    return a


def fitted(a, path, b, bottom, height=None, uniform=False):
    mesh, lo, hi = mesh_bounds(path)
    dims = [hi[k] - lo[k] for k in range(3)]
    target = [b[2] - b[0], b[3] - b[1]]
    # Align the source's long horizontal axis; preserve all source geometry.
    yaw = 0 if (dims[0] >= dims[1]) == (target[0] >= target[1]) else 90
    sizes = dims[:2] if yaw == 0 else dims[1::-1]
    if uniform:
        s = max(.1, math.floor(min(target[k] / sizes[k] for k in (0, 1)) * 10 + .00001) / 10)
        scales = [s, s, s]
    else:
        world_scales = [max(.1, round(target[k] / sizes[k] * 10) / 10) for k in (0, 1)]
        scales = world_scales if yaw == 0 else world_scales[::-1]
        scales += [max(.1, round((height or dims[2]) / dims[2] * 10) / 10)]
    return place(a, path, center(b), bottom, yaw, scales)


def wall_modules(code, gid, index, wall, floor):
    b = wall['b']; horizontal = b[2] - b[0] > b[3] - b[1]
    length = max(b[2] - b[0], b[3] - b[1]); thin = min(b[2] - b[0], b[3] - b[1])
    if code == 'M01':
        lengths = [400] * int(length // 400)
        if length % 400:
            lengths.append(int(length % 400))
        paths = ['/Game/Assets/MapAssets/Showcase/Meshes/SM_Display_Wall_%d_01a' % min((100, 200, 300, 400), key=lambda size: abs(size - n)) for n in lengths]
    else:
        original = '/Game/Assets/MapAssets/PCMHall/Meshes/Dis_Wall/SM_DISPLAY_WALL_B_1' if code == 'M02' else '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_walls_4_1'
        lengths = [length / math.ceil(length / (300 if code == 'M02' else 450))] * math.ceil(length / (300 if code == 'M02' else 450))
        paths = [original] * len(lengths)
    specs = []
    for path, goal in zip(paths, lengths):
        mesh, lo, hi = mesh_bounds(path); dims = [hi[k] - lo[k] for k in range(3)]
        long_axis = 0 if dims[0] >= dims[1] else 1
        long_scale = max(.1, round(goal / dims[long_axis] * 10) / 10)
        scales = [0, 0, max(.1, round(wall['height'] / dims[2] * 10) / 10)]
        scales[long_axis] = long_scale
        scales[1 - long_axis] = max(.1, round(thin / dims[1 - long_axis] * 10) / 10)
        yaw = 0 if (long_axis == 0) == horizontal else 90
        specs.append((path, scales, yaw, dims[long_axis] * long_scale))
    cursor = center(b)[0 if horizontal else 1] - sum(s[3] for s in specs) / 2
    actors = []
    for j, (path, scales, yaw, size) in enumerate(specs):
        p = center(b); p[0 if horizontal else 1] = cursor + size / 2; cursor += size
        a = new_static(code, gid, 'Wall%d' % index, j)
        place(a, path, p, floor, yaw, scales); actors.append(a)
    bs = [SIMPLE['component_bounds'](a.static_mesh_component) for a in actors]
    return actors, [[min(t[0][k] for t in bs) for k in range(3)], [max(t[1][k] for t in bs) for k in range(3)]]


def moved_painting(a, target, wall, floor):
    c = next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith('SM_Canvas_Painting_'))
    old = SIMPLE['component_bounds'](c); old_center = [(old[0][k] + old[1][k]) / 2 for k in range(3)]
    raw = c.static_mesh.get_bounding_box(); thin_axis = 0 if raw.max.x - raw.min.x < raw.max.y - raw.min.y else 1
    normal = c.get_forward_vector() if thin_axis == 0 else c.get_right_vector()
    wc = [(wall[0][k] + wall[1][k]) / 2 for k in range(3)]
    desired = center(target['b']); horizontal = wall[1][0] - wall[0][0] > wall[1][1] - wall[0][1]
    n = [0, 1 if desired[1] > wc[1] else -1] if horizontal else [1 if desired[0] > wc[0] else -1, 0]
    delta = math.degrees(math.atan2(n[1], n[0]) - math.atan2(normal.y, normal.x))
    a.modify(); old_rotation = a.get_actor_rotation()
    a.set_actor_rotation(unreal.Rotator(yaw=old_rotation.yaw + delta), False)
    rb = SIMPLE['component_bounds'](c); rc = [(rb[0][k] + rb[1][k]) / 2 for k in range(3)]
    depth = (rb[1][1] - rb[0][1]) if horizontal else (rb[1][0] - rb[0][0])
    axis = 1 if horizontal else 0
    desired[axis] = (wall[1][axis] if n[axis] > 0 else wall[0][axis]) + n[axis] * (depth / 2 + 5)
    # Move the whole BP, preserving the canvas, panel and interaction assembly.
    loc = a.get_actor_location()
    a.set_actor_location(unreal.Vector(math.trunc(loc.x + desired[0] - rc[0]), math.trunc(loc.y + desired[1] - rc[1]), math.trunc(loc.z)), False, False)
    actual = SIMPLE['component_bounds'](c)
    assert actual[1][2] < wall[1][2] + 1 and actual[0][2] > floor - 1, a.get_actor_label() + ' vertical artwork fit'
    footprint = [wall[0][0], wall[0][1], wall[1][0], wall[1][1]]
    assert not overlap(xy_bounds(c), footprint, .01), a.get_actor_label() + ' overlaps new wall'
    return dict(actor=a.get_name(), old_center=old_center, center=[(actual[0][k] + actual[1][k]) / 2 for k in range(3)], delta=delta, normal=n)


def props(code, gid, items, floor):
    result = []
    for index, p in enumerate(items):
        if p['kind'] == 'bench':
            mesh, lo, hi = mesh_bounds(p['asset']); dims = [hi[k] - lo[k] for k in range(3)]
            b = p['b']; horizontal = b[2] - b[0] >= b[3] - b[1]
            goal = max(b[2] - b[0], b[3] - b[1]); count = max(1, math.ceil(goal / max(dims[:2]) - .01))
            for j in range(count):
                part = list(b); axis = 0 if horizontal else 1
                part[axis] = b[axis] + goal * j / count; part[axis + 2] = b[axis] + goal * (j + 1) / count
                a = new_static(code, gid, 'Bench', index * 10 + j); fitted(a, p['asset'], part, floor, uniform=True); result.append(a)
        else:
            a = new_static(code, gid, p['kind'], index)
            if p['kind'] == 'sign':
                # The source is a plaque. Mount it on an existing display stand
                # inside the approved sign footprint, never float it in space.
                guide = new_static(code, gid, 'GuideSupport', index)
                point = center(p['b'])
                place(guide, '/Game/Assets/MapAssets/Showcase/Meshes/SM_Display_Stand_01a', point, floor, 0, [.8, .8, 1.0])
                plaque = list(p['b']); plaque[1] = point[1] - 22; plaque[3] = point[1] - 19
                fitted(a, '/Game/Assets/MapAssets/Showcase/Meshes/SM_Gallery_Sign_04a', plaque, floor + 84, uniform=True)
                result.extend([a, guide])
                a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            else:
                fitted(a, p['asset'], p['b'], floor, uniform=True); result.append(a)
    return result


def update_lights(code, changes, actors):
    changed = set(); added = []
    all_labels = {a.get_actor_label(): a.get_name() for a in actors.values() if isinstance(a, unreal.HeistPaintingDisplayCaseActor)}
    for item in changes:
        painting = actors[item['actor']]; label = painting.get_actor_label()
        fixtures = [a for a in actors.values() if any(str(t) in ('Art_' + label, 'M03ArtLight_' + painting.get_name()) for t in a.tags)]
        assert fixtures, label + ' has no assigned existing artwork light'
        source = fixtures[0]; tags = [str(t) for t in source.tags]
        targets = [t for t in tags if t.startswith('Art_') or t.startswith('M03ArtLight_')]
        # Shared fixtures stay with their remaining wall group. Reuse the same
        # fixture Blueprint class for the relocated work, with the same 4cd.
        if len(targets) > 1:
            light = A.spawn_actor_from_class(source.get_class(), source.get_actor_location(), source.get_actor_rotation())
            light.set_actor_scale3d(source.get_actor_scale3d())
            light.set_actor_label('AE_' + code + '_ArtLight_' + painting.get_name())
            light.set_folder_path('ApprovedExhibition/' + code + '/ArtworkLights')
            light.set_editor_property('tags', [TAG, 'Artwork', 'Art_' + label])
            for dest in light.get_components_by_class(unreal.LightComponent):
                src = next(c for c in source.get_components_by_class(unreal.LightComponent) if c.get_name() == dest.get_name())
                dest.set_relative_transform(src.get_relative_transform(), False, True)
                for k in ('intensity', 'intensity_units', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle', 'use_inverse_squared_falloff', 'indirect_lighting_intensity', 'temperature', 'use_temperature', 'cast_shadows'):
                    try: dest.set_editor_property(k, src.get_editor_property(k))
                    except Exception: pass
            source.modify(); source.set_editor_property('tags', [t for t in tags if t != 'Art_' + label]); changed.add(source.get_name()); added.append(light)
        else:
            light = source; changed.add(source.get_name())
        light.modify(); p = light.get_actor_location(); old = item['old_center']; new = item['center']; theta = math.radians(item['delta'])
        dx, dy = p.x - old[0], p.y - old[1]
        light.set_actor_location(unreal.Vector(math.trunc(new[0] + dx * math.cos(theta) - dy * math.sin(theta)), math.trunc(new[1] + dx * math.sin(theta) + dy * math.cos(theta)), math.trunc(p.z)), False, False)
        r = light.get_actor_rotation(); light.set_actor_rotation(unreal.Rotator(yaw=r.yaw + item['delta']), False)
    return changed, added


def apply_map(code, name):
    baseline = json.loads((OUT / (code + '_before.json')).read_text(encoding='utf-8'))
    path = ROOT / 'Content/Maps' / (name + '.umap')
    assert hashlib.sha256(path.read_bytes()).hexdigest() == baseline['sha256'], name + ' changed since live audit'
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + name)
    actors = {a.get_name(): a for a in A.get_all_level_actors()}
    assert not any(a.actor_has_tag(TAG) for a in actors.values()), 'Already applied: use saved verification, never duplicate'
    plan = PLAN['maps'][code]; floor = 10 if code == 'M02' else 0
    moved_ids = {a['id'] for g in plan['groups'] for a in g['art']}
    supports = {c['support'] for g in plan['groups'] for c in g['cases'] if c.get('support')}
    spawns = {c['source'] for g in plan['groups'] for c in g['cases'] if c.get('source')}
    protected = {a.get_name() for a in actors.values() if isinstance(a, unreal.HeistPaintingDisplayCaseActor) and str(a.get_display_case_id()).endswith('_Target')}
    protected.update(a.get_protected_painting_case().get_name() for a in actors.values() if isinstance(a, unreal.HeistLaserBarrierActor))
    assert not moved_ids.intersection(protected)
    assert moved_ids | supports | spawns <= set(actors), 'Plan actor names differ from live map'
    before = {n: SIMPLE['snapshot'](a) for n, a in actors.items()}
    added = []; changes = []; station_rows = []
    for g in plan['groups']:
        wall_bounds = []
        for i, wall in enumerate(g['walls']):
            new, b = wall_modules(code, g['id'], i, wall, floor); added.extend(new); wall_bounds.append(b)
        candidate = next((c for c in g['cases'] if c.get('source')), None)
        base_actors = []
        for i, base in enumerate(g['bases']):
            support = actors[candidate['support']] if i == 0 and candidate else new_static(code, g['id'], 'Plinth', i)
            if i > 0 or not candidate: added.append(support)
            original = actors[candidate['support']].static_mesh_component.static_mesh if candidate else None
            model = original.get_path_name().split('.')[0] if original else ('/Game/Assets/MapAssets/Showcase/Meshes/SM_Display_Stand_01c' if code == 'M01' else '/Game/Assets/MapAssets/PCMHall/Meshes/Prop/SM_TABLE_A' if code == 'M02' else '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_table_1_1')
            fitted(support, model, base['b'], floor, height=80); base_actors.append(support)
        for art in g['art']:
            changes.append(moved_painting(actors[art['id']], art, wall_bounds[art['wall']], floor))
        for i, case in enumerate(g['cases']):
            p = case['p']; support = next(a for a in base_actors if xy_bounds(a.static_mesh_component)[0] - 1 <= p[0] <= xy_bounds(a.static_mesh_component)[2] + 1 and xy_bounds(a.static_mesh_component)[1] - 1 <= p[1] <= xy_bounds(a.static_mesh_component)[3] + 1)
            surface = SIMPLE['component_bounds'](support.static_mesh_component)[1][2]
            if case.get('source'):
                # Decorative cloth can extend above the flat display surface.
                hit = unreal.SystemLibrary.line_trace_single(E.get_editor_world(), unreal.Vector(p[0], p[1], surface + 40), unreal.Vector(p[0], p[1], floor + 2), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [a for a in A.get_all_level_actors() if a != support], unreal.DrawDebugTrace.NONE, True)
                fields = hit.to_dict() if hit is not None else {}
                assert fields.get('blocking_hit') and fields.get('hit_actor') == support, 'Missing actual display support surface'
                surface = fields['impact_point'].z
                anchor = actors[case['source']]; anchor.modify(); anchor.set_actor_location(unreal.Vector(math.trunc(p[0]), math.trunc(p[1]), math.ceil(surface)), False, False)
                anchor.set_actor_rotation(unreal.Rotator(), False)
                station_rows.append(dict(actor=anchor.get_name(), support=support.get_name(), location=v(anchor.get_actor_location()), surface_z=surface))
            else:
                path_decor = '/Game/Assets/MapAssets/Showcase/Meshes/SM_Bust_01a' if code == 'M01' else '/Game/Assets/MapAssets/PCMHall/Meshes/Prop/SM_POT_SET_A' if code == 'M02' else '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_sculpt_%d_1' % (2 + i % 2)
                a = new_static(code, g['id'], 'DecorativeExhibit', i); fitted(a, path_decor, case['b'], math.ceil(surface), uniform=True); added.append(a)
        added.extend(props(code, g['id'], g.get('props', []), floor))
    for g in plan['furnishings']:
        added.extend(props(code, g['id'], g['props'], floor))
    changed_lights, new_lights = update_lights(code, changes, actors); added.extend(new_lights)
    allowed = moved_ids | supports | spawns | changed_lights
    if code == 'M01':
        route_points = [a for a in actors.values() if isinstance(a, unreal.HeistGuardWaypoint) and abs(a.get_actor_location().x + 2700) < 1 and abs(a.get_actor_location().y - 2600) < 1]
        assert len(route_points) == 1, 'A8 live waypoint changed'
        point = route_points[0]; route = point.get_editor_property('patrol_route_id'); old_order = point.get_editor_property('patrol_order')
        assert old_order == 10
        for a in actors.values():
            if isinstance(a, unreal.HeistGuardWaypoint) and a.get_editor_property('patrol_route_id') == route and a.get_editor_property('patrol_order') >= old_order:
                allowed.add(a.get_name()); a.modify(); a.set_editor_property('patrol_order', a.get_editor_property('patrol_order') + 2)
        point.set_actor_location(unreal.Vector(-2700, 2130, 25), False, False)
        for j, p in enumerate([[-3470, 2600, 25], [-3470, 2130, 25]]):
            a = A.spawn_actor_from_class(point.get_class(), unreal.Vector(*p)); a.set_actor_label('AE_M01_A8_Patrol_%d' % j)
            a.set_editor_properties(dict(patrol_route_id=route, patrol_order=old_order + j, tags=[TAG]))
            a.set_folder_path('ApprovedExhibition/M01/A8'); added.append(a)
    unchanged = {n: row for n, row in before.items() if n not in allowed}
    after = {a.get_name(): SIMPLE['snapshot'](a) for a in A.get_all_level_actors()}
    assert unchanged == {n: after[n] for n in unchanged}, 'Unapproved original actor changed'
    assert len([a for a in A.get_all_level_actors() if isinstance(a, unreal.HeistPaintingDisplayCaseActor)]) == 60
    assert len([a for a in A.get_all_level_actors() if isinstance(a, unreal.HeistLootSpawnPoint)]) == 12
    # Validate actual authored footprints, including a changed M03 baseline.
    collision_rows = []
    for a in added:
        if not isinstance(a, unreal.StaticMeshActor): continue
        c = a.static_mesh_component; ab = xy_bounds(c)
        role = next((str(t) for t in a.tags if str(t).startswith('ExhibitionRole_')), '')
        for n, row in unchanged.items():
            if row['klass'] != 'StaticMeshActor': continue
            for b in row['components']:
                if 'QUERY_AND_PHYSICS' not in b.get('collision', ''): continue
                bb = b['bounds']
                if bb[0][2] > floor + 200 or bb[1][2] < floor + 5: continue
                oldb = [bb[0][0], bb[0][1], bb[1][0], bb[1][1]]
                if overlap(ab, oldb, 1): collision_rows.append(dict(new=a.get_actor_label(), original=row['label']))
    assert not collision_rows, 'New placement intersects current fixed geometry: ' + str(collision_rows[:5])
    assert unreal.EditorLoadingAndSavingUtils.save_map(E.get_editor_world(), '/Game/Maps/' + name)
    expected = {a.get_name(): SIMPLE['snapshot'](a) for a in A.get_all_level_actors()}
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + name)
    reloaded = {a.get_name(): SIMPLE['snapshot'](a) for a in A.get_all_level_actors()}
    assert expected == reloaded, 'Saved placements changed after reload'
    sha = hashlib.sha256(path.read_bytes()).hexdigest()
    result = dict(code=code, map=name, before_sha256=baseline['sha256'], after_sha256=sha, paintings=60, loot_candidates=12, moved_paintings=len(changes), moved_stations=len(station_rows), new_actors=len(added), preserved_actors=len(unchanged), stations=station_rows, actor_allowlist=sorted(allowed), added_actor_names=[a['name'] for a in reloaded.values() if a['name'] not in before], direct_geometry_overlaps=collision_rows)
    (OUT / (code + '_after.json')).write_text(json.dumps(dict(code=code, map=name, sha256=sha, actors=list(reloaded.values())), indent=2), encoding='utf-8')
    PLAN['maps'][code]['applied'] = result
    return result


def main():
    assert E.get_game_world() is None
    try:
        for code, name in MAPS.items():
            REPORT['maps'].append(apply_map(code, name))
            (OUT / 'apply.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
        REPORT['status'] = 'PASS'
        PLAN['source']['applied_scope'] = '2026-10-07 user-approved Editor placement; derived Nav and runtime checks recorded separately'
        PLAN_PATH.write_text(json.dumps(PLAN, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        unreal.log('APPROVED_EXHIBITION_APPLY_PASS maps=3')
    except Exception:
        REPORT.update(status='ERROR', error=traceback.format_exc())
        raise
    finally:
        (OUT / 'apply.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
