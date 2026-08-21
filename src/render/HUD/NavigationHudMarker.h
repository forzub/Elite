#pragma once

#include <string>

#include <glm/glm.hpp>

namespace game::presentation
{

enum class NavigationHudMarkerShape
{
    TacticalTriangle = 0,
    CelestialDiamond,
    WaypointCorners
};

enum class NavigationHudSpeedMode
{
    None = 0,
    Relative,
    Global
};

struct NavigationHudVocabulary
{
    std::string objectText = "Object";
    std::string celestialText = "Celestial";
    std::string finishText = "FINISH";
    std::string waypointText = "WAYPOINT";
    std::string relativeSpeedShort = "REL";
    std::string globalSpeedShort = "GLOB";
};

struct NavigationHudMarker
{
    std::string stableId;
    NavigationHudMarkerShape shape = NavigationHudMarkerShape::TacticalTriangle;

    glm::dvec3 relativePositionMeters {0.0};
    double distanceMeters = 0.0;

    std::string typeText;
    std::string nameText;
    int displayIndex = 0;

    NavigationHudSpeedMode speedMode = NavigationHudSpeedMode::None;
    double speedMps = 0.0;
    std::string speedPrefixText;

    glm::vec4 color {0.70f, 0.90f, 1.00f, 0.82f};

    // Filled by PlayerShipView using the actual cockpit camera/HUD boundary.
    glm::vec2 screenPos {0.0f};
    glm::vec2 edgeDir {0.0f, -1.0f};
    bool onScreen = false;
};

} // namespace game::presentation
