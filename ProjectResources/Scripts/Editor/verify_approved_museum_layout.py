"""Saved layout and runtime reachability checks for the approved plans."""
import json
from pathlib import Path
import unreal


def verify_layout(world, code, actors):
    plan = next(m for m in json.loads((Path(unreal.Paths.project_dir()) /
        "ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json").read_text(encoding="utf-8"))["maps"] if m["id"] == code)
    by_label = {a.get_actor_label(): a for a in actors}
    prefix = "LDV2_"+code+"_Gallery_"
    failures, destinations, door_checks = [], [], []
    for class_name, expected in (("PlayerStart",4),("BP_Vent_C",1),
            ("BP_PaintingDisplayCase_C",20),("BP_LootSpawnPoint_C",12),
            ("BP_Guard_C",len(plan["guards"])),("BP_SecurityCamera_C",len(plan["cameras"])),
            ("BP_LaserBarrier_C",len(plan["lasers"])),("BP_SecurityHoldButton_C",len(plan["lasers"]))):
        count=sum(a.get_class().get_name()==class_name for a in actors)
        if count!=expected: failures.append("{} count {} != {}".format(class_name,count,expected))
    for tag,expected in (("HeistDetentionSpawn",4),("HeistEvidenceSlot",25),
            ("HeistLootSpawnExhibitionRoom",10),("HeistLootSpawnVaultFixed",2)):
        if sum(a.actor_has_tag(tag) for a in actors)!=expected: failures.append(tag+": anchor count mismatch")
    mode = unreal.GameplayStatics.get_game_mode(world)
    player = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
    capsule = player.get_component_by_class(unreal.CapsuleComponent)
    radius, half = capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height()
    ignored = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn))
    start = next(a for a in actors if isinstance(a, unreal.PlayerStart)).get_actor_location()

    def nav(p):
        return unreal.NavigationSystemV1.project_point_to_navigation(world,p,None,None,unreal.Vector(60,60,200))

    nav_start = nav(start)

    def reachable(name, x, y, required=True):
        point = unreal.Vector(x*100,y*100,half+3)
        hit = unreal.SystemLibrary.capsule_trace_single_by_profile(world,point,point,radius-1,half-1,
            capsule.get_collision_profile_name(),False,ignored,unreal.DrawDebugTrace.NONE,True)
        free = not (hit and hit.to_dict().get("blocking_hit"))
        target = nav(point)
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world,nav_start,target) if nav_start and target else None
        at_target = bool(nav_start and target and (nav_start-target).length() < 1.0)
        valid = bool(free and (at_target or path and path.is_valid() and not path.is_partial()))
        if required:
            destinations.append(dict(id=name,free_capsule=free,complete_path=valid,xy=[x,y]))
            if not valid: failures.append(name+": blocked capsule or incomplete route")
        return valid

    def bounds_match(label, bounds, bottom, height):
        actor = by_label.get(label)
        if actor is None:
            failures.append(label+": missing")
            return
        o,e=actor.get_actor_bounds(False)
        actual=(o.x-e.x,o.y-e.y,o.x+e.x,o.y+e.y,o.z-e.z,o.z+e.z)
        expected=tuple(v*100 for v in bounds)+(bottom*100,(bottom+height)*100)
        if any(abs(a-b)>.3 for a,b in zip(actual,expected)):
            failures.append(label+": saved measured bounds disagree with approved plan")

    for wall in plan["walls"]:
        f,a,b,t=wall["fixed"],wall["start"],wall["end"],wall["thickness"]
        bounds=[a,f-t/2,b,f+t/2] if wall["axis"]=="h" else [f-t/2,a,f+t/2,b]
        bounds_match(prefix+wall["id"],bounds,0,wall["height"])
    for prop in plan["props"]:
        bounds_match(prefix+prop["id"],prop["bounds"],0,prop["height"])
    floor=by_label.get("LDV2_"+code+"_Floor_Approved")
    bounds_match(floor.get_actor_label(),plan["bounds"],-.2,.2)
    for door in plan["doors"]:
        x,y=door["xy"]; w=door["width"]; d=.4
        bounds=[x-w/2,y-d/2,x+w/2,y+d/2] if door["axis"]=="h" else [x-d/2,y-w/2,x+d/2,y+w/2]
        bounds_match(prefix+"Door_"+door["id"],bounds,3,plan["walls"][0]["height"]-3)
        okay=True
        for offset in (-w/2+radius/100+.05,0,w/2-radius/100-.05):
            dx,dy=(offset,0) if door["axis"]=="h" else (0,offset)
            okay &= reachable(door["id"],x+dx,y+dy,False)
        door_checks.append(dict(id=door["id"],width_cm=w*100,clear_and_reachable=okay))
        if not okay: failures.append(door["id"]+": opening width obstructed or unreachable")
    for room in plan["rooms"].values():
        x0,y0,x1,y1=room["bounds"]
        found=None
        for u,v in ((.5,.5),(.25,.5),(.75,.5),(.5,.25),(.5,.75)):
            x,y=x0+(x1-x0)*u,y0+(y1-y0)*v
            if reachable(room["id"],x,y,False):
                found=(x,y);break
        if found: reachable(room["id"],*found)
        else: failures.append(room["id"]+": no reachable room inspection position")
    for kind in ("player_starts","detention_starts"):
        for p in plan[kind]: reachable(p["id"],*p["xy"])
    reachable("Vent",*plan["vent"])
    ex,ey=plan["evidence"]
    # The evidence shelf itself is solid: test the surrounding recovery aisle.
    reachable("EvidenceRecovery",ex,ey-2.3)
    for p in plan["paintings"]:
        if p["active"]:
            actor=by_label.get("LDV2_"+code+"_Painting_"+p["case_key"])
            expected="Case_"+code+("_Target" if p["case_key"]=="Target" else "_Optional_"+p["case_key"])
            if actor is None or str(actor.get_editor_property("display_case_id"))!=expected:
                failures.append(p["id"]+": preserved case identity mismatch")
    target_slot = next(p for p in plan["paintings"] if p.get("case_key") == "Target")
    presentation = unreal.load_asset("/Game/Data/DataTable/DT_MapPresentation")
    row = next(r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(presentation)) if r["MapId"] == code)
    target_zone = "Zone_"+target_slot["room"]
    zone = next((z for z in row["ZoneAnchors"] if z["ZoneId"] == target_zone), None)
    b = plan["rooms"][target_slot["room"]]["bounds"]
    if row["ContractTargetGalleryZoneId"] != target_zone or not zone or abs(zone["WorldLocation"]["X"]-(b[0]+b[2])*50) > .1 or abs(zone["WorldLocation"]["Y"]-(b[1]+b[3])*50) > .1:
        failures.append("Target gallery marker disagrees with the authored target room")
    for key, xy in plan.get("loot_spawn_overrides", {}).items():
        anchor = by_label["LDV2_"+code+"_LootSpawn_"+key].get_actor_location()
        pedestal = by_label["LDV2_"+code+"_Gallery_LootPedestal_"+key].get_actor_bounds(False)[0]
        if any(abs(v-expect)>0.1 for v,expect in ((anchor.x,xy[0]*100),(anchor.y,xy[1]*100),(pedestal.x,xy[0]*100),(pedestal.y,xy[1]*100))):
            failures.append(key+": loot anchor and pedestal placement mismatch")
    runtime_lasers = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistLaserBarrierActor)}
    laser_checks = []
    for i,l in enumerate(plan["lasers"]):
        barrier=by_label["LDV2_{}_Laser_{:02}".format(code,i+1)]
        button=by_label["LDV2_{}_LaserButton_{:02}".format(code,i+1)]
        case=barrier.get_editor_property("protected_painting_case")
        expected="LDV2_"+code+"_Painting_"+plan["case_mapping"][l["cases"][0]]
        if case.get_actor_label()!=expected or button.get_editor_property("linked_laser_barrier")!=barrier:
            failures.append(l["id"]+": gameplay link mismatch")
        reachable(l["id"]+"Button",*l["button"])
        runtime = runtime_lasers.get(barrier.get_actor_label())
        active = bool(runtime and runtime.is_barrier_enabled() and runtime.is_beam_active())
        configured_artifact = plan.get("case_artifacts", {}).get(plan["case_mapping"][l["cases"][0]])
        artifact_matches = configured_artifact is not None and str(case.get_editor_property("target_artifact_id")) == configured_artifact
        laser_checks.append(dict(id=l["id"],enabled_and_beam_active=active,artifact_matches=artifact_matches))
        if not active or not artifact_matches:
            failures.append(l["id"]+": runtime activation or authored artifact mismatch")
    camera_checks=[]
    for i,c in enumerate(plan["cameras"]):
        camera=by_label["LDV2_{}_CCTV_{:02}".format(code,i+1)]
        origin=camera.get_actor_location()
        rotation=camera.get_actor_rotation()
        expected=unreal.Vector(c["xy"][0]*100,c["xy"][1]*100,c["z"]*100)
        transform_matches=(origin-expected).length()<0.1 and all(abs((actual-wanted+180)%360-180)<0.01
            for actual,wanted in ((rotation.pitch,c["pitch"]),(rotation.yaw,c["yaw"]),(rotation.roll,0)))
        if not transform_matches:failures.append(c["id"]+": authored sensor transform mismatch")
        coverage_matches=(abs(camera.get_editor_property("detection_range")-c["range"]*100)<0.1
            and abs(camera.get_editor_property("detection_half_angle_degrees")-c["angle"]/2)<0.01)
        if not coverage_matches:failures.append(c["id"]+": authored coverage mismatch")
        target=origin+camera.get_actor_forward_vector()*600
        clear=True
        for eye_height in (152.0,175.0):
            target.z=eye_height
            hit=unreal.SystemLibrary.line_trace_single(world,origin,target,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                False,ignored+[camera],unreal.DrawDebugTrace.NONE,True)
            clear=clear and not bool(hit and hit.to_dict().get("blocking_hit"))
        camera_checks.append(dict(id=c["id"],transform_matches=transform_matches,coverage_matches=coverage_matches,centerline_clear=clear))
        if not clear:failures.append(c["id"]+": doorway blocks CCTV centerline at player eye height")
    return dict(status="FAIL" if failures else "PASS",failures=failures,wall_segments=len(plan["walls"]),
                doors=door_checks,destinations=destinations,player_capsule_cm=[radius,half],
                room_count=len(plan["rooms"]),evidence_slots=sum(a.actor_has_tag("HeistEvidenceSlot") for a in actors),
                camera_sightlines=camera_checks, runtime_lasers=laser_checks)
