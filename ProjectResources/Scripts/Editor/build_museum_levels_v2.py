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
    "door": "/Game/Assets/StarterContent/Props/SM_Door",
    "couch": "/Game/Assets/StarterContent/Props/SM_Couch",
    "chair": "/Game/Assets/StarterContent/Props/SM_Chair",
    "table": "/Game/Assets/StarterContent/Props/SM_TableRound",
    "statue": "/Game/Assets/StarterContent/Props/SM_Statue",
    "shelf": "/Game/Assets/StarterContent/Props/SM_Shelf",
    "rock": "/Game/Assets/StarterContent/Props/SM_Rock",
    "bush": "/Game/Assets/StarterContent/Props/SM_Bush",
    "wall_lamp": "/Game/Assets/StarterContent/Props/SM_Lamp_Wall",
    "ceiling_lamp": "/Game/Assets/StarterContent/Props/SM_Lamp_Ceiling",
    "corner_frame": "/Game/Assets/StarterContent/Props/SM_CornerFrame",
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
        "exit": (6400, 4400, 180.0),
        "nav_scale": (72.0, 52.0, 10.0),
        "cases": [
            ("Target", 6000, 2800, 90.0),
            ("HighValue", 6000, -2800, 90.0),
            ("01", -6800, -2400, -90.0), ("02", -6800, 0, -90.0),
            ("03", -6800, 2400, -90.0), ("04", -5200, -1200, 90.0),
            ("05", -5200, 1200, 90.0), ("06", -3200, -4400, 180.0),
            ("07", -800, -4400, 180.0), ("08", 2000, -4400, 180.0),
            ("09", 3600, -4400, 180.0), ("10", 4800, 0, 90.0),
            ("11", 3600, 4400, 0.0), ("12", 1200, 4400, 0.0),
            ("13", -1200, 4400, 0.0), ("14", -3600, 4400, 0.0),
            ("15", -2800, 800, -90.0), ("16", -2800, -800, -90.0),
            ("17", 2800, 800, 90.0), ("18", 2800, -800, 90.0),
        ],
        "guard_routes": [
            [(-6000, -3200), (-1600, -4400), (3600, -3800), (5400, -2800), (5400, 2800), (2400, 4400), (-3600, 3800), (-6000, 2400)],
            [(-6000, -1600), (-5800, 1200), (-3600, 3600), (-1200, 3800), (-2800, 1600)],
            [(4800, -2800), (6000, -800), (4800, 1200), (5400, 2800), (3200, 3200), (3400, 800)],
            [(-2200, -800), (-1600, -2400), (1600, -2400), (2200, -800), (2200, 800), (1600, 2400), (-1600, 2400), (-2200, 800)],
            [(-6000, -4400), (-4400, -4400), (-4400, -3400), (-6000, -3400)],
        ],
        "cameras": [
            (-5200, -2800, 315, 25), (-4400, 3200, 315, -40),
            (-800, -3600, 315, 70), (1600, 3600, 315, -110),
            (4800, -2000, 315, 155), (5600, 2400, 315, -150),
        ],
        "lasers": [
            ("HighValue", (5400, -2800, 120, 90), (4600, -3600, 0, 0)),
            ("11", (3600, 4000, 120, 0), (2800, 3600, 0, 0)),
        ],
    },
    "M02": {
        "path": "/Game/Maps/M02_MoonlitPrototype",
        "half_x": 6400,
        "half_y": 5600,
        "floor_material": "m02_floor",
        "wall_material": "m02_wall",
        "vent_entries": [(-6000, -2400, 90.0), (2400, 5200, 0.0)],
        "player_starts": [(-5400, -2850), (-5400, -2250), (-5800, -2850), (-5800, -2250)],
        "exit": (5600, 4800, 180.0),
        "nav_scale": (64.0, 56.0, 10.0),
        "cases": [
            ("Target", 5200, 3200, 90.0),
            ("HighValue", -5200, 3200, -90.0),
            ("01", -5600, -3600, -90.0), ("02", -5600, -1200, -90.0),
            ("03", -5600, 1200, -90.0), ("04", -3600, 4400, 0.0),
            ("05", -1600, 4400, 0.0), ("06", 800, 4400, 0.0),
            ("07", 3200, 4400, 0.0), ("08", 5200, 800, 90.0),
            ("09", 5200, -1600, 90.0), ("10", 5200, -4000, 90.0),
            ("11", 2800, -4400, 180.0), ("12", 400, -4400, 180.0),
            ("13", -2000, -4400, 180.0), ("14", -3600, -2800, -90.0),
            ("15", -2800, -800, 90.0), ("16", -1200, 2200, 180.0),
            ("17", 1200, -2200, 0.0), ("18", 2800, 800, -90.0),
        ],
        "guard_routes": [
            [(-3200, -2400), (-1200, -800), (-2800, 800), (-1200, 3000), (1200, 2200), (2400, 1200), (1200, -2800), (3200, -2400)],
            [(-5000, -3600), (-5000, 1200), (-4600, 3200), (-3600, 3800), (-2000, 2800), (-3000, -2800)],
            [(4600, -4000), (4000, -2800), (5200, -800), (4000, 1200), (4600, 3200), (3200, 3800)],
            [(-1600, 3800), (800, 3800), (2400, 3200), (800, 2400), (-1200, 2800), (-2800, 3200)],
            [(4200, -5000), (5800, -5000), (5800, -3800), (4200, -3800)],
        ],
        "cameras": [
            (-4000, -3200, 315, 35), (-3200, 3200, 315, -55),
            (3200, -2400, 315, 145), (4000, 2400, 315, -125),
        ],
        "lasers": [
            ("HighValue", (-4800, 3200, 120, 90), (-4000, 4000, 0, 0)),
            ("07", (3200, 4000, 120, 0), (2400, 3200, 0, 0)),
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
        "exit": (7200, 3200, 180.0),
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
            [(-6600, 0), (-4800, -300), (-3200, -400), (-1600, 300), (0, 400), (1600, -300), (3200, -400), (4800, 300), (6800, 0)],
            [(-6000, 2400), (-4400, 2600), (-2800, 2400), (-1200, 3200), (400, 2400), (2000, 3200), (3600, 2400), (5200, 2600), (6200, 2400)],
            [(-6200, -2800), (-4000, -2400), (-2000, -3200), (0, -2400), (2400, -2800), (4000, -2400), (6000, -3200)],
            [(5200, -3400), (7400, -3800), (7400, -2800), (5000, -2600)],
        ],
        "cameras": [
            (-6400, -2200, 315, 30), (-5200, 2400, 315, -35),
            (-2800, -1800, 315, 60), (-1200, 2400, 315, -65),
            (2000, -2200, 315, 120), (3600, 2400, 315, -120),
            (6000, -2200, 315, 150), (6800, 2000, 315, -155),
        ],
        "lasers": [
            ("HighValue", (6200, -1600, 120, 90), (5600, -2400, 0, 0)),
            ("08", (4000, -2800, 120, 0), (3200, -2200, 0, 0)),
            ("10", (5200, 2800, 120, 0), (4400, 2200, 0, 0)),
        ],
    },
}


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

    def ceiling_rect(self, label_prefix, center, size, material_name, max_tile_size=(1600.0, 1600.0), mesh_name="floor"):
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
                    (start_x + x_index * tile_x, start_y + y_index * tile_y, 412),
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
        self.static("LDV2_{}_SecurityDesk".format(self.code), "table", (origin_x, origin_y, 0), yaw, (1.0, 1.0, 1.0), None, "SecurityDetention")
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
    builder.ceiling_rect("LDV2_M01_Ceiling_West", (-5000, 0), (4400, 10400), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_East", (5000, 0), (4400, 10400), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_North", (0, 3800), (5600, 2800), mat)
    builder.ceiling_rect("LDV2_M01_Ceiling_South", (0, -3800), (5600, 2800), mat)
    builder.wall_v("OuterRingWest", -4400, -3600, 3600, doors=(-2000, 400, 2800), material_name=mat)
    builder.wall_v("OuterRingEast", 4400, -3600, 3600, doors=(-2800, -400, 2000), material_name=mat)
    builder.wall_h("OuterRingSouth", -4000, -3600, 3600, doors=(-2800, -400, 2000), material_name=mat)
    builder.wall_h("OuterRingNorth", 4000, -3600, 3600, doors=(-2000, 400, 2800), material_name=mat)
    builder.wall_h("WestWingLower", -2000, -6800, -4400, doors=(-5200,), material_name=mat)
    builder.wall_h("WestWingUpper", 2000, -6800, -4400, doors=(-6000,), material_name=mat)
    builder.wall_h("EastWingLower", -2000, 4400, 6800, doors=(6000,), material_name=mat)
    builder.wall_h("EastWingUpper", 2000, 4400, 6800, doors=(5200,), material_name=mat)
    builder.wall_v("SecurityEast", -4400, -5200 + 400, -3200, doors=(-4000,), material_name=mat)
    builder.wall_h("SecurityNorth", -3200, -6800, -4400, doors=(-5200,), material_name=mat)
    builder.wall_v("DetentionDivider", -6000, -4800, -3200, doors=(-4000,), material_name=mat)
    builder.wall_v("InnerGalleryWest", -2800, -1200, 1200, doors=(-400, 1200), material_name=mat)
    builder.wall_v("InnerGalleryEast", 2800, -1200, 1200, doors=(-1200, 400), material_name=mat)
    builder.wall_h("InnerGalleryNorth", 2400, -2000, 2000, doors=(-400, 1200), material_name=mat)
    builder.wall_h("InnerGallerySouth", -2400, -2000, 2000, doors=(-1200, 400), material_name=mat)
    builder.security_detention_props(-5200, -4200, 90.0)
    rotunda_pillars = []
    for index in range(8):
        angle = math.radians(22.5 + index * 45.0)
        rotunda_pillars.append((math.cos(angle) * 1800, math.sin(angle) * 1550))
    rotunda_pillars.extend(((-2400, 0), (2400, 0), (0, -2050), (0, 2050)))
    for index, pos in enumerate(rotunda_pillars):
        builder.static("LDV2_M01_RotundaPillar_{:02d}".format(index), "pillar", (pos[0], pos[1], -12), 0.0, (1.0, 1.0, 0.8), "gold" if index >= 8 else None, "Theme/Rotunda")
    builder.static("LDV2_M01_HeroPlinth", "platform", (0, 0, 0), 45.0, (1.65, 1.65, 4.0), "gold", "Theme/Rotunda")
    statue_specs = ((0, 0, 45.0, 2.4, 40), (-5600, 0, 90.0, 1.6, 0), (5200, 0, -90.0, 1.6, 0), (0, 3500, 180.0, 1.5, 0))
    for index, spec in enumerate(statue_specs):
        builder.static("LDV2_M01_RotundaStatue_{:02d}".format(index), "statue", (spec[0], spec[1], spec[4]), spec[2], (spec[3], spec[3], spec[3]), None, "Theme/Rotunda" if index == 0 else "Theme/GalleryLandmarks")
    for index, pos in enumerate(((-5650, -2850, 25), (-5650, -500, -20))):
        builder.static("LDV2_M01_EntryPlinth_{:02d}".format(index), "platform", (pos[0], pos[1], 0), pos[2], (0.8, 0.8, 3.0), "gold", "Theme/EntryGallery")
        builder.static("LDV2_M01_EntryStatue_{:02d}".format(index), "statue", (pos[0], pos[1], 30), pos[2], (1.4, 1.4, 1.4), None, "Theme/EntryGallery")
    couch_specs = ((-6000, -800, 90), (-5600, 1400, 90), (5600, -600, -90), (5200, 1200, -90), (-2000, -4200, 0), (2800, -4200, 180), (-2800, 4200, 0), (2000, 4200, 180))
    for index, pos in enumerate(couch_specs):
        builder.static("LDV2_M01_GalleryCouch_{:02d}".format(index), "couch", (pos[0], pos[1], 0), pos[2], (0.9, 0.9, 0.9), None, "Theme/Galleries")
    for index, pos in enumerate(((-3800, -2600, 45), (-3600, 2800, -45), (3800, -2600, 135), (3600, 2800, -135))):
        builder.static("LDV2_M01_ReadingTable_{:02d}".format(index), "table", (pos[0], pos[1], 0), pos[2], (1.05, 1.05, 1.05), None, "Theme/Galleries")
    portal_specs = ((-4400, -2000, 90), (-4400, 400, 90), (-4400, 2800, 90), (4400, -2800, 90), (4400, -400, 90), (4400, 2000, 90), (-400, -4000, 0), (400, 4000, 0))
    for index, pos in enumerate(portal_specs):
        corrected_yaw = 0.0 if abs(pos[0]) == 4400 else 90.0
        builder.portal("LDV2_M01_Portal_{:02d}".format(index), (pos[0], pos[1], 0), corrected_yaw, "gold", (1.0, 1.8, 1.05))
    inner_portals = ((-2800, -400, 0), (-2800, 1200, 0), (2800, -1200, 0), (2800, 400, 0), (-1200, -2400, 90), (400, -2400, 90), (-400, 2400, 90), (1200, 2400, 90))
    for index, pos in enumerate(inner_portals):
        builder.portal("LDV2_M01_InnerPortal_{:02d}".format(index), (pos[0], pos[1], 0), pos[2], "gold", (1.0, 1.8, 1.05))
    for label, location, scale in (
        ("SkylightRimNorth", (0, 2400, 390), (56.0, 0.18, 0.15)),
        ("SkylightRimSouth", (0, -2400, 390), (56.0, 0.18, 0.15)),
        ("SkylightRimWest", (-2800, 0, 390), (0.18, 48.0, 0.15)),
        ("SkylightRimEast", (2800, 0, 390), (0.18, 48.0, 0.15)),
        ("SkylightBeamX", (0, 0, 390), (56.0, 0.16, 0.15)),
        ("SkylightBeamY", (0, 0, 390), (0.16, 48.0, 0.15)),
    ):
        builder.static("LDV2_M01_{}".format(label), "cube", location, 0.0, scale, "gold", "Theme/RotundaCeiling")
    light_positions = ((0, 0), (-5600, 0), (5600, 0), (0, -3400), (0, 3400), (-3600, -2600), (3600, -2600), (-3600, 2800), (3600, 2800), (-5200, -4200))
    for index, pos in enumerate(light_positions):
        builder.static("LDV2_M01_CeilingLamp_{:02d}".format(index), "ceiling_lamp", (pos[0], pos[1], 370), 0.0, (0.8, 0.8, 0.8), "gold", "Lighting/Fixtures")
        builder.point_light("LDV2_M01_WarmLight_{:02d}".format(index), (pos[0], pos[1], 320), (255, 205, 145), 1150.0 if index else 1900.0, 1350.0)


