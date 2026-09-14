"""Author bounded night lights in the existing approved maps through Editor.

Uses hanging groups, never interaction eligibility, to light the collection.
Does not rebuild architecture, navigation, actors or gameplay assignments.
"""
import hashlib
import json
import math
import runpy
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PROFILES = {
    "M01": dict(room_cd=220.0, art_cd=220.0, fill_cd=200.0, control_cd=30.0, color=(255, 214, 174), service=(160, 192, 235)),
    "M02": dict(room_cd=44.0, art_cd=70.0, fill_cd=60.0, control_cd=12.0, color=(255, 210, 162), service=(145, 184, 235)),
    "M03": dict(room_cd=72.0, art_cd=100.0, fill_cd=90.0, control_cd=18.0, color=(200, 220, 255), service=(160, 198, 230)),
}


def non_lighting_snapshot():
    rows = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        cls = actor.get_class().get_name()
        if actor.get_component_by_class(unreal.LightComponent) or cls in ("SkyLight", "PostProcessVolume", "ExponentialHeightFog", "SkyAtmosphere", "VolumetricCloud"):
            continue
        location, rotation, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
        rows.append((actor.get_actor_label(), cls, [round(v, 5) for v in (location.x, location.y, location.z, rotation.pitch, rotation.yaw, rotation.roll, scale.x, scale.y, scale.z)]))
    return dict(count=len(rows), sha256=hashlib.sha256(json.dumps(sorted(rows)).encode()).hexdigest())


