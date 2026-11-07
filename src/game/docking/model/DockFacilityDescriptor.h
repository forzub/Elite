#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::docking
{

enum class DockSizeClass : std::uint8_t
{
    Unknown = 0,
    Small,
    Medium,
    Large
};

// What a dock physically is (author 2026-11-07). Dispatcher and autopilot treat
// every kind as one facility; the kind only selects the approach and landing.
enum class DockKind : std::uint8_t
{
    RotatingDrum = 0,   // spun chamber with pads on the walls (artificial gravity); roll sync at the
                        // entrance; Assisted ships (they manoeuvre easily); hold points in front
    TrackTerminal,      // non-rotating "railway station": parallel tracks, one per arriving ship, no
                        // hold points; for Newtonian ships (less manoeuvring)
    SurfacePad          // a pad on a body's surface (a spaceport): vertical touchdown
};

// Why a ship goes to a dock / what a place serves. Empty = any.
inline constexpr const char* DockPurposeCargo = "cargo";
inline constexpr const char* DockPurposePassenger = "passenger";
inline constexpr const char* DockPurposeService = "service";   // repair / refit
inline constexpr const char* DockPurposeRefuel = "refuel";

struct DockingEnvelope
{
    double widthMeters = 0.0;
    double heightMeters = 0.0;
    double lengthMeters = 0.0;
    double sweptWidthMeters = 0.0;
    double sweptHeightMeters = 0.0;
    double landingFootprintWidthMeters = 0.0;
    double landingFootprintLengthMeters = 0.0;
    double landingGearClearanceMeters = 0.0;
    double massKg = 0.0;
};

struct AccessPortalDescriptor
{
    std::string id;
    DockSizeClass sizeClass = DockSizeClass::Unknown;
    double usableWidthMeters = 0.0;
    double usableHeightMeters = 0.0;
    glm::dvec3 centerLocalMeters {0.0};
    glm::dvec3 outwardLocal {0.0, 0.0, 1.0};
    std::string holdPointId;
    std::vector<std::string> conflictResourceIds;
};

struct EntryHoldPointDescriptor
{
    std::string id;
    std::string portalId;
    glm::dvec3 positionLocalMeters {0.0};
    glm::dvec3 forwardLocal {0.0, 0.0, -1.0};
};

struct ParkingPadDescriptor
{
    std::string id;
    DockSizeClass sizeClass = DockSizeClass::Unknown;
    double usableWidthMeters = 0.0;
    double usableLengthMeters = 0.0;
    double verticalClearanceMeters = 0.0;
    double maxMassKg = 0.0;
    glm::dvec3 surfaceCenterLocalMeters {0.0};
    glm::dvec3 surfaceNormalLocal {0.0, 1.0, 0.0};
    glm::dvec3 approachForwardLocal {0.0, 0.0, -1.0};
    std::vector<std::string> conflictResourceIds;
    // What this place serves (DockPurpose*); empty = every purpose.
    std::vector<std::string> purposes;
};

struct TrafficConflictZoneDescriptor
{
    std::string id;
};

struct InternalLaneDescriptor
{
    std::string id;
    std::string fromAnchorId;
    std::string toAnchorId;
    std::vector<std::string> conflictResourceIds;
};

struct DockFacilityDescriptor
{
    std::string facilityId;
    int systemId = -1;
    DockKind kind = DockKind::RotatingDrum;
    std::string kinematicFrameId;
    std::vector<AccessPortalDescriptor> portals;
    std::vector<EntryHoldPointDescriptor> holdPoints;
    std::vector<TrafficConflictZoneDescriptor> conflictZones;
    std::vector<InternalLaneDescriptor> internalLanes;
    std::vector<ParkingPadDescriptor> pads;
};

} // namespace game::docking
