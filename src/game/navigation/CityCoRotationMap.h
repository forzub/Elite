#pragma once

// THE CITY'S CO-ROTATING MAP (author 2026-11-25, the ring pad's split: «... держит ее вектор - это 1-я задача автопилота
// - как константу и простраивает маршрут уже относительно неподвижной платформы»). A route from a ring's sync gate to a
// rim pad is planned against the city FROZEN at the planning epoch t0 (its obstacles posed by the hub-attachment law at
// t0 - ClientWorldState / HubAttachedPose.h). While it is flown, the ship's state is carried back into that frozen map:
// the city's turn since t0 undone about its own attachment pivot - position, velocity RELATIVE to the turning city,
// hull axes, and the body rates minus the city's turn. The route's pilot then sees a still city; the server charges
// the turning frame's cost (ShipControlState::deckCoRotating). Tactical hub-local coordinates throughout.

#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"

namespace game::navigation
{

// the attached module's rotation at universe time t, as tactical hub-local axes (columns)
inline glm::dmat3 attachmentRotationTactical(const simulation::HubAttachmentSnapshot& a, double t)
{
    const glm::dvec3 angles = a.localRotationDeg + a.localAngularVelocityDegPerSecond * t;
    const glm::dmat3 rv(glm::mat3(hubLocalEulerDegToMatrix(angles)));
    glm::dmat3 out;
    for (int i = 0; i < 3; ++i)
        out[i] = hubVisualToTacticalVector(rv * glm::dvec3(i == 0, i == 1, i == 2));
    return out;
}

struct CityCoRotationAt
{
    glm::dvec3 pivot {0.0};    // the module's attachment point (on its spin axis), tactical local
    glm::dmat3 toMap {1.0};    // local axes now -> the frozen map's axes (the city's turn since t0 undone)
    glm::dvec3 omega {0.0};    // the city's turn, tactical local axes, rad/s

    // THE ONE CO-ROTATION RULE (stabilization stage 5, A2): the turning city's own velocity at a tactical local point,
    // and a velocity relative to it - about the attachment PIVOT (the spin axis), never the hub origin
    [[nodiscard]] glm::dvec3 cityVelocityAt(const glm::dvec3& localPoint) const { return glm::cross(omega, localPoint - pivot); }
    [[nodiscard]] glm::dvec3 relativeVelocity(const glm::dvec3& localPoint, const glm::dvec3& localVelocity) const
    {
        return localVelocity - cityVelocityAt(localPoint);
    }
    // a point / a relative velocity carried into the frozen map
    [[nodiscard]] glm::dvec3 pointToMap(const glm::dvec3& localPoint) const { return pivot + toMap * (localPoint - pivot); }
    [[nodiscard]] glm::dvec3 vectorToMap(const glm::dvec3& v) const { return toMap * v; }
};

inline CityCoRotationAt cityCoRotationAt(const simulation::HubAttachmentSnapshot& a, double t0, double t)
{
    CityCoRotationAt out;
    out.pivot = hubVisualToTacticalVector(a.localOffsetMeters);
    const glm::dmat3 B0 = attachmentRotationTactical(a, t0);
    const glm::dmat3 Bt = attachmentRotationTactical(a, t);
    out.toMap = B0 * glm::transpose(Bt);
    // omega from the attachment law's own derivative (dB/dt B^T is the skew matrix of omega)
    constexpr double h = 0.01;
    const glm::dmat3 dR = attachmentRotationTactical(a, t + h) * glm::transpose(Bt);
    out.omega = glm::dvec3(dR[1][2] - dR[2][1], dR[2][0] - dR[0][2], dR[0][1] - dR[1][0]) / (2.0 * h);
    return out;
}

// the attachment frozen at t0 (its turn stopped there): a port / pad resolved from it stays where it was at t0
inline simulation::HubAttachmentSnapshot frozenAttachment(simulation::HubAttachmentSnapshot a, double t0)
{
    a.localRotationDeg += a.localAngularVelocityDegPerSecond * t0;
    a.localAngularVelocityDegPerSecond = glm::dvec3(0.0);
    return a;
}

} // namespace game::navigation
