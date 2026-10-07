"""Read-only live geometry prerequisite for the approved full exhibition layout.

Uses the current Editor packages, not a retired map generator or old snapshot.
No map, Blueprint, mesh, material or DataTable is saved.
"""
import hashlib
import json
import os
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/ApprovedExhibition20261007'
OUT.mkdir(parents=True, exist_ok=True)
PLAN = json.loads((ROOT / 'ProjectResources/SourceArt/Gallery/ApprovedExhibitionLayout.json').read_text(encoding='utf-8'))
LIB = runpy.run_path(str(ROOT / 'ProjectResources/Scripts/Editor/audit_common_map_rules.py'))
A = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAPS = dict(M01='M01_ClassicalPrototype', M02='M02_MoonlitPrototype', M03='M03_GlasshousePrototype')
EXTRA = [
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Display_Wall_%s_01a' % n for n in (100, 200, 300, 400)
] + [
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Display_Stand_01a',
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Bust_01a',
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Gallery_Sign_02a',
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Gallery_Sign_03a',
    '/Game/Assets/MapAssets/Showcase/Meshes/SM_Gallery_Sign_04a',
    '/Game/Assets/MapAssets/PCMHall/Meshes/Prop/SM_POT_SET_A',
    '/Game/Assets/MapAssets/PCMHall/Meshes/Dis_Wall/SM_DISPLAY_WALL_B_1',
    '/Game/Assets/MapAssets/PCMHall/Meshes/Wall_B/SM_WALL_B_1',
    '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_walls_4_1',
    '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_sculpt_2_1',
    '/Game/Assets/MapAssets/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_sculpt_3_1',
]


def main():
    stage = os.environ.get('MH_EXHIBITION_AUDIT_STAGE', 'before')
    assert stage in ('before', 'fullafter')
    meshes = set(EXTRA)
    result = dict(read_only=True, maps=[], meshes={})
    for code, name in MAPS.items():
        row = LIB['snapshot'](name)
        live = {a.get_name(): a for a in A.get_all_level_actors()}
        for actor in row['actors']:
            a = live[actor['name']]
            if isinstance(a, unreal.HeistGuardWaypoint):
                actor['route'] = str(a.get_editor_property('patrol_route_id'))
                actor['order'] = a.get_editor_property('patrol_order')
                actor['wait_override'] = a.get_editor_property('wait_duration_override')
            if isinstance(a, unreal.HeistLootSpawnPoint):
                actor['spawn_category'] = str(a.get_editor_property('spawn_category'))
            if isinstance(a, unreal.HeistLaserBarrierActor):
                p = a.get_protected_painting_case()
                actor['security_link'] = p.get_name() if p else None
            if isinstance(a, unreal.HeistSecurityHoldButtonActor):
                p = a.get_linked_laser_barrier()
                actor['security_link'] = p.get_name() if p else None
            for c in actor['components']:
                if c.get('mesh'):
                    meshes.add(c['mesh'].split('.')[0])
        (OUT / (code + '_' + stage + '.json')).write_text(json.dumps(row, indent=2, default=str), encoding='utf-8')
        result['maps'].append(dict(code=code, name=name, sha256=row['sha256'], actors=len(row['actors'])))
        for group in PLAN['maps'][code]['groups'] + PLAN['maps'][code]['furnishings']:
            meshes.update(p['asset'] for p in group.get('props', []) if p.get('asset'))
    for path in sorted(meshes):
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            result['meshes'][path] = dict(missing=True)
            continue
        m = unreal.load_asset(path)
        if not isinstance(m, unreal.StaticMesh):
            continue
        box = m.get_bounding_box()
        result['meshes'][path] = dict(path=m.get_path_name(), bounds=[LIB['vec'](box.min), LIB['vec'](box.max)], materials=[v.material_interface.get_path_name() if v.material_interface else None for v in m.static_materials])
    for code, name in MAPS.items():
        file = ROOT / 'Content/Maps' / (name + '.umap')
        assert hashlib.sha256(file.read_bytes()).hexdigest() == next(m['sha256'] for m in result['maps'] if m['code'] == code)
    (OUT / ('audit.json' if stage == 'before' else 'audit_fullafter.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    unreal.log('APPROVED_EXHIBITION_AUDIT_PASS maps=3 saved=0')


if __name__ == '__main__':
    main()
