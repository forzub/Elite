#include "src/game/navigation/DockingCorridorPresentation.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "src/game/navigation/DockingAdvisoryCorridor.h"
#include "src/game/navigation/RouteFrameField.h"

namespace game::navigation
{

namespace
{
std::uint64_t mix(std::uint64_t h, double v)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof bits);
    h ^= bits + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}

std::uint64_t mix(std::uint64_t h, const glm::dvec3& v)
{
    return mix(mix(mix(h, v.x), v.y), v.z);
}
} // namespace

std::uint64_t dockingCorridorPlanKey(std::uint64_t serial, const DockingCorridorPresentation::RouteInput& in)
{
    std::uint64_t h = 1469598103934665603ull ^ serial;
    h = mix(h, in.routeUpReference);
    h = mix(mix(mix(mix(h, in.lateralToleranceMeters), in.verticalToleranceMeters), in.shipWidthMeters),
            in.shipHeightMeters);
    if (in.gates)
        for (const auto& g : *in.gates)
            h = mix(mix(mix(h, g.positionMeters), g.forward), g.speedMps);
    if (in.frameUp)
        for (const auto& u : *in.frameUp)
            h = mix(h, u);
    if (in.dockPhaseWeights)
        for (const double w : *in.dockPhaseWeights)
            h = mix(h, w);
    return h == 0 ? 1 : h;
}

void DockingCorridorPresentation::prepare(const RouteInput& in)
{
    m_gates.clear();
    m_localPositions.clear();
    if (!in.gates || in.gates->empty())
        return;
    const auto& gates = *in.gates;
    m_gates.reserve(gates.size());
    m_localPositions.reserve(gates.size());
    for (std::size_t index = 0; index < gates.size(); ++index)
    {
        const auto& gate = gates[index];
        Gate g;
        g.localPosition = gate.positionMeters;
        m_localPositions.push_back(gate.positionMeters);

        glm::dvec3 forward = gate.forward;
        if (glm::length(forward) <= 1.0e-9)
            forward = glm::dvec3(0.0, 0.0, -1.0);
        g.localForward = glm::normalize(forward);

        const glm::dvec3 frameUp = in.frameUp && index < in.frameUp->size() ? (*in.frameUp)[index] : in.routeUpReference;
        // Orthonormal local up before the dock roll (the same construction
        // as RouteFrameField::rotateUpAroundForward at phase 0).
        const glm::dvec3 up = RouteFrameField::rotateUpAroundForward(g.localForward, frameUp, 0.0);
        const glm::dvec3 right = glm::normalize(glm::cross(g.localForward, up));
        const glm::dvec3 orthoUp = glm::normalize(glm::cross(right, g.localForward));
        g.localOrientation = glm::normalize(glm::quat_cast(glm::dmat3(right, orthoUp, -g.localForward)));

        g.dockPhaseWeight = in.dockPhaseWeights && index < in.dockPhaseWeights->size()
            ? std::clamp((*in.dockPhaseWeights)[index], 0.0, 1.0)
            : 1.0;

        const double remainingMeters = glm::length(gates.back().positionMeters - gate.positionMeters);
        const auto section =
            dockingAdvisoryCrossSection(remainingMeters, in.lateralToleranceMeters, in.verticalToleranceMeters);
        g.widthMeters = dockingAdvisoryFrameExtentMeters(in.shipWidthMeters, section.lateralToleranceMeters);
        g.heightMeters = dockingAdvisoryFrameExtentMeters(in.shipHeightMeters, section.verticalToleranceMeters);
        g.lateralToleranceMeters = section.lateralToleranceMeters;
        g.verticalToleranceMeters = section.verticalToleranceMeters;
        g.recommendedSpeedMps = std::max(0.0, gate.speedMps);
        g.requiredVehiclePose = index + 1 == gates.size();
        m_gates.push_back(g);
    }
}

void DockingCorridorPresentation::present(
    GuidanceCorridor& route,
    const RouteInput& input,
    std::uint64_t planKey,
    const KinematicFrame& renderFrame,
    double renderTimeSeconds,
    double dockRollPhaseRad)
{
    if (planKey != m_planKey)
    {
        prepare(input);
        m_planKey = planKey;
    }

    route.hubLocalGatePositionsMeters = m_localPositions;
    route.frames.clear();
    route.frames.reserve(m_gates.size());

    const glm::dquat hubRotation = glm::normalize(glm::quat_cast(renderFrame.localToWorldBasis));
    for (const auto& g : m_gates)
    {
        GuidanceFrame frame;
        frame.universeTimeSeconds = renderTimeSeconds;
        frame.centerMeters = renderFrame.localToWorldPosition(g.localPosition);
        // Dynamic dock roll belongs only to the terminal-owned part of the
        // route (weight 1); traffic-volume stages keep their own frame
        // (weight 0); transition spans blend.
        const double roll = dockRollPhaseRad * g.dockPhaseWeight;
        const glm::dquat local = roll != 0.0 ? glm::angleAxis(roll, g.localForward) * g.localOrientation
                                             : g.localOrientation;
        frame.orientation = glm::normalize(hubRotation * local);
        frame.widthMeters = g.widthMeters;
        frame.heightMeters = g.heightMeters;
        frame.lateralToleranceMeters = g.lateralToleranceMeters;
        frame.verticalToleranceMeters = g.verticalToleranceMeters;
        frame.recommendedSpeedMps = g.recommendedSpeedMps;
        frame.requiredVehiclePose = g.requiredVehiclePose;
        route.frames.push_back(frame);
    }
}

} // namespace game::navigation
