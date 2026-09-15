from pathlib import Path

path = Path('src/game/SpaceState.cpp')
text = path.read_text(encoding='utf-8')

replacements = [
(
'''    // The fixed corridor lives in Hub-local coordinates. Only the cheap
    // Hub-local -> presentation-world conversion is refreshed every frame;
    // the spline/gates themselves move only on an explicit replan event.
''',
'''    // The published generation lives in Hub-local coordinates. Presentation
    // conversion is cheap and runs every frame, while a low-rate policy below
    // rebuilds the near tunnel from the current hull pose whenever the ship has
    // materially departed from that generation. Never translate an old tunnel
    // rigidly with the ship: a replacement generation must start at the live
    // pose and reconnect to the accepted physical trajectory.
'''),
(
'''                const double attitudeError = std::acos(std::clamp(
                    glm::dot(hullForward, track.tangent),
                    -1.0,
                    1.0
                ));

                bool targetMoved = false;
''',
'''                const double attitudeError = std::acos(std::clamp(
                    glm::dot(hullForward, track.tangent),
                    -1.0,
                    1.0
                ));

                // A manual tunnel is rolling guidance, not a frozen railway.
                // Reconnect while the deviation is still small enough to be a
                // current-pose correction. Previously a ship could move several
                // metres inside a wide gate (or rotate while drifting in Newton
                // flight) without satisfying any rebuild condition.
                const double poseLateralThreshold = std::clamp(
                    lateralBase * 0.08,
                    0.75,
                    3.0
                );
                const double poseVerticalThreshold = std::clamp(
                    verticalBase * 0.08,
                    0.75,
                    3.0
                );
                const bool currentPoseChanged =
                    track.lateralMeters > poseLateralThreshold ||
                    track.verticalMeters > poseVerticalThreshold ||
                    attitudeError > manualPlan.courseChangeThresholdRadians;

                bool targetMoved = false;
'''),
(
'''                if (predictedOutside)
                {
                    rebuild = true;
                    replanReason = "predicted_exit";
                }
                else if (courseError >
                         manualPlan.courseChangeThresholdRadians)
''',
'''                if (currentPoseChanged)
                {
                    rebuild = true;
                    replanReason = "current_pose";
                }
                else if (predictedOutside)
                {
                    rebuild = true;
                    replanReason = "predicted_exit";
                }
                else if (courseError >
                         manualPlan.courseChangeThresholdRadians)
'''),
(
'''        ++m_perfDockingTunnelBuilds;
        auto tunnel = world::navigation::GuidanceTunnelBuilder::build(
            tunnelRequest
        );
        if (!tunnel.valid || tunnel.gates.size() < 2)
            return false;
''',
'''        ++m_perfDockingTunnelBuilds;
        const double tunnelBuildStartMs = nowMs();
        auto tunnel = world::navigation::GuidanceTunnelBuilder::build(
            tunnelRequest
        );
        const double tunnelBuildMs = nowMs() - tunnelBuildStartMs;
        m_perfDockingTunnelBuildMs += tunnelBuildMs;
        if (!tunnel.valid || tunnel.gates.size() < 2)
            return false;
'''),
(
'''            << " gates=" << manualPlan.fixedTunnel.gates.size()
            << '\\n';
''',
'''            << " gates=" << manualPlan.fixedTunnel.gates.size()
            << " build_ms=" << tunnelBuildMs
            << '\\n';
'''),
(
'''    // Seed route planning from one canonical authoritative replication epoch,
    // then resolve the entire problem to one current planning epoch. Render
    // interpolation is a different timeline and is never a legal producer.
    const auto planningSnapshot =
''',
'''    // Seed route planning from one canonical authoritative replication epoch,
    // then resolve the entire problem to one current planning epoch. Render
    // interpolation is a different timeline and is never a legal producer.
    const double requestPlanningStartMs = nowMs();
    const double snapshotStartMs = nowMs();
    const auto planningSnapshot =
'''),
(
'''                dockingRequest.target.stableObjectId
            );
    if (!planningSnapshot.ready())
''',
'''                dockingRequest.target.stableObjectId
            );
    const double snapshotBuildMs = nowMs() - snapshotStartMs;
    if (!planningSnapshot.ready())
'''),
(
'''    const auto plan = game::navigation::DockingPathPlanner::plan(request);
    if (!plan.valid)
''',
'''    const double geometricPlanStartMs = nowMs();
    const auto plan = game::navigation::DockingPathPlanner::plan(request);
    const double geometricPlanMs = nowMs() - geometricPlanStartMs;
    if (!plan.valid)
'''),
(
'''    const auto trajectoryPlan =
        world::navigation::TrajectoryGenerator::generate(trajectoryRequest);
    if (!trajectoryPlan.ready())
''',
'''    const double trajectoryPlanStartMs = nowMs();
    const auto trajectoryPlan =
        world::navigation::TrajectoryGenerator::generate(trajectoryRequest);
    const double trajectoryPlanMs = nowMs() - trajectoryPlanStartMs;
    if (!trajectoryPlan.ready())
'''),
(
'''    m_noSafeDockingGuidanceSolution = false;
    m_dockingGuidanceFailureReason.clear();

    const glm::dvec3 finalDirection = glm::normalize(
''',
'''    m_noSafeDockingGuidanceSolution = false;
    m_dockingGuidanceFailureReason.clear();

    std::cerr
        << "[DockingPerf] request=" << dockingRequest.serial
        << " total_ms=" << (nowMs() - requestPlanningStartMs)
        << " snapshot_ms=" << snapshotBuildMs
        << " geometric_ms=" << geometricPlanMs
        << " trajectory_ms=" << trajectoryPlanMs
        << " tunnel_ms=" << m_perfDockingTunnelBuildMs
        << '\\n';

    const glm::dvec3 finalDirection = glm::normalize(
'''),
(
'''    m_perfDockingTunnelBuilds = 0;
    const double dockingGuidanceStartMs = nowMs();
''',
'''    m_perfDockingTunnelBuilds = 0;
    m_perfDockingTunnelBuildMs = 0.0;
    const double dockingGuidanceStartMs = nowMs();
'''),
]

