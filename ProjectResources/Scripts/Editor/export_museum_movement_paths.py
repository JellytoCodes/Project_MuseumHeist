"""Read-only saved-map route queries in an initialized SIE world.

Exports native Recast paths, swept player/guard capsules, authored security
transforms and live shell tuning. No map saves, nav rebuilds or player input.
Run with UnrealEditor -ExecutePythonScript=<this file> -RenderOffscreen.
"""
import hashlib
import itertools
import json
import math
from pathlib import Path
import runpy
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json"
PLANS = json.loads(SOURCE.read_text(encoding="utf-8"))["maps"]
MAPS = ("M01_ClassicalPrototype", "M02_MoonlitPrototype", "M03_GlasshousePrototype")
_, switches, _ = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
SIGHTLINES = "museummovementsightlines" in {str(v).casefold() for v in switches}
OUT = ROOT / "Saved/Automation/MuseumMovement" / ("sightlines.json" if SIGHTLINES else "native-paths.json")
OUT.parent.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
intersects_beam = runpy.run_path(str(ROOT / "ProjectResources/Scripts/Editor/verify_gallery_refinement.py"))["intersects_beam"]
state = dict(index=0, phase="load", phase_at=time.monotonic(), started=time.monotonic(), in_tick=False, callback=None)


def hashes():
    return {name: hashlib.sha256((ROOT / "Content/Maps" / (name + ".umap")).read_bytes()).hexdigest() for name in MAPS}


report = dict(status="RUNNING", map_sha256_before=hashes(), layout_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
              maps=[], user_pie="NOT_TESTED", packages_saved=False, nav_rebuilt=False,
              scope="Native navigation and static geometry sweeps; no character locomotion or detection simulation")


def xyz(v):
    return [round(v.x / 100, 5), round(v.y / 100, 5), round(v.z / 100, 5)]


def prop(obj, key, fallback=None):
    try:
        value = obj.get_editor_property(key)
        return value
    except Exception:
        return fallback


def change(phase):
    state.update(phase=phase, phase_at=time.monotonic())


