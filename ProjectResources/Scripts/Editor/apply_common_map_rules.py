"""Apply shared authoring rules to the reviewed saved maps, preserving their layouts.

Run in a separate full Editor to let the existing PCG graph generate. Original
floor actors are deleted only after exact footprint, collision and deterministic
generation checks. No derived meshes, materials or Blueprint assets are created.
"""
import hashlib
import json
import math
import shutil
import sys
import time
import traceback
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from refine_canvas_and_light_fixtures import painting_geometry
from create_floor_tile_pcg import ensure_floor_bounds, sync_floor_bounds

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Automation/CommonMapRules'
ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ED = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
BL = unreal.BlueprintEditorLibrary
EL = unreal.EditorAssetLibrary
SUB = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
MAPS = ['M01_ClassicalPrototype', 'M02_MoonlitPrototype', 'M03_GlasshousePrototype']
FLOOR_BP = '/Game/Blueprints/Environment/BP_FloorTilePCG'
FLOOR_GRAPH = '/Game/Assets/PCG/PCG_FloorTiles'
CASE_BP = '/Game/Blueprints/World/Actors/Loot/BP_PaintingDisplayCase'
STATE = dict(index=0, phase='prepare', stamp=0, busy=False, floors=[], old=[])
RESULT = dict(errors=[], maps={}, saved=False, new_assets=[])


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def placement(a):
    t=a.get_actor_transform(); p=t.translation; s=t.scale3d; q=t.rotation
    return (p.x,p.y,p.z,s.x,s.y,s.z,q.x,q.y,q.z,q.w)


def backup(package):
    for ext in ('.uasset', '.umap'):
        p = ROOT / 'Content' / (package.removeprefix('/Game/') + ext)
        if p.exists():
            d = OUT / 'Backup' / p.relative_to(ROOT / 'Content')
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists():
                shutil.copy2(p, d)


def obj(h):
    return LIB.get_object(LIB.get_data(h))


def extend_floor_graph():
    backup(FLOOR_BP); backup(FLOOR_GRAPH)
    bp = unreal.load_asset(FLOOR_BP)
    if 'TileScale' not in [str(n) for n in BL.list_member_variable_names(bp)]:
        typ = BL.get_struct_type(unreal.load_object(None, '/Script/CoreUObject.Vector'))
        assert BL.add_member_variable(bp, 'TileScale', typ)
    BL.set_blueprint_variable_instance_editable(bp, 'TileScale', True)
    BL.set_blueprint_variable_category(bp, 'TileScale', 'Floor Tiles')
    BL.compile_blueprint(bp)
    unreal.get_default_object(bp.generated_class()).set_editor_property('TileScale', unreal.Vector(1, 1, 1))
    ensure_floor_bounds(bp)
    BL.compile_blueprint(bp)
    g = unreal.load_asset(FLOOR_GRAPH)
    offset = next(n for n in g.nodes if isinstance(n.get_settings(), unreal.PCGTransformPointsSettings))
    scale = next((n for n in g.nodes if str(n.node_title) == 'TileScale'), None)
    if not scale:
        scale, settings = g.add_node_of_type(unreal.PCGGetActorPropertySettings)
        scale.set_editor_property('node_title', 'TileScale')
        scale.set_node_position(0, 550)
        settings.set_editor_property('property_name', 'TileScale')
        g.add_edge(scale, 'Out', offset, 'ScaleMin')
        g.add_edge(scale, 'Out', offset, 'ScaleMax')
    offset.get_settings().set_editor_property('uniform_scale', False)
    assert EL.save_loaded_asset(g)
    assert EL.save_loaded_asset(bp)


def case_panel():
    # One functional flat security marker in the existing common Shell.
    # It uses existing assets; no per-painting meshes, classes or materials.
    backup(CASE_BP)
    bp = unreal.load_asset(CASE_BP)
    handles = SUB.k2_gather_subobject_data_for_blueprint(bp)
    panel = next((obj(h) for h in handles if obj(h).get_name().startswith('SecurityPanelVisual')), None)
    if not panel:
        parent = next(h for h in handles if isinstance(obj(h), unreal.SceneComponent)
                      and obj(h).get_name() == 'PaintingRoot')
        h, reason = SUB.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=parent, new_class=unreal.StaticMeshComponent, blueprint_context=bp))
        assert not str(reason), reason
        assert SUB.rename_subobject(h, 'SecurityPanelVisual')
        panel = obj(h)
    panel.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    panel.set_material(0, unreal.load_asset('/Game/Assets/StarterContent/Materials/M_Tech_Panel'))
    panel.set_editor_properties(dict(relative_scale3d=unreal.Vector(.2, .2, .2),
        relative_location=unreal.Vector(8, 80, 10), relative_rotation=unreal.Rotator(pitch=90),
        cast_shadow=False, affect_dynamic_indirect_lighting=False))
    panel.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    BL.compile_blueprint(bp)
    assert EL.save_loaded_asset(bp)