def add_m02_geometry(builder):
    builder.floor_grid()
    builder.perimeter(False)
    mat = builder.config["wall_material"]
    builder.ceiling_rect("LDV2_M02_Ceiling_West", (-4200, 0), (4400, 11200), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_East", (4200, 0), (4400, 11200), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_North", (0, 3600), (4000, 4000), "m01_wall")
    builder.ceiling_rect("LDV2_M02_Ceiling_South", (0, -3600), (4000, 4000), "m01_wall")
    builder.wall_h("CourtyardNorth", 1600, -1600, 1600, doors=(0,), windows=(-1600, -800, 800, 1600), material_name=None)
    builder.wall_h("CourtyardSouth", -1600, -1600, 1600, doors=(0,), windows=(-1600, -800, 800, 1600), material_name=None)
    builder.wall_v("CourtyardWest", -2000, -1200, 1200, doors=(0,), windows=(-800, 800), material_name=None)
    builder.wall_v("CourtyardEast", 2000, -1200, 1200, doors=(0,), windows=(-800, 800), material_name=None)
    builder.wall_v("GalleryWest", -4000, -4400, 4400, doors=(-2800, -400, 2000), material_name=mat)
    builder.wall_v("GalleryEast", 4000, -4400, 4400, doors=(-2000, 400, 2800), material_name=mat)
    builder.wall_h("GallerySouth", -3600, -3600, 3600, doors=(-2800, -400, 2000), material_name=mat)
    builder.wall_h("GalleryNorth", 3600, -3600, 3600, doors=(-2000, 400, 2800), material_name=mat)
    builder.wall_h("WestBendLower", -2400, -6000, -4000, doors=(-5200,), material_name=mat)
    builder.wall_h("WestBendUpper", 2400, -6000, -4000, doors=(-4400,), material_name=mat)
    builder.wall_h("EastBendLower", -2400, 4000, 6000, doors=(4400,), material_name=mat)
    builder.wall_h("EastBendUpper", 2400, 4000, 6000, doors=(5200,), material_name=mat)
    builder.wall_v("SecurityWest", 4400, -5200, -3600, doors=(-4400,), material_name=mat)
    builder.wall_h("SecurityNorth", -3600, 4400, 6000, doors=(5200,), material_name=mat)
    builder.wall_v("DetentionDivider", 5200, -5200, -3600, doors=(-4400,), material_name=mat)
    builder.security_detention_props(4800, -4500, -90.0)
    screen_specs = (
        ("00A", -3750, -2400, 0, 600), ("00B", -2300, -2400, 0, 500),
        ("01", -3600, -400, 90, 600), ("02", -2800, 1800, 0, 700),
        ("03", -300, 3100, 0, 500), ("04", 2200, 2200, 90, 600),
        ("05", 3000, 800, 90, 700), ("06", 2600, -1800, 0, 700),
        ("07", 400, -3000, 90, 500), ("08", -1200, -2200, 0, 600),
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
    builder.static("LDV2_M02_MoonPool", "cube", (400, -200, -7), 0.0, (11.0, 7.5, 0.12), "water", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeNorth", "cube", (400, 190, 12), 0.0, (12.0, 0.18, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeSouth", "cube", (400, -590, 12), 0.0, (12.0, 0.18, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeWest", "cube", (-160, -200, 12), 0.0, (0.18, 8.0, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolEdgeEast", "cube", (960, -200, 12), 0.0, (0.18, 8.0, 0.18), "moss", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonPoolBridge", "cube", (400, -200, 15), 90.0, (8.5, 3.0, 0.08), "oak", "Theme/MoonCourtyard")
    garden_specs = ((-850, -650, "bush", 0.8), (-450, 500, "rock", 0.7), (950, 450, "bush", 0.9), (1300, -700, "rock", 0.8), (-1100, 100, "rock", 0.55), (1500, 100, "bush", 0.65), (0, 850, "rock", 0.6), (700, -1150, "bush", 0.7))
    for index, spec in enumerate(garden_specs):
        builder.static("LDV2_M02_MoonCourt_{:02d}".format(index), spec[2], (spec[0], spec[1], 0), index * 37.0, (spec[3], spec[3], spec[3]), "moss" if spec[2] == "rock" else None, "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonCourtPlinth", "platform", (-650, 250, 0), 25.0, (1.2, 1.2, 3.0), "copper", "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonCourtCenter", "statue", (-650, 250, 30), 25.0, (2.0, 2.0, 2.0), "copper", "Theme/MoonCourtyard")
    portal_specs = ((-4000, -2800, 90), (-4000, -400, 90), (-4000, 2000, 90), (4000, -2000, 90), (4000, 400, 90), (4000, 2800, 90), (0, -3600, 0), (0, 3600, 0))
    for index, pos in enumerate(portal_specs):
        corrected_yaw = 0.0 if abs(pos[0]) == 4000 else 90.0
        builder.portal("LDV2_M02_Portal_{:02d}".format(index), (pos[0], pos[1], 0), corrected_yaw, "oak", (1.0, 1.8, 1.05))
    lamp_positions = ((-5200, -400, 90), (-5200, 3200, 90), (5200, 800, -90), (5200, -3200, -90), (-2800, 4400, 180), (2800, 4400, 180), (-2800, -4400, 0), (2400, -4400, 0), (-1200, 2200, 180), (1200, -2200, 0))
    for index, pos in enumerate(lamp_positions):
        builder.static("LDV2_M02_GalleryLamp_{:02d}".format(index), "wall_lamp", (pos[0], pos[1], 250), pos[2], (1.0, 1.0, 1.0), "copper", "Lighting/Fixtures")
        builder.point_light("LDV2_M02_WarmLight_{:02d}".format(index), (pos[0], pos[1], 230), (255, 176, 105), 900.0, 1050.0)
    for index, pos in enumerate(((-4050, -3150, 90), (-4050, -2450, 90)), start=len(lamp_positions)):
        builder.static("LDV2_M02_GalleryLamp_{:02d}".format(index), "wall_lamp", (pos[0], pos[1], 230), pos[2], (1.0, 1.0, 1.0), "copper", "Lighting/Fixtures")
        builder.point_light("LDV2_M02_WarmLight_{:02d}".format(index), (pos[0], pos[1], 220), (255, 176, 105), 800.0, 900.0)
    builder.point_light("LDV2_M02_MoonCourtLight", (400, -200, 360), (145, 185, 255), 1900.0, 2100.0)


def add_m03_geometry(builder):
    builder.floor_grid()
    builder.perimeter(True)
    mat = builder.config["wall_material"]
    builder.ceiling_rect("LDV2_M03_GlassRoof", (0, 0), (16000, 2400), "glass", (1600.0, 2400.0))
    cross_links = (-3600, 400, 3600)
    builder.wall_h("SpineNorth", 1200, -7200, 7200, doors=cross_links, windows=(-6800, -5200, -2000, 2000, 5200, 6800), material_name=mat)
    builder.wall_h("SpineSouth", -1200, -7200, 7200, doors=(-3600, -400, 3600), windows=(-6800, -5200, -2000, 2000, 5200, 6800), material_name=mat)
    builder.wall_h("GlassLane", 2800, -7200, 7200, doors=(-4400, -1200, 2000, 5200), windows=tuple(range(-7200, 7201, 800)), material_name=None)
    builder.wall_h("BackstageLane", -2800, -7200, 7200, doors=(-5200, -2000, 1200, 4400), material_name=mat)
    for index, x in enumerate((-4800, -1600, 1600, 4800)):
        north_door = (2000, 2800, 3600)[index % 3]
        south_door = (-3600, -2800, -2000)[(index + 1) % 3]
        builder.wall_v("NorthBay_{:02d}".format(index), x, 2000, 3600, doors=(north_door,), material_name=mat)
        builder.wall_v("SouthBay_{:02d}".format(index), x, -3600, -2000, doors=(south_door,), material_name=mat)
    builder.wall_v("SecurityWest", 5200, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.wall_h("SecurityNorth", -4000, 5600, 7200, doors=(6400,), material_name=mat)
    builder.wall_v("DetentionDivider", 6400, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.security_detention_props(6000, -3500, -90.0)
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
    for index, x in enumerate((-4800, -1600, 1600, 4800)):
        builder.static("LDV2_M03_SpinePortal_{:02d}".format(index), "pillar_frame", (x, 0, 0), 90.0, (1.15, 1.15, 1.0), "nickel", "Theme/PublicSpine")
        builder.static("LDV2_M03_PodWindowFrame_{:02d}".format(index), "window_frame", (x, 2800, 145), 90.0, (1.0, 1.8, 2.0), "nickel", "Theme/GlassLane")
    side_portals = ((-3600, 1200), (400, 1200), (3600, 1200), (-3600, -1200), (-400, -1200), (3600, -1200))
    for index, pos in enumerate(side_portals):
        builder.portal("LDV2_M03_SidePortal_{:02d}".format(index), (pos[0], pos[1], 0), 90.0, "nickel", (1.0, 1.8, 1.05), "Theme/PublicSpine")
    baffle_specs = ((-2800, 930, 75), (-2500, -930, 105), (800, 930, 75), (1100, -930, 105), (4200, 930, 75), (4500, -930, 105))
    for index, spec in enumerate(baffle_specs):
        builder.static("LDV2_M03_SpineGlassBaffle_{:02d}".format(index), "glass_window", (spec[0], spec[1], 0), spec[2], (0.55, 1.5, 1.2), "glass", "Theme/PublicSpine")
    restore_tables = ((-5000, -2200, 15), (-2600, -2400, -12), (200, -2200, 12), (2800, -2400, -15))
    for index, spec in enumerate(restore_tables):
        builder.static("LDV2_M03_RestorationTable_{:02d}".format(index), "table", (spec[0], spec[1], 0), spec[2], (1.05, 1.05, 1.05), "nickel", "Theme/Backstage")
        builder.static("LDV2_M03_RestorationChair_{:02d}".format(index), "chair", (spec[0] + 260, spec[1] - 160, 0), spec[2] + 180.0, (0.85, 0.85, 0.85), None, "Theme/Backstage")
    for index, x in enumerate((-8000, -6400, -4800, -3200, -1600, 0, 1600, 3200, 4800, 6400, 8000)):
        builder.static("LDV2_M03_RoofRib_{:02d}".format(index), "cube", (x, 0, 400), 0.0, (0.15, 24.0, 0.15), "nickel", "Theme/GlassRoof")
    for index, y in enumerate((-1200, 1200)):
        builder.static("LDV2_M03_RoofRail_{:02d}".format(index), "cube", (0, y, 400), 0.0, (160.0, 0.15, 0.15), "nickel", "Theme/GlassRoof")
    for index, x in enumerate((-6800, -5200, -3600, -2000, -400, 1400, 3000, 4600, 6200)):
        y = 0 if index % 2 == 0 else 180
        builder.static("LDV2_M03_CeilingLamp_{:02d}".format(index), "ceiling_lamp", (x, y, 360), 0.0, (0.72, 0.72, 0.72), "nickel", "Lighting/Fixtures")
        builder.point_light("LDV2_M03_SpineLight_{:02d}".format(index), (x, y, 315), (172, 216, 255), 1000.0, 1150.0)
    for index, pos in enumerate(((-5600, -2600), (-800, -2600), (3600, -2600), (6000, -3000))):
        builder.point_light("LDV2_M03_EmergencyLight_{:02d}".format(index), (pos[0], pos[1], 240), (255, 72, 58), 650.0, 800.0)


for code, config in MAPS.items():
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
