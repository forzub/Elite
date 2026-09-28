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
    std::string kinematicFrameId;
    std::vector<AccessPortalDescriptor> portals;
    std::vector<EntryHoldPointDescriptor> holdPoints;
    std::vector<TrafficConflictZoneDescriptor> conflictZones;
    std::vector<InternalLaneDescriptor> internalLanes;
    std::vector<ParkingPadDescriptor> pads;
};

} // namespace game::docking
