#pragma once

#include <array>
#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "src/game/docking/model/DockFacilityDescriptor.h"

// Rotating hexagonal dock + its fixed service block: authored semantic
// geometry (meters). The rendered meshes / collision boxes are generated from
// these same numbers, traffic and landing consume only this description.
//
// Frames (both centred on the dock centre):
//   FIXED    - the station-fixed dock frame (the dock's base rotation without
//              spin). Hold points, gates and approach lanes live here; the
//              service block does not rotate.
//   ROTATING - the dock body frame = FIXED rotated about +Z by the live dock
//              phase. Portal slot, chamber and pads live here.
// +Z points INTO the dock (entry face at -Z, chamber deeper at +Z, service
// block behind the back wall). Ship body basis everywhere: +X right, +Y up,
// -Z nose; an orientation matrix holds (right, up, back) as columns.
namespace game::docking::hex
{

inline constexpr double Pi = 3.14159265358979323846;

struct HexDockGeometry
{
    // Rotating body: regular hexagonal prism, flats top/bottom (vertices at
    // 0, 60, ... deg), wall normals at 30 + 60 k deg.
    double outerApothem = 150.0;
    double innerApothem = 120.0;
    double entryFaceZ = -200.0;   // outer front face (slot opening)
    double chamberFrontZ = -100.0; // end of the entry corridor = chamber gate
    double chamberBackZ = 180.0;
    double backFaceZ = 200.0;
    double slotWidth = 160.0;      // along rotating X
    double slotHeight = 60.0;      // along rotating Y
    double backPortHalf = 20.0;    // square service port into the block tunnel
    // Pads: one per inner wall, centred on the wall, along the axis.
    double padCenterZ = 60.0;
    double padLength = 70.0;
    double padWidth = 56.0;
    double padThickness = 2.0;
    // Spin. 2 deg/s is the rate the docking autopilot's roll sync is proven
    // on; the resulting centrifugal "down" (~0.015 g at the pads) only
    // defines the local vertical, the dock's gravity is not modelled.
    double spinDegPerSecond = 2.0;

    // Fixed service block (not rotating), coaxial behind the dock.
    double serviceGap = 10.0;
    double serviceHalfLength = 300.0;
    double serviceHalfSize = 120.0;  // square cross-section 240 x 240
    double serviceBoreHalf = 40.0;   // through tunnel 80 x 80

    // Traffic layout in the FIXED frame.
    double holdZ = -2600.0;       // hold "flower" plane: beyond the pilot planner's ~900 m terminal axis (dk6: at -1600 the route looped out)
    double holdRadius = 450.0;    // 6 holds, 60 deg apart -> 450 m chord
    double axisJoinZ = -1000.0;   // arrivals join the axis here
    double gateZ = -8000.0;       // queue waiting gates (one per ship)
    double gateRadius = 2000.0;
    double departureClearZ = -2400.0;  // departures release the outer lane
    double departureTurnZ = -11000.0;  // and leave the axis only here (beyond every queue gate)