def flush():
    OUT.write_text(json.dumps(report, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")


def finish(error=None):
    state["phase"] = "done"
    report.update(status="FAIL" if error else "PASS", error=error, map_sha256_after=hashes(),
                  elapsed_seconds=round(time.monotonic() - state["started"], 2),
                  dirty_map_packages=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
    report["map_files_unchanged"] = report["map_sha256_before"] == report["map_sha256_after"]
    if not report["map_files_unchanged"] or report["dirty_map_packages"]:
        report["status"] = "FAIL"
    flush()
    unreal.log_warning("MH_MOVEMENT_DONE=" + json.dumps({k: report[k] for k in ("status", "error", "elapsed_seconds", "map_files_unchanged")}))
    unreal.unregister_slate_post_tick_callback(state["callback"])
    if editor.get_game_world():
        levels.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def begin_queries(world):
    plan = PLANS[state["index"]]
    authored = state["authored"]
    mode = unreal.GameplayStatics.get_game_mode(world)
    player = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
    capsule = player.get_component_by_class(unreal.CapsuleComponent)
    state.update(world=world, ignored=list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)),
                 radius=capsule.get_scaled_capsule_radius(), half=capsule.get_scaled_capsule_half_height(),
                 profile=capsule.get_collision_profile_name(), sweep_cache={})
    row = dict(id=plan["id"], nodes={}, paths={}, guards=[], cameras=[], lasers=[], failures=[],
               capsule_cm=[state["radius"], state["half"]], game_seconds=unreal.GameplayStatics.get_time_seconds(world),
               movement={k: prop(player, k) for k in ("walk_move_speed", "sprint_move_speed", "walk_weight_speed_penalty",
                   "sprint_weight_speed_penalty", "minimum_walk_move_speed", "minimum_sprint_move_speed")})
    action = player.get_component_by_class(unreal.HeistActionComponent)
    row["observation_seconds"] = prop(action, "observation_cast_duration_seconds")
    row["runtime_lasers"] = [dict(actor=a.get_actor_label(), enabled=a.is_barrier_enabled(), beam_active=a.is_beam_active(),
        protected_artifact=str(prop(a.get_protected_painting_case(), "target_artifact_id")))
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistLaserBarrierActor)]
    report["maps"].append(row)
    state["row"] = row
    state["nav_nodes"] = {}

    def add_node(key, point, kind, **extra):
        projected = unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None, unreal.Vector(60, 60, 200))
        row["nodes"][key] = dict(kind=kind, xyz=xyz(point), nav=xyz(projected) if projected else None, **extra)
        if projected:
            state["nav_nodes"][key] = projected
        else:
            row["failures"].append("Nav projection: " + key)

    starts = sorted([a for a in authored if isinstance(a, unreal.PlayerStart)], key=lambda a: a.get_actor_label())
    for i, a in enumerate(starts):
        add_node("SP" + str(i + 1), a.get_actor_location(), "start", actor=a.get_actor_label())
    for a in authored:
        name = a.get_actor_label()
        if isinstance(a, unreal.HeistPaintingDisplayCaseActor):
            piece = next(p for p in plan["paintings"] if p.get("case_key") == name.split("_Painting_")[-1])
            comp = next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "OriginalVisualComponent")
            point = a.get_actor_location() + comp.get_up_vector() * 120
            point.z = state["half"] + 3
            add_node(piece["id"], point, "painting", room=piece["room"], case_key=piece["case_key"], actor=name,
                     artifact_id=str(prop(a, "target_artifact_id", "unknown")))
        elif a.get_class().get_name() == "BP_Vent_C":
            add_node("Vent", a.get_actor_location(), "vent", actor=name)
        elif a.get_class().get_name() == "BP_SecurityHoldButton_C":
            key = "B" + name.rsplit("_", 1)[-1]
            add_node(key, a.get_actor_location(), "button", actor=name)
    add_node("Evidence", unreal.Vector(plan["evidence"][0]*100, (plan["evidence"][1]-2.3)*100, state["half"]+3), "evidence")
    ds = next(a for a in authored if a.actor_has_tag("HeistDetentionSpawn"))
    add_node("Detention", ds.get_actor_location(), "detention", actor=ds.get_actor_label())
    for d in plan["doors"]:
        add_node(d["id"], unreal.Vector(d["xy"][0]*100, d["xy"][1]*100, state["half"]+3), "door", rooms=d["rooms"], width_m=d["width"])
    barriers = sorted([a for a in authored if a.get_class().get_name() == "BP_LaserBarrier_C"], key=lambda a: a.get_actor_label())
    state["barriers"] = []
    for i, a in enumerate(barriers):
        box = a.get_component_by_class(unreal.BoxComponent)
        key = "L%02d" % (i+1)
        state["barriers"].append((key, box))
        row["lasers"].append(dict(id=key, origin=xyz(box.get_world_location()), extent=xyz(box.get_scaled_box_extent()),
                                  forward=xyz(box.get_forward_vector()*100), right=xyz(box.get_right_vector()*100)))
    for i, a in enumerate(sorted([a for a in authored if a.get_class().get_name() == "BP_SecurityCamera_C"], key=lambda a: a.get_actor_label())):
        sensor = prop(a, "sensor_origin_component")
        box = prop(a, "detection_volume_component")
        row["cameras"].append(dict(id="C%02d"%(i+1), origin=xyz(sensor.get_world_location()),
            forward=xyz(sensor.get_forward_vector()*100), up=xyz(sensor.get_up_vector()*100),
            range_m=prop(a, "detection_range")/100, half_angle=prop(a, "detection_half_angle_degrees"),
            sweep_half_angle=prop(a, "sweep_half_angle_degrees"), sweep_period=prop(a, "sweep_period_seconds"),
            volume=dict(origin=xyz(box.get_world_location()), extent=xyz(box.get_scaled_box_extent()),
                axes=[xyz(box.get_forward_vector()*100), xyz(box.get_right_vector()*100), xyz(box.get_up_vector()*100)])))
    state["jobs"] = [] if SIGHTLINES else list(itertools.combinations(state["nav_nodes"], 2))
    state["job"] = 0
    state["guard_actors"] = sorted([a for a in authored if isinstance(a, unreal.HeistGuardCharacter)], key=lambda a: a.get_actor_label())
    change("query")


