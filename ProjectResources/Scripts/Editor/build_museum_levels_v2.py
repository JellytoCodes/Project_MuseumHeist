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
}

BLUEPRINTS = {
    "painting": "/Game/Blueprints/World/Actors/Loot/BP_PaintingDisplayCase",
    "guard": "/Game/Blueprints/Guard/BP_Guard",
    "camera": "/Game/Blueprints/World/Actors/Security/BP_SecurityCamera",
    "laser": "/Game/Blueprints/World/Actors/Security/BP_LaserBarrier",
    "button": "/Game/Blueprints/World/Actors/Security/BP_SecurityHoldButton",
}


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
            ("Target", 5600, 2400, 90.0),
            ("HighValue", 6000, -2400, 90.0),
            ("01", -5600, -2400, -90.0), ("02", -5600, 0, -90.0),
            ("03", -5600, 2400, -90.0), ("04", -3600, -4200, 180.0),
            ("05", -1200, -4400, 180.0), ("06", 1200, -4400, 180.0),
            ("07", 3600, -4200, 180.0), ("08", 4800, -3200, 90.0),
            ("09", 4800, 0, 90.0), ("10", 4800, 3200, 90.0),
            ("11", 3600, 4200, 0.0), ("12", 1200, 4400, 0.0),
            ("13", -1200, 4400, 0.0), ("14", -3600, 4200, 0.0),
            ("15", -4400, 2800, -90.0), ("16", -4400, -2800, -90.0),
            ("17", 0, 3000, 0.0), ("18", 0, -3000, 180.0),
        ],
        "guard_routes": [
            [(-5600, -3200), (0, -4400), (5600, -3200), (5600, 3200), (0, 4400), (-5600, 3200)],
            [(-5200, -2400), (-5200, 2400), (-2400, 4000), (-2400, -4000)],
            [(4800, -2400), (6000, 0), (4800, 2400), (3200, 0)],
            [(-2400, -2400), (2400, -2400), (2400, 2400), (-2400, 2400)],
            [(-6000, -4400), (-4400, -4400), (-4400, -3400), (-6000, -3400)],
        ],
        "cameras": [
            (-5200, -3000, 315, 35), (-5200, 3000, 315, -35),
            (0, -4000, 315, 90), (0, 4000, 315, -90),
            (5200, -3000, 315, 145), (5200, 3000, 315, -145),
        ],
        "lasers": [
            ("HighValue", (5400, -2400, 120, 90), (5000, -3200, 0, 0)),
            ("10", (4400, 3200, 120, 90), (4000, 4000, 0, 0)),
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
            ("Target", 5200, 2800, 90.0),
            ("HighValue", 5200, -2400, 90.0),
            ("01", -5200, -3600, -90.0), ("02", -5200, -1200, -90.0),
            ("03", -5200, 1200, -90.0), ("04", -5200, 3600, -90.0),
            ("05", -3200, -4600, 180.0), ("06", -800, -4800, 180.0),
            ("07", 1600, -4800, 180.0), ("08", 4000, -4400, 180.0),
            ("09", 4400, -2800, 90.0), ("10", 4400, 0, 90.0),
            ("11", 4400, 2800, 90.0), ("12", 3600, 4400, 0.0),
            ("13", 1200, 4800, 0.0), ("14", -1200, 4800, 0.0),
            ("15", -3600, 4400, 0.0), ("16", -3200, 2800, -90.0),
            ("17", -3200, -2800, -90.0), ("18", 0, 2600, 0.0),
        ],
        "guard_routes": [
            [(-2800, -2400), (2800, -2400), (2800, 2400), (-2800, 2400)],
            [(-5200, -3600), (-5200, 3600), (-3600, 4400), (-3600, -4400)],
            [(4400, -3600), (5600, -1200), (5600, 3600), (3600, 4400), (3600, -4400)],
            [(-2400, 4200), (0, 5000), (2400, 4200), (0, 3200)],
            [(4200, -5000), (5800, -5000), (5800, -3800), (4200, -3800)],
        ],
        "cameras": [
            (-3600, -3600, 315, 45), (-3600, 3600, 315, -45),
            (3600, -3600, 315, 135), (3600, 3600, 315, -135),
        ],
        "lasers": [
            ("HighValue", (4800, -2400, 120, 90), (4400, -3200, 0, 0)),
            ("11", (4000, 2800, 120, 90), (3600, 3600, 0, 0)),
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
            ("Target", 6400, 2200, 90.0),
            ("HighValue", 6400, -1600, 90.0),
            ("01", -6800, -2400, -90.0), ("02", -6800, 0, -90.0),
            ("03", -6800, 2400, -90.0), ("04", -5200, -3200, 180.0),
            ("05", -3600, -3200, 180.0), ("06", -2000, -3200, 180.0),
            ("07", -400, -3200, 180.0), ("08", 1200, -3200, 180.0),
            ("09", 2800, -3200, 180.0), ("10", 4400, -3200, 180.0),
            ("11", 5600, -2600, 90.0), ("12", 5200, 3200, 0.0),
            ("13", 3600, 3200, 0.0), ("14", 2000, 3200, 0.0),
            ("15", 400, 3200, 0.0), ("16", -1200, 3200, 0.0),
            ("17", -2800, 3200, 0.0), ("18", -4400, 3200, 0.0),
        ],
        "guard_routes": [
            [(-6800, 0), (-3600, 0), (0, 0), (3600, 0), (6800, 0)],
            [(-6000, 2600), (-2000, 2600), (2000, 2600), (6000, 2600)],
            [(-6000, -3000), (-2000, -3000), (2000, -3000), (6000, -3000)],
            [(5400, -3800), (7400, -3800), (7400, -2800), (5400, -2800)],
        ],
        "cameras": [
            (-6400, -2600, 315, 35), (-6400, 2600, 315, -35),
            (-3200, -2600, 315, 55), (-3200, 2600, 315, -55),
            (3200, -2600, 315, 125), (3200, 2600, 315, -125),
            (6400, -2600, 315, 145), (6400, 2600, 315, -145),
        ],
        "lasers": [
            ("HighValue", (6000, -1600, 120, 90), (5600, -2400, 0, 0)),
            ("10", (4000, -3200, 120, 0), (3600, -2400, 0, 0)),
            ("15", (400, 2800, 120, 0), (1200, 2400, 0, 0)),
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
        self.world = unreal.EditorLoadingAndSavingUtils.load_map(config["path"])
        if not self.world:
            raise RuntimeError("Map load failed: " + config["path"])
        self.actors = list(actor_subsystem.get_all_level_actors())
        self.by_label = {actor.get_actor_label(): actor for actor in self.actors}

    def register(self, actor):
        self.actors.append(actor)
        self.by_label[actor.get_actor_label()] = actor

    def folder(self, actor, suffix):
        actor.set_folder_path(unreal.Name("LDV2/{}/{}".format(self.code, suffix)))

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
        return actor

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
            if actor is None:
                label = "LDV2_{}_Painting_{}".format(self.code, case_key)
                actor = self.blueprint(label, "painting", (x, y, 0), yaw, "Gameplay/PaintingCases")
            else:
                set_transform(actor, (x, y, 0), yaw)
                self.folder(actor, "Gameplay/PaintingCases")
                self.updated += 1
            safe_set(actor, "display_case_id", unreal.Name(display_case_id))
            safe_set(actor, "target_artifact_id", unreal.Name(self.case_artifact_id(case_key)))
            result[case_key] = actor
        return result

    def configure_guards(self):
        guards = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_Guard_C"), key=lambda actor: actor.get_actor_label())
        waypoints = sorted((actor for actor in self.actors if actor.get_class().get_name() == "HeistGuardWaypoint"), key=lambda actor: actor.get_actor_label())
        waypoint_cursor = 0

        for route_index, route in enumerate(self.config["guard_routes"]):
            route_id = unreal.Name("LDV2_{}_Route_{:02d}".format(self.code, route_index + 1))
            if route_index < len(guards):
                guard = guards[route_index]
                set_transform(guard, (route[0][0], route[0][1], 88), 0.0)
                self.folder(guard, "Gameplay/Guards")
                self.updated += 1
            else:
                guard = self.blueprint(
                    "LDV2_{}_Guard_{:02d}".format(self.code, route_index + 1),
                    "guard", (route[0][0], route[0][1], 88), 0.0, "Gameplay/Guards",
                )
            safe_set(guard, "guard_profile_id", unreal.Name("Guard_Alert_Medium"))
            for component in guard.get_components_by_class(unreal.ActorComponent):
                if component.get_class().get_name() == "HeistPatrolPathComponent":
                    safe_set(component, "patrol_route_id", route_id)

            for order, point in enumerate(route):
                if waypoint_cursor < len(waypoints):
                    waypoint = waypoints[waypoint_cursor]
                    set_transform(waypoint, (point[0], point[1], 25), 0.0)
                    self.folder(waypoint, "Gameplay/GuardRoutes")
                    self.updated += 1
                else:
                    waypoint = actor_subsystem.spawn_actor_from_class(waypoint_class, vec((point[0], point[1], 25)), rot(0.0))
                    waypoint.set_actor_label("LDV2_{}_Route_{:02d}_Point_{:02d}".format(self.code, route_index + 1, order))
                    self.register(waypoint)
                    self.folder(waypoint, "Gameplay/GuardRoutes")
                    self.created += 1
                safe_set(waypoint, "patrol_route_id", route_id)
                safe_set(waypoint, "patrol_order", order)
                waypoint_cursor += 1

    def configure_cameras(self):
        existing = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_SecurityCamera_C"), key=lambda actor: actor.get_actor_label())
        for index, spec in enumerate(self.config["cameras"]):
            x, y, z, yaw = spec
            if index < len(existing):
                actor = existing[index]
                set_transform(actor, (x, y, z), yaw)
                self.folder(actor, "Gameplay/CCTV")
                self.updated += 1
            else:
                actor = self.blueprint("LDV2_{}_CCTV_{:02d}".format(self.code, index + 1), "camera", (x, y, z), yaw, "Gameplay/CCTV")
            safe_set(actor, "detection_range", 1800.0)
            safe_set(actor, "detection_half_angle_degrees", 35.0)
            safe_set(actor, "sweep_half_angle_degrees", 35.0)
            safe_set(actor, "sweep_period_seconds", 6.0)

    def configure_lasers(self, cases):
        barriers = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_LaserBarrier_C"), key=lambda actor: actor.get_actor_label())
        buttons = sorted((actor for actor in self.actors if actor.get_class().get_name() == "BP_SecurityHoldButton_C"), key=lambda actor: actor.get_actor_label())
        for index, spec in enumerate(self.config["lasers"]):
            case_key, barrier_spec, button_spec = spec
            if index < len(barriers):
                barrier = barriers[index]
                set_transform(barrier, barrier_spec[:3], barrier_spec[3])
                self.folder(barrier, "Gameplay/Lasers")
                self.updated += 1
            else:
                barrier = self.blueprint("LDV2_{}_Laser_{:02d}".format(self.code, index + 1), "laser", barrier_spec[:3], barrier_spec[3], "Gameplay/Lasers")
            if index < len(buttons):
                button = buttons[index]
                set_transform(button, button_spec[:3], button_spec[3])
                self.folder(button, "Gameplay/Lasers")
                self.updated += 1
            else:
                button = self.blueprint("LDV2_{}_LaserButton_{:02d}".format(self.code, index + 1), "button", button_spec[:3], button_spec[3], "Gameplay/Lasers")
            safe_set(barrier, "protected_painting_case", cases[case_key])
            safe_set(button, "linked_laser_barrier", barrier)

    def security_detention_props(self, origin_x, origin_y, yaw=0.0):
        self.static("LDV2_{}_SecurityDesk".format(self.code), "table", (origin_x, origin_y, 0), yaw, (1.0, 1.0, 1.0), None, "SecurityDetention")
        for index, offset in enumerate((-240, 0, 240)):
            self.static("LDV2_{}_SecurityChair_{:02d}".format(self.code, index), "chair", (origin_x + offset, origin_y - 260, 0), yaw + 180.0, (1.0, 1.0, 1.0), None, "SecurityDetention")
        for index in range(5):
            self.static("LDV2_{}_EvidenceRack_{:02d}".format(self.code, index), "shelf", (origin_x - 700 + index * 300, origin_y + 520, 0), yaw, (0.8, 0.8, 0.8), None, "SecurityDetention")
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
    builder.security_detention_props(-5200, -4200, 90.0)
    for index in range(12):
        angle = math.radians(index * 30.0)
        builder.static("LDV2_M01_RotundaPillar_{:02d}".format(index), "pillar", (math.cos(angle) * 1700, math.sin(angle) * 1700, -12), 0.0, (1.0, 1.0, 0.9), None, "Theme/Rotunda")
    for index, pos in enumerate(((-900, 0), (900, 0), (0, -900), (0, 900))):
        builder.static("LDV2_M01_RotundaStatue_{:02d}".format(index), "statue", (pos[0], pos[1], 0), index * 90.0, (0.75, 0.75, 0.75), None, "Theme/Rotunda")
    for index, pos in enumerate(((-5600, -800), (-5600, 800), (5600, -800), (5600, 800), (-2400, -4400), (2400, -4400), (-2400, 4400), (2400, 4400))):
        builder.static("LDV2_M01_GalleryCouch_{:02d}".format(index), "couch", (pos[0], pos[1], 0), 90.0 if abs(pos[0]) > abs(pos[1]) else 0.0, (0.9, 0.9, 0.9), None, "Theme/Galleries")


def add_m02_geometry(builder):
    builder.floor_grid()
    builder.perimeter(False)
    mat = builder.config["wall_material"]
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
    for index in range(8):
        angle = math.radians(index * 45.0)
        mesh = "bush" if index % 2 == 0 else "rock"
        builder.static("LDV2_M02_MoonCourt_{:02d}".format(index), mesh, (math.cos(angle) * 1100, math.sin(angle) * 850, 0), index * 45.0, (0.75, 0.75, 0.75), None, "Theme/MoonCourtyard")
    builder.static("LDV2_M02_MoonCourtCenter", "statue", (0, 0, 0), 0.0, (0.85, 0.85, 0.85), None, "Theme/MoonCourtyard")
    for index, pos in enumerate(((-5200, -400), (-5200, 3200), (5200, 400), (5200, -3200), (-2800, 4600), (2800, 4600))):
        builder.static("LDV2_M02_GalleryLamp_{:02d}".format(index), "wall_lamp", (pos[0], pos[1], 250), 90.0 if abs(pos[0]) > abs(pos[1]) else 0.0, (1.0, 1.0, 1.0), None, "Theme/Galleries")


def add_m03_geometry(builder):
    builder.floor_grid()
    builder.perimeter(True)
    mat = builder.config["wall_material"]
    cross_links = (-3600, 400, 3600)
    builder.wall_h("SpineNorth", 1200, -7200, 7200, doors=cross_links, windows=(-6800, -5200, -2000, 2000, 5200, 6800), material_name=mat)
    builder.wall_h("SpineSouth", -1200, -7200, 7200, doors=(-3600, -400, 3600), windows=(-6800, -5200, -2000, 2000, 5200, 6800), material_name=mat)
    builder.wall_h("GlassLane", 2800, -7200, 7200, doors=(-4400, -1200, 2000, 5200), windows=tuple(range(-7200, 7201, 800)), material_name=None)
    builder.wall_h("BackstageLane", -2800, -7200, 7200, doors=(-5200, -2000, 1200, 4400), material_name=mat)
    for index, x in enumerate((-4800, -1600, 1600, 4800)):
        builder.wall_v("NorthBay_{:02d}".format(index), x, 2000, 3600, doors=(2800,), material_name=mat)
        builder.wall_v("SouthBay_{:02d}".format(index), x, -3600, -2000, doors=(-2800,), material_name=mat)
    builder.wall_v("SecurityWest", 5200, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.wall_h("SecurityNorth", -4000, 5600, 7200, doors=(6400,), material_name=mat)
    builder.wall_v("DetentionDivider", 6400, -4000, -2800, doors=(-3600,), material_name=mat)
    builder.security_detention_props(6000, -3500, -90.0)
    for index, x in enumerate(range(-6800, 6801, 1600)):
        builder.static("LDV2_M03_SpineTechPlinth_{:02d}".format(index), "platform", (x, 0, 0), 0.0, (0.55, 0.55, 0.55), "tech", "Theme/PublicSpine")
    for index, x in enumerate(range(-6000, 6001, 2000)):
        builder.static("LDV2_M03_GlassLaneDisplay_{:02d}".format(index), "glass_window", (x, 3400, 0), 0.0, (1.0, 1.0, 1.0), None, "Theme/GlassLane")
        builder.static("LDV2_M03_BackstageShelf_{:02d}".format(index), "shelf", (x, -3500, 0), 180.0, (0.9, 0.9, 0.9), None, "Theme/Backstage")


for code, config in MAPS.items():
    builder = LevelBuilder(code, config)
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
    builder.save()
