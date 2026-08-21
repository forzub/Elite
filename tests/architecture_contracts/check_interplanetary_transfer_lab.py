#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, message: str) -> None:
    if needle not in haystack:
        raise SystemExit(f"[FAIL] {message}: missing {needle!r}")


def main() -> int:
    lab = text("src/game/diagnostics/InterplanetaryTransferLab.h")
    scene = text("src/game/scene/GameSceneSetup.cpp")
    sim_h = text("src/game/simulation/GameSimulation.h")
    sim_cpp = text("src/game/simulation/GameSimulation.cpp")

    for needle in (
        "InterplanetaryInitialElapsedDays",
        "InterplanetarySunMuM3s2",
        "solveTransferEccentricAnomaly",
        "evaluateInterplanetaryTransferLab",
        "InterplanetaryEarthOrbitAu",
        "InterplanetaryMarsOrbitAu",
    ):
        require(lab, needle, "transfer trajectory contract")

    for needle in (
        "spawnInterplanetaryTransferLabNpc",
        "InterplanetaryTransferLabLabel",
        "registerInterplanetaryTransferLabShip",
    ):
        require(scene, needle, "scene bootstrap must materialize transfer NPC")

    require(sim_h, "updateInterplanetaryTransferLabActor", "simulation updater declaration")
    require(sim_cpp, "MotionMode::PassiveTrajectory", "transfer actor must publish passive trajectory semantics")
    require(sim_cpp, "updateInterplanetaryTransferLabActor();", "transfer actor must advance on production universe time")
    require(sim_cpp, "InterplanetaryTransferLabSunBodyId", "transfer actor must remain Sun-centered")

    print("[PASS] Mars->Earth interplanetary transfer lab architecture guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
