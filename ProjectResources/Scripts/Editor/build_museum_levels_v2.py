import json
import math

import unreal


STATIC_MESHES = {
    "floor": "/Game/Assets/StarterContent/Architecture/Floor_400x400",
    "wall": "/Game/Assets/StarterContent/Architecture/Wall_400x400",
    "door_wall": "/Game/Assets/StarterContent/Architecture/Wall_Door_400x400",
    "window_wall": "/Game/Assets/StarterContent/Architecture/Wall_Window_400x400",
    "pillar": "/Game/Assets/StarterContent/Architecture/Pillar_50x500",
    "platform": "/Game/Assets/StarterContent/Architecture/SM_AssetPlatform",
    "cube": "/Game/Assets/StarterContent/Shapes/Shape_Cube",
    "glass_window": "/Game/Assets/StarterContent/Props/SM_GlassWindow",
    "door_frame": "/Game/Assets/StarterContent/Props/SM_DoorFrame",
    "couch": "/Game/Assets/StarterContent/Props/SM_Couch",
    "chair": "/Game/Assets/StarterContent/Props/SM_Chair",
    "table": "/Game/Assets/StarterContent/Props/SM_TableRound",
    "statue": "/Game/Assets/StarterContent/Props/SM_Statue",
    "shelf": "/Game/Assets/StarterContent/Props/SM_Shelf",
    "rock": "/Game/Assets/StarterContent/Props/SM_Rock",
    "bush": "/Game/Assets/StarterContent/Props/SM_Bush",
    "wall_lamp": "/Game/Assets/StarterContent/Props/SM_Lamp_Wall",
    "ceiling_lamp": "/Game/Assets/StarterContent/Props/SM_Lamp_Ceiling",
    "pillar_frame": "/Game/Assets/StarterContent/Props/SM_PillarFrame",
    "window_frame": "/Game/Assets/StarterContent/Props/SM_WindowFrame",
}

MATERIALS = {
    "m01_floor": "/Game/Assets/StarterContent/Materials/M_Rock_Marble_Polished",
    "m01_wall": "/Game/Assets/StarterContent/Materials/M_Basic_Wall",
    "m02_floor": "/Game/Assets/StarterContent/Materials/M_Wood_Floor_Walnut_Polished",
    "m02_wall": "/Game/Assets/StarterContent/Materials/M_Brick_Hewn_Stone",
    "m03_floor": "/Game/Assets/StarterContent/Materials/M_Concrete_Tiles",
    "m03_wall": "/Game/Assets/StarterContent/Materials/M_Concrete_Panels",
    "steel": "/Game/Assets/StarterContent/Materials/M_Metal_Steel",
    "burnished": "/Game/Assets/StarterContent/Materials/M_Metal_Burnished_Steel",
    "tech": "/Game/Assets/StarterContent/Materials/M_Tech_Panel",
    "glass": "/Game/Assets/StarterContent/Materials/M_Glass",
    "gold": "/Game/Assets/StarterContent/Materials/M_Metal_Gold",
    "copper": "/Game/Assets/StarterContent/Materials/M_Metal_Copper",
    "nickel": "/Game/Assets/StarterContent/Materials/M_Metal_Brushed_Nickel",
    "oak": "/Game/Assets/StarterContent/Materials/M_Wood_Oak",
    "worn_wood": "/Game/Assets/StarterContent/Materials/M_Wood_Floor_Walnut_Worn",
    "moss": "/Game/Assets/StarterContent/Materials/M_Ground_Moss",
    "water": "/Game/Assets/StarterContent/Materials/M_Water_Lake",
}

BLUEPRINTS = {
    "painting": "/Game/Blueprints/World/Actors/Loot/BP_PaintingDisplayCase",
    "guard": "/Game/Blueprints/Guard/BP_Guard",
    "camera": "/Game/Blueprints/World/Actors/Security/BP_SecurityCamera",
    "laser": "/Game/Blueprints/World/Actors/Security/BP_LaserBarrier",
    "button": "/Game/Blueprints/World/Actors/Security/BP_SecurityHoldButton",
}

# The broad Architecture/M##_ legacy-label migration already ran on 2026-08-27.
# Keep it opt-in so a future hand-authored actor is never removed on a normal rebuild.
REMOVE_LEGACY_ARCHITECTURE = False


