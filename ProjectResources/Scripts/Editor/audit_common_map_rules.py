"""Read saved maps for the shared authoring-rule migration. Never saves assets."""
import hashlib
import itertools
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/CommonMapRules'
MAPS = ['M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ML = unreal.MaterialEditingLibrary


def vec(p):
    return [float(getattr(p, k)) for k in 'xyz']


def prop(o, k):
    try:
        return o.get_editor_property(k)
    except Exception:
        return None


def transform(t):
    q = t.rotation
    return dict(location=vec(t.translation), scale=vec(t.scale3d), rotation=[q.x, q.y, q.z, q.w])


def bounds(c):
    b = c.static_mesh.get_bounding_box()
    points = [vec(unreal.MathLibrary.transform_location(c.get_world_transform(), unreal.Vector(*p)))
              for p in itertools.product(*zip(vec(b.min), vec(b.max)))]
    return [[min(p[k] for p in points) for k in range(3)], [max(p[k] for p in points) for k in range(3)]]


def material(m):
    r = {'path': m.get_path_name() if m else None}
    if isinstance(m, unreal.MaterialInstanceConstant):
        r['parent'] = m.parent.get_path_name() if m.parent else None
        for k in ['CanvasAspect', 'PaintingAspect', 'CanvasEllipse']:
            r[k] = ML.get_material_instance_scalar_parameter_value(m, k)
        texture = ML.get_material_instance_texture_parameter_value(m, 'PaintingTexture')
        r['texture'] = texture.get_path_name() if texture else None
        if texture:
            r['texture_size'] = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
    return r


def snapshot(name):
    f = ROOT / 'Content/Maps' / (name + '.umap')
    before = hashlib.sha256(f.read_bytes()).hexdigest()
    world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/' + name)
    assert world, name
    rows = []
    for a in ACTORS.get_all_level_actors():
        row = dict(name=a.get_name(), label=a.get_actor_label(), klass=a.get_class().get_name(),
                   folder=str(a.get_folder_path()), transform=transform(a.get_actor_transform()),
                   components=[], tags=[str(t) for t in a.tags])
        if isinstance(a, unreal.HeistPaintingDisplayCaseActor):
            row['case_id'] = str(a.get_display_case_id())
            row['artifact_id'] = str(prop(a, 'target_artifact_id'))
        for k in ['protected_painting_case', 'laser_barrier', 'detection_range', 'detection_half_angle_degrees',
                  'detection_time', 'sweep_angle_degrees', 'sweep_speed_degrees', 'beam_width']:
            value = prop(a, k)
            if value is not None:
                row[k] = value.get_name() if isinstance(value, unreal.Object) else value
        if isinstance(a, unreal.PostProcessVolume):
            st = a.settings
            row['postprocess'] = {k: str(prop(st, k)) for k in ['auto_exposure_method', 'auto_exposure_min_brightness',
                'auto_exposure_max_brightness', 'auto_exposure_bias', 'ambient_cubemap_intensity', 'ambient_cubemap_tint',
                'ambient_cubemap', 'color_gamma_shadows', 'color_offset_shadows', 'film_toe', 'vignette_intensity',
                'color_correction_shadows_max', 'lumen_final_gather_screen_traces', 'override_lumen_final_gather_screen_traces']}
            row['unbound'] = a.unbound
        for c in a.get_components_by_class(unreal.SceneComponent):
            d = dict(name=c.get_name(), klass=c.get_class().get_name(), root=c == a.root_component,
                     relative=transform(c.get_relative_transform()), world=transform(c.get_world_transform()))
            if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                m = c.static_mesh
                d.update(mesh=m.get_path_name(), bounds=bounds(c), mesh_bounds=[vec(m.get_bounding_box().min), vec(m.get_bounding_box().max)],
                         materials=[material(x) for x in c.get_materials()], collision=str(c.get_collision_enabled()),
                         affect_dynamic_indirect_lighting=c.get_editor_property('affect_dynamic_indirect_lighting'))
                if isinstance(c, unreal.InstancedStaticMeshComponent):
                    d['instances'] = [transform(c.get_instance_transform(i, False)) for i in range(c.get_instance_count())]
            if isinstance(c, unreal.LightComponentBase):
                d['light'] = {k: prop(c, k) for k in ['intensity', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle',
                    'use_inverse_squared_falloff', 'indirect_lighting_intensity', 'volumetric_scattering_intensity',
                    'temperature', 'use_temperature', 'cast_shadows', 'visible']}
                d['light']['units'] = str(prop(c, 'intensity_units'))
                d['direction'] = vec(c.get_forward_vector())
            if isinstance(c, unreal.BoxComponent):
                d['box_extent'] = vec(c.get_unscaled_box_extent())
            row['components'].append(d)
        pcg = a.get_component_by_class(unreal.PCGComponent)
        if pcg:
            row['pcg'] = dict(graph=pcg.get_graph().get_path_name(), seed=pcg.seed, generated=pcg.generated,
                              trigger=str(pcg.generation_trigger), parameters={})
            for k in ['GridExtents', 'CellSize', 'TilePivotOffset', 'TileMeshes', 'TileScale']:
                value = prop(a, k)
                if value is None:
                    continue
                row['pcg']['parameters'][k] = [v.get_path_name() for v in value] if k == 'TileMeshes' else vec(value)
        rows.append(row)
    assert before == hashlib.sha256(f.read_bytes()).hexdigest(), 'Read-only audit wrote map'
    return dict(name=name, sha256=before, actors=rows)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in MAPS:
        result = snapshot(name)
        (OUT / (name + '_before.json')).write_text(json.dumps(result, indent=2, default=str), encoding='utf-8')
        unreal.log('HEIST_RULES_AUDIT ' + name + ' actors=' + str(len(result['actors'])))


if __name__ == '__main__':
    main()
