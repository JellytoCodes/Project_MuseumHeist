"""Retired one-shell migration; use apply_art_detail.py for the four fixture types."""
raise RuntimeError('Retired one-shell lighting migration. Use apply_art_detail.py.')
import json
from pathlib import Path
import unreal

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
def objects(bp):
    return [(h, lib.get_object(lib.get_data(h))) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]

bp = unreal.load_asset('/Game/Blueprints/Environment/BP_HeistLightFixture')
bp.modify()
items = objects(bp)
root = next(h for h,o in items if o.get_name() == 'DefaultSceneRoot_GEN_VARIABLE')
mesh_handle, mesh = next((h,o) for h,o in items if isinstance(o,unreal.StaticMeshComponent))
scale_handle = next((h for h,o in items if o.get_name() == 'FixtureVisualScale_GEN_VARIABLE'), None)
if scale_handle is None:
    scale_handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.SceneComponent, blueprint_context=bp))
    assert lib.is_handle_valid(scale_handle), str(reason)
    sub.rename_subobject(scale_handle,'FixtureVisualScale')
    # Keep each mesh's authored relative transform. The new parent doubles both
    # size and pivot correction, preserving the mesh's ceiling contact point.
    original = mesh.get_relative_transform()
    assert sub.attach_subobject(scale_handle,mesh_handle)
    mesh.set_relative_transform(original,False,True)
scale = lib.get_object(lib.get_data(scale_handle))
scale.set_editor_property('relative_scale3d',unreal.Vector(2,2,2))
scale.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

laser = unreal.load_asset('/Game/Blueprints/World/Actors/Security/BP_LaserBarrier')
laser.modify()
cdo = unreal.get_default_object(laser.generated_class())
effect = unreal.load_asset('/Game/Assets/VFX/NS_HeistSecurityLaser')
assert effect
cdo.set_editor_property('beam_effect',effect)
cdo.set_editor_property('beam_width',1.2)
# This Actor is placed at floor level. Lift the existing 240 cm high query box
# so its lower face is at the floor; all beam rows derive from that same box.
box = next(o for h,o in objects(laser) if isinstance(o,unreal.BoxComponent))
box.set_editor_property('relative_location',unreal.Vector(0,0,120))
unreal.BlueprintEditorLibrary.compile_blueprint(laser)
assert unreal.EditorAssetLibrary.save_loaded_asset(laser)
out=Path(unreal.Paths.project_saved_dir())/'FixtureLaser';out.mkdir(exist_ok=True)
(out/'applied.json').write_text(json.dumps({'fixture_parent_scale':[2,2,2],
    'laser_system':effect.get_path_name(),'query_box_center_z':120,'map_saves':0},indent=2))