MAPS = {
    "M01": {
        "path": "/Game/Maps/M01_ClassicalPrototype",
        "half_x": 7200,
        "half_y": 5200,
        "floor_material": "m01_floor",
        "wall_material": "m01_wall",
        "vent_entries": [(-6800, -1200, 90.0), (800, -4800, 0.0), (6800, 2400, 90.0)],
        "player_starts": [(-6200, -1650), (-6200, -1050), (-6600, -1650), (-6600, -1050)],
        "exit": (-6800, -1200, 90.0),
        "nav_scale": (72.0, 52.0, 10.0),
        "cases": [
            ("Target", 6200, 2200, 90.0),
            ("HighValue", 6200, -2400, 90.0),
            ("01", -6600, -2600, -90.0), ("02", -6600, -400, -90.0),
            ("03", -6600, 1800, -90.0), ("04", -5200, 3600, 0.0),
            ("05", -3600, 4200, 0.0), ("06", -1200, 4200, 0.0),
            ("07", 1200, 4200, 0.0), ("08", 3600, 4200, 0.0),
            ("09", 5000, 3600, 0.0), ("10", 5200, 400, 90.0),
            ("11", 3600, -4200, 180.0), ("12", 1200, -4200, 180.0),
            ("13", -1200, -4200, 180.0), ("14", -3600, -4200, 180.0),
            ("15", -2400, 900, -90.0), ("16", -2400, -900, -90.0),
            ("17", 2400, 900, 90.0), ("18", 2400, -900, 90.0),
        ],
        "guard_routes": [
            [(-5800, 2600), (-4200, 2600), (-1800, 2800), (0, 2000), (1800, 2800), (4200, 2600), (5600, 2400), (4200, 1200), (0, 1600), (-4200, 1200)],
            [(-5600, -2800), (-4000, -2600), (-1800, -2800), (0, -2000), (1800, -2800), (4200, -2600), (5600, -1800), (4200, -1200), (0, -1600), (-4200, -1200)],
            [(-1600, -500), (-800, -1600), (800, -1600), (1600, -500), (1600, 500), (800, 1600), (-800, 1600), (-1600, 500)],
            [(4800, -2200), (5600, -1000), (4600, 1000), (5400, 2000), (4200, 3000), (3200, 1800), (3400, -800)],
            [(-5800, -4600), (-4600, -4600), (-4600, -3400), (-5800, -3400)],
        ],
        "cameras": [
            (-4000, -2200, 315, 35), (-4000, 2200, 315, -35),
            (0, -2600, 315, 90), (0, 2600, 315, -90),
            (4000, -2200, 315, 145), (4000, 2200, 315, -145),
        ],
        "lasers": [
            ("HighValue", (5600, -2400, 120, 90), (4800, -3200, 0, 0)),
            ("09", (5000, 3200, 120, 0), (4200, 2800, 0, 0)),
        ],
    },
    "M02": {
        "path": "/Game/Maps/M02_MoonlitPrototype",
        "half_x": 6400,
        "half_y": 5600,
        "floor_material": "m02_floor",
        "wall_material": "m02_wall",
        "vent_entries": [(-6000, -3200, 90.0), (2400, 5200, 0.0), (6000, 800, 90.0)],
        "player_starts": [(-5400, -3450), (-5400, -2850), (-5800, -3450), (-5800, -2850)],
        "exit": (2400, 5200, 0.0),
        "nav_scale": (64.0, 56.0, 10.0),
        "cases": [
            ("Target", 5200, 3000, 90.0),
            ("HighValue", -5200, 3600, -90.0),
            ("01", -5600, -3600, -90.0), ("02", -4800, -2200, 180.0),
            ("03", -5200, 600, -90.0), ("04", -4400, 2600, -90.0),
            ("05", -3000, 4600, 0.0), ("06", -600, 4600, 0.0),
            ("07", 2200, 4600, 0.0), ("08", 5200, 1000, 90.0),
            ("09", 4400, -800, -90.0), ("10", 5200, -3400, 90.0),
            ("11", 3000, -4600, 180.0), ("12", 600, -4600, 180.0),
            ("13", -1800, -4600, 180.0), ("14", -3200, -1000, 90.0),
            ("15", -2600, 1600, -90.0), ("16", -600, 2800, 180.0),
            ("17", 1200, -2600, 0.0), ("18", 3000, 600, -90.0),
        ],
        "guard_routes": [
            [(-5000, -3000), (-4000, -3200), (-3600, -1800), (-4400, -200), (-3600, 1200), (-4200, 3400), (-2400, 3800), (0, 3600), (1600, 3400), (3600, 3000), (4200, 1400), (3600, 0), (4200, -1800), (3400, -3600), (1600, -3600), (-600, -3600), (-2600, -3000)],
            [(-1800, -600), (-1800, 1000), (-1200, 2200), (0, 2400), (1000, 1600), (1000, 200), (0, -600)],
            [(4800, -2800), (3600, -2200), (5200, -200), (4200, 800), (4600, 2200), (3400, 3800)],
            [(-5000, -2800), (-4200, -1400), (-4600, 0), (-3800, 1200), (-5000, 2400), (-3800, 3800)],
            [(4200, -5000), (5800, -5000), (5800, -4000), (4200, -4000)],
        ],
        "cameras": [
            (-3200, -800, 315, 15), (-1200, 2600, 315, -65),
            (2400, -1600, 315, 135), (4400, 2200, 315, -145),
        ],
        "lasers": [
            ("HighValue", (-4800, 3200, 120, 90), (-4000, 4000, 0, 0)),
            ("07", (2200, 4200, 120, 0), (1400, 3400, 0, 0)),
        ],
    },
    "M03": {
        "path": "/Game/Maps/M03_GlasshousePrototype",
        "half_x": 8000,
        "half_y": 4400,
        "floor_material": "m03_floor",
        "wall_material": "m03_wall",
        "vent_entries": [(-7600, 0, 90.0), (-2400, 4000, 0.0), (7200, -4000, 0.0)],
        "player_starts": [(-7000, -900), (-7000, -300), (-7000, 300), (-7000, 900)],
        "exit": (-7600, 0, 90.0),
        "nav_scale": (80.0, 44.0, 10.0),
        "cases": [
            ("Target", 6800, 2400, 90.0),
            ("HighValue", 6800, -1600, 90.0),
            ("01", -7200, -2400, -90.0), ("02", -7200, 0, -90.0),
            ("03", -7200, 2400, -90.0), ("04", -5600, -3200, 180.0),
            ("05", -3200, -3200, 180.0), ("06", -800, -3200, 180.0),
            ("07", 1600, -3200, 180.0), ("08", 4000, -3200, 180.0),
            ("09", 5600, -2800, 90.0), ("10", 5200, 3200, 0.0),
            ("11", 2800, 3200, 0.0), ("12", 400, 3200, 0.0),
            ("13", -2000, 3200, 0.0), ("14", -4400, 3200, 0.0),
            ("15", -4800, 400, 90.0), ("16", -1600, -400, 90.0),
            ("17", 1600, 400, -90.0), ("18", 4800, -400, -90.0),
        ],
        "guard_routes": [
            [(-6600, 0), (-5200, -400), (-3600, 400), (-2400, -500), (-400, 400), (1000, -400), (2800, 400), (4200, -500), (6200, 0)],
            [(-6200, 2200), (-5000, 2400), (-3400, 2600), (-1000, 2400), (1200, 2600), (2600, 2400), (4400, 2400), (6000, 1800)],
            [(-6200, -2600), (-4400, -2400), (-2400, -2600), (-400, -2400), (2000, -2600), (3600, -2400), (5600, -2200)],
            [(5200, -3400), (7400, -3800), (7400, -2800), (5000, -2600)],
        ],
        "cameras": [
            (-6400, -2200, 315, 30), (-5200, 2400, 315, -35),
            (-2800, -1800, 315, 60), (-1200, 2400, 315, -65),
            (2000, -2200, 315, 120), (3600, 2400, 315, -120),
            (6000, -2200, 315, 150), (6800, 2000, 315, -155),
        ],
        "lasers": [
            ("HighValue", (6800, -2400, 120, 0), (6000, -3400, 0, 90)),
            ("08", (4000, -2800, 120, 0), (3200, -2200, 0, 0)),
            ("10", (5200, 2800, 120, 0), (4400, 2200, 0, 0)),
        ],
    },
}


def resolve_level_codes(available_codes):
    _, command_line_switches, command_line_parameters = unreal.SystemLibrary.parse_command_line(
        unreal.SystemLibrary.get_command_line()
    )
    parameter_name = "MuseumLevelCodes"
    normalized_parameter_name = parameter_name.casefold()
    bare_switches = [
        switch
        for switch in command_line_switches
        if str(switch).casefold() == normalized_parameter_name
    ]
    matching_values = [
        value
        for key, value in command_line_parameters.items()
        if str(key).casefold() == normalized_parameter_name
    ]

    if bare_switches:
        raise RuntimeError("-{} requires a non-empty comma-separated value".format(parameter_name))
    if not matching_values:
        return list(available_codes)
    if len(matching_values) != 1:
        raise RuntimeError("-{} was specified more than once".format(parameter_name))

    requested_values = str(matching_values[0]).split(",")
    if any(not value.strip() for value in requested_values):
        raise RuntimeError("-{} contains an empty level code".format(parameter_name))

    selected_codes = []
    selected_set = set()
    for value in requested_values:
        code = value.strip().upper()
        if code not in available_codes:
            raise RuntimeError(
                "Unknown level code '{}' in -{}. Expected one of: {}".format(
                    code,
                    parameter_name,
                    ",".join(available_codes),
                )
            )
        if code not in selected_set:
            selected_codes.append(code)
            selected_set.add(code)
    return selected_codes


selected_level_codes = resolve_level_codes(MAPS)


actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = {name: unreal.load_asset(path) for name, path in STATIC_MESHES.items()}
materials = {name: unreal.load_asset(path) for name, path in MATERIALS.items()}
blueprint_classes = {name: unreal.EditorAssetLibrary.load_blueprint_class(path) for name, path in BLUEPRINTS.items()}
waypoint_class = unreal.load_class(None, "/Script/Project_MuseumHeist.HeistGuardWaypoint")

if any(value is None for value in assets.values()):
    raise RuntimeError("StarterContent static mesh load failed")
if any(value is None for value in materials.values()):
    raise RuntimeError("StarterContent material load failed")
if any(value is None for value in blueprint_classes.values()) or waypoint_class is None:
    raise RuntimeError("Gameplay Blueprint or waypoint class load failed")


def vec(values):
    return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))


def rot(yaw):
    return unreal.Rotator(pitch=0.0, yaw=float(yaw), roll=0.0)


