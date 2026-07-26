#pragma once

#include <glm/vec3.hpp>

struct GalaxyMapVisualSettings
{
    // Начальная камера.
    float initialCameraDistance = 82.0f;

    /*
        Плавное перемещение камеры при выборе звезды,
        куба или системы из списка.

        Длительность вычисляется по расстоянию:
        соседний куб — короткий перелёт;
        далёкая система — более продолжительный.
    */
    float cameraFlightMinSeconds = 0.28f;
    float cameraFlightMaxSeconds = 0.68f;

    /*
        Расстояние в render units, после которого используется
        почти максимальная длительность перелёта.
    */
    float cameraFlightReferenceDistance = 90.0f;

    /*
        Радиус ядра звезды в экранных пикселях.
        Итоговый диаметр будет примерно вдвое больше.
    */
    float starBaseRadiusPx = 2.6f;

    float currentStarScale = 1.16f;
    float selectedStarScale = 1.30f;
    float multipleStarScale = 0.08f;

    // Гало звёзд зафиксированного куба.
    float fixedCubeHaloRadiusScale = 3.60f;
    float fixedCubeHaloAlpha = 0.48f;

    // Более заметное гало куба под курсором.
    float hoveredCubeHaloRadiusScale = 4.80f;
    float hoveredCubeHaloAlpha = 0.82f;

    int starHaloRingCount = 6;
    int starHaloSegments = 40;

    // Подписи.
    float labelScale = 1.32f;
    float labelReferenceHeightPx = 1080.0f;
    float labelMinimumScreenScale = 0.72f;
    float labelMaximumScreenScale = 1.45f;
    float labelMaxCameraDistance = 155.0f;

    int labelTitleBasePx = 13;
    int labelSelectedTitleBasePx = 15;
    int labelSubtitleBasePx = 10;

    int labelTitleMinPx = 10;
    int labelTitleMaxPx = 27;

    int labelSubtitleMinPx = 8;
    int labelSubtitleMaxPx = 21;

    /*
        Galaxy uses a separate distant-only starfield instance.
        Nearby/game systems are excluded so they are not drawn twice:
        once as map objects and once on the background sphere.
    */
    bool drawStarfield = true;
    float starfieldFieldOfViewDeg = 48.0f;
    float starfieldSizeScale = 0.68f;
    float starfieldMinimumDistanceLy = 120.0f;

    /*
        Map-only controls for the shared sky renderer.

        Defaults inside GalaxyStarfieldRenderer remain 1.0, so the same
        renderer keeps its original brightness in the actual game scene.
    */
    float starfieldBrightnessScale = 0.48f;

    /*
        Млечный путь вернуть в видимость, но оставить второстепенным.
    */
    float milkyWayIntensityScale = 0.28f;

    glm::vec3 milkyWayColorTint {
        0.46f,
        0.62f,
        0.86f
    };

    /*
        Map-only atmospheric veil.

        It is drawn after the shared starfield, so the game sky renderer and
        its shader remain untouched. Map geometry, labels and UI are drawn
        afterwards and stay crisp.
    */
    bool drawAtmosphereVeil = true;

    /*
        Насколько сильно затемняется уже нарисованный starfield.
        Это не влияет на геометрию карты, подписи и UI.
    */
    float atmosphereVeilCenterAlpha = 0.24f;
    float atmosphereVeilEdgeAlpha = 0.72f;

    /*
        Холодный акцент, но без лишнего свечения.
    */
    float atmosphereVeilAquaStrength = 0.24f;
};
