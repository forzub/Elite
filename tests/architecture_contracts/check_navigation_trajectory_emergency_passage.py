#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = ROOT / "src/world/navigation/trajectory"
TESTS = ROOT / "tests/navigation_trajectory"

HEADER = (TRAJECTORY / "EmergencyPassageMitigator.h").read_text(encoding="utf-8")
SOURCE = (TRAJECTORY / "EmergencyPassageMitigator.cpp").read_text(encoding="utf-8")
TRAJECTORY_CMAKE = (TRAJECTORY / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_SOURCE = (TESTS / "NavigationTrajectoryEmergencyPassageTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (TESTS / "CMakeLists.txt").read_text(encoding="utf-8")
MODEL = (ROOT / "src/world/navigation/ORIENTED_PASSAGE_MODEL.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class EmergencyPassageMitigator",
    "kOrientationSamples = 17",
    "EmergencyStopBeforeEntry",
    "EmergencyMitigatedContact",
    "commandValid",
    "collisionFreeEntryPoseProven",
    "contactExpected",
    "maximumBrakingRecommended",
    "recommendedAimPointMapMeters",
    "desiredTravelDirectionMap",
    "clearanceDeficitMeters",
):
    require(marker in HEADER, f"emergency passage boundary marker missing: {marker}")

for forbidden in (
    "#include <glm",
    "#include <GL/",
    "#include <GLFW",
    "EliteGame",
    "EliteServer",
    "Renderer",
    "NavigationMap.h",
    "NavigationSpace.h",
):
    require(forbidden not in HEADER + SOURCE,
            f"emergency passage mitigation must stay backend-neutral: {forbidden}")

for marker in (
    "timeToEntryWithMaxBraking",
    "maxRestToRestAngle",
    "kOrientationSamples",
    "betterSample",
    "passage.passageToMap.forward",
    "query.passage.centerMapMeters",
    "Status::EmergencyMitigatedContact",
    "result.contactExpected = true",
    "Status::EmergencyStopBeforeEntry",
):
    require(marker in SOURCE, f"emergency passage implementation marker missing: {marker}")

for marker in (
    "testEnoughRoomProducesSafeReachableEntryPose",
    "testPartialReachableRollCanFitEvenWhenPreferredRollIsTooLate",
    "testUnreachableSafeRollStillProducesMitigatedContactCommand",
    "testEmergencyIntentTargetsGapCenterAndTravelAxis",
    "testIfShipCanStopItDoesNotIntentionallyCrash",
    "testZeroAngularAuthorityStillKeepsEmergencyCommandAlive",
):
    require(marker in TEST_SOURCE, f"emergency passage fixture missing: {marker}")

require("EmergencyPassageMitigator.cpp" in TRAJECTORY_CMAKE,
        "trajectory library must compile emergency passage mitigator")
require("navigation_trajectory_emergency_passage_tests" in TEST_CMAKE,
        "emergency passage test executable missing")

for marker in (
    "EmergencyMitigatedContact",
    "collision-free proof",
    "contact",
    "ricochet",
    "passage axis",
    "gap center",
):
    require(marker in MODEL, f"emergency passage architecture marker missing: {marker}")

print("NAVIGATION TRAJECTORY EMERGENCY PASSAGE CONTRACT: PASS")
print(" - failure to prove collision-free entry does not disable navigation intent")
print(" - stopping before entry is preferred when physically possible")
print(" - unavoidable contact remains explicitly non-safe and is handed to physics/damage")
print(" - fixed-size reachable-attitude sampling minimizes entry geometry deficit")
print(" - emergency intent aims at gap center and along the passage axis")
print(" - braking is retained as impact-energy mitigation rather than planner shutdown")
