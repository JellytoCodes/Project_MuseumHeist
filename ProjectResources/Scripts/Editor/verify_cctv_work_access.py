"""Read-only saved-map work access and continuous CCTV sweep validation.

Import from a dedicated delegated Editor wrapper after collision/Nav preparation.
No load/save/quit, actor mutation, subprocess, settings or callback side effects.
`export_map(world)` returns JSON-ready geometry plus valid 3x3 work patches.
`evaluate_map(world, camera_candidates=None, geometry=None)` recomputes static LOS
for supplied candidate sensor transforms without changing authored cameras.

This proves saved geometry under the current native interaction/sight contract;
it does not prove AI Perception registration/timing, a player-driven contract,
dynamic other-player/guard obstruction, network replication or light readability.
"""
import collections
import hashlib
import itertools
import math
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Automation/CCTVWorkAccess20261009"
PATCH_OFFSETS_CM = (-8.0, 0.0, 8.0)
COMFORT_PATCH_OFFSETS_CM = (-40.0, 0.0, 40.0)
LOOT_PATCH_OFFSETS_CM = (-8.0, 0.0, 8.0)
# Center-relative normal offsets. Migrated frames have different mesh pivots
# and an authored 180-degree Box-relative turn; actor-forward is not the front.
NORMAL_OFFSETS_CM = (-60, -40, -20, 0, 20, 40, 60, 80)
LATERAL_OFFSETS_CM = (-120, -80, -40, 0, 40, 80, 120)
LOOT_RADII_CM = (45, 55, 65, 70, 72, 75, 78)
NAV_XY_LIMIT_CM = 5.0
NAV_Z_LIMIT_CM = 35.0
PLAYER_NAV_ACCESS_DISTANCE_CM = 150.0
STANDING_FLOOR_CLEARANCE_CM = 2.15
ANGLE_GUARD_BAND_DEG = 2.0
RANGE_GUARD_BAND_CM = 25.0


def xyz(v):
    return [float(getattr(v, k)) for k in "xyz"]


def prop(obj, name):
    return obj.get_editor_property(name)


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def sub(a, b):
    return [x - y for x, y in zip(a, b)]


def scale(v, s):
    return [x * s for x in v]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def norm(v):
    length = math.sqrt(dot(v, v))
    assert length > 1e-8 and math.isfinite(length), "Invalid normalized vector"
    return scale(v, 1.0 / length)


def qrotate(q, v):
    qv = [q.x, q.y, q.z]
    uv = cross(qv, v)
    return add(v, add(scale(uv, 2*q.w), scale(cross(qv, uv), 2)))


def rotation_axes(component):
    q = component.get_world_transform().rotation
    return [qrotate(q, axis) for axis in ([1,0,0], [0,1,0], [0,0,1])]


def actor_name(actor):
    return actor.get_name() if actor else None


def line(world, a, b, ignored, trace_complex=True):
    h = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*a), unreal.Vector(*b),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, trace_complex, ignored, unreal.DrawDebugTrace.NONE, True)
    f = h.to_dict() if h else {}
    return {"blocked": bool(f.get("blocking_hit")), "actor": actor_name(f.get("hit_actor")),
            "impact": xyz(f["impact_point"]) if f.get("blocking_hit") else None,
            "normal": xyz(f["impact_normal"]) if f.get("blocking_hit") else None}