def path_query(start, end, radius=None, half=None, profile=None):
    world = state["world"]
    if (start-end).length() < 1:
        return dict(complete=True, points=[xyz(start)], length_m=0, capsule_clear=True, blocking_actors=[], lasers=[])
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, end)
    if not path or not path.is_valid() or path.is_partial():
        return dict(complete=False, partial=bool(path and path.is_partial()))
    points = list(path.get_editor_property("path_points"))
    radius, half, profile = radius or state["radius"], half or state["half"], profile or state["profile"]
    blocked, crossed = set(), set()
    for a, b in zip(points, points[1:]):
        # Native nav points are on the floor. Lift capsule centers by their actual
        # shell half height; reduce radius/half by 1cm only for contact tolerance.
        ca, cb = a+unreal.Vector(0,0,half), b+unreal.Vector(0,0,half)
        key = (tuple(xyz(a)), tuple(xyz(b)), round(radius,2), round(half,2), str(profile))
        if key not in state["sweep_cache"]:
            hit = unreal.SystemLibrary.capsule_trace_single_by_profile(world, ca, cb, radius-1, half-1, profile,
                    False, state["ignored"], unreal.DrawDebugTrace.NONE, True)
            fields = hit.to_dict() if hit else {}
            blocker = fields.get("hit_actor")
            state["sweep_cache"][key] = (blocker.get_actor_label() if blocker else "unknown") if fields.get("blocking_hit") else None
        if state["sweep_cache"][key]:
            blocked.add(state["sweep_cache"][key])
        for name, box in state["barriers"]:
            if intersects_beam(a,b,box,radius):
                crossed.add(name)
    return dict(complete=True, points=[xyz(v) for v in points], length_m=round(sum((b-a).length() for a,b in zip(points,points[1:]))/100,5),
                capsule_clear=not blocked, blocking_actors=sorted(blocked), lasers=sorted(crossed))


def guard_queries():
    row = state["row"]
    for i, guard in enumerate(state["guard_actors"]):
        patrol = guard.get_component_by_class(unreal.HeistPatrolPathComponent)
        route_id = str(prop(patrol,"patrol_route_id"))
        waypoints = sorted([a for a in state["authored"] if isinstance(a,unreal.HeistGuardWaypoint) and str(prop(a,"patrol_route_id")) == route_id], key=lambda a: int(prop(a,"patrol_order")))
        cap = guard.get_component_by_class(unreal.CapsuleComponent)
        path_points, segments, blocked = [], [], []
        for a,b in zip(waypoints, waypoints[1:]+waypoints[:1]):
            ends = [unreal.NavigationSystemV1.project_point_to_navigation(state["world"],a.get_actor_location(),None,None,unreal.Vector(60,60,200)),
                    unreal.NavigationSystemV1.project_point_to_navigation(state["world"],b.get_actor_location(),None,None,unreal.Vector(60,60,200))]
            result = path_query(*ends, cap.get_scaled_capsule_radius(),cap.get_scaled_capsule_half_height(),cap.get_collision_profile_name())
            segments.append(result)
            path_points += result.get("points",[]) if not path_points else result.get("points",[])[1:]
            blocked += result.get("blocking_actors",[])
        row["guards"].append(dict(id="R%02d"%(i+1), actor=guard.get_actor_label(), route_id=route_id,
            profile=str(prop(guard,"guard_profile_id")), waypoint_wait_seconds=prop(patrol,"waypoint_wait_duration"),
            look_around_degrees=prop(patrol,"look_around_yaw_angle"), start=xyz(guard.get_actor_location()),
            points=path_points, waypoint_count=len(waypoints), length_m=round(sum(s.get("length_m",0) for s in segments),3),
            complete=all(s["complete"] for s in segments), capsule_clear=not blocked, blocking_actors=sorted(set(blocked)),
            segment_lengths=[s.get("length_m",0) for s in segments]))
    row["native_pair_count"] = len(row["paths"])
    row["unique_sweeps"] = len(state["sweep_cache"])
    row["incomplete_pairs"] = sum(not p["complete"] for p in row["paths"].values())
    row["blocked_pairs"] = sum(p["complete"] and not p["capsule_clear"] for p in row["paths"].values())
    flush()
    unreal.log_warning("MH_MOVEMENT_MAP="+json.dumps({k:row[k] for k in ("id","native_pair_count","unique_sweeps","incomplete_pairs","blocked_pairs")}))


