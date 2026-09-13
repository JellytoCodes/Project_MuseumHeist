"""Author the approved measured floor plans through Unreal Editor only.

Build review copies first. Promotion is an explicit second Editor operation after
saved geometry, hanging and runtime navigation checks. Case identities are retained.
"""
import copy
import json
import math
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PLAN_PATH = ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json"
PLANS = json.loads(PLAN_PATH.read_text(encoding="utf-8"))["maps"]


def light_button_pockets(builder, plan):
    """Keep the sheltered controls visible beneath the partition shadow."""
    for index, laser in enumerate(plan["lasers"], 1):
        spec = laser.get("pocket_light")
        if spec:
            builder.point_light("LDV2_{}_ButtonLight_{:02}".format(plan["id"], index),
                (laser["button"][0]*100, laser["button"][1]*100, spec["height_m"]*100),
                spec["color"], spec["intensity"], spec["radius_m"]*100, "Lighting/Controls")


def build():
    base = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/build_museum_levels_v2.py"))
    gallery = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/refine_museum_galleries.py"))
    actors = base["actor_subsystem"]
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    review_paths=[]
    for plan in PLANS:
        code = plan["id"]
        if code not in base["selected_level_codes"]:
            continue
        config = copy.deepcopy(base["MAPS"][code])
        config["case_artifacts"] = plan.get("case_artifacts", {})
        canonical = config["path"]
        review = "/Game/Maps/Review/" + canonical.rsplit("/", 1)[-1] + "_LayoutReview"
        world = unreal.EditorLoadingAndSavingUtils.load_map(canonical)
        if not world or not unreal.EditorLoadingAndSavingUtils.save_map(world, review):
            raise RuntimeError("Review copy failed: " + code)
        config["path"] = review
        builder = base["LevelBuilder"](code, config)
        # Review copies contain no old generated architecture or props. Preserve
        # only gameplay identity, world settings and environmental singletons.
        remove_classes = {"StaticMeshActor", "PointLight", "SpotLight", "RectLight", "TextRenderActor",
                          "TargetPoint", "HeistGuardWaypoint", "BP_LootSpawnPoint_C", "BP_Guard_C",
                          "BP_SecurityCamera_C", "BP_LaserBarrier_C", "BP_SecurityHoldButton_C"}
        for actor in list(builder.actors):
            if actor.get_class().get_name() in remove_classes:
                label = actor.get_actor_label()
                if not actors.destroy_actor(actor):
                    raise RuntimeError("Review cleanup failed: " + label)
                builder.actors.remove(actor)
                builder.by_label.pop(label, None)

        def box(label, bounds, bottom, height, material, folder, collision="BlockAll"):
            x0, y0, x1, y1 = [v * 100 for v in bounds]
            return gallery["box"](builder, label, ((x0+x1)/2, (y0+y1)/2, bottom*100+height*50),
                                  (x1-x0, y1-y0, height*100), material, folder=folder, collision=collision)

        def fit(label, mesh, bounds, bottom, height, folder, collision="NoCollision"):
            mesh_bounds = base["assets"][mesh].get_bounding_box()
            dimensions = (mesh_bounds.max.x-mesh_bounds.min.x, mesh_bounds.max.y-mesh_bounds.min.y,
                          mesh_bounds.max.z-mesh_bounds.min.z)
            x0, y0, x1, y1 = [v*100 for v in bounds]
            size = (x1-x0, y1-y0, height*100)
            scale = [a/b for a, b in zip(size, dimensions)]
            center = ((x0+x1)/2, (y0+y1)/2, bottom*100+height*50)
            pivot = ((mesh_bounds.min.x+mesh_bounds.max.x)/2, (mesh_bounds.min.y+mesh_bounds.max.y)/2,
                     (mesh_bounds.min.z+mesh_bounds.max.z)/2)
            return builder.static("LDV2_"+code+"_"+label, mesh,
                                  [c-p*s for c,p,s in zip(center,pivot,scale)], scale=scale,
                                  folder=folder, collision_profile=collision)

        floor = box("Floor_Approved", plan["bounds"], -.2, .2, code.lower()+"_floor", "Architecture/Floor")
        builder.relabel(floor, "LDV2_"+code+"_Floor_Approved")
        # Fixed four-metre visual modules preserve texture scale. A single huge
        # scaled cube stretches parquet planks across entire rooms.
        x0,y0,x1,y1=plan["bounds"]
        for ix,x in enumerate(range(int(x0),int(x1),4)):
            for iy,y in enumerate(range(int(y0),int(y1),4)):
                material=code.lower()+"_floor"
                if code=="M02":
                    gx0,gy0,gx1,gy1=plan["rooms"]["G"]["bounds"]
                    if gx0 <= x+2 <= gx1 and gy0 <= y+2 <= gy1:material="m03_floor"
                box("FloorTile_{}_{}".format(ix,iy),[x,y,min(x+4,x1),min(y+4,y1)],0,.02,
                    material,"Architecture/FloorFinish","NoCollision")
        wall_height = plan["walls"][0]["height"]
        trim = {"M01":"gold", "M02":"oak", "M03":"nickel"}[code]
        # Neutral gallery plaster keeps wall art readable without stretching a
        # brick/concrete texture over variable-length measured wall segments.
        wall_material = "m01_wall"
        for wall in plan["walls"]:
            f, a, b, t = wall["fixed"], wall["start"], wall["end"], wall["thickness"]
            bounds = [a,f-t/2,b,f+t/2] if wall["axis"] == "h" else [f-t/2,a,f+t/2,b]
            actor = box(wall["id"], bounds, 0, wall["height"], wall_material, "Architecture/Walls")
            builder.add_tags(actor, "MuseumPlanWall_"+wall["id"])
            # Trim shares the wall footprint: it cannot reduce an authored opening.
            box(wall["id"]+"_Skirt", bounds, .02, .16, trim, "Architecture/Trim", "NoCollision")
            box(wall["id"]+"_Cornice", bounds, wall_height-.18, .12, trim, "Architecture/Trim", "NoCollision")
        for door in plan["doors"]:
            x,y = door["xy"]; width=door["width"]; depth=.4
            bounds = [x-width/2,y-depth/2,x+width/2,y+depth/2] if door["axis"]=="h" else [x-depth/2,y-width/2,x+depth/2,y+width/2]
            # Lintel bounds encode the actual opening width for the Floor Plan exporter.
            lintel = box("Door_"+door["id"], bounds, 3.0, wall_height-3.0, wall_material, "Architecture/ApprovedDoors")
            builder.add_tags(lintel, "MuseumPlanDoor_"+door["id"])
            box("DoorTrim_"+door["id"], bounds, 2.94, .06, trim, "Architecture/Trim", "NoCollision")
        for room in plan["rooms"].values():
            x0,y0,x1,y1 = room["bounds"]
            center=((x0+x1)*50,(y0+y1)*50)
            if room["id"] != "G":
                glass = code=="M03" and room["id"].startswith("N") or code=="M01" and room["id"]=="H"
                box("Ceiling_"+room["id"],room["bounds"],wall_height,.12,"glass" if glass else "m01_wall","Architecture/GalleryCeiling","NoCollision")
            else:
                # Open courtyard: no ceiling luminaires floating in the sky.
                continue
            # Two fixtures in long rooms keep corridors legible at the saved night exposure.
            count = max(1, math.ceil((x1-x0)/16))
            for index in range(count):
                x = (x0+(x1-x0)*(index+.5)/count)*100
                colour = (255,224,183) if code=="M01" else (255,210,160) if code=="M02" else (218,233,255)
                light = builder.point_light("LDV2_{}_RoomLight_{}_{:02}".format(code,room["id"],index),
                    (x,center[1],wall_height*100-60),colour,1800 if code=="M01" else 700,1800,"Lighting/Rooms")
                light.get_component_by_class(unreal.PointLightComponent).set_editor_property("cast_shadows",True)
                fit("CeilingFixture_{}_{:02}".format(room["id"],index),"ceiling_lamp",
                    [x/100-.35,center[1]/100-.35,x/100+.35,center[1]/100+.35],wall_height-.24,.24,"Lighting/Fixtures")
        for prop in plan["props"]:
            kind=prop["kind"]; bounds=prop["bounds"]; height=prop["height"]
            material = "moss" if kind=="planting" else "water" if kind=="pool" else trim
            if kind in ("wall","screen"):
                material="oak" if prop["id"].startswith("SCREEN_") or kind=="screen" else wall_material
            folder="Architecture/GalleryPartitions" if kind in ("wall","screen") else "Theme/ApprovedProps"
            box(prop["id"],bounds,0,height,material,folder)
            if kind=="bench":
                # Seat detail stays entirely inside the approved furniture footprint.
                fit(prop["id"]+"_Seat","m01_showcase_long_bench" if code=="M01" else "couch",bounds,0,height,"Theme/ApprovedProps")
            if kind=="planting":
                x0,y0,x1,y1=bounds
                count=max(1,int(max(x1-x0,y1-y0)/2))
                for i in range(count):
                    t=(i+.5)/count
                    x=x0+(x1-x0)*t if x1-x0>y1-y0 else (x0+x1)/2
                    y=(y0+y1)/2 if x1-x0>y1-y0 else y0+(y1-y0)*t
                    fit(prop["id"]+"_Plant_"+str(i),"bush",[x-.6,y-.6,x+.6,y+.6],height,.45,"Theme/Garden")
            if prop["id"]=="ROTUNDA_PLINTH":
                fit("RotundaStatue","statue",[-.8,-.8,.8,.8],height,2.3,"Theme/Landmark")

        config["cases"]=[]
        for p in plan["paintings"]:
            if p["active"]:
                facing=math.degrees(math.atan2(p["normal"][1],p["normal"][0]))
                config["cases"].append((p["case_key"],p["xy"][0]*100,p["xy"][1]*100,facing+180))
        cases=builder.configure_cases()
        gallery["refine_exhibits"](builder,cases,gallery["mounting_walls"](builder))

        starts=sorted((a for a in builder.actors if isinstance(a,unreal.PlayerStart)),key=lambda a:a.get_actor_label())
        for actor,spec in zip(starts,plan["player_starts"]):
            half=actor.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
            base["set_transform"](actor,(*[v*100 for v in spec["xy"]],half+3))
            builder.folder(actor,"Gameplay/Entry")
        vent=next(a for a in builder.actors if a.get_class().get_name()=="BP_Vent_C")
        base["set_transform"](vent,(*[v*100 for v in plan["vent"]],50),0)
        builder.folder(vent,"Gameplay/Exit")
        for spec in plan["detention_starts"]:
            builder.target_point("LDV2_"+code+"_Detention_"+spec["id"],(*[v*100 for v in spec["xy"]],99),
                                 folder="Gameplay/Detention",tags=("HeistDetentionSpawn",))
        for spec in plan["evidence_slots"]:
            builder.target_point("LDV2_"+code+"_Evidence_"+spec["id"],[v*100 for v in spec["xyz"]],
                                 folder="Gameplay/Evidence",tags=("HeistEvidenceTableAnchor","HeistEvidenceSlot"))
        ex,ey=plan["evidence"]
        table=box("EvidenceTable",[ex-1.75,ey-1.75,ex+1.75,ey+1.75],.75,.15,"steel","SecurityDetention")
        builder.add_tags(table,"HeistEvidenceTableVisual")
        for dx in (-1.5,1.5):
            for dy in (-1.5,1.5):
                box("EvidenceLeg_{}_{}".format(dx,dy),[ex+dx-.06,ey+dy-.06,ex+dx+.06,ey+dy+.06],0,.75,"steel","SecurityDetention")

        # Preserve the existing 10 exhibition + 2 vault loose-loot anchors. Place
        # each deliberately in a room corner with a small supporting pedestal.
        rooms=list(dict.fromkeys(p["room"] for p in plan["paintings"] if p["active"]))
        config["loot_spawn_points"]=[]
        for i in range(12):
            room=plan["rooms"][rooms[i%len(rooms)]];x0,y0,x1,y1=room["bounds"]
            x,y=x0+2,y0+2
            if i>=10:x,y=x1-2,y0+2
            category="ExhibitionRoom" if i<10 else "VaultFixed"
            key="Exhibition_{:02}".format(i+1) if i<10 else "Vault_{:02}".format(i-9)
            x,y=plan.get("loot_spawn_overrides",{}).get(key,(x,y))
            config["loot_spawn_points"].append((key,category,x*100,y*100,100,0))
            box("LootPedestal_"+key,[x-.45,y-.45,x+.45,y+.45],0,.8,"m01_wall","Theme/LootDisplay")
        builder.configure_loot_spawn_points()
        # Keep a closed loop's first point; remove only its repeated endpoint.
        config["guard_routes"]=[]
        occupied_starts=[]
        for guard in plan["guards"]:
            points=guard["polyline"]
            if points[0]==points[-1]:points=points[:-1]
            # Shared loops may begin at the same graph node. Rotate the loop
            # without changing its shape so authored capsules never overlap.
            for offset in range(len(points)):
                if all(math.dist(points[offset],p)>2 for p in occupied_starts):
                    points=points[offset:]+points[:offset]
                    occupied_starts.append(points[0])
                    break
            config["guard_routes"].append([(p[0]*100,p[1]*100) for p in points])
        builder.configure_guards()
        for actor in builder.actors:
            if isinstance(actor,unreal.HeistGuardCharacter):
                builder.move_actor_z(actor,actor.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()+3)
        config["cameras"]=[(*[v*100 for v in c["xy"]],c["z"]*100,c["yaw"]) for c in plan["cameras"]]
        builder.configure_cameras()
        for i,c in enumerate(plan["cameras"]):
            camera=builder.by_label["LDV2_{}_CCTV_{:02}".format(code,i+1)]
            camera.set_actor_rotation(unreal.Rotator(pitch=c["pitch"],yaw=c["yaw"],roll=0),False)
            camera.set_editor_property("detection_range",c["range"]*100)
            camera.set_editor_property("detection_half_angle_degrees",c["angle"]/2)
        config["lasers"]=[]
        for l in plan["lasers"]:
            door=next(d for d in plan["doors"] if d["id"]==l["door"])
            yaw=0 if door["axis"]=="v" else 90
            config["lasers"].append((plan["case_mapping"][l["cases"][0]],(*[v*100 for v in l["xy"]],0,yaw),
                                      (*[v*100 for v in l["button"]],0,0)))
        builder.configure_lasers(cases)
        light_button_pockets(builder, plan)
        for i,l in enumerate(plan["lasers"]):
            door=next(d for d in plan["doors"] if d["id"]==l["door"])
            builder.by_label["LDV2_{}_Laser_{:02}".format(code,i+1)].set_actor_scale3d(unreal.Vector(1,door["width"]/3,1))
        nav=next(a for a in builder.actors if a.get_class().get_name()=="NavMeshBoundsVolume")
        nav.set_actor_scale3d(unreal.Vector(1,1,1))
        origin,extent=nav.get_actor_bounds(False)
        x0,y0,x1,y1=plan["bounds"]
        nav.set_actor_scale3d(unreal.Vector((x1-x0)*50/extent.x,(y1-y0)*50/extent.y,500/extent.z))
        nav.set_actor_location(unreal.Vector(0,0,250),False,False)
        builder.configure_night_environment()
        if not levels.save_current_level():
            raise RuntimeError("Review save failed: "+code)
        unreal.log_warning("MH_APPROVED_REVIEW_SAVED="+code+" path="+review)
        review_paths.append(review)
    return review_paths