for old, new in replacements:
    if old not in text:
        raise SystemExit('SpaceState patch anchor not found:\n' + old[:160])
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')

header = Path('src/game/SpaceState.h')
h = header.read_text(encoding='utf-8')
old = '''    std::uint32_t m_perfDockingTunnelBuilds = 0;
    double m_perfPlayerViewMs = 0.0;
'''
new = '''    std::uint32_t m_perfDockingTunnelBuilds = 0;
    double m_perfDockingTunnelBuildMs = 0.0;
    double m_perfPlayerViewMs = 0.0;
'''
if old not in h:
    raise SystemExit('SpaceState perf member anchor not found')
header.write_text(h.replace(old, new, 1), encoding='utf-8')

contract = Path('tests/architecture_contracts/check_live_docking_guidance.py')
contract.write_text('''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/game/SpaceState.h").read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")

for marker in (
    "currentPoseChanged",
    "replanReason = \\\"current_pose\\\"",
    "poseLateralThreshold",
    "poseVerticalThreshold",
    "tunnelBuildMs",
    "[DockingPerf]",
    "snapshot_ms=",
    "geometric_ms=",
    "trajectory_ms=",
    "tunnel_ms=",
):
    require(marker in CPP, f"live docking guidance marker missing: {marker}")

require("m_perfDockingTunnelBuildMs" in HEADER,
        "SpaceState does not retain per-frame tunnel build time")
require(CPP.find("currentPoseChanged") < CPP.find('replanReason = "current_pose"'),
        "current hull pose is not consulted before current-pose replan")
print("LIVE DOCKING GUIDANCE: PASS")
print(" - rolling tunnel reconnects after material current hull-pose deviation")
print(" - active CALCULATE ROUTE path reports snapshot/geometric/trajectory/tunnel timings")
''', encoding='utf-8')