def sightline_queries():
    world,row=state["world"],state["row"]
    input_maps=json.loads((ROOT/"Saved/Automation/MuseumMovement/sightline-probe-input.json").read_text(encoding="utf-8"))["maps"]
    points=next(m["points"] for m in input_maps if m["id"]==row["id"])
    cameras=sorted(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeistSecurityCameraActor),key=lambda a:a.get_actor_label())
    row["camera_los_masks"]=[]
    def camera_mask(p,eye_m):
        target=unreal.Vector(p[0]*100,p[1]*100,p[2]*100+eye_m*100)
        mask=0
        for i,a in enumerate(cameras):
            sensor=prop(a,"sensor_origin_component").get_world_location()
            if (sensor-target).length()>prop(a,"detection_range")+150:continue
            hit=unreal.SystemLibrary.line_trace_single(world,sensor,target,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                False,state["ignored"]+[a],unreal.DrawDebugTrace.NONE,True)
            if not (hit and hit.to_dict().get("blocking_hit")):mask |= 1<<i
        return mask
    # Two plausible standing eye heights bound a static sightline check. This
    # does not sample animation/camera socket pose or player detection events.
    for p in points:row["camera_los_masks"].append([camera_mask(p,1.52),camera_mask(p,1.75)])
    row["stationary_visibility"]={}
    native=json.loads((ROOT/"Saved/Automation/MuseumMovement/native-paths.json").read_text(encoding="utf-8"))
    original=next(m for m in native["maps"] if m["id"]==row["id"])
    for key,node in row["nodes"].items():
        if node["kind"] not in ("painting","button","vent"):continue
        p=node["nav"]
        item=dict(camera_masks=[camera_mask(p,1.52),camera_mask(p,1.75)],guards=[])
        for g in original["guards"]:
            nearest=999;visible=[]
            for a,b in zip(g["points"],g["points"][1:]):
                n=max(1,math.ceil(math.dist(a,b)))
                for j in range(n):
                    q=[a[k]+(b[k]-a[k])*j/n for k in range(3)]
                    distance=math.dist(q[:2],p[:2]);nearest=min(nearest,distance)
                    if distance>9:continue
                    hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(q[0]*100,q[1]*100,160),
                        unreal.Vector(p[0]*100,p[1]*100,165),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,
                        state["ignored"],unreal.DrawDebugTrace.NONE,True)
                    if not (hit and hit.to_dict().get("blocking_hit")):visible.append(round(distance,3))
            item["guards"].append(dict(id=g["id"],nearest_route_m=round(nearest,3),los_within_9m_samples=len(visible),
                nearest_visible_m=min(visible) if visible else None))
        row["stationary_visibility"][key]=item
    row["sightline_point_count"]=len(points)
    row["tables"]={}
    for name in ("DT_ArtifactData","DT_ContractData","DT_GuardData","DT_ForgeryTemplate"):
        asset=unreal.load_asset("/Game/Data/DataTable/"+name)
        if asset:
            row["tables"][name]=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
    flush()
    unreal.log_warning("MH_MOVEMENT_SIGHTLINES="+json.dumps(dict(map=row["id"],points=len(points),lasers=row["runtime_lasers"])))


def tick(_):
    if state["in_tick"] or state["phase"] == "done":
        return
    state["in_tick"] = True
    try:
        now = time.monotonic()
        if now-state["started"] > 1200 or now-state["phase_at"] > 300:
            finish("Timeout:"+state["phase"])
        elif state["phase"] == "load":
            if state["index"] == len(MAPS):
                finish()
                return
            change("loading")
            if not levels.load_level("/Game/Maps/"+MAPS[state["index"]]):
                raise RuntimeError("Map load failed")
            state["authored"] = list(actors.get_all_level_actors())
            change("wait_editor")
        elif state["phase"] == "wait_editor" and now-state["phase_at"] >= 10:
            change("wait_runtime")
            levels.editor_play_simulate()
            state["stable"] = None
        elif state["phase"] == "wait_runtime":
            world = editor.get_game_world()
            if not world or unreal.GameplayStatics.get_time_seconds(world)<2 or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
                state["stable"] = None
                return
            if state["stable"] is None:
                state["stable"] = now
            elif now-state["stable"] >= .5:
                begin_queries(world)
        elif state["phase"] == "query":
            # Bounded batches keep Slate responsive and the report observable.
            until = time.monotonic()+.15
            while state["job"] < len(state["jobs"]) and time.monotonic()<until:
                a,b=state["jobs"][state["job"]]
                state["row"]["paths"][a+"|"+b]=path_query(state["nav_nodes"][a],state["nav_nodes"][b])
                state["job"] += 1
            if state["job"] == len(state["jobs"]):
                change("finishing_map")
                if SIGHTLINES:sightline_queries()
                else:guard_queries()
                change("wait_end")
                levels.editor_request_end_play()
        elif state["phase"] == "wait_end" and editor.get_game_world() is None:
            state["index"] += 1
            change("load")
    except Exception:
        finish(traceback.format_exc())
    finally:
        state["in_tick"] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state["callback"] = unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("MH_MOVEMENT_STARTED="+str(OUT))
