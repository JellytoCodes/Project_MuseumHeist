"""Read-only checks against an initialized game world's collision/navigation.

Call verify_gallery(world, code, authored_actors) after SIE navigation settles.
Authored transforms are used so moving guards cannot hide placement defects.
"""
import unreal


def intersects_beam(a, b, trigger, padding):
    origin = trigger.get_world_location()
    extent = trigger.get_scaled_box_extent()
    axes = (trigger.get_forward_vector(), trigger.get_right_vector())
    low, high = 0.0, 1.0
    for axis, size in zip(axes, (extent.x + padding, extent.y + padding)):
        start = (a.x - origin.x) * axis.x + (a.y - origin.y) * axis.y
        end = (b.x - origin.x) * axis.x + (b.y - origin.y) * axis.y
        delta = end - start
        if abs(delta) < 0.001:
            if abs(start) > size:
                return False
            continue
        enter, leave = sorted(((-size - start) / delta, (size - start) / delta))
        low, high = max(low, enter), min(high, leave)
        if low > high:
            return False
    return True


def verify_gallery(world, code, authored_actors):
    failures, exhibits, approaches = [], [], {}
    cases = [a for a in authored_actors if isinstance(a, unreal.HeistPaintingDisplayCaseActor)]
    decorative = [a for a in authored_actors if a.actor_has_tag("MuseumDecorativePainting")]
    pawns = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn))
    player = next((a for a in pawns if isinstance(a, unreal.HeistPlayerCharacter)), None)
    if player is None:
        # SIE intentionally has no possessed player. Read the actual map's pawn
        # shell dimensions while querying its initialized runtime geometry.
        mode = unreal.GameplayStatics.get_game_mode(world)
        player = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
    capsule = player.get_component_by_class(unreal.CapsuleComponent)
    radius, half = capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height()
    profile = capsule.get_collision_profile_name()
    start = next(a for a in authored_actors if isinstance(a, unreal.PlayerStart)).get_actor_location()
    nav_start = unreal.NavigationSystemV1.project_point_to_navigation(world, start, None, None, unreal.Vector(50, 50, 200))
    if len(cases) != 20 or len(decorative) != 40:
        failures.append("Expected 20 active cases and 40 decorative paintings")
    for actor in decorative:
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        if isinstance(actor, unreal.HeistInteractableActor) or comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
            failures.append(actor.get_actor_label() + ": decorative interaction/collision")
    runtime_cases = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HeistPaintingDisplayCaseActor))
    # Map walls are the intended support; ignore gameplay shells during these rays.
    ignored = pawns + runtime_cases
    for actor in cases:
        label = actor.get_actor_label()
        components = {c.get_name(): c for c in actor.get_components_by_class(unreal.StaticMeshComponent)}
        original, replica = components["OriginalVisualComponent"], components["ReplicaVisualComponent"]
        center = original.get_world_location()
        normal = original.get_up_vector()
        tangent = original.get_forward_vector()
        vertical = original.get_right_vector()
        row = {"case": label, "height_cm": round(center.z, 2), "support_samples": 0}
        # Engine Plane's V axis runs down the image, so local +Y must point down.
        if abs(center.z - 160) > 0.1 or vertical.z > -0.99:
            failures.append(label + ": height or upright orientation")
        if any((a - b).length() > 0.01 for a, b in (
                (original.get_world_location(), replica.get_world_location()),
                (original.get_up_vector(), replica.get_up_vector()),
                (original.get_forward_vector(), replica.get_forward_vector()),
                (original.get_world_scale(), replica.get_world_scale()))):
            failures.append(label + ": original/replica plane mismatch")
        for along, up in ((0, 0), (-60, -60), (-60, 60), (60, -60), (60, 60)):
            point = center + tangent * along + unreal.Vector(0, 0, up)
            hit = unreal.SystemLibrary.line_trace_single(world, point + normal * 20, point - normal * 40,
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignored, unreal.DrawDebugTrace.NONE, True)
            fields = hit.to_dict() if hit else {}
            if fields.get("blocking_hit"):
                row["support_samples"] += 1
            else:
                failures.append(label + ": unsupported picture surface " + str((along, up)))
        front = actor.get_actor_location() + normal * 120
        front.z = half + 3
        hit = unreal.SystemLibrary.capsule_trace_single_by_profile(world, front, front, radius - 1, half - 1,
            profile, False, ignored, unreal.DrawDebugTrace.NONE, True)
        fields = hit.to_dict() if hit else {}
        if fields.get("blocking_hit"):
            blocker = fields.get("hit_actor")
            failures.append(label + ": approach capsule blocked by " + (blocker.get_actor_label() if blocker else "unknown"))
        target = unreal.NavigationSystemV1.project_point_to_navigation(world, front, None, None, unreal.Vector(50, 50, 200))
        approaches[label] = target
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, nav_start, target) if nav_start and target else None
        row["reachable"] = bool(path and path.is_valid() and not path.is_partial())
        if not row["reachable"]:
            failures.append(label + ": no complete path from player start to interaction approach")
        sphere = actor.get_component_by_class(unreal.SphereComponent)
        row["interaction_radius_cm"] = sphere.get_scaled_sphere_radius()
        if sphere.get_scaled_sphere_radius() + radius < 120:
            failures.append(label + ": approach outside interaction overlap")
        exhibits.append(row)
    rays = {
        "M01": [((-5500, 4000), (5500, 4000)), ((-3500, -4000), (3500, -4000)), ((-6200, -1200), (-2600, -1200))],
        "M02": [((-5000, -2000), (-5000, 3200)), ((-1900, 0), (2000, 0))],
        "M03": [((-7000, -400), (7000, -400)), ((-7000, 400), (7000, 400))],
    }
    blocked = 0
    for a, b in rays[code]:
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*a, 170), unreal.Vector(*b, 170),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignored, unreal.DrawDebugTrace.NONE, True)
        if hit and hit.to_dict().get("blocking_hit"):
            blocked += 1
        else:
            failures.append("Long sight line remains open: " + str((a, b)))
    # Demonstrate a complete path to the Vent that avoids the active beam,
    # including the player's horizontal capsule radius. No collision/nav edits.
    detours = {
        "M01": {"HighValue": [(5000, -4500)], "09": [(5000, 2300)]},
        "M02": {"HighValue": [(-3800, 3400)], "07": []},
        "M03": {"HighValue": [(6800, -2600), (7200, -3000), (7600, -4200), (4000, -4300)], "08": [(1200, -2800)],
                "10": [(4800, 3800), (2000, 3400), (2000, 2400)]},
    }
    exit_actor = next(a for a in authored_actors if a.get_class().get_name() == "BP_Vent_C")
    exit_point = unreal.NavigationSystemV1.project_point_to_navigation(world, exit_actor.get_actor_location(), None, None, unreal.Vector(100, 100, 200))
    egress = []
    for barrier in (a for a in authored_actors if a.get_class().get_name() == "BP_LaserBarrier_C"):
        case = barrier.get_editor_property("protected_painting_case")
        key = case.get_actor_label().split("_Painting_")[-1]
        points = [approaches[case.get_actor_label()]]
        for x, y in detours[code][key]:
            points.append(unreal.NavigationSystemV1.project_point_to_navigation(world, unreal.Vector(x, y, 95), None, None, unreal.Vector(50, 50, 200)))
        points.append(exit_point)
        trigger = barrier.get_component_by_class(unreal.BoxComponent)
        complete, crosses = True, False
        for a, b in zip(points, points[1:]):
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b) if a and b else None
            if not path or not path.is_valid() or path.is_partial():
                complete = False
                break
            samples = list(path.get_editor_property("path_points"))
            crosses |= any(intersects_beam(p, q, trigger, radius) for p, q in zip(samples, samples[1:]))
        row = {"case": key, "complete_path": complete, "crosses_active_beam": crosses}
        egress.append(row)
        if not complete or crosses:
            failures.append(case.get_actor_label() + ": independent laser egress failed")
    return {"status": "FAIL" if failures else "PASS", "failures": failures,
            "active_cases": len(cases), "decorative_paintings": len(decorative), "exhibits": exhibits,
            "sight_lines_blocked": blocked, "sight_lines_checked": len(rays[code]),
            "independent_laser_egress": egress, "user_pie": "NOT_TESTED"}
