import json
import os
import time

import unreal

from editor_toolset.toolsets.blueprint import BlueprintTools


PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())

DATA_TABLE_IMPORTS = {
    "/Game/Data/DataTable/DT_ForgeryTemplate": "DT_ForgeryTemplateRow.json",
    "/Game/Data/DataTable/DT_GuardData": "DT_GuardData.json",
    "/Game/Data/DataTable/DT_LootData": "DT_LootDataRow.json",
    "/Game/Data/DataTable/DT_ObjectAssemblyPart": "DT_ObjectAssemblyPartRow.json",
    "/Game/Data/DataTable/DT_ObjectAssemblyTemplate": "DT_ObjectAssemblyTemplateRow.json",
    "/Game/Data/DataTable/DT_ArtifactData": "DT_ArtifactDataRow.json",
}

MAPS_TO_RESAVE = (
    "/Game/Maps/M01_ClassicalPrototype",
    "/Game/Maps/M02_MoonlitPrototype",
    "/Game/Maps/M03_GlasshousePrototype",
    "/Game/Maps/SandBoxMap",
)

REDIRECTOR_CANDIDATES = (
    "/Game/Blueprints/World/Actors/Loot/BP_DisplayCase",
    "/Game/Blueprints/World/Actors/Loot/BP_LootRoyalCrown",
    "/Game/Data/DataTable/DT_LootDataRow",
    "/Game/StarterContent/Architecture/Floor_400x400",
    "/Game/StarterContent/Architecture/Pillar_50x500",
    "/Game/StarterContent/Architecture/Wall_400x400",
    "/Game/StarterContent/Architecture/Wall_Door_400x300",
    "/Game/StarterContent/Architecture/Wall_Door_400x400",
    "/Game/StarterContent/Architecture/Wall_Window_400x400",
    "/Game/StarterContent/Materials/M_Ground_Moss",
)

ZERO_REFERENCE_CANDIDATES = (
    "/Game/Blueprints/World/Materials/M_HeistPaintingSurface",
    "/Game/Assets/Art/ObjectAssembly/Prototype/SM_ObjectAssembly_Prototype_Core",
)

M01_OBJECT_CASE_RENAMES = {
    "ObjectCase_M01_Test_Sculpture": (
        "ObjectCase_M01_Optional_Sculpture_01",
        "M01_Optional_ObjectCase_Sculpture_01",
    ),
    "ObjectCase_M01_Test_Ceramic": (
        "ObjectCase_M01_Optional_Ceramic_01",
        "M01_Optional_ObjectCase_Ceramic_01",
    ),
}


def _dependency_options():
    options = unreal.AssetRegistryDependencyOptions()
    for property_name in (
        "include_soft_package_references",
        "include_hard_package_references",
        "include_searchable_names",
        "include_soft_management_references",
        "include_hard_management_references",
    ):
        try:
            options.set_editor_property(property_name, True)
        except Exception:
            pass
    return options


def _scan_game_assets(registry):
    try:
        registry.scan_paths_synchronous(["/Game"], True, True)
    except TypeError:
        registry.scan_paths_synchronous(["/Game"], True)
    return list(registry.get_assets_by_path("/Game", True, True))


def _direct_referencers(registry, package_path, all_assets):
    options = _dependency_options()
    referencers = set(
        str(path) for path in (registry.get_referencers(package_path, options) or [])
    )
    # ObjectRedirector reverse lookups have returned incomplete results in UE 5.8.
    # Also inspect every /Game package's direct dependency list before deleting.
    for asset_data in all_assets:
        source_package = str(asset_data.package_name)
        if source_package == package_path:
            continue
        dependencies = registry.get_dependencies(source_package, options) or []
        if any(str(dependency) == package_path for dependency in dependencies):
            referencers.add(source_package)
    return sorted(referencers)


def _get_asset_data(registry, package_path):
    asset_data = list(registry.get_assets_by_package_name(package_path, True))
    return asset_data[0] if asset_data else None


def _asset_object_path(asset_data):
    """Build the exact object path without relying on SoftObjectPath.__str__()."""
    return f"{asset_data.package_name}.{asset_data.asset_name}"


def _package_files_on_disk(package_path):
    if not package_path.startswith("/Game/"):
        return []
    relative_path = package_path[len("/Game/") :].replace("/", os.sep)
    package_base = os.path.join(PROJECT_DIR, "Content", relative_path)
    return [
        path
        for path in (package_base + ".uasset", package_base + ".umap")
        if os.path.isfile(path)
    ]