def common_defaults():
    paths=['/Game/Blueprints/Environment/BP_HeistLight_'+n for n in ['DropCeiling_01a','Droplights_01a','Droplights_01b','Droplights_01c']]
    paths+=['/Game/Blueprints/World/Actors/Security/BP_SecurityCamera']
    report=[]
    for path in paths:
        bp=unreal.load_asset(path)
        handles=SUB.k2_gather_subobject_data_for_blueprint(bp)
        c=next((LIB.get_object(LIB.get_data(h)) for h in handles if isinstance(LIB.get_object(LIB.get_data(h)),unreal.SpotLightComponent)),None)
        assert c,path
        # Existing assets only. Retain fixture geometry, cone, color and CCTV behavior.
        c.set_editor_properties(dict(intensity=2. if 'SecurityCamera' in path else 4.,
            intensity_units=unreal.LightUnits.CANDELAS,use_inverse_squared_falloff=True,
            indirect_lighting_intensity=0.,volumetric_scattering_intensity=0.))
        if 'HeistLight' in path:
            for h in SUB.k2_gather_subobject_data_for_blueprint(bp):
                o=LIB.get_object(LIB.get_data(h))
                if isinstance(o,unreal.StaticMeshComponent):o.set_editor_property('affect_dynamic_indirect_lighting',False)
        BL.compile_blueprint(bp);assert EL.save_loaded_asset(bp)
        report.append(path)
    # The existing wood/concrete masters need to support the PCG instance renderer.
    for path in ['/Game/Assets/MapAssets/ConferenceRoom/Meshes/Floor/SM_Floor_01','/Game/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_stone_tile_1_1']:
        mesh=unreal.load_asset(path)
        for slot in mesh.static_materials:
            m=slot.material_interface
            while isinstance(m,unreal.MaterialInstanceConstant):m=m.parent
            unreal.MaterialEditingLibrary.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
            unreal.MaterialEditingLibrary.recompile_material(m);EL.save_loaded_asset(m)
    return report


def validate_authored_transforms(actors):
    for a in actors:
        assert all(abs(x - math.trunc(x)) < .001 for x in (a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)), a.get_name()
        for c in a.get_components_by_class(unreal.SceneComponent):
            s = c.get_relative_transform().scale3d
            assert all(abs(x * 10 - round(x * 10)) < .001 for x in (s.x, s.y, s.z)), (a.get_name(), c.get_name())


