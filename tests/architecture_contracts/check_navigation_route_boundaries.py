#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")

def forbid(body: str, owner: str, tokens) -> None:
    for token in tokens:
        if token in body:
            raise AssertionError(
                f"{owner}: illegal navigation boundary token {token!r}"
            )

try:
    contract = read("src/game/navigation/NAVIGATION_ROUTE_BOUNDARY_CONTRACT.md")
    for token in (
        "TrafficRoutePlanAdapter",
        "RoutePlannerApi.h",
        "ClientRouteAutopilot / follower",
        "SpaceState",
        "explicit value contracts",
    ):
        if token not in contract:
            raise AssertionError(f"boundary contract missing {token!r}")

    api = read("src/game/navigation/planner/RoutePlannerApi.h")
    forbid(
        api,
        "RoutePlannerApi.h",
        (
            '#include "src/game/navigation/traffic/',
            "CompiledTrafficRoute",
            "TrafficRouteGraph",
            "TrafficPortalDefinition",
            "PortalConnectionKind",
            "NavigationVolumeCatalog",
        ),
    )
    for token in (
        "struct RoutePlanRequest",
        "struct RoutePlan",
        "struct RouteStageSpan",
        "struct RouteFrameAnchor",
        "MandatoryTangentStraightConstraint",
    ):
        if token not in api:
            raise AssertionError(f"generic planner API lost {token!r}")

    adapter_h = read(
        "src/game/navigation/traffic/TrafficRoutePlanAdapter.h"
    )
    adapter_cpp = read(
        "src/game/navigation/traffic/TrafficRoutePlanAdapter.cpp"
    )
    for token in (
        "class TrafficRoutePlanAdapter final",
        "buildStageContract(",
        "CompiledTrafficRoute",
        "planner::RoutePlan",
    ):
        if token not in adapter_h and token not in adapter_cpp:
            raise AssertionError(f"traffic adapter missing {token!r}")

    # Follower/autopilot may consume only generic route contracts.
    autopilot_dir = ROOT / "src/game/navigation/autopilot"
    for path in autopilot_dir.glob("*"):
        if path.suffix not in (".h", ".cpp"):
            continue
        body = path.read_text(encoding="utf-8", errors="replace")
        forbid(
            body,
            str(path.relative_to(ROOT)),
            (
                '#include "src/game/navigation/traffic/',
                "TrafficRouteGraph",
                "CompiledTrafficRoute",
                "CompiledTrafficStage",
                "TrafficRouteStageKind",
                "PortalConnectionKind",
                "NavigationVolumeCatalog",
                "NavigationVolumePolicy",
                "plan.stages",
                "RouteStageKind::",
                ".volumeId",
                ".sourceId",
            ),
        )

    # The generic planner and its docking backend cannot reach upstream into
    # traffic semantics. Traffic is converted before this seam.
    for rel in (
        "src/game/navigation/planner/RoutePlanner.cpp",
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "src/game/navigation/DockingAdvisoryPlanner.h",
    ):
        body = read(rel)
        forbid(
            body,
            rel,
            (
                '#include "src/game/navigation/traffic/',
                "TrafficRouteGraph",
                "CompiledTrafficRoute",
                "CompiledTrafficStage",
                "TrafficRouteStageKind",
                "PortalConnectionKind",
                "NavigationVolumePolicy",
            ),
        )

    # SpaceState is composition only. Stage construction belongs to the
    # explicit adapter and must not silently migrate back into orchestration.
    space = read("src/game/SpaceState.cpp")
    if "TrafficRoutePlanAdapter::" not in space:
        raise AssertionError("SpaceState does not use traffic route adapter")
    forbid(
        space,
        "SpaceState.cpp",
        (
            "RouteStageSpan span;",
            "job->plan.stages.push_back",
            "appendStage(",
            "const auto routeProgressAt",
            "planner::RouteStageKind::VolumeEntryCapture",
            "planner::RouteStageKind::VolumeTransit",
            "planner::RouteStageKind::VolumeExit",
        ),
    )

    # The execution seam must remain an explicit value call, never a traffic
    # object pointer/reference threaded into the autopilot.
    if "ClientRouteAutopilot::start(" not in space:
        raise AssertionError("SpaceState lost explicit autopilot start seam")
    start_window = space[
        max(0, space.find("ClientRouteAutopilot::start(") - 1200):
        space.find("ClientRouteAutopilot::start(") + 2200
    ]
    forbid(
        start_window,
        "ClientRouteAutopilot::start call",
        (
            "compiledTrafficRoute",
            "trafficGraph",
            "navigationVolumes",
            "TrafficRoutePlanAdapter",
        ),
    )

    print("[PASS] navigation route module boundaries")
    print(" - traffic semantics terminate at TrafficRoutePlanAdapter")
    print(" - generic planner backend has no traffic dependency")
    print(" - autopilot/follower has no traffic or stage-semantic dependency")
    print(" - SpaceState is composition-only for route stage assembly")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
