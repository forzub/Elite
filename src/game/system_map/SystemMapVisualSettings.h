#pragma once

#include <glm/vec3.hpp>

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
    float starfieldBrightnessScale = 0.42f;

    /*
        В System Map Млечный путь нужен как очень мягкая глубина,
        а не как главный объект.
    */
    float milkyWayIntensityScale = 0.24f;

    glm::vec3 milkyWayColorTint {
        0.42f,
        0.58f,
        0.84f
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
};