def artwork_and_lighting(actors, code):
    art = []
    for a in actors:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if not c.static_mesh or not c.static_mesh.get_name().startswith('SM_Canvas_Painting_'):
                continue
            p = painting_geometry(c); p['actor'] = a; art.append(p)
            s = c.get_world_scale()
            assert max(s.x, s.y, s.z) - min(s.x, s.y, s.z) < .001, 'Nonuniform painting'
            m = c.get_material(0)
            tex = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m, 'PaintingTexture')
            assert tex and 'WhiteSquare' not in tex.get_name(), a.get_name()
            assert abs(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(m, 'CanvasEllipse')) < .001
            assert abs(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(m, 'PaintingAspect') - tex.blueprint_get_size_x()/tex.blueprint_get_size_y()) < .001
            if isinstance(a, unreal.HeistPaintingDisplayCaseActor):
                box = a.get_component_by_class(unreal.BoxComponent)
                assert tuple(round(v) for v in (box.get_unscaled_box_extent().x, box.get_unscaled_box_extent().y, box.get_unscaled_box_extent().z)) == (60, 60, 80)
                assert a.get_actor_scale3d() == unreal.Vector(1, 1, 1)
                panel = next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('SecurityPanelVisual'))
                # Keep the marker on the front plane and above floor level.
                target = p['center'] - unreal.Vector(0, 0, p['height']/2 + 16) + p['normal'] * 2
                if target.z < 12:
                    target = p['center'] + p['horizontal'] * (p['width']/2 + 16) + p['normal'] * 2
                    target.z = p['center'].z - p['height']/2 + 16
                rotation = unreal.MathLibrary.make_rot_from_z(p['normal'])
                parent = panel.get_attach_parent().get_world_transform()
                panel.set_editor_properties(dict(relative_location=unreal.MathLibrary.inverse_transform_location(parent, target),
                    relative_rotation=unreal.MathLibrary.inverse_transform_rotation(parent, rotation)))
    fixtures = [a for a in actors if a.get_class().get_name().startswith('BP_HeistLight_')
                and a.get_class().get_name() != 'BP_HeistLightFixture_C']
    assigned = {}
    uncovered = []
    for p in art:
        name = p['actor'].get_name(); label = p['actor'].get_actor_label()
        owners = [a for a in fixtures if 'M03ArtLight_' + name in [str(t) for t in a.tags]
                  or 'Art_' + label in [str(t) for t in a.tags]]
        if not owners:
            # Check proximity only among artwork-purpose fixtures; do not consume corridor lights.
            pool = [a for a in fixtures if a.get_class().get_name() != 'BP_HeistLight_DropCeiling_01a_C']
            owners = [min(pool, key=lambda a: (a.get_actor_location() - p['center']).length())]
            uncovered.append(name)
        owner = min(owners, key=lambda a: (a.get_actor_location()-p['center']).length())
        assigned.setdefault(owner.get_name(), []).append(p)
    for a in fixtures:
        c = a.get_component_by_class(unreal.SpotLightComponent)
        props = dict(intensity_units=unreal.LightUnits.CANDELAS, intensity=4., use_inverse_squared_falloff=True,
                     indirect_lighting_intensity=0., volumetric_scattering_intensity=0.)
        if code != 'M03' and a.get_name() in assigned:
            group = assigned[a.get_name()]
            source = c.get_world_location()
            target = sum((p['center'] for p in group), unreal.Vector()) / len(group)
            direction = (target - source) / (target - source).length()
            angles, distances = [], []
            for p in group:
                for h in (-.5, .5):
                    for v in (-.5, .5):
                        corner = p['center'] + p['horizontal'] * (p['width']*h) + unreal.Vector(0, 0, p['height']*v)
                        delta = corner - source
                        distances.append(delta.length())
                        angles.append(math.degrees(math.acos(max(-1., min(1., unreal.MathLibrary.dot_vector_vector(direction, delta / delta.length()))))))
            # Outer cone covers complete compositions; retain falloff to avoid a hard wall blob.
            outer = min(80., max(25., math.ceil(max(angles)) + 4.))
            inner = max(12., outer - 12.)
            props.update(relative_rotation=unreal.MathLibrary.inverse_transform_rotation(c.get_attach_parent().get_world_transform(), unreal.MathLibrary.find_look_at_rotation(source, target)),
                         inner_cone_angle=inner, outer_cone_angle=outer,
                         attenuation_radius=max(600., math.ceil(max(distances)/100)*100 + 100.))
        c.set_editor_properties(props)
        for mesh in a.get_components_by_class(unreal.StaticMeshComponent):
            mesh.set_editor_property('affect_dynamic_indirect_lighting', False)
    cameras = []
    for a in actors:
        if a.get_class().get_name() == 'BP_SecurityCamera_C':
            cameras.append(a)
            a.get_component_by_class(unreal.SpotLightComponent).set_editor_properties(dict(
                intensity=2., intensity_units=unreal.LightUnits.CANDELAS, use_inverse_squared_falloff=True))
        elif isinstance(a, unreal.PointLight):
            a.get_component_by_class(unreal.PointLightComponent).set_editor_properties(dict(
                intensity=4., intensity_units=unreal.LightUnits.CANDELAS, use_inverse_squared_falloff=True))
        elif isinstance(a, unreal.PostProcessVolume) and a.unbound:
            st = a.settings
            for key, value in dict(auto_exposure_min_brightness=1., auto_exposure_max_brightness=1.,
                auto_exposure_bias=0., ambient_cubemap=unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'),
                ambient_cubemap_intensity=.005, lumen_final_gather_screen_traces=False).items():
                st.set_editor_property(key, value)
                if key != 'ambient_cubemap':
                    st.set_editor_property('override_'+key, True)
            # Adopt the approved M03 shadow floor without changing map grading.
            for key, value in dict(ambient_cubemap_tint=unreal.LinearColor(.65,.75,1,1),
                color_offset_shadows=unreal.Vector4(.0006,.0009,.0012,0),
                film_toe=.53,vignette_intensity=.3,color_correction_shadows_max=.035).items():
                st.set_editor_property(key,value);st.set_editor_property('override_'+key,True)
            a.set_editor_property('settings', st)
    return dict(artworks=len(art), cases=sum(isinstance(a, unreal.HeistPaintingDisplayCaseActor) for a in actors),
                fixtures=len(fixtures), cctv=len(cameras), assignment_fallback=uncovered,
                artwork_lights={k:[p['actor'].get_name() for p in v] for k,v in assigned.items()})