def player_template(world):
    actual = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistPlayerCharacter)
    klass = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/Player/BP_HeistPlayerCharacter")
    assert klass, "Canonical player Blueprint missing"
    actor = actual[0] if actual else unreal.get_default_object(klass)
    cap = actor.get_component_by_class(unreal.CapsuleComponent)
    assert cap, "Player capsule missing"
    radius, half = cap.get_scaled_capsule_radius(), cap.get_scaled_capsule_half_height()
    assert 0 < radius <= half, "Player capsule geometry invalid"
    channels = unreal.CollisionChannel
    # Python exposes configured project channel display names, not the hidden
    # generic ECC_GameTraceChannel2/3/4 C++ names (runtime preflight evidence).
    player_channel = channels.ECC_HEIST_PLAYER
    guard_channel = channels.ECC_HEIST_GUARD
    interaction_channel = channels.ECC_HEIST_INTERACTABLE
    eye_offset = [0.0, 0.0, 64.0]
    eye_basis = "Native missing-socket fallback Capsule+(0,0,64), used only for extra usability LOS"
    if actual:
        camera = actor.get_component_by_class(unreal.CameraComponent)
        if camera:
            eye_offset = sub(xyz(camera.get_world_location()), xyz(cap.get_world_location()))
            # Only use vertical height; horizontal socket position follows chosen facing.
            eye_offset = [0.0, 0.0, eye_offset[2]]
            eye_basis = "Actual runtime canonical player Camera height relative to capsule center"
    else:
        # An unregistered CDO has no posed component-space bone transforms;
        # GetSocketLocation can report zero despite DoesSocketExist=True.
        # This additional placement/viewability check uses the native fallback
        # eye height explicitly. Actual runtime eye is validated separately.
        eye_basis = "Diagnostic Capsule+(0,0,64) native fallback height; unregistered CDO socket transform not used; actual runtime eye NOT_TESTED"
    object_types = [getattr(unreal.ObjectTypeQuery, n) for n in dir(unreal.ObjectTypeQuery)
                    if hasattr(getattr(unreal.ObjectTypeQuery, n), "value")]
    assert object_types, "No configured object-query channels"
    def response(channel):
        # Reproduce HeistPlayerCharacter::BeginPlay explicitly; Custom profiles
        # are not a registered sweep/overlap profile and are never queried here.
        if channel == guard_channel:
            return unreal.CollisionResponseType.ECR_BLOCK
        if channel == interaction_channel:
            return unreal.CollisionResponseType.ECR_OVERLAP
        return cap.get_collision_response_to_channel(channel)
    return {"actor": actor, "cap": cap, "radius": float(radius), "half": float(half),
            "channel": player_channel, "response": response, "object_types": object_types,
            "eye_offset": eye_offset, "json": {"class": klass.get_path_name(),
                "radius_cm": float(radius), "half_height_cm": float(half),
                "runtime_player_template": bool(actual), "capsule_profile": str(cap.get_collision_profile_name()),
                "collision_basis": "Explicit current capsule response plus native BeginPlay channel overrides; no profile query",
                "eye_offset_cm": eye_offset, "eye_basis": eye_basis}}


def blockers(world, center, player, ignored, native_contract=True):
    result = unreal.SystemLibrary.capsule_overlap_components(world, unreal.Vector(*center),
        player["radius"], player["half"], player["object_types"], None, ignored) or []
    return [{"actor": actor_name(c.get_owner()), "component": c.get_name(),
             "object_type": str(c.get_collision_object_type()),
             "profile":str(c.get_collision_profile_name())}
            for c in result if c.get_collision_enabled().value != 0
            and not (native_contract and isinstance(c.get_owner(),unreal.HeistInteractableActor)
                     and c.get_name()=="InteractionCollision")
            and c.get_collision_response_to_channel(player["channel"]) == unreal.CollisionResponseType.ECR_BLOCK
            and player["response"](c.get_collision_object_type()) == unreal.CollisionResponseType.ECR_BLOCK]


def approach_sweep(world, start, end, player, ignored):
    # Native reflected UFUNCTION in KismetSystemLibrary.h:1512. Object query
    # yields all hit components, then applies the same bilateral player/native
    # interaction-response contract as actual static occupancy. No Custom
    # collision profile is submitted to Engine query APIs.
    hits=unreal.SystemLibrary.capsule_trace_multi_for_objects(world,unreal.Vector(*start),
        unreal.Vector(*end),player["radius"],player["half"],player["object_types"],
        False,ignored,unreal.DrawDebugTrace.NONE,True) or []
    result=[]
    for hit in hits:
        fields=hit.to_dict()
        component=fields.get("hit_component")
        assert component,"Native object sweep hit lacks reflected hit_component"
        owner=component.get_owner()
        if isinstance(owner,unreal.HeistInteractableActor) and component.get_name()=="InteractionCollision":
            continue
        if (component.get_collision_response_to_channel(player["channel"])==unreal.CollisionResponseType.ECR_BLOCK
            and player["response"](component.get_collision_object_type())==unreal.CollisionResponseType.ECR_BLOCK):
            result.append({"actor":actor_name(owner),"component":component.get_name(),
                "start_penetrating":bool(fields.get("initial_overlap",False)),
                "impact":xyz(fields["impact_point"])})
    return result


