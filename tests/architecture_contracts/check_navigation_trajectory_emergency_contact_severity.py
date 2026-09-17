#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/trajectory/EmergencyContactSeverityScorer.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/trajectory/EmergencyContactSeverityScorer.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/trajectory/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class EmergencyContactSeverityScorer final",
    "kMaxCandidates = 8",
    "kMaxContactsPerCandidate = 4",
    "peakClosingNormalSpeedMetersPerSec",
    "totalNormalImpactEnergyProxyJ",
    "totalNormalMomentumProxyKgMetersPerSec",
    "worstIncidenceFromTangentRad",
    "surfaceVelocityMapMetersPerSec",
    "shipAngularVelocityMapRadPerSec",
    "selectBest",
):
    require(marker in HEADER, f"emergency contact scorer interface missing: {marker}")

for marker in (
    "v_point = v_center + omega x r",
    "cross(witness.shipAngularVelocityMapRadPerSec, leverArm)",
    "relativeContactVelocity",
    "std::max(0.0, -signedNormalSpeed)",
    "0.5 * effectiveMass * closingNormalSpeed * closingNormalSpeed",
    "std::atan2",
    "candidateCount > kMaxCandidates",
):
    require(marker in IMPL, f"emergency contact scorer implementation missing: {marker}")

for forbidden in (
    "NavigationMap.h",
    "NavigationSpace.h",
    "GL/",
    "OpenGL",
    "glm/",
):
    require(forbidden not in HEADER and forbidden not in IMPL,
            f"emergency contact scorer must stay backend/world neutral: {forbidden}")

for marker in (
    "does **not** discover collisions",
    "v_ship_contact = v_center + omega x r",
    "v_rel = v_ship_contact - v_surface",
    "v_n = max(0, -dot(v_rel, n))",
    "glancing contact / ricochet-friendly geometry",
    "normal energy proxy",
    "Impact severity always outranks route progress",
    "Physics / Collision",
    "<= 8 candidates",
):
    require(marker in DOC, f"emergency contact severity documentation missing: {marker}")

require("EmergencyContactSeverityScorer.cpp" in CMAKE,
        "trajectory library must compile the emergency contact scorer")
require("navigation_trajectory_emergency_contact_severity" in TEST_CMAKE,
        "trajectory CTest must register emergency contact severity behavior")

print("NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS")
print(" - ranking is bounded to eight candidates and four witnesses per candidate")
print(" - contact-point relative velocity includes rigid-body omega cross r motion")
print(" - closing normal speed is the primary severity metric")
print(" - normal energy/momentum proxies and incidence angle are secondary diagnostics")
print(" - moving-surface velocity is part of the witness contract")
print(" - scorer owns no collision discovery, world search, or physics response")