def _import_data_tables():
    imported = []
    for asset_path, source_name in DATA_TABLE_IMPORTS.items():
        source_path = os.path.join(PROJECT_DIR, "DataTableImports", source_name)
        data_table = unreal.load_asset(asset_path)
        if not isinstance(data_table, unreal.DataTable):
            raise RuntimeError(f"Missing DataTable: {asset_path}")
        if not os.path.isfile(source_path):
            raise RuntimeError(f"Missing DataTable source: {source_path}")
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, source_path):
            raise RuntimeError(f"DataTable JSON import failed: {asset_path} <- {source_path}")

        import_data = data_table.get_editor_property("asset_import_data")
        if not isinstance(import_data, unreal.AssetImportData):
            raise RuntimeError(f"Missing AssetImportData: {asset_path}")
        import_data.scripted_add_filename(source_path, 0, "JSON Source")

        if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, only_if_is_dirty=False):
            raise RuntimeError(f"DataTable save failed: {asset_path}")
        imported.append((asset_path, source_path, data_table))

    _validate_data_tables(imported)
    return [asset_path for asset_path, _source_path, _table in imported]


def _validate_data_tables(imported):
    by_path = {asset_path: (source_path, table) for asset_path, source_path, table in imported}

    guard_rows = {str(name) for name in by_path["/Game/Data/DataTable/DT_GuardData"][1].get_row_names()}
    expected_guard_rows = {"Guard_Alert_Low", "Guard_Alert_Medium", "Guard_Alert_High"}
    if guard_rows != expected_guard_rows:
        raise RuntimeError(f"Guard DataTable row drift: actual={sorted(guard_rows)}")

    loot_rows = {str(name) for name in by_path["/Game/Data/DataTable/DT_LootData"][1].get_row_names()}
    required_loot_rows = {
        "Loot_RoyalCrown",
        "Loot_Painting",
        "Loot_AncientSword",
        "Loot_GoldenVase",
        "Loot_JewelNecklace",
    }
    if not required_loot_rows.issubset(loot_rows):
        raise RuntimeError(f"Loot DataTable missing required rows: {sorted(required_loot_rows - loot_rows)}")

    for asset_path, (source_path, data_table) in by_path.items():
        exported = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(data_table)
        if asset_path == "/Game/Data/DataTable/DT_ForgeryTemplate" and "TimeoutPenalty" in exported:
            raise RuntimeError("DT_ForgeryTemplate still serializes TimeoutPenalty")
        if asset_path in {
            "/Game/Data/DataTable/DT_ObjectAssemblyPart",
            "/Game/Data/DataTable/DT_ObjectAssemblyTemplate",
            "/Game/Data/DataTable/DT_ArtifactData",
        } and any(token in exported for token in ("_Prototype", "_Test_", "/Game/Art/")):
            raise RuntimeError(f"Object Assembly legacy row/path remains in {asset_path}")

        import_data = data_table.get_editor_property("asset_import_data")
        filenames = [os.path.normcase(os.path.abspath(path)) for path in import_data.extract_filenames()]
        expected_source = os.path.normcase(os.path.abspath(source_path))
        if not filenames or filenames[0] != expected_source:
            raise RuntimeError(
                f"DataTable import source drift: {asset_path} expected={expected_source} actual={filenames}"
            )


def _set_actor_folder(actor, folder_path):
    if hasattr(actor, "set_folder_path"):
        actor.set_folder_path(folder_path)
        return
    try:
        actor.set_editor_property("folder_path", unreal.Name(folder_path))
    except Exception as error:
        unreal.log_warning(f"Could not update actor folder for {actor.get_name()}: {error}")