def local_shape(actor):
    component = prop(actor, "interaction_collision")
    if isinstance(component, unreal.BoxComponent):
        return {"kind": "box", "center": xyz(component.get_world_location()),
                "axes": rotation_axes(component), "extent": xyz(component.get_scaled_box_extent()),
                "component": component.get_name(), "enabled_at_export": str(component.get_collision_enabled())}
    assert isinstance(component, unreal.SphereComponent), "Unknown interaction shape"
    return {"kind": "sphere", "center": xyz(component.get_world_location()),
            "radius": float(component.get_scaled_sphere_radius()), "component": component.get_name()}


def point_box_distance_sq(p, shape):
    d = sub(p, shape["center"])
    return sum(max(0.0, abs(dot(d, axis))-extent)**2
               for axis, extent in zip(shape["axes"], shape["extent"]))


def interaction_overlap(center, player, shape):
    segment = player["half"] - player["radius"]
    if shape["kind"] == "sphere":
        closest = [center[0], center[1], min(center[2]+segment, max(center[2]-segment, shape["center"][2]))]
        return math.dist(closest, shape["center"]) <= player["radius"] + shape["radius"] - 1.0
    # Convex squared distance between a vertical capsule-axis segment and OBB.
    # Ternary minimization avoids assuming that the saved Box has zero pitch/roll.
    low, high = -segment, segment
    for _ in range(40):
        a, b = (2*low+high)/3, (low+2*high)/3
        da = point_box_distance_sq(add(center, [0,0,a]), shape)
        db = point_box_distance_sq(add(center, [0,0,b]), shape)
        if da < db:
            high = b
        else:
            low = a
    return point_box_distance_sq(add(center, [0,0,(low+high)/2]), shape) <= (player["radius"]-1.0)**2


def mesh_view_target(actor):
    meshes = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
    painting = [c for c in meshes if "Canvas_Painting" in c.static_mesh.get_name()]
    if painting:
        c = painting[0]
        bb = c.static_mesh.get_bounding_box()
        return xyz(unreal.MathLibrary.transform_location(c.get_world_transform(), (bb.min+bb.max)*0.5))
    return xyz(actor.get_actor_location())


def cameras(world):
    rows = []
    for camera in sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistSecurityCameraActor), key=lambda a:a.get_name()):
        sensor = prop(camera, "sensor_origin_component")
        # CameraSightConfig itself has UPROPERTY() only and editor property
        # access is prohibited. ConfigureSense stores that exact object in the
        # reflected EditDefaultsOnly SensesConfig array of the real component.
        configs = prop(prop(camera, "camera_perception_component"), "senses_config")
        sight_configs = [c for c in configs if c.get_class().get_name() == "AISenseConfig_Sight"]
        assert len(sight_configs) == 1, "Exactly one configured native Sight required"
        config = sight_configs[0]
        assert abs(float(prop(config, "point_of_view_backward_offset"))) < .001, "Nonzero Sight backward offset requires a different cone calculation"
        assert abs(float(prop(config, "near_clipping_radius"))) < .001, "Nonzero near clip requires a different cone calculation"
        rows.append({"actor": camera.get_name(), "label": camera.get_actor_label(),
            "actor_location_cm": xyz(camera.get_actor_location()),
            "rotation_pyr_deg": [camera.get_actor_rotation().pitch,camera.get_actor_rotation().yaw,camera.get_actor_rotation().roll],
            "origin": xyz(sensor.get_world_location()), "forward": xyz(sensor.get_forward_vector()), "up": xyz(sensor.get_up_vector()),
            **{k:float(prop(camera,k)) for k in ("detection_range", "detection_half_angle_degrees", "sweep_half_angle_degrees", "sweep_period_seconds")},
            "sight_light_location": xyz(prop(camera,"sight_light_component").get_world_location()),
            "intensity_cd": float(prop(prop(camera,"sight_light_component"),"intensity"))})
    return rows