def rebuild_saved_navigation(map_paths):
    """Initialize loaded bounds, rebuild Recast, then save queryable navigation.

    Loading and building in the same Python callback can precede bounds registration.
    Yield Editor ticks before the build and require four valid spawn projections.
    """
    import time
    import traceback
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    state={"index":0,"phase":"load","started":time.monotonic(),"callback":None,"in_tick":False}
    report=[]

    def finish(error=None):
        out=ROOT/"Saved/Automation/MuseumLayout/navigation-build.json"
        out.parent.mkdir(parents=True,exist_ok=True)
        out.write_text(json.dumps(dict(status="FAIL" if error else "PASS",error=error,maps=report),indent=2),encoding="utf-8")
        unreal.log_warning("MH_LAYOUT_NAVIGATION_SAVED="+str(out)+" error="+str(error))
        unreal.unregister_slate_post_tick_callback(state["callback"])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def tick(_):
        # Loading/building/saving can pump Slate and invoke this callback again.
        if state["in_tick"]:return
        state["in_tick"]=True
        try:
            if state["phase"]=="load":
                if state["index"]==len(map_paths):finish();return
                path=map_paths[state["index"]]
                if not levels.load_level(path):raise RuntimeError("Load failed: "+path)
                state["phase"]="initialize";state["started"]=time.monotonic()
                return
            world=editor.get_editor_world()
            elapsed=time.monotonic()-state["started"]
            if elapsed>120:raise RuntimeError("Navigation rebuild timed out")
            if state["phase"]=="initialize":
                if elapsed<5:return
                navigation=unreal.NavigationSystemV1.get_navigation_system(world)
                volumes=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.NavMeshBoundsVolume)]
                if not navigation or len(volumes)!=1:raise RuntimeError("Missing navigation system or bounds")
                navigation.on_navigation_bounds_updated(volumes[0])
                unreal.SystemLibrary.execute_console_command(world,"RebuildNavigation")
                state["phase"]="wait";state["started"]=time.monotonic()
                return
            if elapsed<8 or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):return
            starts=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
            projected=[unreal.NavigationSystemV1.project_point_to_navigation(world,a.get_actor_location(),None,None,unreal.Vector(100,100,200)) for a in starts]
            if len(starts)!=4 or not all(projected):
                raise RuntimeError("Built navigation is not queryable at all four starts")
            if not unreal.EditorLoadingAndSavingUtils.save_map(world,map_paths[state["index"]]):
                raise RuntimeError("Navigation save failed")
            report.append(dict(path=map_paths[state["index"]],projected_starts=len(projected),wait_seconds=elapsed))
            state["index"]+=1;state["phase"]="load"
        except Exception:
            finish(traceback.format_exc())
        finally:
            state["in_tick"]=False
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    state["callback"]=unreal.register_slate_post_tick_callback(tick)


if __name__=="__main__":
    rebuild_saved_navigation(build())
