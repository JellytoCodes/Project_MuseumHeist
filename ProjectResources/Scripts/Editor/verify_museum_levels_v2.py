"""Current layout verification entry point and strict guard navigation queries.

Geometry, hanging and room connectivity are validated against MuseumLevelLayout.json.
"""
import json
import math
import unreal

def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def actor_label(value):
    return value.get_actor_label() if value else ""


def verify_guard_navigation(world, guards, waypoint_routes, mode):
    result = {
        "mode": mode,
        "status": "NOT_TESTED",
        "reason": "NavigationNotRequested",
        "expected_segments": 0,
        "checked_segments": 0,
        "already_at_goal_segments": 0,
        "failed_segments": [],
        "path_scope": "ProjectedPatrolEndpointsAndNavigationGraph",
        "recast_settings": {"status": "NOT_TESTED"},
        "capsule_start_occupancy": {
            "status": "NOT_TESTED", "checked_guards": 0, "failed_guards": [],
            "scope": "SavedGuardCapsulesAgainstWorldGeometry_AuthoredGuardsIgnored",
            "query_inset_cm": 1.0,
        },
        "capsule_path_sweeps": "NOT_TESTED",
        "natural_patrol": "NOT_TESTED",
    }
    if mode == "off":
        return result

    segments = []
    for guard in sorted(guards, key=actor_label):
        patrol_components = [
            component for component in guard.get_components_by_class(unreal.ActorComponent)
            if component.get_class().get_name() == "HeistPatrolPathComponent"
        ]
        route_id = str(prop(patrol_components[0], "patrol_route_id")) if len(patrol_components) == 1 else ""
        route = waypoint_routes.get(route_id, [])
        if not route:
            result["failed_segments"].append({"guard": actor_label(guard), "route": route_id, "reason": "MissingPatrolRoute"})
            continue
        loop_patrol = prop(patrol_components[0], "loop_patrol")
        if loop_patrol is None:
            result.update(reason="PatrolLoopConfigurationUnavailable", guard=actor_label(guard))
            return result
        segments.append((guard, route_id, guard, route[0]))
        segments.extend((guard, route_id, start, end) for start, end in zip(route, route[1:]))
        # AdvanceWaypoint uses ping-pong, not a last-to-first wraparound.
        if loop_patrol:
            segments.extend((guard, route_id, end, start) for start, end in zip(route, route[1:]))
    result["expected_segments"] = len(segments)
    if result["failed_segments"] or not segments:
        result.update(status="FAIL", reason="InvalidPatrolRouteConfiguration")
        return result

    try:
        if mode == "strict":
            nav_meshes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
            simplification_errors = [float(mesh.get_editor_property("max_simplification_error")) for mesh in nav_meshes]
            settings_match = (len(nav_meshes) == 1 and math.isfinite(simplification_errors[0])
                              and abs(simplification_errors[0] - 0.1) <= 0.00001)
            result["recast_settings"] = {
                "status": "PASS" if settings_match else "FAIL", "count": len(nav_meshes),
                "max_simplification_errors": simplification_errors, "expected_max_simplification_error": 0.1,
            }
            if not settings_match:
                result.update(status="FAIL", reason="SavedRecastSettingsMismatch")
                return result

        navigation = unreal.NavigationSystemV1.get_navigation_system(world)
        if navigation is None:
            result["reason"] = "MissingNavigationSystem"
            return result
        # MainNavData is not exposed to Python in UE 5.8; None from prop() cannot
        # distinguish an inaccessible property from a missing navigation object.
        # UE uses MainNavData for a single supported agent. Multiple agents need
        # agent-specific projection and can assert if the pawn has no matching data.
        supported_agents = prop(navigation, "supported_agents")
        if supported_agents is None or len(supported_agents) > 1:
            result["reason"] = "NavigationAgentSelectionUnavailable"
            return result
        if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
            result["reason"] = "NavigationBuildingOrLocked"
            return result

        if mode == "strict":
            # Check the actual authored capsule, including an AlreadyAtGoal start.
            # This is a stationary world-geometry query, not a path sweep or PIE.
            # Match the contract automation's 1cm inset for normal surface contact.
            # Wait for world/navigation readiness before certifying physics queries.
            occupancy = result["capsule_start_occupancy"]
            for guard in guards:
                capsule = guard.get_component_by_class(unreal.CapsuleComponent)
                if capsule is None:
                    occupancy["failed_guards"].append({"guard": actor_label(guard), "reason": "MissingCapsule"})
                    continue
                center = capsule.get_world_location()
                radius = float(capsule.get_scaled_capsule_radius())
                half_height = float(capsule.get_scaled_capsule_half_height())
                if (not all(math.isfinite(value) for value in (center.x, center.y, center.z, radius, half_height))
                        or radius <= 0.0 or half_height < radius):
                    occupancy["failed_guards"].append({"guard": actor_label(guard), "reason": "InvalidCapsuleDimensions"})
                    continue
                hit = unreal.SystemLibrary.capsule_trace_single_by_profile(
                    world, center, center, max(0.1, radius - occupancy["query_inset_cm"]),
                    max(0.1, half_height - occupancy["query_inset_cm"]), capsule.get_collision_profile_name(),
                    False, guards, unreal.DrawDebugTrace.NONE, True,
                )
                occupancy["checked_guards"] += 1
                hit_fields = hit.to_dict() if hit is not None else {}
                if hit_fields.get("blocking_hit"):
                    hit_actor = hit_fields.get("hit_actor")
                    occupancy["failed_guards"].append({
                        "guard": actor_label(guard), "reason": "SavedGuardCapsuleBlocked",
                        "hit_actor": actor_label(hit_actor) if hit_actor else None,
                        "initial_overlap": bool(hit_fields.get("initial_overlap")),
                    })
            occupancy["status"] = "FAIL" if occupancy["failed_guards"] else "PASS"
            if occupancy["status"] != "PASS":
                result.update(status="FAIL", reason="SavedGuardCapsuleOccupancyFailed")
                return result

        # UE Python maps bool + FVector out to FVector on success, None on failure.
        # NavData=None resolves existing default data with DontCreate. The actual
        # projection/path results below establish availability without rebuild/save.
        locations = {}
        for _, _, start, end in segments:
            for actor in (start, end):
                actor_path = actor.get_path_name()
                if actor_path not in locations:
                    locations[actor_path] = unreal.NavigationSystemV1.project_point_to_navigation(
                        world, actor.get_actor_location(), None, None, unreal.Vector(50.0, 50.0, 200.0)
                    )
        if all(location is None for location in locations.values()):
            result["reason"] = "NoQueryableNavigationAtRoutePoints"
            return result

        for guard, route_id, start, end in segments:
            segment = {
                "guard": actor_label(guard),
                "route": route_id,
                "from": actor_label(start),
                "to": actor_label(end),
            }
            start_location = locations[start.get_path_name()]
            end_location = locations[end.get_path_name()]
            if start_location is None or end_location is None:
                segment["reason"] = "EndpointOffNavigation"
                result["failed_segments"].append(segment)
                continue
            # AAIController returns AlreadyAtGoal before finding a path when the
            # guard is placed exactly at its first waypoint. A one-point path is
            # not FNavigationPath::IsValid, so count this narrow case separately.
            original_start, original_end = start.get_actor_location(), end.get_actor_location()
            if (start == guard
                    and math.hypot(original_start.x - original_end.x, original_start.y - original_end.y) <= 0.01
                    and all(abs(a - b) <= 0.01 for a, b in zip(
                        (start_location.x, start_location.y, start_location.z),
                        (end_location.x, end_location.y, end_location.z)))):
                result["checked_segments"] += 1
                result["already_at_goal_segments"] += 1
                continue
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                world, start_location, end_location, guard
            )
            result["checked_segments"] += 1
            if path is None or not path.is_valid() or path.is_partial():
                segment["reason"] = "MissingPath" if path is None else "InvalidOrPartialPath"
                result["failed_segments"].append(segment)

        # A dirty/locked nav graph cannot certify paths even if earlier queries succeeded.
        if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
            result["reason"] = "NavigationChangedDuringQueries"
            return result
    except Exception as error:
        result.update(reason="NavigationQueryUnavailable", error=str(error))
        return result

    result.update(
        status="FAIL" if result["failed_segments"] else "PASS",
        reason="UnreachablePatrolSegments" if result["failed_segments"] else "AllPatrolSegmentsReachable",
    )
    return result


if __name__ == "__main__":
    import runpy
    from pathlib import Path
    runpy.run_path(str(Path(__file__).with_name("run_museum_layout_validation.py")), run_name="__main__")