def safe_set(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        unreal.log_warning("MH_LDV2_PROPERTY_FAILED target={} property={} reason={}".format(target.get_name(), property_name, exc))
        return False


def set_transform(actor, location, yaw=0.0, scale=(1.0, 1.0, 1.0)):
    actor.set_actor_location(vec(location), False, False)
    actor.set_actor_rotation(rot(yaw), False)
    actor.set_actor_scale3d(vec(scale))


class LevelBuilder:
    def __init__(self, code, config):
        self.code = code
        self.config = config
        self.created = 0
        self.updated = 0
        self.legacy_removed = 0
        self.orphaned_removed = 0
        self.expected_labels = set()
        self.world = unreal.EditorLoadingAndSavingUtils.load_map(config["path"])
        if not self.world:
            raise RuntimeError("Map load failed: " + config["path"])
        self.actors = list(actor_subsystem.get_all_level_actors())
        self.by_label = {actor.get_actor_label(): actor for actor in self.actors}

    def register(self, actor):
        self.actors.append(actor)
        self.by_label[actor.get_actor_label()] = actor

    def mark_generated(self, actor):
        self.expected_labels.add(actor.get_actor_label())
        tags = list(actor.get_editor_property("tags"))
        generated_tag = unreal.Name("MuseumLevelGenerated")
        if generated_tag not in tags:
            tags.append(generated_tag)
            safe_set(actor, "tags", tags)

    def add_tags(self, actor, *tag_names):
        tags = list(actor.get_editor_property("tags"))
        changed = False
        for tag_name in tag_names:
            tag = unreal.Name(tag_name)
            if tag not in tags:
                tags.append(tag)
                changed = True
        if changed:
            safe_set(actor, "tags", tags)

    def is_generated(self, actor):
        label = actor.get_actor_label()
        tags = list(actor.get_editor_property("tags"))
        return label.startswith("LDV2_{}_".format(self.code)) and unreal.Name("MuseumLevelGenerated") in tags

    def cleanup_legacy_static(self):
        legacy_prefix = self.code + "_"
        for actor in list(self.actors):
            if actor.get_class().get_name() != "StaticMeshActor":
                continue
            label = actor.get_actor_label()
            folder = str(actor.get_folder_path())
            if not label.startswith(legacy_prefix) or not folder.startswith("Architecture/"):
                continue
            if not actor_subsystem.destroy_actor(actor):
                raise RuntimeError("Legacy actor cleanup failed: " + label)
            self.actors.remove(actor)
            self.by_label.pop(label, None)
            self.legacy_removed += 1
        unreal.log_warning("MH_LEVEL_LEGACY_CLEANUP={} removed={}".format(self.code, self.legacy_removed))

    def cleanup_orphaned_generated(self):
        for actor in list(self.actors):
            if not self.is_generated(actor) or actor.get_actor_label() in self.expected_labels:
                continue
            label = actor.get_actor_label()
            if not actor_subsystem.destroy_actor(actor):
                raise RuntimeError("Generated orphan cleanup failed: " + label)
            self.actors.remove(actor)
            self.by_label.pop(label, None)
            self.orphaned_removed += 1
        unreal.log_warning("MH_LEVEL_ORPHAN_CLEANUP={} removed={}".format(self.code, self.orphaned_removed))

    def folder(self, actor, suffix):
        actor.set_folder_path(unreal.Name("LDV2/{}/{}".format(self.code, suffix)))

    def relabel(self, actor, label):
        old_label = actor.get_actor_label()
        if old_label != label:
            self.by_label.pop(old_label, None)
            actor.set_actor_label(label)
            self.by_label[label] = actor

    def static(self, label, mesh_name, location, yaw=0.0, scale=(1.0, 1.0, 1.0), material_name=None, folder="Architecture"):
        actor = self.by_label.get(label)
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, vec(location), rot(yaw))
            actor.set_actor_label(label)
            self.register(actor)
            self.created += 1
        else:
            self.updated += 1
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        if not components:
            raise RuntimeError("StaticMeshComponent missing for " + label)
        component = components[0]
        component.set_editor_property("static_mesh", assets[mesh_name])
        if material_name:
            component.set_material(0, materials[material_name])
        # Persist collision through map saves and later navigation rebuilds.
        # InvisibleWall keeps Pawn blocking while allowing the interaction
        # visibility channel to pass through the M03 glass partitions.
        collision_profile = "InvisibleWall" if material_name == "glass" else "BlockAll"
        component.set_collision_profile_name(unreal.Name(collision_profile))
        set_transform(actor, location, yaw, scale)
        self.folder(actor, folder)
        self.mark_generated(actor)
        return actor

    def blueprint(self, label, class_name, location, yaw=0.0, folder="Gameplay"):
        actor = self.by_label.get(label)
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(blueprint_classes[class_name], vec(location), rot(yaw))
            actor.set_actor_label(label)
            self.register(actor)
            self.created += 1
        else:
            self.updated += 1
        set_transform(actor, location, yaw)
        self.folder(actor, folder)
        self.mark_generated(actor)
        return actor

    def target_point(self, label, location, yaw=0.0, folder="Gameplay/Anchors", tags=()):
        actor = self.by_label.get(label)
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(unreal.TargetPoint, vec(location), rot(yaw))
            actor.set_actor_label(label)
            self.register(actor)
            self.created += 1
        else:
            self.updated += 1
        set_transform(actor, location, yaw)
        self.folder(actor, folder)
        self.mark_generated(actor)
        self.add_tags(actor, *tags)
        return actor

    def point_light(self, label, location, color, intensity=1800.0, radius=1500.0, folder="Lighting"):
        actor = self.by_label.get(label)
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(unreal.PointLight, vec(location), rot(0.0))
            actor.set_actor_label(label)
            self.register(actor)
            self.created += 1
        else:
            self.updated += 1
        actor.set_actor_location(vec(location), False, False)
        component = actor.get_component_by_class(unreal.PointLightComponent)
        if component is None:
            raise RuntimeError("PointLightComponent missing for " + label)
        safe_set(component, "intensity", float(intensity))
        safe_set(component, "attenuation_radius", float(radius))
        safe_set(component, "light_color", unreal.Color(int(color[0]), int(color[1]), int(color[2]), 255))
        safe_set(component, "mobility", unreal.ComponentMobility.MOVABLE)
        safe_set(component, "cast_shadows", True)
        self.folder(actor, folder)
        self.mark_generated(actor)
        return actor

    def portal(self, label, location, yaw=0.0, material_name=None, scale=(1.0, 1.0, 1.0), folder="Theme/Portals"):
        return self.static(label, "door_frame", location, yaw, scale, material_name, folder)

    def screen(self, label, location, yaw, length, material_name, height=2.2, folder="Theme/Partitions"):
        return self.static(label, "cube", location, yaw, (length / 100.0, 0.18, height), material_name, folder)

    def ceiling_rect(self, label_prefix, center, size, material_name, max_tile_size=(1600.0, 1600.0), mesh_name="floor", z=412.0):
        count_x = max(1, int(math.ceil(float(size[0]) / float(max_tile_size[0]))))
        count_y = max(1, int(math.ceil(float(size[1]) / float(max_tile_size[1]))))
        tile_x = float(size[0]) / count_x
        tile_y = float(size[1]) / count_y
        start_x = float(center[0]) - float(size[0]) * 0.5 + tile_x * 0.5
        start_y = float(center[1]) - float(size[1]) * 0.5 + tile_y * 0.5
        for x_index in range(count_x):
            for y_index in range(count_y):
                self.static(
                    "{}_{}_{:02d}".format(label_prefix, x_index, y_index),
                    mesh_name,
                    (start_x + x_index * tile_x, start_y + y_index * tile_y, z),
                    0.0,
                    (tile_x / 400.0, tile_y / 400.0, 1.0),
                    material_name,
                    "Architecture/Ceiling",
                )

    def floor_grid(self):
        half_x = self.config["half_x"]
        half_y = self.config["half_y"]
        index = 0
        for x in range(-half_x + 400, half_x, 800):
            for y in range(-half_y + 400, half_y, 800):
                self.static(
                    "LDV2_{}_Floor_{:03d}".format(self.code, index),
                    "floor", (x, y, -12), 0.0, (2.0, 2.0, 1.0),
                    self.config["floor_material"], "Architecture/Floor",
                )
                index += 1

    def wall_h(self, name, y, start_x, end_x, doors=(), windows=(), material_name=None):
        index = 0
        for x in range(start_x, end_x + 1, 800):
            mesh_name = "door_wall" if x in doors else ("window_wall" if x in windows else "wall")
            self.static(
                "LDV2_{}_{}_{:02d}".format(self.code, name, index),
                mesh_name, (x, y, -12), 0.0, (2.0, 1.0, 1.0),
                material_name, "Architecture/Walls",
            )
            index += 1

    def wall_v(self, name, x, start_y, end_y, doors=(), windows=(), material_name=None):
        index = 0
        for y in range(start_y, end_y + 1, 800):
            mesh_name = "door_wall" if y in doors else ("window_wall" if y in windows else "wall")
            self.static(
                "LDV2_{}_{}_{:02d}".format(self.code, name, index),
                mesh_name, (x, y, -12), 90.0, (2.0, 1.0, 1.0),
                material_name, "Architecture/Walls",
            )
            index += 1

    def perimeter(self, use_glass=False):
        hx = self.config["half_x"]
        hy = self.config["half_y"]
        x_values = list(range(-hx + 400, hx, 800))
        y_values = list(range(-hy + 400, hy, 800))
        horizontal_windows = tuple(x_values) if use_glass else ()
        self.wall_h("PerimeterNorth", hy, x_values[0], x_values[-1], windows=horizontal_windows, material_name=None if use_glass else self.config["wall_material"])
        self.wall_h("PerimeterSouth", -hy, x_values[0], x_values[-1], windows=horizontal_windows, material_name=None if use_glass else self.config["wall_material"])
        self.wall_v("PerimeterWest", -hx, y_values[0], y_values[-1], material_name=self.config["wall_material"])
        self.wall_v("PerimeterEast", hx, y_values[0], y_values[-1], material_name=self.config["wall_material"])

    def vent_visual(self, index, x, y, yaw):
        base = "LDV2_{}_VentEntry_{}".format(self.code, chr(ord("A") + index))
        self.static(base + "_Panel", "cube", (x, y, 75), yaw, (2.8, 0.28, 1.35), "steel", "VentEntries")
        for slat in range(4):
            self.static(
                base + "_Slat_{:02d}".format(slat), "cube",
                (x, y, 40 + slat * 24), yaw, (2.45, 0.38, 0.07),
                "burnished", "VentEntries",
            )

    def configure_entry_exit_and_nav(self):
        for index, entry in enumerate(self.config["vent_entries"]):
            self.vent_visual(index, entry[0], entry[1], entry[2])

        starts = sorted((actor for actor in self.actors if actor.get_class().get_name() == "PlayerStart"), key=lambda actor: actor.get_actor_label())
        for actor, location in zip(starts, self.config["player_starts"]):
            set_transform(actor, (location[0], location[1], 100), 0.0)
            self.folder(actor, "Gameplay/Entry")

        vents = [actor for actor in self.actors if actor.get_class().get_name() == "BP_Vent_C"]
        if vents:
            exit_x, exit_y, exit_yaw = self.config["exit"]
            set_transform(vents[0], (exit_x, exit_y, 50), exit_yaw)
            self.folder(vents[0], "Gameplay/Exit")

        navs = [actor for actor in self.actors if actor.get_class().get_name() == "NavMeshBoundsVolume"]
        if navs:
            set_transform(navs[0], (0, 0, 250), 0.0, self.config["nav_scale"])
            self.folder(navs[0], "Gameplay/Navigation")

    def case_artifact_id(self, case_key):
        if case_key == "Target":
            return "Artifact_Painting_{}".format(self.code)
        if case_key == "HighValue":
            return "Artifact_Painting_{}_HighValue".format(self.code)
        source_index = ((int(case_key) - 1) % 4) + 1
        return "Artifact_Painting_{}_Optional_{:02d}".format(self.code, source_index)

    def case_id(self, case_key):
        if case_key == "Target":
            return "Case_{}_Target".format(self.code)
        if case_key == "HighValue":
            return "Case_{}_Optional_HighValue".format(self.code)
        return "Case_{}_Optional_{}".format(self.code, case_key)

    def configure_cases(self):
        by_case_id = {}
        for actor in self.actors:
            if actor.get_class().get_name() != "BP_PaintingDisplayCase_C":
                continue
            try:
                by_case_id[str(actor.get_editor_property("display_case_id"))] = actor
            except Exception:
                pass

        result = {}
        for case_key, x, y, yaw in self.config["cases"]:
            display_case_id = self.case_id(case_key)
            actor = by_case_id.get(display_case_id)
            label = "LDV2_{}_Painting_{}".format(self.code, case_key)
            if actor is None:
                actor = self.blueprint(label, "painting", (x, y, 0), yaw, "Gameplay/PaintingCases")
            else:
                self.relabel(actor, label)
                set_transform(actor, (x, y, 0), yaw)
                self.folder(actor, "Gameplay/PaintingCases")
                self.mark_generated(actor)
                self.updated += 1
            safe_set(actor, "display_case_id", unreal.Name(display_case_id))
            safe_set(actor, "target_artifact_id", unreal.Name(self.case_artifact_id(case_key)))
            result[case_key] = actor
        return result

    def configure_guards(self):
        guards = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_Guard_C" and self.is_generated(actor)), key=lambda actor: actor.get_actor_label())
        waypoints = sorted((actor for actor in self.actors if actor.get_class().get_name() == "HeistGuardWaypoint" and self.is_generated(actor)), key=lambda actor: actor.get_actor_label())
        required_waypoint_count = sum(len(route) for route in self.config["guard_routes"])
        for waypoint in waypoints[required_waypoint_count:]:
            label = waypoint.get_actor_label()
            if not actor_subsystem.destroy_actor(waypoint):
                raise RuntimeError("Surplus guard waypoint cleanup failed: " + label)
            self.actors.remove(waypoint)
            self.orphaned_removed += 1
        waypoints = waypoints[:required_waypoint_count]
        for index, waypoint in enumerate(waypoints):
            waypoint.set_actor_label("LDV2_{}_WaypointTemp_{:03d}".format(self.code, index))
        self.by_label = {actor.get_actor_label(): actor for actor in self.actors}
        waypoint_cursor = 0

        for route_index, route in enumerate(self.config["guard_routes"]):
            route_id = unreal.Name("LDV2_{}_Route_{:02d}".format(self.code, route_index + 1))
            guard_label = "LDV2_{}_Guard_{:02d}".format(self.code, route_index + 1)
            if route_index < len(guards):
                guard = guards[route_index]
                self.relabel(guard, guard_label)
                set_transform(guard, (route[0][0], route[0][1], 88), 0.0)
                self.folder(guard, "Gameplay/Guards")
                self.mark_generated(guard)
                self.updated += 1
            else:
                guard = self.blueprint(
                    guard_label,
                    "guard", (route[0][0], route[0][1], 88), 0.0, "Gameplay/Guards",
                )
            safe_set(guard, "guard_profile_id", unreal.Name("Guard_Alert_Medium"))
            for component in guard.get_components_by_class(unreal.ActorComponent):
                if component.get_class().get_name() == "HeistPatrolPathComponent":
                    safe_set(component, "patrol_route_id", route_id)

            for order, point in enumerate(route):
                waypoint_label = "LDV2_{}_Route_{:02d}_Point_{:02d}".format(self.code, route_index + 1, order)
                if waypoint_cursor < len(waypoints):
                    waypoint = waypoints[waypoint_cursor]
                    self.relabel(waypoint, waypoint_label)
                    set_transform(waypoint, (point[0], point[1], 25), 0.0)
                    self.folder(waypoint, "Gameplay/GuardRoutes")
                    self.mark_generated(waypoint)
                    self.updated += 1
                else:
                    waypoint = actor_subsystem.spawn_actor_from_class(waypoint_class, vec((point[0], point[1], 25)), rot(0.0))
                    waypoint.set_actor_label(waypoint_label)
                    self.register(waypoint)
                    self.folder(waypoint, "Gameplay/GuardRoutes")
                    self.mark_generated(waypoint)
                    self.created += 1
                safe_set(waypoint, "patrol_route_id", route_id)
                safe_set(waypoint, "patrol_order", order)
                waypoint_cursor += 1

    def configure_cameras(self):
        existing = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_SecurityCamera_C" and self.is_generated(actor)), key=lambda actor: actor.get_actor_label())
        for index, spec in enumerate(self.config["cameras"]):
            x, y, z, yaw = spec
            label = "LDV2_{}_CCTV_{:02d}".format(self.code, index + 1)
            if index < len(existing):
                actor = existing[index]
                self.relabel(actor, label)
                set_transform(actor, (x, y, z), yaw)
                self.folder(actor, "Gameplay/CCTV")
                self.mark_generated(actor)
                self.updated += 1
            else:
                actor = self.blueprint(label, "camera", (x, y, z), yaw, "Gameplay/CCTV")
            safe_set(actor, "detection_range", 1800.0)
            safe_set(actor, "detection_half_angle_degrees", 35.0)
            safe_set(actor, "sweep_half_angle_degrees", 35.0)
            safe_set(actor, "sweep_period_seconds", 6.0)

    def configure_lasers(self, cases):
        barriers = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_LaserBarrier_C" and self.is_generated(actor)), key=lambda actor: actor.get_actor_label())
        buttons = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_SecurityHoldButton_C" and self.is_generated(actor)), key=lambda actor: actor.get_actor_label())
        for index, spec in enumerate(self.config["lasers"]):
            case_key, barrier_spec, button_spec = spec
            barrier_label = "LDV2_{}_Laser_{:02d}".format(self.code, index + 1)
            button_label = "LDV2_{}_LaserButton_{:02d}".format(self.code, index + 1)
            if index < len(barriers):
                barrier = barriers[index]
                self.relabel(barrier, barrier_label)
                set_transform(barrier, barrier_spec[:3], barrier_spec[3])
                self.folder(barrier, "Gameplay/Lasers")
                self.mark_generated(barrier)
                self.updated += 1
            else:
                barrier = self.blueprint(barrier_label, "laser", barrier_spec[:3], barrier_spec[3], "Gameplay/Lasers")
            if index < len(buttons):
                button = buttons[index]
                self.relabel(button, button_label)
                set_transform(button, button_spec[:3], button_spec[3])
                self.folder(button, "Gameplay/Lasers")
                self.mark_generated(button)
                self.updated += 1
            else:
                button = self.blueprint(button_label, "button", button_spec[:3], button_spec[3], "Gameplay/Lasers")
            safe_set(barrier, "protected_painting_case", cases[case_key])
            safe_set(button, "linked_laser_barrier", barrier)

    def security_detention_props(self, origin_x, origin_y, yaw=0.0):
        evidence_table = self.static(
            "LDV2_{}_SecurityDesk".format(self.code),
            "table",
            (origin_x, origin_y, 0),
            yaw,
            (2.0, 2.0, 1.0),
            None,
            "SecurityDetention",
        )
        self.add_tags(evidence_table, "HeistEvidenceTableVisual")
        for index, offset in enumerate((-240, 0, 240)):
            self.static("LDV2_{}_SecurityChair_{:02d}".format(self.code, index), "chair", (origin_x + offset, origin_y - 260, 0), yaw + 180.0, (1.0, 1.0, 1.0), None, "SecurityDetention")
        for index in range(5):
            for tier, height in enumerate((100, 190, 280)):
                label = "LDV2_{}_EvidenceRack_{:02d}".format(self.code, index)
                if tier:
                    label += "_Tier_{:02d}".format(tier)
                self.static(label, "shelf", (origin_x - 700 + index * 300, origin_y + 520, height), yaw, (1.0, 0.85, 1.0), None, "SecurityDetention")
        for index in range(5):
            self.static("LDV2_{}_DetentionBar_{:02d}".format(self.code, index), "pillar", (origin_x + 700, origin_y - 500 + index * 220, -12), yaw, (0.65, 0.65, 0.75), "steel", "SecurityDetention")

        # Four deterministic cell positions support the public 2-4 player contract.
        # Z is the Character capsule center height used by the existing PlayerStarts.
        for index, offset in enumerate(((400, -120), (400, 120), (520, -120), (520, 120)), start=1):
            self.target_point(
                "LDV2_{}_DetentionSpawn_{:02d}".format(self.code, index),
                (origin_x + offset[0], origin_y + offset[1], 96),
                yaw,
                "Gameplay/DetentionAnchors",
                ("HeistDetentionSpawn",),
            )

        # A 5x5 evidence grid matches the maximum inventory capacity. Every slot
        # carries both the table-group and slot tags so runtime discovery is based
        # on stable Actor Tags rather than editor-only labels.
        grid_spacing = 70.0
        yaw_radians = math.radians(yaw)
        axis_x = (math.cos(yaw_radians), math.sin(yaw_radians))
        axis_y = (-math.sin(yaw_radians), math.cos(yaw_radians))
        slot_index = 1
        for row in range(5):
            for column in range(5):
                local_x = (column - 2) * grid_spacing
                local_y = (row - 2) * grid_spacing
                slot_x = origin_x + axis_x[0] * local_x + axis_y[0] * local_y
                slot_y = origin_y + axis_x[1] * local_x + axis_y[1] * local_y
                self.target_point(
                    "LDV2_{}_EvidenceSlot_{:02d}".format(self.code, slot_index),
                    (slot_x, slot_y, 110),
                    yaw,
                    "Gameplay/EvidenceAnchors",
                    ("HeistEvidenceTableAnchor", "HeistEvidenceSlot"),
                )
                slot_index += 1

    def save(self):
        save_ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        class_counts = {}
        for actor in actor_subsystem.get_all_level_actors():
            class_name = actor.get_class().get_name()
            class_counts[class_name] = class_counts.get(class_name, 0) + 1
        payload = {
            "map": self.config["path"],
            "save_ok": bool(save_ok),
            "created": self.created,
            "updated": self.updated,
            "legacy_removed": self.legacy_removed,
            "orphaned_removed": self.orphaned_removed,
            "expected_generated_labels": len(self.expected_labels),
            "actor_count": sum(class_counts.values()),
            "static_mesh_actors": class_counts.get("StaticMeshActor", 0),
            "painting_cases": class_counts.get("BP_PaintingDisplayCase_C", 0),
            "guards": class_counts.get("BP_Guard_C", 0),
            "cameras": class_counts.get("BP_SecurityCamera_C", 0),
            "lasers": class_counts.get("BP_LaserBarrier_C", 0),
            "laser_buttons": class_counts.get("BP_SecurityHoldButton_C", 0),
            "gameplay_vents": class_counts.get("BP_Vent_C", 0),
            "target_size_cm": [self.config["half_x"] * 2, self.config["half_y"] * 2],
            "vent_visual_points": len(self.config["vent_entries"]),
        }
        unreal.log_warning("MH_LEVEL_BUILD=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
        if not save_ok:
            raise RuntimeError("Level save failed: " + self.config["path"])


def add_m01_geometry(builder):
    builder.floor_grid()
    builder.perimeter(False)
    mat = builder.config["wall_material"]
    builder.ceiling_rect("LDV2_M01_Ceiling_West", (-4200, 0), (6000, 10400), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_East", (4200, 0), (6000, 10400), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_North", (0, 3100), (2400, 4200), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_South", (0, -3100), (2400, 4200), mat)

    # Two broad gallery loops share the always-visible Rotunda instead of forming one rectangular ring.
    builder.wall_h("NorthLoopInnerWest", 1600, -6000, -2000, doors=(-4400,), material_name=mat)
    builder.wall_h("NorthLoopInnerEast", 1600, 2000, 6000, doors=(4400,), material_name=mat)
    builder.wall_h("SouthLoopInnerWest", -1600, -6000, -2000, doors=(-3600,), material_name=mat)
    builder.wall_h("SouthLoopInnerEast", -1600, 2000, 6000, doors=(3600,), material_name=mat)
    builder.wall_v("RotundaWestNeck", -2400, -800, 800, doors=(0,), material_name=mat)
    builder.wall_v("RotundaEastNeck", 2400, -800, 800, doors=(0,), material_name=mat)
    builder.wall_h("WestLoopNorthGate", 3200, -6800, -4400, doors=(-6000,), material_name=mat)
    builder.wall_h("WestLoopSouthGate", -3200, -6800, -4400, doors=(-5200,), material_name=mat)
    builder.wall_h("EastTargetNorthGate", 3200, 4400, 6800, doors=(6000,), material_name=mat)
    builder.wall_h("EastTargetSouthGate", -3200, 4400, 6800, doors=(5200,), material_name=mat)
    builder.wall_v("SecurityEast", -4400, -5200 + 400, -3200, doors=(-4000,), material_name=mat)
    # WestLoopSouthGate is also the security-room north wall; do not stack a
    # second identical wall run at the same transform.
    builder.wall_v("DetentionDivider", -6000, -4800, -3200, doors=(-4000,), material_name=mat)
    builder.security_detention_props(-5200, -4200, 90.0)

    builder.static("LDV2_M01_Topology_Figure8_North", "cube", (0, 1520, -20), 0.0, (24.0, 0.08, 0.04), "gold", "Theme/Figure8Inlay")
    builder.static("LDV2_M01_Topology_Figure8_South", "cube", (0, -1520, -20), 0.0, (24.0, 0.08, 0.04), "gold", "Theme/Figure8Inlay")
    rotunda_pillars = []
    for index in range(8):
        angle = math.radians(22.5 + index * 45.0)
        rotunda_pillars.append((math.cos(angle) * 1700, math.sin(angle) * 950))
    rotunda_pillars.extend(((-2400, 0), (2400, 0), (0, -1250), (0, 1250)))
    for index, pos in enumerate(rotunda_pillars):
        builder.static("LDV2_M01_RotundaPillar_{:02d}".format(index), "pillar", (pos[0], pos[1], -12), 0.0, (1.0, 1.0, 0.8), "gold" if index >= 8 else None, "Theme/Rotunda")
    builder.static("LDV2_M01_HeroPlinth", "platform", (0, 0, 0), 45.0, (1.65, 1.65, 4.0), "gold", "Theme/Rotunda")
    statue_specs = ((0, 0, 45.0, 2.6, 40), (-5600, 800, 90.0, 1.6, 0), (5200, 800, -90.0, 1.6, 0), (0, 3600, 180.0, 1.5, 0))
    for index, spec in enumerate(statue_specs):
        builder.static("LDV2_M01_RotundaStatue_{:02d}".format(index), "statue", (spec[0], spec[1], spec[4]), spec[2], (spec[3], spec[3], spec[3]), None, "Theme/Rotunda" if index == 0 else "Theme/GalleryLandmarks")
    for index, pos in enumerate(((-5650, -2450, 25), (-5650, 50, -20))):
        builder.static("LDV2_M01_EntryPlinth_{:02d}".format(index), "platform", (pos[0], pos[1], 0), pos[2], (0.8, 0.8, 3.0), "gold", "Theme/EntryGallery")
        builder.static("LDV2_M01_EntryStatue_{:02d}".format(index), "statue", (pos[0], pos[1], 30), pos[2], (1.4, 1.4, 1.4), None, "Theme/EntryGallery")
    couch_specs = ((-6000, -200, 90), (-5200, 2200, 90), (5600, 0, -90), (5200, 2200, -90), (-2000, -3600, 0), (2800, -3600, 180), (-2800, 3600, 0), (2000, 3600, 180))
    for index, pos in enumerate(couch_specs):
        builder.static("LDV2_M01_GalleryCouch_{:02d}".format(index), "couch", (pos[0], pos[1], 0), pos[2], (0.9, 0.9, 0.9), None, "Theme/Galleries")
    for index, pos in enumerate(((-3800, -2400, 45), (-3600, 2400, -45), (3800, -2400, 135), (3600, 2400, -135))):
        builder.static("LDV2_M01_ReadingTable_{:02d}".format(index), "table", (pos[0], pos[1], 0), pos[2], (1.05, 1.05, 1.05), None, "Theme/Galleries")
    portal_specs = ((-6000, -3200, 0), (-6000, 3200, 0), (-4400, 1600, 90), (-3600, -1600, 90), (3600, -1600, 90), (4400, 1600, 90), (5200, -3200, 0), (6000, 3200, 0))
    for index, pos in enumerate(portal_specs):
        builder.portal("LDV2_M01_Portal_{:02d}".format(index), (pos[0], pos[1], 0), pos[2], "gold", (1.0, 1.8, 1.05))
    inner_portals = ((-2400, 0, 0), (2400, 0, 0))
    for index, pos in enumerate(inner_portals):
        builder.portal("LDV2_M01_InnerPortal_{:02d}".format(index), (pos[0], pos[1], 0), pos[2], "gold", (1.0, 1.8, 1.05))
    for label, location, scale in (
        ("SkylightRimNorth", (0, 1000, 390), (24.0, 0.18, 0.15)),
        ("SkylightRimSouth", (0, -1000, 390), (24.0, 0.18, 0.15)),
        ("SkylightRimWest", (-1200, 0, 390), (0.18, 20.0, 0.15)),
        ("SkylightRimEast", (1200, 0, 390), (0.18, 20.0, 0.15)),
        ("SkylightBeamX", (0, 0, 390), (24.0, 0.16, 0.15)),
        ("SkylightBeamY", (0, 0, 390), (0.16, 20.0, 0.15)),
    ):
        builder.static("LDV2_M01_{}".format(label), "cube", location, 0.0, scale, "gold", "Theme/RotundaCeiling")
    light_positions = ((0, 0), (-5600, 0), (5600, 0), (0, -3400), (0, 3400), (-4000, -2400), (4000, -2400), (-4000, 2400), (4000, 2400), (-5200, -4200), (-6000, 3600), (6000, 3600))
    for index, pos in enumerate(light_positions):
        builder.static("LDV2_M01_CeilingLamp_{:02d}".format(index), "ceiling_lamp", (pos[0], pos[1], 370), 0.0, (0.8, 0.8, 0.8), "gold", "Lighting/Fixtures")
        builder.point_light("LDV2_M01_WarmLight_{:02d}".format(index), (pos[0], pos[1], 320), (255, 205, 145), 1150.0 if index else 1900.0, 1350.0)


def add_m02_geometry(builder):
    builder.floor_grid()
    builder.perimeter(False)
    mat = builder.config["wall_material"]
    builder.ceiling_rect("LDV2_M02_Ceiling_West", (-5000, 0), (2800, 11200), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_NorthWest", (-2600, 3600), (2000, 4000), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_SouthMid", (-800, -3600), (2400, 4000), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_NorthMid", (1600, 3600), (2400, 4000), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_East", (4800, 0), (3200, 11200), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_SouthEast", (2600, -3600), (1200, 4000), "m01_wall")

    # Alternating walls create the long safe S-route; each mid-wall door is a watched shortcut.
    builder.wall_v("SerpentineGateA", -4000, -4800, 2400, doors=(-800,), material_name=mat)
    builder.wall_v("SerpentineGateB", -2400, -2400, 4800, doors=(800,), material_name=mat)
    builder.wall_v("SerpentineGateC", -800, -4800, -800, doors=(-2400,), material_name=mat)
    builder.wall_v("SerpentineGateD", 800, 2400, 4800, doors=(3200,), material_name=mat)
    builder.wall_v("SerpentineGateE", 2400, -4800, 2400, doors=(-800,), material_name=mat)
    builder.wall_v("SerpentineGateF", 4000, -2400, 4800, doors=(800,), material_name=mat)
    builder.wall_h("NorthGalleryBend", 4000, -5600, -3200, doors=(-4800,), material_name=mat)
    builder.wall_h("TargetWingBend", 2400, 4000, 6000, doors=(4800,), material_name=mat)
    builder.wall_h("SouthServiceBend", -4000, -3200, 2400, doors=(-1600, 800), material_name=mat)
    builder.wall_v("SecurityWest", 4400, -5200, -3600, doors=(-4400,), material_name=mat)
    builder.wall_h("SecurityNorth", -3600, 4400, 6000, doors=(5200,), material_name=mat)
    builder.wall_v("DetentionDivider", 5200, -5200, -3600, doors=(-4400,), material_name=mat)
    builder.security_detention_props(4800, -4500, -90.0)
    screen_specs = (
        ("00A", -5000, -3500, 10, 500), ("00B", -3600, -3500, -15, 450),
        ("01A", -3400, -400, 75, 520), ("01B", -3000, 700, 20, 420),
        ("02A", -1800, 2600, -20, 520), ("02B", -600, 3200, 15, 480),
        ("03A", 1200, -2600, 20, 520), ("03B", 2100, -1700, 75, 460),
        ("04A", 3000, 1000, -15, 560), ("04B", 3500, 2200, 70, 440),
        ("05A", -1200, -1200, 25, 420), ("05B", 1800, 800, -25, 420),
    )
    for key, x, y, yaw, length in screen_specs:
        builder.screen("LDV2_M02_FoldingScreen_{}".format(key), (x, y, 0), yaw, length, "worn_wood", 2.2, "Theme/FoldingScreens")
        radians = math.radians(yaw)
        half_x = math.cos(radians) * length * 0.5
        half_y = math.sin(radians) * length * 0.5
        for suffix, direction in (("A", -1.0), ("B", 1.0)):
            builder.static(
                "LDV2_M02_ScreenPost_{}_{}".format(key, suffix),
                "pillar",
                (x + half_x * direction, y + half_y * direction, 0),
                yaw,
                (0.6, 0.6, 0.44),
                "copper",
                "Theme/FoldingScreens",
            )
    builder.static("LDV2_M02_Topology_Serpentine_Gate_A", "pillar_frame", (-4000, -800, 0), 0.0, (1.1, 1.1, 1.0), "copper", "Theme/SerpentineGates")
    builder.static("LDV2_M02_Topology_Serpentine_Gate_F", "pillar_frame", (4000, 800, 0), 0.0, (1.1, 1.1, 1.0), "copper", "Theme/SerpentineGates")
    builder.static("LDV2_M02_MoonPool", "cube", (-600, 800, -7), 0.0, (9.0, 6.0, 0.12), "water", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeNorth", "cube", (-600, 1110, 12), 0.0, (10.0, 0.18, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeSouth", "cube", (-600, 490, 12), 0.0, (10.0, 0.18, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeWest", "cube", (-1060, 800, 12), 0.0, (0.18, 6.4, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeEast", "cube", (-140, 800, 12), 0.0, (0.18, 6.4, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolBridge", "cube", (-600, 800, 15), 0.0, (11.0, 2.4, 0.2), "oak", "Theme/MoonCourtyard")
    garden_specs = ((-1300, 200, "bush", 0.8), (-1200, 1450, "rock", 0.7), (150, 1500, "bush", 0.9), (500, 200, "rock", 0.8), (-1500, 800, "rock", 0.55), (700, 900, "bush", 0.65), (200, 1900, "rock", 0.6), (-100, -100, "bush", 0.7))
    for index, spec in enumerate(garden_specs):
        builder.static("LDV2_M02_MoonCourt_{:02d}".format(index), spec[2], (spec[0], spec[1], 0), index * 37.0, (spec[3], spec[3], spec[3]), "moss" if spec[2] == "rock" else None, "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonCourtPlinth", "platform", (1100, 1200, 0), 25.0, (1.2, 1.2, 3.0), "copper", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonCourtCenter", "statue", (1100, 1200, 30), 25.0, (2.0, 2.0, 2.0), "copper", "Theme/MoonCourtyard")
    portal_specs = ((-4000, -800, 0), (-2400, 800, 0), (-800, -2400, 0), (800, 3200, 0), (2400, -800, 0), (4000, 800, 0), (-4800, 4000, 90), (4800, 2400, 90))
    for index, pos in enumerate(portal_specs):
        builder.portal("LDV2_M02_Portal_{:02d}".format(index), (pos[0], pos[1], 0), pos[2], "oak", (1.0, 1.8, 1.05))
    lamp_positions = ((-5200, -3200, 90), (-5200, 800, 90), (-4400, 3200, 90), (-2800, 4400, 180), (-1200, -3600, 0), (1200, 3600, 180), (2800, -3200, 0), (3600, 1600, 90), (5200, -1200, -90), (5200, 3000, -90))
    for index, pos in enumerate(lamp_positions):
        builder.static("LDV2_M02_GalleryLamp_{:02d}".format(index), "wall_lamp", (pos[0], pos[1], 250), pos[2], (1.0, 1.0, 1.0), "copper", "Lighting/Fixtures")
        builder.point_light("LDV2_M02_WarmLight_{:02d}".format(index), (pos[0], pos[1], 230), (255, 176, 105), 900.0, 1050.0)
    for index, pos in enumerate(((-5050, -3500, 90), (-3850, -2850, 90)), start=len(lamp_positions)):
        builder.static("LDV2_M02_GalleryLamp_{:02d}".format(index), "wall_lamp", (pos[0], pos[1], 230), pos[2], (1.0, 1.0, 1.0), "copper", "Lighting/Fixtures")
        builder.point_light("LDV2_M02_WarmLight_{:02d}".format(index), (pos[0], pos[1], 220), (255, 176, 105), 800.0, 900.0)
    builder.point_light("LDV2_M02_MoonCourtLight", (-600, 800, 360), (145, 185, 255), 1900.0, 2100.0)


def add_m03_geometry(builder):
    builder.floor_grid()
    builder.perimeter(True)
    mat = builder.config["wall_material"]
    builder.ceiling_rect("LDV2_M03_GlassRoof_West", (-4400, 0), (7200, 2400), "glass", (1600.0, 2400.0), z=412.0)
    builder.ceiling_rect("LDV2_M03_GlassRoof_Crossing", (400, 0), (2400, 2400), "glass", (1200.0, 2400.0), z=600.0)
    builder.ceiling_rect("LDV2_M03_GlassRoof_East", (4800, 0), (6400, 2400), "glass", (1600.0, 2400.0), z=412.0)
    north_links = (-6400, -2400, 800, 4800, 6400)
    south_links = (-4800, -800, 3200, 6400)
    builder.wall_h("SpineNorth", 1200, -7200, 7200, doors=north_links, windows=(-5600, -4000, -800, 2400, 4000, 5600), material_name=mat)
    builder.wall_h("SpineSouth", -1200, -7200, 7200, doors=south_links, windows=(-6400, -3200, 800, 2400, 4800), material_name=mat)
    builder.wall_h("GlassLane", 2800, -7200, 7200, doors=(-4400, -1200, 2000, 5200), windows=tuple(range(-7200, 7201, 800)), material_name=None)
    builder.wall_h("BackstageLane", -2800, -7200, 7200, doors=(-5200, -2000, 1200, 4400, 6800), material_name=mat)
    for index, x in enumerate((-4800, -1600, 2400, 5600)):
        north_door = (2000, 2800, 3600)[index % 3]
        south_door = (-3600, -2800, -2000)[(index + 1) % 3]
        builder.wall_v("NorthBay_{:02d}".format(index), x, 2000, 3600, doors=(north_door,), material_name=mat)
        builder.wall_v("SouthBay_{:02d}".format(index), x, -3600, -2000, doors=(south_door,), material_name=mat)
    builder.wall_v("HighValueAirlockWest", 6000, -2800, -1200, doors=(-2000,), material_name=mat)
    builder.wall_h("HighValueAirlockNorth", -1200, 6000, 7600, doors=(6800,), material_name=mat)
    builder.wall_v("SecurityWest", 5200, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.wall_h("SecurityNorth", -4000, 5600, 7200, doors=(6400,), material_name=mat)
    builder.wall_v("DetentionDivider", 6400, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.security_detention_props(6000, -3500, -90.0)
    builder.static("LDV2_M03_Topology_BraidedCrossing", "cube", (400, 0, 505), 45.0, (2.4, 2.4, 0.12), "tech", "Theme/BraidedCrossing")
    builder.static("LDV2_M03_CrossingSuspendedFrame", "pillar_frame", (400, 0, 380), 90.0, (1.5, 1.5, 1.2), "nickel", "Theme/BraidedCrossing")
    island_specs = (
        (-6800, -820, -12, 0.9, 2.5), (-5200, 820, 12, 1.0, 3.0),
        (-3600, 820, -8, 0.9, 2.5), (-2000, -820, 15, 1.05, 3.0),
        (-400, -820, -12, 0.95, 2.5), (1400, 820, 10, 1.05, 3.0),
        (3000, 820, -15, 0.9, 2.5), (4600, -820, 8, 1.05, 3.0),
        (6200, -820, -10, 0.95, 2.5),
    )
    for index, spec in enumerate(island_specs):
        builder.static("LDV2_M03_SpineTechPlinth_{:02d}".format(index), "platform", (spec[0], spec[1], 0), spec[2], (spec[3], spec[3], spec[4]), "tech" if index % 3 else "nickel", "Theme/PublicSpine")
        exhibit_material = "glass" if index % 3 == 0 else ("nickel" if index % 3 == 1 else "tech")
        exhibit_scale = (0.38 + (index % 2) * 0.12, 0.38 + ((index + 1) % 2) * 0.12, 0.85 + (index % 3) * 0.18)
        builder.static("LDV2_M03_SpineExhibit_{:02d}".format(index), "cube", (spec[0], spec[1], spec[4] * 10.0), spec[2] + 18.0, exhibit_scale, exhibit_material, "Theme/PublicSpine")
    glass_specs = ((-6000, 3420, 0), (-4000, 3320, 180), (-2000, 3460, 0), (0, 3340, 180), (2000, 3460, 0), (4000, 3320, 180), (6000, 3420, 0))
    for index, spec in enumerate(glass_specs):
        builder.static("LDV2_M03_GlassLaneDisplay_{:02d}".format(index), "glass_window", (spec[0], spec[1], 0), spec[2], (1.0, 1.8, 1.35), "glass", "Theme/GlassLane")
    shelf_specs = ((-6000, -3500, 180), (-4200, -3260, 165), (-2200, -3540, 195), (-200, -3260, 165), (1800, -3540, 195), (3800, -3260, 165), (5600, -3500, 180))
    for index, spec in enumerate(shelf_specs):
        for tier, height in enumerate((100, 190, 280)):
            label = "LDV2_M03_BackstageShelf_{:02d}".format(index)
            if tier:
                label += "_Tier_{:02d}".format(tier)
            builder.static(label, "shelf", (spec[0], spec[1], height), spec[2], (1.0, 0.9, 1.0), "nickel", "Theme/Backstage")
    for index, x in enumerate((-4800, -1600, 2400, 5600)):
        builder.static("LDV2_M03_SpinePortal_{:02d}".format(index), "pillar_frame", (x, 0, 0), 90.0, (1.15, 1.15, 1.0), "nickel", "Theme/PublicSpine")
        builder.static("LDV2_M03_PodWindowFrame_{:02d}".format(index), "window_frame", (x, 2800, 145), 90.0, (1.0, 1.8, 2.0), "nickel", "Theme/GlassLane")
    side_portals = ((-6400, 1200), (-2400, 1200), (800, 1200), (4800, 1200), (6400, 1200), (-4800, -1200), (-800, -1200), (3200, -1200), (6400, -1200))
    for index, pos in enumerate(side_portals):
        builder.portal("LDV2_M03_SidePortal_{:02d}".format(index), (pos[0], pos[1], 0), 90.0, "nickel", (1.0, 1.8, 1.05), "Theme/PublicSpine")
    baffle_specs = ((-6100, 650, 70), (-5800, -650, 110), (-2600, 930, 75), (-2200, -930, 105), (1200, 930, 75), (1600, -930, 105), (4600, 930, 75), (5000, -930, 105))
    for index, spec in enumerate(baffle_specs):
        builder.static("LDV2_M03_SpineGlassBaffle_{:02d}".format(index), "glass_window", (spec[0], spec[1], 0), spec[2], (0.55, 1.5, 1.2), "glass", "Theme/PublicSpine")
    restore_tables = ((-5000, -2200, 15), (-2600, -2400, -12), (200, -2200, 12), (2800, -2400, -15))
    for index, spec in enumerate(restore_tables):
        builder.static("LDV2_M03_RestorationTable_{:02d}".format(index), "table", (spec[0], spec[1], 0), spec[2], (1.05, 1.05, 1.05), "nickel", "Theme/Backstage")
        builder.static("LDV2_M03_RestorationChair_{:02d}".format(index), "chair", (spec[0] + 260, spec[1] - 160, 0), spec[2] + 180.0, (0.85, 0.85, 0.85), None, "Theme/Backstage")
    for index, x in enumerate((-8000, -6400, -4800, -3200, -1600, 0, 800, 1600, 3200, 4800, 6400, 8000)):
        roof_z = 588 if -800 <= x <= 1600 else 400
        builder.static("LDV2_M03_RoofRib_{:02d}".format(index), "cube", (x, 0, roof_z), 0.0, (0.15, 24.0, 0.15), "nickel", "Theme/GlassRoof")
    for index, y in enumerate((-1200, 1200)):
        builder.static("LDV2_M03_RoofRail_{:02d}".format(index), "cube", (0, y, 400), 0.0, (160.0, 0.15, 0.15), "nickel", "Theme/GlassRoof")
    for index, x in enumerate((-6800, -5200, -3600, -2000, -400, 1400, 3000, 4600, 6200)):
        y = 0 if index % 2 == 0 else 180
        builder.static("LDV2_M03_CeilingLamp_{:02d}".format(index), "ceiling_lamp", (x, y, 360), 0.0, (0.72, 0.72, 0.72), "nickel", "Lighting/Fixtures")
        builder.point_light("LDV2_M03_SpineLight_{:02d}".format(index), (x, y, 315), (172, 216, 255), 1000.0, 1150.0)
    for index, pos in enumerate(((-5600, -2600), (-800, -2600), (3600, -2600), (6000, -3000))):
        builder.point_light("LDV2_M03_EmergencyLight_{:02d}".format(index), (pos[0], pos[1], 240), (255, 72, 58), 650.0, 800.0)
    builder.point_light("LDV2_M03_CrossingLight", (400, 0, 520), (150, 220, 255), 1800.0, 1900.0)


for code in selected_level_codes:
    config = MAPS[code]
    builder = LevelBuilder(code, config)
    if REMOVE_LEGACY_ARCHITECTURE:
        builder.cleanup_legacy_static()
    if code == "M01":
        add_m01_geometry(builder)
    elif code == "M02":
        add_m02_geometry(builder)
    else:
        add_m03_geometry(builder)
    builder.configure_entry_exit_and_nav()
    cases = builder.configure_cases()
    builder.configure_guards()
    builder.configure_cameras()
    builder.configure_lasers(cases)
    builder.cleanup_orphaned_generated()
    builder.save()
