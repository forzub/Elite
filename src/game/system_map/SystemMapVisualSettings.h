#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct SystemNavigationGridVisualSettings
{
    glm::vec4 parentEdgeColor {0.24f, 0.52f, 0.68f, 0.140f};
    glm::vec4 parentFaceGridColor {0.24f, 0.52f, 0.68f, 0.092f};
    glm::vec4 currentEdgeColor {0.38f, 0.72f, 0.94f, 0.105f};
    glm::vec4 hoveredEdgeColor {0.45f, 0.78f, 0.92f, 0.18f};
    glm::vec4 selectedEdgeColor {0.92f, 0.66f, 0.20f, 0.24f};
    glm::vec4 currentMarkerColor {0.54f, 0.82f, 1.00f, 0.58f};
    glm::vec4 selectedMarkerColor {1.00f, 0.76f, 0.24f, 0.78f};
    float currentMarkerRadiusPx = 4.0f;
    float selectedMarkerRadiusPx = 5.0f;
    float faceGridAlphaScale = 0.36f;
    float faceGridMinimumAlpha = 0.032f;
    float faceGridMaximumAlpha = 0.090f;
};

struct SystemBodyLabelVisualSettings
{
    glm::vec4 selectedTitleColor {1.0f, 0.78f, 0.30f, 0.96f};
    glm::vec4 starTitleColor {1.0f, 0.82f, 0.46f, 0.90f};
    glm::vec4 bodyTitleColor {0.62f, 0.84f, 1.0f, 0.88f};
    glm::vec4 subtitleColor {0.55f, 0.67f, 0.78f, 0.62f};
};

struct SystemSceneVisualSettings
{
    glm::vec4 moonOrbitColor {0.72f, 0.78f, 0.86f, 0.24f};
    glm::vec4 asteroidBeltOrbitColor {0.62f, 0.66f, 0.72f, 0.30f};
    glm::vec4 planetOrbitColor {0.48f, 0.76f, 1.00f, 0.34f};
    int moonOrbitSegments = 64;
    int primaryOrbitSegments = 160;

    /* System-map ring LOD and edge-on stability. */
    float ringFadeStartOuterRadiusPx = 18.0f;
    float ringFullOpacityOuterRadiusPx = 42.0f;
    float ringMinimumProjectedMinorAxisPx = 0.85f;

    glm::vec4 planetMarkerColor {0.48f, 0.76f, 1.00f, 0.90f};
    glm::vec4 asteroidMarkerColor {0.74f, 0.70f, 0.62f, 0.88f};

    glm::vec4 selectedRingColor {1.0f, 0.82f, 0.25f, 0.98f};
    glm::vec4 selectedSecondaryRingColor {1.0f, 0.82f, 0.25f, 0.34f};
    glm::vec4 selectedHubRingColor {0.30f, 0.92f, 1.00f, 0.98f};
    glm::vec4 selectedHubSecondaryRingColor {0.30f, 0.92f, 1.00f, 0.78f};

    glm::vec4 scalePrimaryTextColor {0.58f, 0.82f, 1.0f, 0.78f};
    glm::vec4 scaleSecondaryTextColor {0.42f, 0.62f, 0.82f, 0.58f};

    glm::vec3 hubObjectLabelColor {0.42f, 0.95f, 1.00f};
    glm::vec3 otherObjectLabelColor {1.00f, 0.86f, 0.42f};
    glm::vec3 objectOwnerLabelColor {0.75f, 0.72f, 0.58f};
    float objectOwnerAlphaScale = 0.70f;
};

struct SystemMapVisualSettings
{
    /*
        Perspective projection for all System-map geometry.

        m_systemCamera.distance still means the visible half-height at the
        camera target plane. Keeping that semantic makes wheel zoom, panning
        and navigation thresholds independent from the projection type.
    */
    float projectionFieldOfViewDeg = 48.0f;

    /*
        Body labels.

        At the 1080 px reference height these values produce:
        title    14 * 1.5 = 21 px
        subtitle 10 * 1.5 = 15 px
    */
    float labelScale = 1.50f;
    float labelReferenceHeightPx = 1080.0f;
    float labelMinimumScreenScale = 0.72f;
    float labelMaximumScreenScale = 1.45f;

    int labelTitleBasePx = 14;
    int labelSelectedTitleBasePx = 16;
    int labelSubtitleBasePx = 10;

    int labelTitleMinPx = 12;
    int labelTitleMaxPx = 30;

    int labelSubtitleMinPx = 9;
    int labelSubtitleMaxPx = 22;

    float labelBodyGapBasePx = 10.0f;
    float labelMinimumOffsetBasePx = 14.0f;
    float labelTitleYOffsetBasePx = -6.0f;
    float labelSubtitleOffsetBasePx = 16.0f;

    /* Visibility policy expressed in presentation units. */
    double moonLabelMaximumKmPerPixel = 200.0;
    float asteroidBeltLabelMinimumRadiusPx = 2.0f;
    float subtitleMinimumBodyRadiusPx = 10.0f;

    /*
        Reuse the world star catalog as a system-map sky.
        The observer is the opened system or empty-sector position.
    */
    bool drawStarfield = true;
    // Must match projectionFieldOfViewDeg or the sky and geometry diverge.
    float starfieldFieldOfViewDeg = 48.0f;
    float starfieldSizeScale = 0.82f;

    /*
        System-map-only sky treatment. The shared renderer uses 1.0 defaults
        everywhere else, including the actual flight scene and Hub/Detail maps.
    */
    float starfieldBrightnessScale = 0.68f;

    /*
        В System Map Млечный путь нужен как очень мягкая глубина,
        а не как главный объект.
    */
    float milkyWayIntensityScale = 0.38f;

    glm::vec3 milkyWayColorTint {
        0.54f,
        0.68f,
        0.90f
    };

    /*
        Map-only atmospheric veil drawn after the shared starfield and before
        system bodies, orbits, navigation graphics and labels.
    */
    bool drawAtmosphereVeil = true;

    /*
        System Map тоже надо притушить, но оставить чуть больше читаемости
        в центре, чтобы локальная сцена не теряла глубину.
    */
    float atmosphereVeilCenterAlpha = 0.30f;
    float atmosphereVeilEdgeAlpha = 0.78f;
    float atmosphereVeilAquaStrength = 0.20f;

    SystemNavigationGridVisualSettings navigationGrid;
    SystemBodyLabelVisualSettings bodyLabels;
    SystemSceneVisualSettings scene;
};
