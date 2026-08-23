#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        raise AssertionError(f"missing {path}")
    return p.read_text(encoding="utf-8")


def require(path: str, *tokens: str) -> str:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")
    return body


def main() -> int:
    require(
        "src/game/navigation/NavigationAssetRef.h",
        "enum class NavigationAssetKind",
        "ShipInstanceId shipInstanceId",
        "DroneInstanceId droneInstanceId",
        "sameNavigationAsset",
    )
    require(
        "src/game/navigation/RoutePlan.h",
        "struct NavigationRouteStart",
        "NavigationAssetRef executor",
        "setStartExecutor",
        "NavigationRouteStart m_start",
    )
    workspace = require(
        "src/game/navigation/ClientNavigationWorkspace.h",
        "OwnedNavigationAssetState m_ownedAssets",
        "syncOwnedAssets",
        "selectRouteExecutor",
        "localControlledAsset",
    )
    if "EntityId" in text("src/game/navigation/RoutePlan.h"):
        raise AssertionError("RoutePlan START must not store transient EntityId")

    require(
        "src/game/simulation/ClientSessionSnapshot.h",
        "ownedNavigationAssets",
        "OwnedNavigationAsset",
    )
    server = require(
        "src/game/server/GameServer.cpp",
        "ownedNavigationAssetsForSession",
        "m_shipOwnership.all()",
        "ShipOwnerKind::Player",
        "ownership.owner.playerId != playerId",
        "asset.commandable = true",
    )
    if "m_controls" in server[server.index("ownedNavigationAssetsForSession"):server.index("const SimulationSnapshot& GameServer::snapshot")]:
        raise AssertionError("legal ownership projection must not be derived from current ControlRegistry")

    require(
        "src/game/presentation/NavigationHudPresentation.h",
        'marker.stableId = "route:start"',
        "sameNavigationAsset(",
        "navigation.localControlledAsset()",
        "navigation.ownedAssets().find",
    )
    require(
        "src/game/system_map/NavigationRouteOverlay.cpp",
        "START is a fixed semantic node",
        "shortAssetName(routePlan.start().executor)",
    )

    if (ROOT / "src/game/navigation/NavigationPlan.h").exists():
        raise AssertionError("dead pre-route NavigationPlan must stay removed")
    dynamic = text("src/game/navigation/DynamicMotionState.h")
    if "NavigationPlan" in dynamic or "navigationPlan" in dynamic:
        raise AssertionError("DynamicMotionState regained dead NavigationPlan")

    print("[PASS] explicit route START + server-owned execution asset contract")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
