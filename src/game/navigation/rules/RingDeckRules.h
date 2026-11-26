#pragma once

// THE RING'S DISPATCHER RULE (author 2026-11-25: «эта информация должна выдаваться игроку через диспетчера»; memory
// ring-rim-landing). A pad on a TURNING ring's rim: the deck's weight there is omega^2 r toward the rim and the deck
// moves at omega r; a ship lands there only if its vertical (lift) thrust holds that weight with the planet margin
// (x1.2), nose along the spin. Not enough lift: refused - the axial dock (on the axis the deck does not move).
// One source for both sides: the client's dispatcher answers at the request (before the route), the server decides at
// the hand-over. Everything from the hub plan (the ring's radius and period - the spin the scene gives the city).

#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/HubRingBend.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldLaws.h"

namespace game::navigation::rules
{

inline constexpr double RimLiftMargin = 1.2;   // the lift a rim landing needs over the deck's weight (as for a planet)

struct RimPadBriefing
{
    bool rim = false;   // a pad of a turning ring (false: an ordinary pad - no briefing)
    double weightG = 0.0;
    double deckMps = 0.0;
    double liftG = 0.0;
    double needG = 0.0;
    double spinDegPerS = 0.0;
    bool cleared() const { return !rim || liftG >= needG; }
};

// padStrip: the pad in the plan's strip coordinates (y up from the deck - toward the axis); liftMps2: the ship's
// vertical thrust with its damage (the stabilization's authority - as SurfaceLandingPilot's lift)
inline RimPadBriefing rimPadBriefing(const HubRingBend& ring, const glm::dvec3& padStrip, double liftMps2)
{
    RimPadBriefing b;
    if (!ring.valid || ring.periodSeconds <= 0.0)
        return b;
    const double g = world::laws::StandardGravityMps2;
    const double rate = 2.0 * 3.14159265358979323846 / ring.periodSeconds;
    const double r = std::max(0.0, ring.radius - padStrip.y);
    b.rim = true;
    b.weightG = rate * rate * r / g;
    b.deckMps = rate * r;
    b.liftG = liftMps2 / g;
    b.needG = RimLiftMargin * b.weightG;
    b.spinDegPerS = rate * 180.0 / 3.14159265358979323846;
    return b;
}

// the ship's LIFT over a rim (nose along the spin, the deck's weight on its vertical axis): an Assisted ship holds it by
// its stabilization (the deck's charge goes there - ShipControlExecutionGateway); a NEWTONIAN ship's installed nozzles
// fire it (ShipCorePropulsion, audit 2026-11-26) - their real body-up authority, with damage and lost clusters. One
// source for the client's dispatcher and the server's.
inline double rimLiftMps2(const ShipParams& caps, bool newtonianNozzles)
{
    return newtonianNozzles ? game::ship::newtonianRcsDirectionAccelerationMps2(caps, 1, 1.0)
                            : game::ship::assistedLateralStabilizationAccelerationLimitMps2(caps);
}

// the dispatcher's notice: "pad:rim:<addr>:<g>:<m/s>:<lift g>:<need g>:<deg/s>" (cleared) or "refused:rim:..."
inline std::string rimPadNotice(const RimPadBriefing& b, const std::string& address)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s:%.2f:%.0f:%.2f:%.2f:%.1f", address.c_str(), b.weightG, b.deckMps, b.liftG, b.needG,
                  b.spinDegPerS);
    return std::string(b.cleared() ? "pad:rim:" : "refused:rim:") + buf;
}

} // namespace game::navigation::rules
