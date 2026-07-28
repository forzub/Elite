#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct HubMapVisualSettings
{
    glm::vec4 backgroundColor {0.015f, 0.020f, 0.030f, 1.0f};

    bool drawStarfield = true;
    float starfieldFieldOfViewDeg = 60.0f;
    float starfieldSizeScale = 0.88f;
    float starfieldBrightnessScale = 1.0f;
    float milkyWayIntensityScale = 1.0f;
    glm::vec3 milkyWayColorTint {1.0f};

    glm::vec4 planetOrbitColor {0.95f, 0.82f, 0.32f, 0.12f};
    glm::vec4 hubOriginColor {0.70f, 0.96f, 1.0f, 0.30f};
    glm::vec4 localGridColor {0.12f, 0.28f, 0.38f, 0.30f};
    glm::vec4 localGridAxisColor {0.20f, 0.55f, 0.75f, 0.45f};

    glm::vec4 primeModuleWireColor {0.65f, 0.92f, 1.0f, 0.95f};
    glm::vec4 regularModuleWireColor {0.45f, 0.65f, 0.85f, 0.75f};
    glm::vec4 primeModuleMarkerColor {0.85f, 0.98f, 1.0f, 0.95f};
    glm::vec4 regularModuleMarkerColor {0.48f, 0.76f, 1.0f, 0.82f};
    float primeModuleMarkerRadiusPx = 9.0f;
    float regularModuleMarkerRadiusPx = 7.0f;
    float moduleMarkerThresholdPx = 8.0f;
    int moduleMarkerSegments = 32;

    glm::vec4 playerShipWireColor {1.0f, 0.78f, 0.25f, 1.0f};
    glm::vec4 regularShipWireColor {0.95f, 0.65f, 0.35f, 0.85f};
    glm::vec4 playerShipMarkerColor {1.0f, 0.84f, 0.25f, 0.98f};
    glm::vec4 regularShipMarkerColor {1.0f, 0.62f, 0.32f, 0.82f};
    float playerShipMarkerRadiusPx = 13.0f;
    float regularShipMarkerRadiusPx = 8.0f;
    float shipMarkerThresholdPx = 12.0f;
    int shipMarkerSegments = 32;

    glm::vec4 moduleLabelColor {0.65f, 0.92f, 1.0f, 0.88f};
    glm::vec4 moduleSubtitleColor {0.55f, 0.72f, 0.82f, 0.62f};
    glm::vec4 shipLabelColor {1.0f, 0.78f, 0.25f, 0.92f};
    int primaryLabelPx = 13;
    int secondaryLabelPx = 10;

    int sphericalGridLatitudeStepDeg = 10;
    int sphericalGridLongitudeStepDeg = 10;
    int sphericalGridMajorEvery = 3;
    int sphericalGridSamplesPerLine = 180;
    glm::vec4 sphericalGridMinorColor {0.28f, 0.66f, 1.00f, 0.055f};
    glm::vec4 sphericalGridMajorColor {0.46f, 0.82f, 1.00f, 0.105f};
    float sphericalGridHorizonFadeStart = 0.04f;
    float sphericalGridHorizonFadeEnd = 0.28f;

    glm::vec4 marsGridMinorColor {1.00f, 0.56f, 0.26f, 0.07f};
    glm::vec4 marsGridMajorColor {1.00f, 0.72f, 0.34f, 0.13f};
    glm::vec4 venusGridMinorColor {1.00f, 0.74f, 0.34f, 0.08f};
    glm::vec4 venusGridMajorColor {1.00f, 0.86f, 0.48f, 0.15f};
    glm::vec4 titanGridMinorColor {1.00f, 0.62f, 0.24f, 0.07f};
    glm::vec4 titanGridMajorColor {1.00f, 0.76f, 0.34f, 0.13f};
};
