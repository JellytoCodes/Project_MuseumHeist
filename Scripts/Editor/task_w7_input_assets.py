import unreal


ROOT = "/Game/Blueprints/Player/Input"


def log(message):
    unreal.log(f"[W7InputAssets] {message}")


def load_or_create_input_action(name):
    path = f"{ROOT}/{name}"
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory_type = getattr(unreal, "InputAction_Factory", None)
    factory = factory_type() if factory_type else unreal.DataAssetFactory()
    if not factory_type:
        factory.set_editor_property("data_asset_class", unreal.InputAction)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.InputAction, factory)


def load_or_create_mapping_context(name):
    path = f"{ROOT}/{name}"
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory_type = getattr(unreal, "InputMappingContext_Factory", None)
    factory = factory_type() if factory_type else unreal.DataAssetFactory()
    if not factory_type:
        factory.set_editor_property("data_asset_class", unreal.InputMappingContext)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.InputMappingContext, factory)


def key(name):
    try:
        return unreal.Key(name)
    except Exception:
        value = unreal.Key()
        value.set_editor_property("key_name", name)
        return value


def ensure_mapping(context, action, key_name):
    mapping_data = context.get_editor_property("default_key_mappings")
    mappings = list(mapping_data.get_editor_property("mappings"))
    for mapping in mappings:
        if mapping.get_editor_property("action") == action and str(mapping.get_editor_property("key")) == str(key(key_name)):
            return False
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", key(key_name))
    mappings.append(mapping)
    mapping_data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", mapping_data)
    return True


def assign_controller_defaults(sprint_action, map_action, map_context):
    blueprint = unreal.load_asset("/Game/Blueprints/Player/BP_HeistPlayerController")
    if not blueprint:
        raise RuntimeError("BP_HeistPlayerController not found")
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("sprint_input_action", sprint_action)
    cdo.set_editor_property("map_input_action", map_action)
    cdo.set_editor_property("map_input_mapping_context", map_context)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)


def main():
    sprint_action = load_or_create_input_action("IA_Sprint")
    map_action = load_or_create_input_action("IA_Map")
    default_context = unreal.load_asset(f"{ROOT}/IMC_Default")
    map_context = load_or_create_mapping_context("IMC_Map")
    if not all((sprint_action, map_action, default_context, map_context)):
        raise RuntimeError("failed to resolve W7 input assets")

    sprint_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    map_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    ensure_mapping(default_context, sprint_action, "LeftShift")
    ensure_mapping(default_context, map_action, "M")
    ensure_mapping(map_context, map_action, "M")
    assign_controller_defaults(sprint_action, map_action, map_context)

    for asset in (sprint_action, map_action, default_context, map_context):
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    log("Result=PASS IA_Sprint=LeftShift IA_Map=M IMC_Map=M ControllerDefaults=true")


main()