def _clean_m01_case_names(world):
    if not world or world.get_path_name().split(".")[0] != "/Game/Maps/M01_ClassicalPrototype":
        return 0
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    renamed = 0
    painting_case_class = getattr(unreal, "HeistPaintingDisplayCaseActor", None)
    object_case_class = getattr(unreal, "HeistObjectDisplayCaseActor", None)
    resolved_object_case_ids = set()
    required_painting_label_found = False
    for actor in actor_subsystem.get_all_level_actors():
        if object_case_class and isinstance(actor, object_case_class):
            case_id = str(actor.get_editor_property("object_case_id"))
            replacement = M01_OBJECT_CASE_RENAMES.get(case_id)
            if replacement:
                new_case_id, new_label = replacement
                actor.modify()
                actor.set_editor_property("object_case_id", unreal.Name(new_case_id))
                actor.set_actor_label(new_label, mark_dirty=True)
                _set_actor_folder(actor, "M01_ContractExhibits")
                renamed += 1
                case_id = new_case_id
            resolved_object_case_ids.add(case_id)
        elif painting_case_class and isinstance(actor, painting_case_class):
            if actor.get_actor_label() == "M01_Required_PaintingCase":
                required_painting_label_found = True
            elif "M01_Test_PaintingCase" in actor.get_actor_label() or "M01_Test_PaintingCase" in actor.get_name():
                actor.modify()
                actor.set_actor_label("M01_Required_PaintingCase", mark_dirty=True)
                _set_actor_folder(actor, "M01_ContractExhibits")
                renamed += 1
                required_painting_label_found = True
    expected_object_case_ids = {replacement[0] for replacement in M01_OBJECT_CASE_RENAMES.values()}
    if not expected_object_case_ids.issubset(resolved_object_case_ids) or not required_painting_label_found:
        raise RuntimeError(
            "M01 release naming validation failed: "
            f"objectCases={sorted(resolved_object_case_ids)} painting={required_painting_label_found}"
        )
    return renamed


def _resave_maps():
    saved = []
    renamed_case_count = 0
    for map_path in MAPS_TO_RESAVE:
        world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        if not world:
            raise RuntimeError(f"Map load failed: {map_path}")
        renamed_case_count += _clean_m01_case_names(world)
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
            raise RuntimeError(f"Map save failed: {map_path}")
        saved.append(map_path)
    return saved, renamed_case_count


def _remove_hud_score_widget():
    blueprint_path = "/Game/Blueprints/UI/HUD/WBP_HeistHUD"
    widget_blueprint = unreal.load_asset(blueprint_path)
    if widget_blueprint is None:
        raise RuntimeError(f"Missing Widget Blueprint: {blueprint_path}")
    widget_blueprint_ref = {"refPath": widget_blueprint.get_path_name()}

    def execute_umg_tool(tool_name, payload):
        result = unreal.ToolsetRegistry.execute_tool(
            "UMGToolSet.UMGToolSet", tool_name, json.dumps(payload)
        )
        for _index in range(100):
            if result.is_complete:
                break
            time.sleep(0.01)
        if not result.is_complete:
            raise RuntimeError(f"UMGToolSet timed out: {tool_name}")
        if result.error:
            raise RuntimeError(f"UMGToolSet {tool_name} failed: {result.error}")
        if not result.value:
            raise RuntimeError(f"UMGToolSet {tool_name} returned no value")
        return json.loads(result.value).get("returnValue")

    tree = execute_umg_tool("GetWidgets", {"WidgetBlueprint": widget_blueprint_ref})
    matches = [info for info in tree["widgets"] if info["widgetName"] == "ScoreText"]
    if len(matches) > 1:
        raise RuntimeError(f"Multiple ScoreText widgets found: {len(matches)}")
    if matches:
        if matches[0]["bInherited"]:
            raise RuntimeError("ScoreText is inherited and cannot be removed from WBP_HeistHUD")
        if not execute_umg_tool(
            "RemoveWidget",
            {"WidgetBlueprint": widget_blueprint_ref, "Widget": matches[0]["widget"]},
        ):
            raise RuntimeError("UMGToolSet failed to remove ScoreText")
    if not execute_umg_tool("CompileWidgetBlueprint", {"WidgetBlueprint": widget_blueprint_ref}):
        raise RuntimeError("WBP_HeistHUD compile failed after ScoreText cleanup")
    if not unreal.EditorAssetLibrary.save_loaded_asset(widget_blueprint, only_if_is_dirty=False):
        raise RuntimeError("WBP_HeistHUD save failed after ScoreText cleanup")

    saved_tree = execute_umg_tool("GetWidgets", {"WidgetBlueprint": widget_blueprint_ref})
    remaining = [info for info in saved_tree["widgets"] if info["widgetName"] == "ScoreText"]
    if remaining:
        raise RuntimeError("ScoreText still exists after WBP_HeistHUD save")
    return bool(matches)


