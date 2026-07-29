#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct GalaxyNavigationGridVisualSettings
{
    glm::vec4 rootEdgeColor {0.22f, 0.46f, 0.60f, 0.096f};
    glm::vec4 rootFaceGridColor {0.22f, 0.46f, 0.60f, 0.074f};
    glm::vec4 parentEdgeColor {0.24f, 0.52f, 0.68f, 0.140f};
    glm::vec4 parentFaceGridColor {0.24f, 0.52f, 0.68f, 0.092f};

    glm::vec4 currentEdgeColor {0.22f, 0.58f, 0.78f, 0.050f};
    glm::vec4 currentMarkerColor {0.82f, 0.67f, 0.24f, 0.10f};
    float anchorEdgeAlpha = 0.18f;
    float anchorMarkerAlpha = 0.58f;

    glm::vec4 hoveredEdgeColor {0.45f, 0.78f, 0.92f, 0.18f};
    glm::vec4 hoveredMarkerColor {0.92f, 0.76f, 0.28f, 0.58f};
    glm::vec4 selectedEdgeColor {0.78f, 0.58f, 0.16f, 0.25f};
    glm::vec4 selectedMarkerColor {1.00f, 0.75f, 0.18f, 0.72f};

    float currentFaceGridAlphaScale = 0.36f;
    float currentFaceGridMinimumAlpha = 0.032f;
    float currentFaceGridMaximumAlpha = 0.090f;

    float terminalCubeAlpha = 0.42f;
    float terminalLeaderAlpha = 0.52f;
};

struct GalaxyLabelVisualSettings
{
    glm::vec4 selectedLeaderColor {0.98f, 0.72f, 0.34f, 0.55f};
    glm::vec4 normalLeaderColor {0.46f, 0.78f, 1.00f, 0.32f};
    glm::vec4 selectedTitleColor {0.98f, 0.72f, 0.34f, 0.92f};
    glm::vec4 normalTitleColor {0.46f, 0.78f, 1.00f, 0.68f};
    glm::vec4 selectedSubtitleColor {0.70f, 0.86f, 1.00f, 0.68f};
    glm::vec4 normalSubtitleColor {0.38f, 0.64f, 0.90f, 0.46f};
};

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
        Galaxy uses a separate astronomical background instance.

        Do not cut away nearby real stars: those stars carry most familiar
        constellation shapes. Runtime-only game-system proxies are filtered
        by GalaxyStarfieldRenderer, while real catalog stars remain visible.
    */
    bool drawStarfield = true;
    float starfieldFieldOfViewDeg = 48.0f;
    float starfieldSizeScale = 0.68f;
    float starfieldMinimumDistanceLy = 0.0f;

    /*
        Map-only controls for the shared sky renderer.

        Defaults inside GalaxyStarfieldRenderer remain 1.0, so the same
        renderer keeps its original brightness in the actual game scene.
    */
    float starfieldBrightnessScale = 0.72f;

    /*
        Млечный путь вернуть в видимость, но оставить второстепенным.
    */
    float milkyWayIntensityScale = 0.44f;

    glm::vec3 milkyWayColorTint {
        0.56f,
        0.70f,
        0.92f
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

    GalaxyNavigationGridVisualSettings navigationGrid;
    GalaxyLabelVisualSettings labels;
};