def floor_actor(label, location, extents, cell, pivot, scale, mesh, seed):
    cls = unreal.load_class(None, FLOOR_BP + '.BP_FloorTilePCG_C')
    a = ACT.spawn_actor_from_class(cls, unreal.Vector(*location))
    a.set_actor_label(label); a.set_folder_path('Environment/Floor')
    a.set_editor_properties(dict(GridExtents=unreal.Vector(*extents), CellSize=unreal.Vector(*cell),
        TilePivotOffset=unreal.Vector(*pivot), TileScale=unreal.Vector(*scale), TileMeshes=[unreal.load_asset(mesh)]))
    comp = a.get_component_by_class(unreal.PCGComponent)
    comp.set_editor_property('seed', seed)
    sync_floor_bounds(a)
    a.get_component_by_class(unreal.PCGComponent).generate_local(True)
    return a


def rows(a):
    result=[]
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        assert c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
        assert str(c.get_collision_profile_name()) == 'BlockAll'
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i, True); p=t.translation; s=t.scale3d
            result.append((round(p.x,3),round(p.y,3),round(p.z,3),round(s.x,1),round(s.y,1),round(s.z,1),c.static_mesh.get_path_name()))
    return sorted(result)


def prepare():
    if (OUT/'applied.json').exists():
        previous = json.loads((OUT/'applied.json').read_text())
        assert not previous['saved'], 'Already applied; audit before repeating'
    if (OUT/'progress.json').exists():
        completed=json.loads((OUT/'progress.json').read_text())
        RESULT['maps']=completed['maps']
        STATE['index']=sum('floor_instances' in completed['maps'].get(n,{}) for n in MAPS)
    RESULT['asset_inventory_before']=sorted(str(p.relative_to(ROOT/'Content')) for p in (ROOT/'Content').rglob('*') if p.suffix in ('.uasset','.umap'))
    for name in MAPS:
        before=json.loads((OUT/(name+'_before.json')).read_text())
        if MAPS.index(name)>=STATE['index']:
            assert sha(ROOT/'Content/Maps'/(name+'.umap')) == before['sha256'], 'Map changed since audit: '+name
        backup('/Game/Maps/'+name)
    extend_floor_graph(); case_panel(); common_defaults()