def continuous_peak(camera, point):
    delta = sub(point, camera["origin"])
    distance = math.sqrt(dot(delta, delta))
    if distance < 1e-5:
        return {"distance_cm": distance, "max_cosine": 1.0, "peak_sweep_deg": 0.0, "minimum_cone_angle_deg": 0.0}
    direction, forward, up = norm(delta), norm(camera["forward"]), norm(camera["up"])
    c = dot(direction, up)*dot(up, forward)
    a = dot(direction, forward)-c
    b = dot(direction, cross(up, forward))
    half = max(0.0, min(90.0, camera["sweep_half_angle_degrees"]))
    peak = math.degrees(math.atan2(b,a))
    choices = [-half,half]
    choices += [peak+360*k for k in (-1,0,1) if -half <= peak+360*k <= half]
    best = max(choices, key=lambda y:a*math.cos(math.radians(y))+b*math.sin(math.radians(y))+c)
    cosine = min(1.0,max(-1.0,a*math.cos(math.radians(best))+b*math.sin(math.radians(best))+c))
    return {"distance_cm":distance,"max_cosine":cosine,"peak_sweep_deg":best,
            "minimum_cone_angle_deg":math.degrees(math.acos(cosine))}


def native_cone_exposure(camera, point, guard_band=False):
    peak = continuous_peak(camera, point)
    radius = max(100.0,camera["detection_range"])+(RANGE_GUARD_BAND_CM if guard_band else 0)
    angle = min(89.0,max(1.0,camera["detection_half_angle_degrees"]))+(ANGLE_GUARD_BAND_DEG if guard_band else 0)
    return peak["distance_cm"] <= radius and peak["minimum_cone_angle_deg"] <= angle