def _compile_native_shell_blueprints():
    compiled = []
    for blueprint_path in (
        "/Game/Blueprints/Player/BP_HeistPlayerCharacter",
    ):
        blueprint = unreal.load_asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError(f"Missing Blueprint shell: {blueprint_path}")
        BlueprintTools.compile_blueprint(blueprint, warnings_as_errors=False)
        if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Blueprint shell save failed: {blueprint_path}")
        compiled.append(blueprint_path)
    return compiled


def _delete_verified_assets(registry):
    all_assets = _scan_game_assets(registry)
    deleted = []
    for asset_path in REDIRECTOR_CANDIDATES + ZERO_REFERENCE_CANDIDATES:
        asset_data = _get_asset_data(registry, asset_path)
        if asset_data is None:
            continue
        is_redirector_candidate = asset_path in REDIRECTOR_CANDIDATES
        asset_class = str(asset_data.asset_class_path.asset_name)
        if is_redirector_candidate and asset_class != "ObjectRedirector":
            raise RuntimeError(f"Expected ObjectRedirector at {asset_path}, actual={asset_class}")
        referencers = _direct_referencers(registry, asset_path, all_assets)
        if referencers:
            raise RuntimeError(f"Asset still has referencers: {asset_path} <- {referencers}")
        attempted_objects = []
        for _attempt in range(8):
            package_entries = list(registry.get_assets_by_package_name(asset_path, True))
            if not package_entries:
                break
            deleted_object = False
            for entry in package_entries:
                object_path = _asset_object_path(entry)
                attempted_objects.append(object_path)
                if unreal.EditorAssetLibrary.delete_asset(object_path):
                    deleted_object = True
                    _scan_game_assets(registry)
                    break
            if not deleted_object:
                raise RuntimeError(
                    f"Asset deletion failed: {asset_path} objects="
                    f"{[_asset_object_path(entry) for entry in package_entries]}"
                )
        if list(registry.get_assets_by_package_name(asset_path, True)):
            raise RuntimeError(
                f"Asset package still registered after repeated object deletion: "
                f"{asset_path} attempted={attempted_objects}"
            )
        remaining_package_files = _package_files_on_disk(asset_path)
        if remaining_package_files:
            raise RuntimeError(
                "Asset Registry removed the object but the package still exists on disk: "
                f"{asset_path} files={remaining_package_files}"
            )
        deleted.append(asset_path)
        all_assets = _scan_game_assets(registry)
        if _get_asset_data(registry, asset_path) is not None:
            raise RuntimeError(f"Asset package still registered after deletion: {asset_path}")
    return deleted


def _validate_no_legacy_dependencies(registry):
    all_assets = _scan_game_assets(registry)
    failures = []
    cleanup_targets = REDIRECTOR_CANDIDATES + ZERO_REFERENCE_CANDIDATES
    for legacy_path in cleanup_targets:
        if _get_asset_data(registry, legacy_path) is not None:
            failures.append(f"CleanupTargetStillExists:{legacy_path}")
        remaining_package_files = _package_files_on_disk(legacy_path)
        if remaining_package_files:
            failures.append(
                f"CleanupTargetFileStillExists:{legacy_path}<-{remaining_package_files}"
            )
    for legacy_path in REDIRECTOR_CANDIDATES:
        referencers = _direct_referencers(registry, legacy_path, all_assets)
        if referencers:
            failures.append(f"LegacyDependency:{legacy_path}<-{referencers}")
    if failures:
        raise RuntimeError("Legacy asset validation failed: " + ", ".join(failures))
    return list(cleanup_targets)


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    imported_tables = _import_data_tables()
    saved_maps, renamed_case_count = _resave_maps()
    score_widget_removed = _remove_hud_score_widget()
    compiled_blueprints = _compile_native_shell_blueprints()
    deleted_assets = _delete_verified_assets(registry)
    verified_absent_assets = _validate_no_legacy_dependencies(registry)
    result = {
        "result": "PASS",
        "data_tables": imported_tables,
        "maps": saved_maps,
        "m01_release_names": renamed_case_count,
        "score_widget_removed": score_widget_removed,
        "compiled_blueprints": compiled_blueprints,
        "deleted_assets": deleted_assets,
        "verified_absent_assets": verified_absent_assets,
    }
    unreal.log(f"W9 legacy cleanup PASS: {result}")
    print(result)


main()