def load_next():
    name=MAPS[STATE['index']]; code=name[:3]
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/'+name)
    actors=list(ACT.get_all_level_actors())
    validate_authored_transforms(actors)
    STATE['before_transforms']={a.get_name():placement(a) for a in actors}
    RESULT['maps'][name]=artwork_and_lighting(actors,code)
    STATE['floors']=[]; STATE['old']=[]
    if code=='M01':
        a=next(a for a in actors if a.get_class().get_name()=='BP_FloorTilePCG_C')
        assert len(rows(a))==660
        STATE['m01_rows']=rows(a)
        sync_floor_bounds(a)
        STATE['phase']='save'; return
    if code=='M02':
        old=next(a for a in actors if isinstance(a,unreal.StaticMeshActor) and a.get_name()=='StaticMeshActor_25')
        assert old.static_mesh_component.static_mesh.get_path_name().startswith('/Game/Assets/MapAssets/ConferenceRoom/Meshes/Floor/SM_Floor_01.')
        assert old.get_actor_scale3d()==unreal.Vector(36,29.4,1)
        STATE['old']=[old]
        mesh='/Game/Assets/MapAssets/ConferenceRoom/Meshes/Floor/SM_Floor_01'
        STATE['floors']=[floor_actor('M02_FloorTiles_PCG',(0,-70,0),(5400,4350,0),(300,300,100),(-150,-150,0),(1,1,1),mesh,92702),
            floor_actor('M02_FloorTiles_Edge_PCG',(0,4340,0),(5400,60,0),(300,120,100),(-150,-60,0),(1,.4,1),mesh,92702)]
        STATE['expected']=sorted([(float(x),float(y),0.,1.,1.,1.,unreal.load_asset(mesh).get_path_name()) for x in range(-5400,5101,300) for y in range(-4420,3981,300)] +
            [(float(x),4280.,0.,1.,.4,1.,unreal.load_asset(mesh).get_path_name()) for x in range(-5400,5101,300)])
    else:
        mesh='/Game/AIUE5_vol10_01/Mesh/SM_AI_vol10_01_stone_tile_1_1'
        STATE['old']=[a for a in actors if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_path_name()==unreal.load_asset(mesh).get_path_name() and a.get_actor_location().z==-2]
        assert len(STATE['old'])==192
        STATE['expected']=sorted([(round(a.get_actor_location().x,3),round(a.get_actor_location().y,3),-2.,1.,.9,4.,unreal.load_asset(mesh).get_path_name()) for a in STATE['old']])
        STATE['floors']=[floor_actor('M03_FloorTiles_PCG',(0,0,-2),(5280,3600,0),(660,600,100),(0,0,0),(1,.9,4),mesh,92703)]
    STATE.update(phase='generate',stamp=time.monotonic())


def generation_done():
    for a in STATE['floors']:
        c=a.get_component_by_class(unreal.PCGComponent)
        if c.get_editor_property('generation_in_progress') or not c.generated:
            assert time.monotonic()-STATE['stamp']<180, 'PCG generation timeout'
            return False
    return True


def save():
    name=MAPS[STATE['index']]
    removed={a.get_name() for a in STATE['old']}
    for a in STATE['old']:assert ACT.destroy_actor(a)
    for a in ACT.get_all_level_actors():
        if a.get_name() in STATE['before_transforms']:
            assert placement(a)==STATE['before_transforms'][a.get_name()], 'Layout changed '+a.get_name()
    validate_authored_transforms(ACT.get_all_level_actors())
    assert unreal.EditorLoadingAndSavingUtils.save_map(ED.get_editor_world(),'/Game/Maps/'+name)
    data=RESULT['maps'][name]
    data.update(removed_floor_actors=sorted(removed),floor_instances=660 if name.startswith('M01') else len(STATE['expected']),
                layout_transforms_preserved=True,deterministic=True,sha256=sha(ROOT/'Content/Maps'/(name+'.umap')))
    (OUT/'progress.json').write_text(json.dumps(RESULT,indent=2))
    STATE['index']+=1; STATE['phase']='load'


def finish(error=None):
    if error:RESULT['errors'].append(error)
    RESULT['saved']=not RESULT['errors'] and STATE['index']==3
    after=sorted(str(p.relative_to(ROOT/'Content')) for p in (ROOT/'Content').rglob('*') if p.suffix in ('.uasset','.umap'))
    RESULT['new_assets']=sorted(set(after)-set(RESULT.get('asset_inventory_before',after)))
    (OUT/'applied.json').write_text(json.dumps(RESULT,indent=2))
    unreal.unregister_slate_post_tick_callback(STATE['callback'])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    if STATE['busy']:return
    STATE['busy']=True
    try:
        if STATE['phase']=='prepare':prepare();STATE['phase']='load'
        elif STATE['phase']=='load':
            if STATE['index']==3:finish();return
            load_next()
        elif STATE['phase'] in ('generate','repeat') and generation_done():
            got=sorted(r for a in STATE['floors'] for r in rows(a))
            assert got==STATE['expected'], ('Tile transform mismatch',len(got),len(STATE['expected']),list(set(got)-set(STATE['expected']))[:4])
            if STATE['phase']=='generate':
                STATE['first']=got
                for a in STATE['floors']:a.get_component_by_class(unreal.PCGComponent).generate_local(True)
                STATE.update(phase='repeat',stamp=time.monotonic())
            else:
                assert got==STATE['first'],'Seed changed generated tiles'
                STATE['phase']='save'
        elif STATE['phase']=='save':save()
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['callback']=unreal.register_slate_post_tick_callback(tick)
