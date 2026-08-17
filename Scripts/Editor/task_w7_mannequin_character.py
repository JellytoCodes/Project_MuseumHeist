import unreal

from editor_toolset.toolsets.blueprint import BlueprintTools


PLAYER_BLUEPRINT_PATH = "/Game/Blueprints/Player/BP_HeistPlayerCharacter"
MANNY_MESH_PATH = "/Game/Assets/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_ANIM_BLUEPRINT_PATH = "/Game/Assets/Mannequins/Anims/Unarmed/ABP_Unarmed"
MANNY_ANIM_BLUEPRINT_CLASS_PATH = (
    "/Game/Assets/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"
)

# The task is idempotent: it re-applies the approved baseline and verifies the
# saved Blueprint CDO on every run.
APPLY_CHANGES = True


def log(message):
    unreal.log(f"[W7Mannequin] {message}")


def fail(message):
    unreal.log_error(f"[W7Mannequin] {message}")
    raise RuntimeError(message)


def load_required(path, expected_type=None):
    asset = unreal.load_asset(path)
    if not asset:
        fail(f"Missing asset: {path}")
    if expected_type is not None and not isinstance(asset, expected_type):
        fail(
            f"Unexpected type: Path={path} "
            f"Expected={expected_type.__name__} Actual={type(asset).__name__}"
        )
    return asset


def resolve_mesh_component(cdo):
    for property_name in ("mesh", "skeletal_mesh_component"):
        try:
            component = cdo.get_editor_property(property_name)
            if isinstance(component, unreal.SkeletalMeshComponent):
                return component, property_name
        except Exception:
            pass

    try:
        component = cdo.get_mesh()
        if isinstance(component, unreal.SkeletalMeshComponent):
            return component, "get_mesh"
    except Exception:
        pass

    fail("Unable to resolve inherited SkeletalMeshComponent from character CDO")


def main():
    blueprint = load_required(PLAYER_BLUEPRINT_PATH, unreal.Blueprint)
    mesh_asset = load_required(MANNY_MESH_PATH, unreal.SkeletalMesh)
    anim_blueprint = load_required(MANNY_ANIM_BLUEPRINT_PATH, unreal.AnimBlueprint)
    anim_class = unreal.load_class(None, MANNY_ANIM_BLUEPRINT_CLASS_PATH)
    if not anim_class:
        fail(f"Missing AnimBlueprint generated class: {MANNY_ANIM_BLUEPRINT_CLASS_PATH}")

    cdo = unreal.get_default_object(blueprint.generated_class())
    if not cdo:
        fail(f"Missing generated CDO: {PLAYER_BLUEPRINT_PATH}")

    mesh_component, mesh_source = resolve_mesh_component(cdo)
    current_mesh = mesh_component.get_editor_property("skeletal_mesh_asset")
    current_anim = mesh_component.get_editor_property("anim_class")
    current_location = mesh_component.get_editor_property("relative_location")
    current_rotation = mesh_component.get_editor_property("relative_rotation")
    socket_name = cdo.get_editor_property("first_person_camera_socket_name")
    socket_offset = cdo.get_editor_property("first_person_camera_socket_offset")

    log(
        "PROBE "
        f"MeshSource={mesh_source} "
        f"CurrentMesh={current_mesh.get_path_name() if current_mesh else 'None'} "
        f"CurrentAnim={current_anim.get_path_name() if current_anim else 'None'} "
        f"Location={current_location} Rotation={current_rotation} "
        f"CameraSocket={socket_name} CameraOffset={socket_offset} "
        f"TargetMesh={mesh_asset.get_path_name()} "
        f"TargetAnim={anim_class.get_path_name()}"
    )

    if not APPLY_CHANGES:
        log("RESULT ApplyChanges=False Result=PASS")
        return

    BlueprintTools.compile_blueprint(anim_blueprint, warnings_as_errors=False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        anim_blueprint, only_if_is_dirty=False
    ):
        fail(f"AnimBlueprint save failed: {MANNY_ANIM_BLUEPRINT_PATH}")

    mesh_component.set_editor_property("skeletal_mesh_asset", mesh_asset)
    mesh_component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    mesh_component.set_editor_property("anim_class", anim_class)
    mesh_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -90.0))
    # Unreal Python's positional Rotator constructor is Roll, Pitch, Yaw.
    mesh_component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, -90.0))
    cdo.set_editor_property("first_person_camera_socket_name", "head")
    cdo.set_editor_property("first_person_camera_socket_offset", unreal.Vector(0.0, 0.0, 0.0))

    BlueprintTools.compile_blueprint(blueprint, warnings_as_errors=False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        fail(f"Blueprint save failed: {PLAYER_BLUEPRINT_PATH}")

    saved_blueprint = load_required(PLAYER_BLUEPRINT_PATH, unreal.Blueprint)
    saved_cdo = unreal.get_default_object(saved_blueprint.generated_class())
    saved_mesh_component, _ = resolve_mesh_component(saved_cdo)
    saved_mesh = saved_mesh_component.get_editor_property("skeletal_mesh_asset")
    saved_anim = saved_mesh_component.get_editor_property("anim_class")
    saved_location = saved_mesh_component.get_editor_property("relative_location")
    saved_rotation = saved_mesh_component.get_editor_property("relative_rotation")
    saved_socket = saved_cdo.get_editor_property("first_person_camera_socket_name")

    if saved_mesh != mesh_asset:
        fail(f"Saved Mesh mismatch: {saved_mesh}")
    if saved_anim != anim_class:
        fail(f"Saved AnimClass mismatch: {saved_anim}")
    if str(saved_socket) != "head":
        fail(f"Saved camera socket mismatch: {saved_socket}")
    if abs(saved_location.z + 90.0) > 0.01:
        fail(f"Saved mesh Z mismatch: {saved_location.z}")
    if abs(saved_rotation.yaw + 90.0) > 0.01:
        fail(f"Saved mesh Yaw mismatch: {saved_rotation.yaw}")

    log(
        "RESULT ApplyChanges=True Result=PASS "
        f"Mesh={saved_mesh.get_path_name()} "
        f"AnimClass={saved_anim.get_path_name()} "
        f"Location={saved_location} Rotation={saved_rotation} "
        f"CameraSocket={saved_socket}"
    )


main()
