import unreal


APPLY_CHANGES = True

MOVE_MAP = {
    "/Game/Blueprints/Audio/W7/SMX_HeistStunLowPass": "/Game/Assets/Audio/W7/SMX_HeistStunLowPass",
    "/Game/Blueprints/Audio/W7/SW_HeistAlarmLoop": "/Game/Assets/Audio/W7/SW_HeistAlarmLoop",
    "/Game/Blueprints/Audio/W7/SW_HeistArrested": "/Game/Assets/Audio/W7/SW_HeistArrested",
    "/Game/Blueprints/Audio/W7/SW_HeistCarryFootstep": "/Game/Assets/Audio/W7/SW_HeistCarryFootstep",
    "/Game/Blueprints/Audio/W7/SW_HeistHeavyFootstep": "/Game/Assets/Audio/W7/SW_HeistHeavyFootstep",
    "/Game/Blueprints/Audio/W7/SW_HeistRescue": "/Game/Assets/Audio/W7/SW_HeistRescue",
    "/Game/Blueprints/Audio/W7/SW_HeistSuspenseLoop": "/Game/Assets/Audio/W7/SW_HeistSuspenseLoop",
    "/Game/Blueprints/Guard/ST_Guard": "/Game/Assets/AI/StateTrees/ST_Guard",
    "/Game/Blueprints/Player/Input/IA_ForgeryCancel": "/Game/Assets/Input/IA_ForgeryCancel",
    "/Game/Blueprints/Player/Input/IA_Interact": "/Game/Assets/Input/IA_Interact",
    "/Game/Blueprints/Player/Input/IA_Inventory": "/Game/Assets/Input/IA_Inventory",
    "/Game/Blueprints/Player/Input/IA_Look": "/Game/Assets/Input/IA_Look",
    "/Game/Blueprints/Player/Input/IA_Map": "/Game/Assets/Input/IA_Map",
    "/Game/Blueprints/Player/Input/IA_Move": "/Game/Assets/Input/IA_Move",
    "/Game/Blueprints/Player/Input/IA_Sprint": "/Game/Assets/Input/IA_Sprint",
    "/Game/Blueprints/Player/Input/IMC_Default": "/Game/Assets/Input/IMC_Default",
    "/Game/Blueprints/Player/Input/IMC_Forgery": "/Game/Assets/Input/IMC_Forgery",
    "/Game/Blueprints/Player/Input/IMC_Inventory": "/Game/Assets/Input/IMC_Inventory",
    "/Game/Blueprints/Player/Input/IMC_Map": "/Game/Assets/Input/IMC_Map",
    "/Game/Blueprints/UI/Fonts/F_TENADA": "/Game/Assets/UI/Fonts/F_TENADA",
    "/Game/Blueprints/UI/Fonts/FF_TENADA": "/Game/Assets/UI/Fonts/FF_TENADA",
    "/Game/Blueprints/UI/Map/T_FloorPlan_M01": "/Game/Assets/UI/Map/T_FloorPlan_M01",
    "/Game/Blueprints/UI/Map/T_FloorPlan_M02": "/Game/Assets/UI/Map/T_FloorPlan_M02",
    "/Game/Blueprints/UI/Map/T_FloorPlan_M03": "/Game/Assets/UI/Map/T_FloorPlan_M03",
    "/Game/Blueprints/UI/Status/T_HeistStatus_Arrested": "/Game/Assets/UI/Status/T_HeistStatus_Arrested",
    "/Game/Blueprints/UI/Status/T_HeistStatus_CarryingOriginal": "/Game/Assets/UI/Status/T_HeistStatus_CarryingOriginal",
    "/Game/Blueprints/UI/Status/T_HeistStatus_Heavy": "/Game/Assets/UI/Status/T_HeistStatus_Heavy",
    "/Game/Blueprints/UI/Status/T_HeistStatus_Stunned": "/Game/Assets/UI/Status/T_HeistStatus_Stunned",
    "/Game/Blueprints/World/Actors/Loot/Materials/M_HeistPaintingSurface": "/Game/Assets/Art/SurfaceForgery/Materials/M_HeistPaintingSurface",
}

ALLOWED_BLUEPRINT_CLASSES = {
    "Blueprint",
    "WidgetBlueprint",
}


def log(message):
    unreal.log(f"[ContentBoundary] {message}")


def asset_class_name(asset_data):
    return str(asset_data.asset_class_path.asset_name)


def audit_blueprints_folder():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game/Blueprints"], True)
    assets = registry.get_assets_by_path("/Game/Blueprints", recursive=True)
    non_blueprints = []
    for asset_data in assets:
        class_name = asset_class_name(asset_data)
        if class_name not in ALLOWED_BLUEPRINT_CLASSES:
            non_blueprints.append(
                (str(asset_data.package_name), str(asset_data.asset_name), class_name)
            )
    non_blueprints.sort()
    for package_name, asset_name, class_name in non_blueprints:
        log(f"NON_BLUEPRINT Class={class_name} Asset={package_name}.{asset_name}")
    return non_blueprints


def validate_move_map():
    errors = []
    ready = []
    already_moved = []
    for source_path, destination_path in sorted(MOVE_MAP.items()):
        source_exists = unreal.EditorAssetLibrary.does_asset_exist(source_path)
        destination_exists = unreal.EditorAssetLibrary.does_asset_exist(destination_path)
        if source_exists and destination_exists:
            errors.append(f"Source and destination both exist: {source_path} -> {destination_path}")
        elif source_exists:
            ready.append((source_path, destination_path))
        elif destination_exists:
            already_moved.append((source_path, destination_path))
        else:
            errors.append(f"Missing source and destination: {source_path} -> {destination_path}")
    return errors, ready, already_moved


def apply_moves(ready):
    failures = []
    for source_path, destination_path in ready:
        destination_directory = destination_path.rsplit("/", 1)[0]
        unreal.EditorAssetLibrary.make_directory(destination_directory)
        if not unreal.EditorAssetLibrary.rename_asset(source_path, destination_path):
            failures.append(f"Rename failed: {source_path} -> {destination_path}")
        else:
            log(f"MOVED {source_path} -> {destination_path}")
    return failures


def main():
    log(f"START ApplyChanges={APPLY_CHANGES} PlannedMoves={len(MOVE_MAP)}")
    before = audit_blueprints_folder()
    errors, ready, already_moved = validate_move_map()

    for error in errors:
        unreal.log_error(f"[ContentBoundary] {error}")
    for source_path, destination_path in ready:
        log(f"PLAN {source_path} -> {destination_path}")
    for source_path, destination_path in already_moved:
        log(f"ALREADY_MOVED {source_path} -> {destination_path}")

    failures = []
    if APPLY_CHANGES and not errors:
        failures = apply_moves(ready)
        for failure in failures:
            unreal.log_error(f"[ContentBoundary] {failure}")

    after = audit_blueprints_folder() if APPLY_CHANGES and not failures else before
    log(
        "RESULT "
        f"ApplyChanges={APPLY_CHANGES} "
        f"Ready={len(ready)} AlreadyMoved={len(already_moved)} "
        f"Errors={len(errors)} Failures={len(failures)} "
        f"NonBlueprintBefore={len(before)} NonBlueprintAfter={len(after)}"
    )

    if errors or failures:
        raise RuntimeError("Content boundary migration failed. Review Output Log.")


if __name__ == "__main__":
    main()
