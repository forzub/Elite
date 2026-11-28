#pragma once

// THE state the client's CONTROL logic reads (core stabilization D-5; PROJECT_STATE.md E-10).
//
// The SIMULATION layer only, never the presentation (renderTransform, renderWorldPosition, renderOrientation, smoothing,
// attitude layers): what a command or a decision is based on must not change with how the frame is drawn.
//
// Epochs, stated once here:
// - the controlled ship's `transform` is the latest snapshot predicted ownPredictedAheadSeconds() past it, IN its
//   reference frame. The frame itself stays at the snapshot epoch: its world pose is frame(snapshot).localToWorld(
//   local predicted) - CORE_AUDIT R-4, fixed in step D-3;
// - other ships and objects are at the snapshot.
//
// To combine them, every other body is advanced by the same horizon RELATIVE TO THAT SAME FRAME: its local position
// along its local velocity, its axes turned by its angular velocity relative to the frame, then expressed through the
// frame at the snapshot epoch - exactly as the own ship is. The epoch difference is known and accounted for in this one
// place; D-3 changes only this file when the frame is predicted too.
//
// Not covered: other ships' attitude is not advanced (their body rates are not world rates); control code here uses
// their position and velocity only.

#include <cmath>

#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::client::control
{

struct BodyState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dmat3 orientation {1.0};                       // columns: the body's axes in world coordinates
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
};

// the own epoch: the frame the controlled ship is predicted in and how far
struct View
{
    game::navigation::KinematicFrame frame;   // valid only while the own ship is predicted in it
    double aheadSeconds = 0.0;
};

inline View viewOf(const ClientWorldState& world, const ClientShipState& own)
{
    View v;
    v.aheadSeconds = world.ownPredictedAheadSeconds();
    if (own.referenceFrame.valid && v.aheadSeconds > 0.0)
        v.frame = own.referenceFrame.kinematicFrame();
    else
        v.aheadSeconds = 0.0;
    return v;
}

// R turned by an angular velocity (world axes) over dt (Rodrigues)
inline glm::dmat3 turnedBy(const glm::dmat3& R, const glm::dvec3& omegaWorld, double dt)
{
    const double rate = glm::length(omegaWorld);
    const double angle = rate * dt;
    if (!(rate > 1.0e-12) || !std::isfinite(angle) || angle == 0.0)
        return R;
    const glm::dvec3 k = omegaWorld / rate;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const glm::dmat3 K(glm::dvec3(0.0, k.z, -k.y), glm::dvec3(-k.z, 0.0, k.x), glm::dvec3(k.y, -k.x, 0.0));
    const glm::dmat3 rot = glm::dmat3(1.0) + s * K + (1.0 - c) * (K * K);
    return rot * R;
}

// a body's snapshot state carried to the own epoch (see the header)
inline BodyState atOwnEpoch(const View& view, BodyState body)
{
    if (!(view.aheadSeconds > 0.0) || !view.frame.valid)
        return body;
    const auto& f = view.frame;
    const glm::dvec3 local = f.worldToLocalPosition(body.positionMeters);
    const glm::dvec3 localVelocity = f.worldToLocalVelocity(body.positionMeters, body.velocityMps);
    body.positionMeters = f.localToWorldPosition(local + localVelocity * view.aheadSeconds);
    body.orientation =
        turnedBy(body.orientation, body.angularVelocityWorldRadPerSecond - f.angularVelocityWorldRadPerSecond,
                 view.aheadSeconds);
    return body;
}

// the controlled ship: its simulation state as it stands (already at the own epoch)
inline BodyState ownShip(const ClientShipState& ship)
{
    BodyState b;
    b.positionMeters = world::coordinates::fullMeters(ship.transform.worldPosition);
    b.velocityMps = ship.transform.motion.worldVelocityMps;
    b.orientation = glm::dmat3(glm::mat3(ship.transform.orientation));
    return b;
}

// another ship at the own epoch
inline BodyState otherShip(const View& view, const ClientShipState& ship)
{
    BodyState b;
    b.positionMeters = world::coordinates::fullMeters(ship.transform.worldPosition);
    b.velocityMps = ship.transform.motion.worldVelocityMps;
    b.orientation = glm::dmat3(glm::mat3(ship.transform.orientation));
    return atOwnEpoch(view, b);
}

// an object (a hub city, a module) at the own epoch, moved and turned rigidly
inline BodyState object(const View& view, const ClientObjectState& o)
{
    BodyState b;
    b.positionMeters = world::coordinates::fullMeters(o.worldPosition);
    b.velocityMps = o.linearVelocityMps;
    b.orientation = glm::dmat3(glm::mat3(o.orientation));
    b.angularVelocityWorldRadPerSecond = o.angularVelocityWorldRadPerSecond;
    return atOwnEpoch(view, b);
}

// a world point in a body's own axes, relative to its origin
inline glm::dvec3 inBodyAxes(const BodyState& body, const glm::dvec3& worldPoint)
{
    return glm::transpose(body.orientation) * (worldPoint - body.positionMeters);
}

} // namespace game::client::control