def object_geometry(world, actor, kind, player, ignored, loot_cdo):
    location = xyz(actor.get_actor_location())
    if kind == "painting":
        shape = local_shape(actor)
        normal = norm(scale(shape["axes"][0],-1))
        normal = norm([normal[0],normal[1],0])
        tangent = [-normal[1],normal[0],0]
        view_target = mesh_view_target(actor)
        max_distance = float(actor.get_maximum_session_distance())
        base_centers = [add(shape["center"],add(scale(normal,n),scale(tangent,t)))
                        for n,t in itertools.product(NORMAL_OFFSETS_CM,LATERAL_OFFSETS_CM)]
    else:
        sphere = prop(loot_cdo,"interaction_collision")
        assert isinstance(sphere,unreal.SphereComponent),"Canonical BP_Loot must use Sphere"
        actor_scale = xyz(actor.get_actor_scale3d())
        shape = {"kind":"sphere","center":location,"radius":float(sphere.get_scaled_sphere_radius())*max(abs(v) for v in actor_scale),
                 "component":"BP_Loot.InteractionCollision","basis":"Canonical BP_Loot sphere at GameMode SpawnPoint.GetActorTransform"}
        normal,tangent,view_target,max_distance = [1,0,0],[0,1,0],location,None
        base_centers = [add(location,[r*math.cos(math.radians(a)),r*math.sin(math.radians(a)),0])
                        for r,a in itertools.product(LOOT_RADII_CM,range(0,360,30))]
    patch_offsets = PATCH_OFFSETS_CM if kind == "painting" else LOOT_PATCH_OFFSETS_CM
    point_cache, points, patches, comfort_patches, rejects = {},[],[],[],collections.Counter()
    def check(q):
        key = tuple(round(v,3) for v in q[:2])
        if key in point_cache:
            return point_cache[key]
        floor = line(world,[q[0],q[1],location[2]+250],[q[0],q[1],location[2]-450],ignored,False)
        reason = None
        if not floor["blocked"] or floor["normal"][2] < .7:
            reason = "NoWalkableGround"
        elif floor["impact"][2] > location[2]+35:
            reason = "RaisedFurnitureTop"
        else:
            feet = floor["impact"]
            center = [feet[0],feet[1],feet[2]+player["half"]+STANDING_FLOOR_CLEARANCE_CM]
            if not interaction_overlap(center,player,shape):
                reason = "NoNativeInteractionOverlap"
            elif max_distance and math.dist(center,location)>max_distance-1:
                reason = "OutsideSessionDistance"
            else:
                occupied=blockers(world,center,player,ignored)
                if occupied:
                    reason="PlayerCapsuleBlocked"
                else:
                    # Player movement does not follow Recast. Nearby projection
                    # and direct approach are diagnostics, not new interaction
                    # gates. A plinth's nearest Nav polygon may be its top or a
                    # point across the plinth; that does not prohibit physical
                    # player movement around it.
                    projected=unreal.NavigationSystemV1.project_point_to_navigation(world,unreal.Vector(*feet),None,None,
                        unreal.Vector(PLAYER_NAV_ACCESS_DISTANCE_CM,PLAYER_NAV_ACCESS_DISTANCE_CM,NAV_Z_LIMIT_CM))
                    nav_center,path_blockers,nav_status=None,None,"NOT_TESTED"
                    nav_reason=None
                    if (projected is None or math.dist(feet[:2],xyz(projected)[:2])>PLAYER_NAV_ACCESS_DISTANCE_CM
                        or abs(feet[2]-projected.z)>NAV_Z_LIMIT_CM):
                        nav_reason="NoNearbyAccessibleNav"
                    else:
                        nav_floor=line(world,[projected.x,projected.y,feet[2]+100],
                            [projected.x,projected.y,feet[2]-100],ignored,False)
                        if not nav_floor["blocked"] or nav_floor["normal"][2]<.7:
                            nav_reason="NavApproachUngrounded"
                        else:
                            nav_center=add(nav_floor["impact"],[0,0,player["half"]+STANDING_FLOOR_CLEARANCE_CM])
                            path_blockers=approach_sweep(world,nav_center,center,player,ignored)
                            nav_status="PASS" if not path_blockers else "FAIL_DIRECT_SEGMENT_ONLY"
                            if path_blockers:nav_reason="NavApproachCapsuleBlocked"
                    view=line(world,add(center,player["eye_offset"]),view_target,ignored+[actor])
                    if view["blocked"]:
                        reason="ViewingLOSBlocked"
                    else:
                        idx=len(points)
                        points.append({"feet":feet,"center":center,"nav":xyz(projected) if projected is not None else None,
                            "nav_xy_offset_cm":math.dist(feet[:2],xyz(projected)[:2]) if projected is not None else None,
                            "nav_approach_start_center":nav_center,"native_capsule_approach_sweep_clear":not path_blockers if path_blockers is not None else None,
                            "nav_approach_status":nav_status,"nav_approach_reason":nav_reason,
                            "nav_direct_approach_blockers":path_blockers,
                            "distance_to_actor_cm":math.dist(center,location),
                            "native_interaction_overlap":True,"full_native_capsule_clear":True,
                            "standing_floor_clearance_cm":STANDING_FLOOR_CLEARANCE_CM,
                            "view_los_clear":True,"floor_actor":floor["actor"]})
                        point_cache[key]=idx
                        return idx
        rejects[reason]+=1
        point_cache[key]=None
        return None
    for base in base_centers:
        indices = [check(add(base,add(scale(normal,n),scale(tangent,t))))
                   for n,t in itertools.product(patch_offsets,patch_offsets)]
        if all(index is not None for index in indices):
            patches.append({"center_xy_cm":base[:2],"point_indices":indices})
    if kind=="painting":
        for base in base_centers:
            indices=[check(add(base,add(scale(normal,n),scale(tangent,t))))
                     for n,t in itertools.product(COMFORT_PATCH_OFFSETS_CM,COMFORT_PATCH_OFFSETS_CM)]
            if all(index is not None for index in indices):
                comfort_patches.append({"center_xy_cm":base[:2],"point_indices":indices})
    tolerance_patches=patches
    # An actual single stance is the primary native-gameplay proof. Never
    # duplicate one point nine times and label it a spatial 3x3 tolerance patch.
    indices=sorted(range(len(points)),key=lambda i:(points[i]["nav_approach_status"]!="PASS",math.dist(points[i]["center"][:2],shape["center"][:2])))
    patches=[{"center_xy_cm":points[i]["center"][:2],"point_indices":[i],"kind":"single_native_work_stance"} for i in indices]
    return {"kind":kind,"actor":actor.get_name(),"label":actor.get_actor_label(),
        "case_id":str(actor.get_display_case_id()) if kind=="painting" else None,
        "spawn_category":str(prop(actor,"spawn_category")) if kind=="loot" else None,
        "actor_location_cm":location,"shape":shape,"front_normal":normal,"tangent":tangent,
        "candidate_basis":"Actual InteractionCollision world center and local -X front; actor pivot and rotation are not assumed to be frame center/front" if kind=="painting" else "Actual spawn anchor and canonical loot Sphere",
        "view_target":view_target,"maximum_session_distance_cm":max_distance,
        "primary_proof":"One actual native-interaction stance with full native player body; no fabricated 3x3 patch",
        "patch_offsets_cm":[0],"patch_center_freedom_width_cm":0,
        "advisory_tolerance_patch_offsets_cm":list(patch_offsets),"advisory_tolerance_patch_width_cm":16,
        "patch_size_basis":"Actual single stance primary; real 16cm 3x3 tolerance and Painting 80cm comfort are advisory. Full native body and unchanged CCTV margin remain mandatory.",
        "points":points,"patches":patches,"advisory_tolerance_patches":tolerance_patches,"comfort_patches":comfort_patches,
        "comfort_patch_width_cm":80 if kind=="painting" else None,
        "candidate_point_rejections":dict(rejects),
        "native_interaction_collision_response":"HeistInteractableActor::BeginPlay sets Interactable object type, all channels Ignore, HeistPlayer Overlap; shape never blocks the player. Other components remain actual bilateral Block queries.",
        "geometry_status":"PASS" if patches else "FAIL"}


