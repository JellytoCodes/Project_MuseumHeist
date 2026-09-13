"""Apply the approved movement amendments in Editor review copies.

Keep artwork geometry and its presentation together when exchanging the two
Case identities. All actor/asset mutations run through Unreal Editor APIs.
"""
import copy
import json
import re
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PLANS = json.loads((ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json").read_text(encoding="utf-8"))["maps"]


def apply():
    base = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/build_museum_levels_v2.py"))
    gallery = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/refine_museum_galleries.py"))
    paths = []
    for plan in PLANS:
        code = plan["id"]
        config = copy.deepcopy(base["MAPS"][code])
        config["case_artifacts"] = plan["case_artifacts"]
        review = "/Game/Maps/Review/" + config["path"].rsplit("/", 1)[-1] + "_LayoutReview"
        world = unreal.EditorLoadingAndSavingUtils.load_map(config["path"])
        if not world or not unreal.EditorLoadingAndSavingUtils.save_map(world, review):
            raise RuntimeError("Review copy failed: " + code)
        config["path"] = review
        builder = base["LevelBuilder"](code, config)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        target_slot = next(p for p in plan["paintings"] if p.get("case_key") == "Target")
        target = builder.by_label["LDV2_" + code + "_Painting_Target"]
        target_xy = unreal.Vector(target_slot["xy"][0]*100, target_slot["xy"][1]*100, 0)
        if (target.get_actor_location()-target_xy).length() > 20:
            replacement = min((a for a in builder.actors if isinstance(a, unreal.HeistPaintingDisplayCaseActor)),
                              key=lambda a: (a.get_actor_location()-target_xy).length())
            if (replacement.get_actor_location()-target_xy).length() > 20:
                raise RuntimeError("Target destination has no existing painting: " + code)
            other_key = replacement.get_actor_label().split("_Painting_")[-1]
            exchange = {"Target": other_key, other_key: "Target"}
            pattern = re.compile(r"^(LDV2_" + code + r"_(?:Painting_|ExhibitLight_|Gallery_Secure_|Gallery_Print_))(Target|" + other_key + r")(?=_|$)")
            # Snapshot first: changes cannot cascade through another lookup.
            changes = [(a, pattern.sub(lambda m: m[1]+exchange[m[2]], a.get_actor_label())) for a in builder.actors]
            for actor, label in changes:
                actor.set_actor_label(label)
                tags = [str(t) for t in actor.get_editor_property("tags")]
                actor.set_editor_property("tags", [unreal.Name("MuseumHangingGroup_" + exchange[t.removeprefix("MuseumHangingGroup_")])
                    if t.removeprefix("MuseumHangingGroup_") in exchange and t.startswith("MuseumHangingGroup_") else unreal.Name(t) for t in tags])
            target.set_editor_property("display_case_id", unreal.Name(builder.case_id(other_key)))
            replacement.set_editor_property("display_case_id", unreal.Name(builder.case_id("Target")))
        builder.by_label = {a.get_actor_label(): a for a in builder.actors}
        for p in plan["paintings"]:
            if not p["active"]:
                continue
            actor = builder.by_label["LDV2_"+code+"_Painting_"+p["case_key"]]
            actor.set_editor_property("target_artifact_id", unreal.Name(builder.case_artifact_id(p["case_key"])))

        def box(suffix, bounds, bottom, height, material, folder, collision="BlockAll"):
            x0, y0, x1, y1 = [v*100 for v in bounds]
            return gallery["box"](builder, suffix, ((x0+x1)/2, (y0+y1)/2, bottom*100+height*50),
                                  (x1-x0, y1-y0, height*100), material, folder=folder, collision=collision)

        for p in plan["props"]:
            if p["id"].startswith("BUTTON_"):
                box(p["id"], p["bounds"], 0, p["height"], "oak" if code=="M02" else "m01_wall", "Architecture/GalleryPartitions")
        if code == "M02":
            for suffix in ("Door_D08", "DoorTrim_D08", "Door_D30", "DoorTrim_D30"):
                actor = builder.by_label.get("LDV2_M02_Gallery_"+suffix)
                if actor and not base["actor_subsystem"].destroy_actor(actor):
                    raise RuntimeError("Closed opening cleanup failed: " + suffix)
            for wall in (w for w in plan["walls"] if w["id"].endswith("_CLOSED")):
                f,a,b,t = (wall[k] for k in ("fixed","start","end","thickness"))
                bounds = [a,f-t/2,b,f+t/2] if wall["axis"]=="h" else [f-t/2,a,f+t/2,b]
                actor = box(wall["id"], bounds, 0, wall["height"], "m01_wall", "Architecture/Walls")
                builder.add_tags(actor, "MuseumPlanWall_"+wall["id"])
                box(wall["id"]+"_Skirt", bounds, .02, .16, "oak", "Architecture/Trim", "NoCollision")
                box(wall["id"]+"_Cornice", bounds, wall["height"]-.18, .12, "oak", "Architecture/Trim", "NoCollision")
        for i, l in enumerate(plan["lasers"], 1):
            button = builder.by_label["LDV2_{}_LaserButton_{:02}".format(code,i)]
            base["set_transform"](button, (l["button"][0]*100,l["button"][1]*100,0))
            barrier = builder.by_label["LDV2_{}_Laser_{:02}".format(code,i)]
            barrier.set_editor_property("protected_painting_case", builder.by_label["LDV2_"+code+"_Painting_"+plan["case_mapping"][l["cases"][0]]])
            button.set_editor_property("linked_laser_barrier", barrier)
        runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/build_approved_museum_layout.py"))["light_button_pockets"](builder, plan)
        for key, xy in plan.get("loot_spawn_overrides", {}).items():
            for label in ("LDV2_"+code+"_LootSpawn_"+key, "LDV2_"+code+"_Gallery_LootPedestal_"+key):
                actor = builder.by_label[label]
                old = actor.get_actor_location()
                center = actor.get_actor_bounds(False)[0] if "LootPedestal_" in label else old
                actor.set_actor_location(old+unreal.Vector(xy[0]*100-center.x,xy[1]*100-center.y,0),False,False)
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, review):
            raise RuntimeError("Review amendment save failed: " + code)
        unreal.log_warning("MH_MOVEMENT_AMENDMENT_SAVED=" + code)
        paths.append(review)
    return paths


if __name__ == "__main__":
    runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/build_approved_museum_layout.py"))["rebuild_saved_navigation"](apply())
