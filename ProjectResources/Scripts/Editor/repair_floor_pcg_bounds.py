"""Repair the existing floor Blueprint's bounds without changing authored tiles.

Run in a separate full Unreal Editor. Generated ISMs are deliberately excluded
from PCG source bounds; one non-colliding runtime Box supplies the authored grid.
Existing floor instances must match before/after two regenerations and reload.
No source mesh, material, graph, room layout, or new asset is authored here.
"""
import hashlib
import json
import shutil
import sys
import time
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/PCGRepair20260930'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(Path(__file__).resolve().parent))
from create_floor_tile_pcg import ensure_floor_bounds, sync_floor_bounds

ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ED = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
BP_PATH = '/Game/Blueprints/Environment/BP_FloorTilePCG'
MAPS = ['M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']
EXPECTED = [660, 1080, 192]
STATE = {'phase': 'baseline', 'i': 0, 'stamp': time.monotonic(), 'busy': False}
RESULT = {'maps': [], 'errors': [], 'new_assets': [], 'saved': False}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def vector(v):
    return [round(float(getattr(v, k)), 5) for k in 'xyz']


def transform(t):
    q = t.rotation
    return [*vector(t.translation), *vector(t.scale3d),
            *[round(float(getattr(q, k)), 6) for k in 'xyzw']]


def floors():
    return sorted((a for a in ACT.get_all_level_actors()
                   if a.get_class().get_name() == 'BP_FloorTilePCG_C'),
                  key=lambda a: a.get_name())


def floor_state(a):
    pcg = a.get_component_by_class(unreal.PCGComponent)
    rows = []
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        material = [m.get_path_name() if m else None for m in c.get_materials()]
        for i in range(c.get_instance_count()):
            rows.append({'mesh': c.static_mesh.get_path_name(), 'materials': material,
                         'transform': transform(c.get_instance_transform(i, True)),
                         'collision': str(c.get_collision_enabled()),
                         'profile': str(c.get_collision_profile_name())})
    rows.sort(key=lambda row: json.dumps(row, sort_keys=True))
    parameters = {}
    for key in ['GridExtents', 'CellSize', 'TilePivotOffset', 'TileScale', 'TileMeshes']:
        v = a.get_editor_property(key)
        parameters[key] = [m.get_path_name() for m in v] if key == 'TileMeshes' else vector(v)
    assert pcg.generation_trigger == unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND
    assert not pcg.is_component_partitioned and not pcg.regenerate_in_editor
    return {'actor': a.get_name(), 'label': a.get_actor_label(),
            'actor_transform': transform(a.get_actor_transform()),
            'graph': pcg.get_graph().get_path_name(), 'seed': pcg.seed,
            'parameters': parameters, 'instances': rows}


def non_floor_state():
    rows = []
    for a in ACT.get_all_level_actors():
        if a.get_class().get_name() == 'BP_FloorTilePCG_C':
            continue
        components = []
        for c in a.get_components_by_class(unreal.SceneComponent):
            row = {'name': c.get_name(), 'class': c.get_class().get_name(),
                   'transform': transform(c.get_world_transform())}
            if isinstance(c, unreal.StaticMeshComponent):
                row.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,
                           materials=[m.get_path_name() if m else None for m in c.get_materials()])
            components.append(row)
        rows.append({'actor': a.get_name(), 'class': a.get_class().get_name(),
                     'transform': transform(a.get_actor_transform()),
                     'components': sorted(components, key=lambda x: x['name'])})
    return sorted(rows, key=lambda x: x['actor'])


def check_bounds(a):
    boxes = [c for c in a.get_components_by_class(unreal.BoxComponent)
             if c.get_name().startswith('FloorBounds')]
    assert len(boxes) == 1, (a.get_actor_label(), 'FloorBounds', len(boxes))
    b = boxes[0]
    ext = a.get_editor_property('GridExtents')
    expected = [ext.x, ext.y, max(50., ext.z)]
    assert vector(b.get_unscaled_box_extent()) == expected
    assert vector(b.get_relative_transform().scale3d) == [1., 1., 1.]
    assert vector(b.get_relative_transform().translation) == [0., 0., 0.]
    assert not b.get_editor_property('is_editor_only')
    assert not b.get_editor_property('generate_overlap_events')
    assert b.get_editor_property('hidden_in_game')
    assert not b.get_editor_property('can_ever_affect_navigation')
    assert b.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not b.component_tags
    return {'actor': a.get_actor_label(), 'extent_cm': expected,
            'runtime_component': True, 'collision': 'NoCollision', 'navigation': False}


def emit():
    (OUT / 'repair.json').write_text(json.dumps(RESULT, indent=2, ensure_ascii=False), encoding='utf-8')


