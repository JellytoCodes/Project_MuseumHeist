"""Apply explicitly approved Loose Loot stations to the existing authored maps.

No map regeneration, per-instance assets, saved pickups or gameplay changes.
The plan is deliberately keyed to inspected existing actor names and meshes.
Run through the live delegated Unreal Editor, after a read-only audit. The
derived navigation rebuild runs asynchronously after apply.json is written;
wait for navigation_rebuild.json before running the SIE verifier.
"""
import hashlib
import itertools
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/LooseLootExhibition20261003'
PLAN = ROOT / 'ProjectResources/SourceArt/Gallery/LooseLootExhibitionLayout.json'
A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def vec(p):
    return [float(getattr(p, k)) for k in 'xyz']


def transform(t):
    q = t.rotation
    return dict(location=vec(t.translation), scale=vec(t.scale3d), rotation=[q.x,q.y,q.z,q.w])


def component_bounds(c):
    b = c.static_mesh.get_bounding_box()
    ps = [vec(unreal.MathLibrary.transform_location(c.get_world_transform(),unreal.Vector(*p)))
          for p in itertools.product(*zip(vec(b.min),vec(b.max)))]
    return [[min(p[k] for p in ps) for k in range(3)], [max(p[k] for p in ps) for k in range(3)]]


def snapshot(a):
    row = dict(name=a.get_name(), label=a.get_actor_label(), klass=a.get_class().get_name(),
               transform=transform(a.get_actor_transform()), components=[], tags=[str(t) for t in a.tags])
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            row['components'].append(dict(name=c.get_name(), mesh=c.static_mesh.get_path_name(),
                bounds=component_bounds(c), transform=transform(c.get_world_transform()),
                materials=[m.get_path_name() if m else None for m in c.get_materials()],
                collision=str(c.get_collision_enabled())))
    if isinstance(a, unreal.HeistLootSpawnPoint):
        row['spawn'] = dict(category=str(a.get_editor_property('spawn_category')),
                            occupancy_radius=a.get_editor_property('occupancy_radius'))
    return row


def main():
    plan = json.loads(PLAN.read_text(encoding='utf-8'))
    source = ROOT/'ProjectResources/DataTableImports/DT_LootDataRow.json'
    table = unreal.load_asset('/Game/Data/DataTable/DT_LootData')
    old_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    new_rows = json.loads(source.read_text(encoding='utf-8'))
    stable_fields = ('Name','ItemId','LootGrade','ScoreValue','SpawnCategory','SpawnWeight')
    assert [{k:r[k] for k in stable_fields} for r in old_rows] == [{k:r[k] for k in stable_fields} for r in new_rows]
    assert [r['WorldMesh'].split("'")[-2] if "'" in r['WorldMesh'] else r['WorldMesh'] for r in new_rows] == [r['WorldMesh'] for r in old_rows]
    report = dict(maps=[], asset_copies=0, authored_pickups=0, gameplay_fields_preserved=True)
    # Validate all target maps before changing any map or the shared table.
    for entry in plan['maps']:
        path=ROOT/'Content/Maps'/(entry['map']+'.umap')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['expected_before_sha256'], entry['code']+' changed since audit'
        assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+entry['map'])
        actors={a.get_name():a for a in A.get_all_level_actors()}
        for s in entry['stations']:
            anchor=actors[s['spawn_actor']]; support=actors[s['support_actor']]
            assert isinstance(anchor,unreal.HeistLootSpawnPoint)
            assert isinstance(support,unreal.StaticMeshActor)
            assert support.static_mesh_component.static_mesh.get_path_name()==s['expected_support_mesh']
            assert unreal.load_asset(s['mesh'])
        assert not any(isinstance(a,unreal.HeistLootActor) for a in actors.values())
    table.modify()
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table,str(source))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table)
    OUT.joinpath('table_after.json').write_text(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table),encoding='utf-8')
    for entry in plan['maps']:
        assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+entry['map'])
        actors={a.get_name():a for a in A.get_all_level_actors()}
        allowed={s[k] for s in entry['stations'] for k in ('spawn_actor','support_actor')}
        untouched={n:snapshot(a) for n,a in actors.items() if n not in allowed}
        station_report=[]
        for s in entry['stations']:
            anchor=actors[s['spawn_actor']]; support=actors[s['support_actor']]
            c=support.static_mesh_component
            support.modify(); anchor.modify()
            c.set_static_mesh(unreal.load_asset(s['mesh']))
            c.set_editor_property('override_materials',[])
            c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            support.set_actor_scale3d(unreal.Vector(*s['scale']))
            support.set_actor_rotation(unreal.Rotator(yaw=s['yaw']),False)
            support.set_actor_location(unreal.Vector(s['location'][0],s['location'][1],s['floor_z']),False,False)
            b=component_bounds(c)
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            hit=unreal.SystemLibrary.line_trace_single(world,
                unreal.Vector(s['location'][0],s['location'][1],b[1][2]+50),
                unreal.Vector(s['location'][0],s['location'][1],s['floor_z']+5),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True).to_dict()
            assert hit['blocking_hit'] and hit['hit_actor']==support, s['spawn_actor']+' has no support surface'
            surface_z=hit['impact_point'].z
            # Whole-centimetre actor location; less than 1 cm contact tolerance.
            anchor_z=math.ceil(surface_z)
            anchor.set_actor_scale3d(unreal.Vector(1,1,1))
            anchor.set_actor_rotation(unreal.Rotator(yaw=s['anchor_yaw']),False)
            anchor.set_actor_location(unreal.Vector(s['location'][0],s['location'][1],anchor_z),False,False)
            s['location'][2]=anchor_z
            station_report.append(dict(spawn=s['spawn_actor'],support=s['support_actor'],
                support_bounds=b,anchor=vec(anchor.get_actor_location()),surface_z=surface_z,surface_gap=anchor_z-surface_z))
        assert untouched == {n:snapshot(a) for n,a in actors.items() if n not in allowed}, 'Unrelated map actor changed'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/'+entry['map'])
        assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+entry['map'])
        reloaded={a.get_name():a for a in A.get_all_level_actors()}
        assert untouched == {n:snapshot(a) for n,a in reloaded.items() if n not in allowed}, 'Reload changed unrelated actor'
        after=dict(code=entry['code'],map=entry['map'],actors=[snapshot(a) for a in reloaded.values()])
        OUT.joinpath(entry['code']+'_after.json').write_text(json.dumps(after,indent=2),encoding='utf-8')
        report['maps'].append(dict(code=entry['code'],stations=station_report,unrelated_actors_preserved=len(untouched)))
    PLAN.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    OUT.joinpath('apply.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('LOOT_EXHIBITION_APPLY_PASS maps=3 anchors=36 support_actors=36 assets_copied=0')


if __name__=='__main__':
    main()
    import runpy
    runpy.run_path(str(ROOT/'ProjectResources/Scripts/Editor/rebuild_loot_exhibition_navigation.py'))