def export_map(world):
    assert world,"Missing prepared world"
    navs = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RecastNavMesh)
    assert len(navs)==1 and not unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world),"Nav must be prepared first"
    actor_list = list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor))
    ignored = [a for a in actor_list if isinstance(a,unreal.Pawn)]
    player = player_template(world)
    loot_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/World/Actors/Loot/BP_Loot")
    assert loot_class,"Canonical loot Blueprint missing"
    loot_cdo = unreal.get_default_object(loot_class)
    paintings = sorted((a for a in actor_list if isinstance(a,unreal.HeistPaintingDisplayCaseActor)),key=lambda a:a.get_name())
    spawns = sorted((a for a in actor_list if isinstance(a,unreal.HeistLootSpawnPoint)),key=lambda a:a.get_name())
    assert len(paintings)==60 and len(spawns)==12,"All 60 painting and 12 loot candidates required"
    starts = [a for a in actor_list if isinstance(a,unreal.PlayerStart)]
    assert starts,"PlayerStart needed for collision controls"
    control_location = xyz(starts[0].get_actor_location())
    floor = line(world,add(control_location,[0,0,250]),add(control_location,[0,0,-450]),ignored,False)
    assert floor["blocked"] and floor["normal"][2]>.7,"Ground line-trace positive control failed"
    footprint = [control_location[0],control_location[1],floor["impact"][2]]
    floor_blockers = blockers(world,footprint,player,ignored)
    assert floor_blockers,"Actual object-channel floor-penetration positive control failed"
    clear_air = not line(world,add(footprint,[0,0,180]),add(footprint,[0,0,180.5]),ignored)["blocked"]
    assert clear_air,"Air line-trace negative control failed"
    first_case=paintings[0]
    first_shape=local_shape(first_case)
    first_floor=line(world,add(first_shape["center"],[0,0,250]),add(first_shape["center"],[0,0,-450]),ignored,False)
    case_diagnostic={"actor":first_case.get_name(),"interaction_shape":first_shape,"floor":first_floor}
    if first_floor["blocked"]:
        diagnostic_center=add(first_floor["impact"],[0,0,player["half"]+STANDING_FLOOR_CLEARANCE_CM])
        case_diagnostic.update(center=diagnostic_center,
            raw_editor_blockers=blockers(world,diagnostic_center,player,ignored,False),
            native_contract_blockers=blockers(world,diagnostic_center,player,ignored,True))
    rows = [object_geometry(world,a,"painting",player,ignored,loot_cdo) for a in paintings]
    rows += [object_geometry(world,a,"loot",player,ignored,loot_cdo) for a in spawns]
    waypoints = []
    camrows = cameras(world)
    route_nodes=collections.defaultdict(list)
    probe_requests=[]
    for a in actor_list:
        if isinstance(a,unreal.HeistGuardWaypoint):
            route=str(prop(a,"patrol_route_id"));order=int(prop(a,"patrol_order"))
            p=xyz(a.get_actor_location())
            route_nodes[route].append((order,a.get_name(),p))
            probe_requests.append({"actor":a.get_name(),"point":p,"route":route,"order":order,"kind":"authored_waypoint"})
    for route,nodes in route_nodes.items():
        nodes.sort()
        for (_,name,start),(_,_,end) in zip(nodes,nodes[1:]+nodes[:1]):
            count=max(1,math.ceil(math.dist(start[:2],end[:2])/150))
            for i in range(1,count):
                p=add(start,scale(sub(end,start),i/count))
                probe_requests.append({"actor":name+"_segment_"+str(i),"point":p,"route":route,"order":None,"kind":"route_segment_ground_sample"})
    for cam in camrows:
        for radius,angle in itertools.product((300,600,900),range(0,360,45)):
            p=add(cam["origin"],[radius*math.cos(math.radians(angle)),radius*math.sin(math.radians(angle)),0])
            probe_requests.append({"actor":cam["actor"]+"_grid_"+str(radius)+"_"+str(angle),"point":p,"route":None,"order":None,"kind":"local_camera_ground_grid"})
    by_name={a.get_name():a for a in actor_list}
    seen_probes=set()
    for request in probe_requests:
            p=request["point"]
            key=tuple(round(v,2) for v in p[:2])
            if key in seen_probes:continue
            seen_probes.add(key)
            floor=line(world,add(p,[0,0,250]),[p[0],p[1],-150],ignored,False)
            if floor["blocked"] and floor["normal"][2]>.7:
                feet=floor["impact"]
                nav=unreal.NavigationSystemV1.project_point_to_navigation(world,unreal.Vector(*feet),None,None,unreal.Vector(15,15,100))
                center=add(feet,[0,0,player["half"]+STANDING_FLOOR_CLEARANCE_CM])
                if nav is None or math.dist(feet[:2],xyz(nav)[:2])>NAV_XY_LIMIT_CM or abs(feet[2]-nav.z)>NAV_Z_LIMIT_CM or blockers(world,center,player,ignored):
                    continue
                los_mask,exposure_mask=0,0
                for ci,cam in enumerate(camrows):
                    if math.dist(center,cam["origin"])>max(100,cam["detection_range"])+RANGE_GUARD_BAND_CM:
                        continue
                    camera=by_name[cam["actor"]]
                    if not line(world,cam["origin"],center,ignored+[camera])["blocked"]:
                        los_mask|=1<<ci
                        if native_cone_exposure(cam,center):exposure_mask|=1<<ci
                distance=min(math.dist(center[:2],r["actor_location_cm"][:2]) for r in rows)
                waypoints.append({"actor":request["actor"],"kind":request["kind"],"location":center,"feet":feet,"nav":xyz(nav),
                    "native_capsule_clear":True,"native_los_mask":los_mask,"native_exposure_mask":exposure_mask,
                    "nearest_exhibit_actor_xy_distance_cm":distance,"transit_goal_eligible":distance>=250,
                    "route":request["route"],"order":request["order"]})
    return {"schema":1,"map":unreal.GameplayStatics.get_current_level_name(world,True),
        "player":player["json"],"primary_stance_point_count":1,"painting_advisory_tolerance_patch_offsets_cm":list(PATCH_OFFSETS_CM),"painting_advisory_tolerance_patch_width_cm":16,
        "painting_advisory_comfort_patch_width_cm":80,
        "loot_advisory_tolerance_patch_offsets_cm":list(LOOT_PATCH_OFFSETS_CM),"loot_advisory_tolerance_patch_width_cm":16,
        "native_sight_target":"Player ActorLocation/capsule center; no IAISightTargetInterface",
        "sight_trace":"AISense Sight default ECC_Visibility complex trace; current Default AI config must be Visibility",
        "interaction_gate":"Geometry overlap, Painting 300cm max-session distance; no native facing/LOS gate",
        "extra_usability_gate":"Front-side painting candidate grid and clear eye-to-visible-frame center LOS",
        "hypothetical_activation":"Every saved candidate tested independently even if currently inactive; flags not changed",
        "static_geometry_scope":"Pawn bodies ignored for placement; dynamic guards still threaten work",
        "ground_model":"Simple collision for physical ground, full native capsule at floor+half+2.15cm (mean of engine MIN/MAX_FLOOR_DIST 1.9/2.4); complex Visibility only for Sight and extra view LOS",
        "player_navigation_access":"Nearby same-floor Recast projection and continuous full native capsule direct approach are diagnostics only. Exact Nav XY/failed straight segment do not restrict native player E interaction; runtime player approach is separate proof.",
        "controls":{"floor_hit":floor,"floor_penetration_blockers":floor_blockers,"air_clear":clear_air,
                    "first_case_blocker_diagnostic":case_diagnostic},
        "cameras":camrows,"objects":rows,"corridor_probes":waypoints,
        "counts":{"paintings":len(paintings),"loot":len(spawns),"cameras":len(camrows),
                  "objects_without_work_patch":sum(not r["patches"] for r in rows)},
        "geometry_status":"PASS" if all(r["patches"] for r in rows) else "FAIL"}