def finish(error=None):
    if error:
        RESULT['errors'].append(error)
    RESULT['status'] = 'FAIL' if RESULT['errors'] else 'PASS'
    RESULT['after_hashes'] = {str(p.relative_to(ROOT)): sha(p) for p in STATE['files']}
    graph_file = str((ROOT / 'Content/Assets/PCG/PCG_FloorTiles.uasset').relative_to(ROOT))
    RESULT['graph_unchanged'] = RESULT['after_hashes'][graph_file] == RESULT['before_hashes'][graph_file]
    if not RESULT['graph_unchanged']:
        RESULT['errors'].append('Floor PCG graph unexpectedly changed')
        RESULT['status'] = 'FAIL'
    RESULT['new_assets'] = sorted(str(p.relative_to(ROOT)) for p in set((ROOT / 'Content').rglob('*.uasset')) - STATE['assets'])
    if RESULT['new_assets']:
        RESULT['errors'].append('Unexpected new assets')
        RESULT['status'] = 'FAIL'
    emit()
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def set_phase(phase):
    STATE.update(phase=phase, stamp=time.monotonic())


def tick(dt):
    if STATE['busy']:
        return
    STATE['busy'] = True
    try:
        assert time.monotonic() - STATE['stamp'] < 240, 'Timeout: ' + STATE['phase']
        i = STATE['i']
        if STATE['phase'] == 'baseline':
            if i == len(MAPS):
                bp = unreal.load_asset(BP_PATH)
                ensure_floor_bounds(bp)
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
                STATE['i'] = 0
                set_phase('load')
                return
            name = MAPS[i]
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + name)
            row = {'map': name, 'before': [floor_state(a) for a in floors()],
                   'non_floor_before': non_floor_state()}
            assert sum(len(x['instances']) for x in row['before']) == EXPECTED[i]
            RESULT['maps'].append(row)
            STATE['i'] += 1
            emit()
        elif STATE['phase'] == 'load':
            if i == len(MAPS):
                RESULT['saved'] = True
                STATE['i'] = 0
                set_phase('reload')
                return
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + MAPS[i])
            row = RESULT['maps'][i]
            assert [floor_state(a) for a in floors()] == row['before'], 'BP compile changed floor'
            assert non_floor_state() == row['non_floor_before'], 'BP compile changed unrelated actors'
            for a in floors():
                sync_floor_bounds(a)
            row['bounds'] = [check_bounds(a) for a in floors()]
            assert [floor_state(a) for a in floors()] == row['before'], 'Bounds edit changed floor'
            for a in floors():
                a.get_component_by_class(unreal.PCGComponent).generate_local(True)
            STATE['round'] = 1
            set_phase('generate')
        elif STATE['phase'] == 'generate':
            pcgs = [a.get_component_by_class(unreal.PCGComponent) for a in floors()]
            if any(c.get_editor_property('generation_in_progress') or not c.generated for c in pcgs):
                return
            row = RESULT['maps'][i]
            assert [floor_state(a) for a in floors()] == row['before'], 'Regenerated tile selection/placement differs'
            assert non_floor_state() == row['non_floor_before'], 'Unrelated actor changed'
            if STATE['round'] == 1:
                STATE['round'] = 2
                for c in pcgs:
                    c.generate_local(True)
                STATE['stamp'] = time.monotonic()
                return
            row.update(deterministic_twice=True, floor_preserved=True, unrelated_actors_preserved=True)
            assert unreal.EditorLoadingAndSavingUtils.save_map(ED.get_editor_world(), '/Game/Maps/' + MAPS[i])
            row['saved'] = True
            STATE['i'] += 1
            set_phase('load')
            emit()
        elif STATE['phase'] == 'reload':
            if i == len(MAPS):
                finish()
                return
            assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + MAPS[i])
            row = RESULT['maps'][i]
            assert [floor_state(a) for a in floors()] == row['before'], 'Saved floor reload differs'
            assert non_floor_state() == row['non_floor_before'], 'Saved unrelated actors differ'
            row['bounds_reload'] = [check_bounds(a) for a in floors()]
            assert all(a.get_component_by_class(unreal.PCGComponent).generated for a in floors())
            row['reload_preserved'] = True
            STATE['i'] += 1
            emit()
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE['busy'] = False


def main():
    files = [ROOT / 'Content/Blueprints/Environment/BP_FloorTilePCG.uasset',
             ROOT / 'Content/Assets/PCG/PCG_FloorTiles.uasset']
    files += [ROOT / 'Content/Maps' / (name + '.umap') for name in MAPS]
    STATE.update(files=files, assets=set((ROOT / 'Content').rglob('*.uasset')))
    RESULT['before_hashes'] = {str(p.relative_to(ROOT)): sha(p) for p in files}
    for p in files:
        target = OUT / 'Backup' / p.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(p, target)
    grid = next(n.get_settings() for n in unreal.load_asset('/Game/Assets/PCG/PCG_FloorTiles').nodes
                if isinstance(n.get_settings(), unreal.PCGCreatePointsGridSettings))
    assert not grid.get_editor_property('cull_points_outside_volume'), 'Bounds would change grid culling'
    unreal.SystemLibrary.execute_console_command(ED.get_editor_world(), 'Slate.bAllowThrottling 0')
    unreal.SystemLibrary.execute_console_command(ED.get_editor_world(), 't.MaxFPS 30')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE['callback'] = unreal.register_slate_post_tick_callback(tick)


if __name__ == '__main__':
    main()
