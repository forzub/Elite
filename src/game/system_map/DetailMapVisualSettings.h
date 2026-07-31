#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct DetailMapVisualSettings
{
    glm::vec4 backgroundColor {0.02f, 0.025f, 0.035f, 1.0f};

    bool drawStarfield = true;
    float starfieldFieldOfViewDeg = 60.0f;
    float starfieldSizeScale = 0.88f;
    float starfieldBrightnessScale = 1.0f;
    float milkyWayIntensityScale = 1.0f;
    glm::vec3 milkyWayColorTint {1.0f};

    glm::vec4 axisXColor {1.0f, 0.25f, 0.25f, 0.90f};
    glm::vec4 axisYColor {0.25f, 1.0f, 0.25f, 0.90f};
    glm::vec4 axisZColor {0.25f, 0.55f, 1.0f, 0.90f};

    glm::vec4 selectedOrbitColor {1.0f, 0.92f, 0.25f, 0.95f};
    glm::vec4 hubOrbitColor {0.45f, 0.78f, 1.0f, 0.75f};
    glm::vec4 selectedHubOrbitColor {1.0f, 0.75f, 0.25f, 0.90f};

    glm::vec4 hubMarkerColor {0.30f, 0.90f, 1.0f, 1.0f};
    glm::vec4 selectedHubLabelColor {0.42f, 0.95f, 1.0f, 0.96f};
    glm::vec4 stationMarkerColor {0.80f, 0.95f, 1.0f, 1.0f};
    glm::vec4 selectedStationMarkerColor {1.0f, 0.85f, 0.25f, 1.0f};

    double ringMinimumProjectedMinorAxisPx = 0.75;

    bool drawBodyTitle = true;
    float bodyTitleHeightFraction = 0.026f;
    int bodyTitleMinimumPx = 17;
    int bodyTitleMaximumPx = 38;
    float bodyTitleMarginHeightFraction = 0.020f;
    glm::vec4 bodyTitleColor {0.78f, 0.84f, 0.90f, 0.92f};
};