def evaluate_map(world,camera_candidates=None,geometry=None):
    geometry = geometry or export_map(world)
    camrows = camera_candidates if camera_candidates is not None else geometry["cameras"]
    actor_list = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    ignored = [a for a in actor_list if isinstance(a,unreal.Pawn)]
    by_name = {a.get_name():a for a in actor_list}
    objects = []
    for obj in geometry["objects"]:
        point_masks,guard_masks,los_masks = [],[],[]
        for point in obj["points"]:
            exact_mask,guard_mask,los_mask=0,0,0
            for ci,camera in enumerate(camrows):
                if math.dist(camera["origin"],point["center"])>max(100,camera["detection_range"])+RANGE_GUARD_BAND_CM:
                    continue
                observer=by_name.get(camera["actor"])
                los=not line(world,camera["origin"],point["center"],ignored+([observer] if observer else []))["blocked"]
                if los:
                    los_mask|=1<<ci
                    if native_cone_exposure(camera,point["center"],False):exact_mask|=1<<ci
                    if native_cone_exposure(camera,point["center"],True):guard_mask|=1<<ci
            point_masks.append(exact_mask);guard_masks.append(guard_mask);los_masks.append(los_mask)
        safe,robust,patch_masks=[],[],[]
        for pi,patch in enumerate(obj["patches"]):
            mask,gmask=0,0
            for p in patch["point_indices"]:
                mask|=point_masks[p];gmask|=guard_masks[p]
            patch_masks.append(mask)
            if mask==0:safe.append(pi)
            if gmask==0:robust.append(pi)
        objects.append({"actor":obj["actor"],"kind":obj["kind"],"geometry_status":obj["geometry_status"],
            "point_exact_exposure_masks":point_masks,"point_guard_band_exposure_masks":guard_masks,"point_los_masks":los_masks,
            "patch_exact_exposure_masks":patch_masks,"safe_patch_indices":safe,"robust_safe_patch_indices":robust,
            "status":"PASS" if robust else "FAIL"})
    return {"schema":1,"map":geometry["map"],"cameras":camrows,"objects":objects,
        "continuous_sweep":"Analytic max dot product over exact local-up Rodrigues rotation; no phase sample gap",
        "guard_band":{"angle_deg":ANGLE_GUARD_BAND_DEG,"range_cm":RANGE_GUARD_BAND_CM},
        "counts":{"painting_pass":sum(r["status"]=="PASS" for r in objects if r["kind"]=="painting"),
                  "loot_pass":sum(r["status"]=="PASS" for r in objects if r["kind"]=="loot"),
                  "fail":sum(r["status"]!="PASS" for r in objects)},
        "status":"PASS" if all(r["status"]=="PASS" for r in objects) else "FAIL",
        "native_runtime_detection":"NOT_TESTED","user_pie":"NOT_TESTED"}


def self_check_math():
    # Independent dense rotation agreement catches the world-Yaw approximation.
    cases=[([1,0,0],[0,0,1]),(norm([1,0,-1]),norm([1,0,1])),(norm([.7,.4,-.8]),norm([.6,.2,.8]))]
    maximum_error=0.0
    for forward,up in cases:
        for target in ([3,4,-2],[-1,4,1],[.3,-3,1]):
            cam={"origin":[0,0,0],"forward":forward,"up":up,"sweep_half_angle_degrees":35}
            analytic=continuous_peak(cam,target)["max_cosine"]
            direction=norm(target)
            brute=max(dot(direction,add(add(scale(forward,math.cos(math.radians(a))),scale(cross(up,forward),math.sin(math.radians(a)))),
                scale(up,dot(up,forward)*(1-math.cos(math.radians(a)))))) for a in (i/100 for i in range(-3500,3501)))
            maximum_error=max(maximum_error,abs(analytic-brute))
    assert maximum_error<1e-7,"Continuous Rodrigues peak math mismatch"
    return {"status":"PASS","dense_independent_rotation_max_error":maximum_error}
