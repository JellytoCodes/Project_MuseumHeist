"""Read saved artist-authored maps without spawning, deleting or saving actors.

Run in a dedicated Unreal Python commandlet. Output is diagnostic evidence only;
it is never an input for rebuilding the maps.
"""
import collections
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
MAPS = ('M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype')
OUT = ROOT / 'Saved/Automation/AuthoredMapAudit'
OUT.mkdir(parents=True, exist_ok=True)


def vector(value):
    return [round(float(getattr(value, axis)), 3) for axis in ('x', 'y', 'z')]


def audit():
    report = {'read_only': True, 'maps': []}
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for name in MAPS:
        file = ROOT / 'Content/Maps' / (name + '.umap')
        before = hashlib.sha256(file.read_bytes()).hexdigest()
        world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + name)
        if not world:
            raise RuntimeError('Unable to load ' + name)
        rows = []
        for actor in subsystem.get_all_level_actors():
            row = {'label': actor.get_actor_label(), 'class': actor.get_class().get_name(),
                   'path': actor.get_path_name(), 'folder': str(actor.get_folder_path()),
                   'location': vector(actor.get_actor_location()),
                   'tags': [str(t) for t in actor.tags], 'meshes': []}
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                mesh = component.get_editor_property('static_mesh')
                row['meshes'].append({'component': component.get_name(),
                    'mesh': mesh.get_path_name() if mesh else None,
                    'scale': vector(component.get_world_scale()),
                    'visible': component.is_visible(),
                    'collision': str(component.get_collision_enabled()),
                    'materials': [m.get_path_name() if m else None for m in component.get_materials()]})
            rows.append(row)
        mesh_counts = collections.Counter(m['mesh'] for r in rows for m in r['meshes'] if m['mesh'])
        primitives = [r for r in rows if any(m['mesh'] and (
            '/BasicShapes/' in m['mesh'] or '/StarterContent/Shapes/' in m['mesh']) for m in r['meshes'])]
        after = hashlib.sha256(file.read_bytes()).hexdigest()
        assert before == after, 'Map changed during read-only audit: ' + name
        settings = world.get_world_settings()
        game_mode = settings.get_editor_property('default_game_mode')
        result = {'name': name, 'sha256': before, 'unchanged': before == after,
                  'game_mode_override': game_mode.get_path_name() if game_mode else None,
                  'actor_count': len(rows), 'classes': dict(collections.Counter(r['class'] for r in rows)),
                  'mesh_counts': dict(mesh_counts.most_common()), 'primitive_actor_count': len(primitives),
                  'primitive_actors': primitives, 'actors': rows}
        (OUT / (name + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        report['maps'].append({k: v for k, v in result.items() if k not in ('actors', 'primitive_actors')})
    (OUT / 'summary.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('HEIST_AUTHORED_MAP_AUDIT_PASS maps=3 saved=0')


if __name__ == '__main__':
    audit()