    [[nodiscard]] double serviceCenterZ() const noexcept
    {
        return backFaceZ + serviceGap + serviceHalfLength;
    }
    [[nodiscard]] double spinRadPerSecond() const noexcept
    {
        return spinDegPerSecond * Pi / 180.0;
    }
};

[[nodiscard]] inline const HexDockGeometry& hexDockGeometry()
{
    static const HexDockGeometry geometry {};
    return geometry;
}

inline constexpr int PadCount = 6;
inline constexpr int HoldCount = 6;

// Outward unit normal of inner wall k (pad k sits on it), rotating frame.
[[nodiscard]] inline glm::dvec3 wallNormal(int k)
{
    const double a = (30.0 + 60.0 * k) * Pi / 180.0;
    return {std::cos(a), std::sin(a), 0.0};
}

// Hold azimuths sit between pads (60 k deg); departures leave the axis in the
// sectors half way between holds (60 k + 30 deg).
[[nodiscard]] inline double holdAzimuthRad(int k)
{
    return (60.0 * k) * Pi / 180.0;
}

[[nodiscard]] inline glm::dvec3 holdPosition(const HexDockGeometry& g, int k)
{
    const double a = holdAzimuthRad(k);
    return {g.holdRadius * std::cos(a), g.holdRadius * std::sin(a), g.holdZ};
}

[[nodiscard]] inline glm::dmat3 rotationAboutZ(double angleRad)
{
    const double c = std::cos(angleRad), s = std::sin(angleRad);
    return glm::dmat3(glm::dvec3(c, s, 0.0), glm::dvec3(-s, c, 0.0), glm::dvec3(0.0, 0.0, 1.0));
}

// Ship orientation (columns right, up, back) for a nose direction and an up hint.
[[nodiscard]] inline glm::dmat3 shipBasis(const glm::dvec3& nose, const glm::dvec3& upHint)
{
    const glm::dvec3 back = -glm::normalize(nose);
    glm::dvec3 right = glm::cross(upHint, back);
    if (glm::dot(right, right) < 1.0e-12)
        right = glm::cross(glm::dvec3(0.0, 1.0, 0.0), back);
    if (glm::dot(right, right) < 1.0e-12)
        right = glm::cross(glm::dvec3(1.0, 0.0, 0.0), back);
    right = glm::normalize(right);
    const glm::dvec3 up = glm::cross(back, right);
    return glm::dmat3(right, up, back);
}

// Landed pose on pad k (ROTATING frame): down = wall normal, nose to the exit.
[[nodiscard]] inline glm::dvec3 padLandedCenter(const HexDockGeometry& g, int k, double shipHalfHeight)
{
    const glm::dvec3 n = wallNormal(k);
    const double standOff = g.padThickness + shipHalfHeight + 1.0;
    return n * (g.innerApothem - standOff) + glm::dvec3(0.0, 0.0, g.padCenterZ);
}

[[nodiscard]] inline glm::dmat3 padLandedBasis(int k)
{
    return shipBasis(glm::dvec3(0.0, 0.0, -1.0), -wallNormal(k));
}

inline const char* padIdFor(int k)
{
    static const std::array<const char*, PadCount> ids {"P1", "P2", "P3", "P4", "P5", "P6"};
    return ids[static_cast<std::size_t>(k)];
}

inline const char* holdIdFor(int k)
{
    static const std::array<const char*, HoldCount> ids {"H1", "H2", "H3", "H4", "H5", "H6"};
    return ids[static_cast<std::size_t>(k)];
}

inline int indexOfId(const std::string& id)
{
    // "P3" / "H5" -> 2 / 4; -1 otherwise.
    if (id.size() == 2 && (id[0] == 'P' || id[0] == 'H') && id[1] >= '1' && id[1] <= '6')
        return id[1] - '1';
    return -1;
}

// Shared traffic resources (conflict zones). Conservative policy: every
// movement through the portal / axis / chamber holds both.
inline constexpr const char* OuterLaneResource = "outer_lane";   // hold->axis->portal->corridor, departure axis
inline constexpr const char* ChamberLaneResource = "chamber_lane"; // free movement inside the chamber

[[nodiscard]] inline DockFacilityDescriptor hexDockFacilityDescriptor(
    const std::string& facilityId,
    int systemId,
    const std::string& kinematicFrameId)
{
    const HexDockGeometry& g = hexDockGeometry();
    DockFacilityDescriptor facility;
    facility.facilityId = facilityId;
    facility.systemId = systemId;
    facility.kinematicFrameId = kinematicFrameId;
    facility.kind = DockKind::RotatingDrum;

    AccessPortalDescriptor portal;
    portal.id = "portal_front";
    portal.sizeClass = DockSizeClass::Medium;
    portal.usableWidthMeters = g.slotWidth;
    portal.usableHeightMeters = g.slotHeight;
    portal.centerLocalMeters = {0.0, 0.0, g.entryFaceZ};
    portal.outwardLocal = {0.0, 0.0, -1.0};
    portal.conflictResourceIds = {OuterLaneResource, ChamberLaneResource};
    facility.portals.push_back(portal);

    for (int k = 0; k < HoldCount; ++k)
    {
        EntryHoldPointDescriptor hold;
        hold.id = holdIdFor(k);
        hold.portalId = portal.id;
        hold.positionLocalMeters = holdPosition(g, k);
        hold.forwardLocal = {0.0, 0.0, 1.0};
        facility.holdPoints.push_back(hold);
    }
    facility.conflictZones.push_back({OuterLaneResource});
    facility.conflictZones.push_back({ChamberLaneResource});

    for (int k = 0; k < PadCount; ++k)
    {
        ParkingPadDescriptor pad;
        pad.id = padIdFor(k);
        pad.sizeClass = DockSizeClass::Medium;
        pad.usableWidthMeters = g.padWidth;
        pad.usableLengthMeters = g.padLength;
        pad.verticalClearanceMeters = 60.0;
        pad.maxMassKg = 2.0e6;
        pad.surfaceCenterLocalMeters = wallNormal(k) * (g.innerApothem - g.padThickness) + glm::dvec3(0.0, 0.0, g.padCenterZ);
        pad.surfaceNormalLocal = -wallNormal(k);
        pad.approachForwardLocal = {0.0, 0.0, -1.0};
        pad.conflictResourceIds = {ChamberLaneResource};
        facility.pads.push_back(pad);
    }
    InternalLaneDescriptor chamber;
    chamber.id = "chamber";
    chamber.fromAnchorId = "chamber_gate";
    chamber.toAnchorId = "pads";
    chamber.conflictResourceIds = {ChamberLaneResource};
    facility.internalLanes.push_back(chamber);
    return facility;
}

} // namespace game::docking::hex