def apply_lighting(plan):
    code = plan["id"]
    profile = PROFILES[code]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    prefix = "LDV2_" + code + "_"
    by_label = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    room_positions = {}
    # Remove only the superseded generated light actors, preserving all fixtures.
    for label, actor in list(by_label.items()):
        if any(label.startswith(prefix + token) for token in ("RoomLight_", "ExhibitLight_", "GalleryLight_", "NightArt_", "NightFill_")):
            if not isinstance(actor, (unreal.PointLight, unreal.SpotLight)):
                raise RuntimeError("Unexpected non-light in lighting namespace: " + label)
            if label.startswith(prefix + "RoomLight_"):
                room_positions[label] = actor.get_actor_location()
            if not actors.destroy_actor(actor):
                raise RuntimeError("Light replacement failed: " + label)

    def spot(label, location, target, intensity, radius, outer, color, shadows, folder):
        delta = unreal.Vector(target[0]-location[0], target[1]-location[1], target[2]-location[2])
        yaw = math.degrees(math.atan2(delta.y, delta.x))
        pitch = math.degrees(math.atan2(delta.z, math.hypot(delta.x, delta.y)))
        actor = actors.spawn_actor_from_class(unreal.SpotLight, unreal.Vector(*location), unreal.Rotator(pitch=pitch, yaw=yaw))
        if not actor:
            raise RuntimeError("SpotLight spawn failed: " + label)
        actor.set_actor_label(label)
        actor.set_folder_path("Lighting/Night/" + folder)
        component = actor.get_component_by_class(unreal.SpotLightComponent)
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        component.set_editor_property("intensity", intensity)
        component.set_editor_property("attenuation_radius", radius)
        component.set_editor_property("inner_cone_angle", outer * 0.48)
        component.set_editor_property("outer_cone_angle", outer)
        component.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        component.set_editor_property("cast_shadows", shadows)
        component.set_editor_property("indirect_lighting_intensity", 0.65)
        component.set_editor_property("specular_scale", 0.25)
        component.set_editor_property("source_radius", 8.0)
        return actor

    for label, pos in sorted(room_positions.items()):
        room_id, index = label[len(prefix + "RoomLight_"):].rsplit("_", 1)
        room = plan["rooms"][room_id]
        # Galleries retain one night light; long service corridors keep every other fixture.
        enabled = int(index) == 0 or (room["kind"] == "service" and int(index) % 2 == 0)
        intensity = profile["room_cd"] if enabled else 0.0
        color = profile["color"] if room["kind"] == "gallery" else profile["service"]
        if room["kind"] == "security":
            intensity *= 1.3
        spot(label, (pos.x, pos.y, pos.z), (pos.x, pos.y, 0), intensity, min(800.0, pos.z + 350), 58.0, color, enabled, "Rooms")
        # Very low cool bounce fill preserves floor/door silhouettes between lit pools.
        # It does not allocate omnidirectional shadow maps or identify stealable art.
        fill = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(pos.x, pos.y, 230), unreal.Rotator())
        fill.set_actor_label(label.replace("RoomLight_", "NightFill_"))
        fill.set_folder_path("Lighting/Night/BounceFill")
        component = fill.get_component_by_class(unreal.PointLightComponent)
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        component.set_editor_property("intensity", profile["fill_cd"])
        component.set_editor_property("attenuation_radius", 1000.0)
        component.set_editor_property("light_color", unreal.Color(r=155, g=185, b=235, a=255))
        component.set_editor_property("cast_shadows", False)
        component.set_editor_property("specular_scale", 0.0)
        component.set_editor_property("indirect_lighting_intensity", 0.5)

    for group in plan["groups"]:
        paintings = [p for p in plan["paintings"] if p["group"] == group["id"]]
        normal = group["normal"]
        # Space accents across the entire hanging group, including decorative-only walls.
        axis = 1 if normal[0] else 0
        lo, hi = group["span"]
        count = max(1, math.ceil((hi-lo)/3.5))
        wall_z = plan["walls"][0]["height"] * 100 - 35
        for index in range(count):
            along = lo + (hi-lo) * (index + 0.5) / count
            target = [sum(p["xy"][0] for p in paintings)/len(paintings)*100,
                      sum(p["xy"][1] for p in paintings)/len(paintings)*100, 180.0]
            target[axis] = along * 100
            location = (target[0] + normal[0]*240, target[1] + normal[1]*240, wall_z)
            distance = math.sqrt(sum((a-b)**2 for a,b in zip(location,target)))
            # Keep the art inside the useful attenuation range. Limit spill to a few metres,
            # rather than the former 18m spheres; room and moon lights retain occlusion.
            variation = (1.0, 0.7, 0.85)[int(group["id"][1:]) % 3]
            spot(prefix + "NightArt_{}_{}".format(group["id"], index), location, target,
                 profile["art_cd"]*variation, distance*1.8, 56.0, profile["color"], False, "Collection")

    controls = 0
    for label, actor in by_label.items():
        if label.startswith(prefix + "ButtonLight_"):
            component = actor.get_component_by_class(unreal.PointLightComponent)
            component.set_editor_property("intensity", profile["control_cd"])
            component.set_editor_property("attenuation_radius", 220.0)
            component.set_editor_property("cast_shadows", True)
            controls += 1
    return dict(room_lights=len(room_positions), control_lights=controls)


def main():
    base = runpy.run_path(str(Path(__file__).with_name("build_museum_levels_v2.py")))
    plans = json.loads((ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json").read_text(encoding="utf-8"))["maps"]
    report = []
    for plan in plans:
        if plan["id"] not in base["selected_level_codes"]:
            continue
        builder = base["LevelBuilder"](plan["id"], base["MAPS"][plan["id"]])
        before = non_lighting_snapshot()
        builder.configure_night_environment()
        result = apply_lighting(plan)
        after = non_lighting_snapshot()
        if before != after:
            raise RuntimeError("Non-lighting actor transforms changed: " + plan["id"])
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        path = base["MAPS"][plan["id"]]["path"]
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
            raise RuntimeError("Night lighting save failed: " + path)
        unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
        report.append(dict(map=plan["id"], before=before, after=after, **result))
    out = ROOT / "Saved/Screenshots/NightLighting/apply.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(dict(status="PASS", maps=report), indent=2), encoding="utf-8")
    unreal.log_warning("MH_NIGHT_APPLY_DONE=PASS")


if __name__ == "__main__":
    main()
