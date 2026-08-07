#include "src/game/system_map/HubMapPlanetPass.h"
#include "src/game/system_map/LocalMapPrimitiveRenderer.h"
#include "src/game/system_map/HubMapBackend.h"
#include "src/game/system_map/LocalMapAtmosphereRenderer.h"
#include "src/game/system_map/PlanetBodyOrientation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>


namespace
{
    glm::dvec3 safeNormalizeD(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    )
    {
        const double lengthSquared = glm::dot(value, value);
        if (lengthSquared <= 1.0e-18)
            return fallback;
        return value / std::sqrt(lengthSquared);
    }

    std::string normalizeGeneratedIdentityToken(
        const std::string& text
    )
    {
        std::string out;
        out.reserve(text.size());

        for (unsigned char c : text)
        {
            if (c < 128)
            {
                if (std::isalnum(c))
                {
                    out.push_back(
                        static_cast<char>(
                            std::tolower(c)
                        )
                    );
                }

                continue;
            }

            out.push_back(static_cast<char>(c));
        }

        return out;
    }

    std::string lastGeneratedIdentityPathPart(
        const std::string& text
    )
    {
        std::size_t begin = 0;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '.' || c == '/' || c == '\\')
                begin = i + 1;
        }

        return text.substr(begin);
    }
}

namespace game::system_map
{
void HubMapPlanetPass::drawHubMapCircleLocalXY(
    const glm::dvec3& center,
    double radiusMeters,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (radiusMeters <= 0.0)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const glm::dvec3 p =
            center +
            glm::dvec3(
                std::cos(a) * radiusMeters,
                std::sin(a) * radiusMeters,
                0.0
            );

        const glm::dvec2 s =
            m_owner.activeCamera().project(p);

        glVertex2d(
            s.x,
            s.y
        );
    }

    glEnd();
}



glm::mat3
HubMapPlanetPass::hubCameraToParentPlanetBodyMatrix(
    const world::celestial::HubMapSnapshot& hub
) const
{
    /*
    ====================================================
    1. Текущая серверная орбитальная система хаба
    ====================================================

    Эти оси уже соответствуют текущему server tick.

    Renderer не вычисляет orbital phase и не интегрирует
    положение хаба самостоятельно.
*/
glm::dvec3 progradeWorld =
    safeNormalizeD(
        hub.hubWorldAxes.x,
        glm::dvec3(
            1.0,
            0.0,
            0.0
        )
    );

glm::dvec3 radialWorld =
    safeNormalizeD(
        hub.hubWorldAxes.y,
        glm::dvec3(
            0.0,
            1.0,
            0.0
        )
    );

glm::dvec3 normalWorld =
    safeNormalizeD(
        glm::cross(
            progradeWorld,
            radialWorld
        ),
        safeNormalizeD(
            hub.hubWorldAxes.z,
            glm::dvec3(
                0.0,
                0.0,
                1.0
            )
        )
    );

/*
    Ортогонализируем frame после передачи double -> snapshot.
*/
progradeWorld =
    safeNormalizeD(
        glm::cross(
            radialWorld,
            normalWorld
        ),
        progradeWorld
    );

radialWorld =
    safeNormalizeD(
        glm::cross(
            normalWorld,
            progradeWorld
        ),
        radialWorld
    );

    /*
        ====================================================
        2. Экранная ориентация cinematic globe
        ====================================================

        Центр видимого диска всегда является sub-hub point:

            camera Z -> radialWorld

        Yaw вращает экранные tangent axes вокруг направления
        от планеты к хабу. Pitch меняет композицию горизонта,
        но не физическое положение наблюдателя над планетой.
    */
    const double cameraYaw =
        m_owner.activeCamera().state.yaw;

    const double yawCos =
        std::cos(
            cameraYaw
        );

    const double yawSin =
        std::sin(
            cameraYaw
        );

    const glm::dvec3 screenRightWorld =
        safeNormalizeD(
            progradeWorld *
                yawCos -
            normalWorld *
                yawSin,
            progradeWorld
        );

    const glm::dvec3 screenUpWorld =
        safeNormalizeD(
            -normalWorld *
                yawCos -
            progradeWorld *
                yawSin,
            -normalWorld
        );

    const glm::dvec3 screenTowardViewerWorld =
        radialWorld;

    /*
        ====================================================
        3. Body-fixed система родительской планеты
        ====================================================
    */
    /*
        Та же body-fixed система, которую использует Planet Details.
        Нулевой меридиан нельзя строить отдельной Hub-формулой:
        даже при одинаковом north это даёт постоянный сдвиг по долготе.
    */
    const auto planetOrientation =
        game::system_map::makePlanetTextureOrientation(
            hub.parentPlanetAxialTiltDeg,
            hub.parentPlanetAxisNodeDeg,
            hub.parentPlanetRotationPhaseRad,
            hub.parentPlanetTextureLongitudeOffsetDeg
        );

    /*
        Мировое направление -> координаты texture body:

            X = longitude 0;
            Y = north;
            Z = longitude +90°.
    */
    auto worldDirectionToBody =
        [&](const glm::dvec3& worldDirection) -> glm::vec3
        {
            return glm::vec3(
                game::system_map::worldDirectionToPlanetBody(
                    planetOrientation,
                    worldDirection
                )
            );
        };

    /*
        glm::mat3 принимает столбцы.

        Поэтому matrix * cameraNormal переводит:

            camera X -> body direction screenRight;
            camera Y -> body direction screenUp;
            camera Z -> body direction sub-hub point.
    */
    return
        glm::mat3(
            worldDirectionToBody(
                screenRightWorld
            ),

            worldDirectionToBody(
                screenUpWorld
            ),

            worldDirectionToBody(
                screenTowardViewerWorld
            )
        );
}



void HubMapPlanetPass::drawHubMapPlanetSurfaceHint(
    const world::celestial::HubMapSnapshot& hub,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (hub.parentPlanetRadiusMeters <= 0.0 ||
        hub.hubOrbitRadiusMeters <= 0.0)
    {
        return;
    }


    m_resources.beginEnvironmentRenderSessionIfNeeded(
        MapMode::Hub,
        hub.systemId,
        hub.parentBodyId
    );



    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    const double viewW =
        static_cast<double>(
            std::max(
                viewport[2],
                1
            )
        );

    const double viewH =
        static_cast<double>(
            std::max(
                viewport[3],
                1
            )
        );

    const double maxDim =
        std::max(
            viewW,
            viewH
        );

    const double pitch =
        std::clamp(
            m_owner.activeCamera().state.pitch,
            0.12,
            1.20
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    const double lookDownT =
        smoothStep(
            0.12,
            1.20,
            pitch
        );

    // Оставляем текущую кинематографическую композицию:
    // при малом pitch виден горизонт,
    // при большом pitch уходим к взгляду вниз.
    const double horizonY =
        (1.0 - lookDownT) * (viewH * 0.74) +
        lookDownT * (-viewH * 0.38);

    const double visualRadiusPx =
        maxDim *
        (1.38 + 0.46 * lookDownT);

    const double horizonCenterY =
        horizonY + visualRadiusPx;

    const double nadirCenterY =
        viewH * 0.54;

    const glm::dvec2 visualPlanetCenterPx(
        viewW * 0.50 +
            m_owner.activeCamera().state.pan.x * 0.015,
        (1.0 - lookDownT) * horizonCenterY +
            lookDownT * nadirCenterY
    );

    m_lastHubPlanetVisualRadiusPx =
        visualRadiusPx;

    m_lastHubPlanetVisualCenterPx =
        visualPlanetCenterPx;


    const LocalMapAtmosphereStyle atmosphereStyle =
    hubPlanetAtmosphereStyleForHub(
        hub
    );


    auto mixColor =
        [](const glm::vec4& a, const glm::vec4& b, float t) -> glm::vec4
        {
            return glm::vec4(
                a.r + (b.r - a.r) * t,
                a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t,
                a.a + (b.a - a.a) * t
            );
        };







    const GLuint albedoTexture =
        m_resources.globalAlbedoTextureForHubSnapshot(
            hub
        );

    const GLuint normalTexture =
        globalNormalTextureForHubSnapshot(
            hub
        );

    const GLuint previewTexture =
        mapPreviewTextureForHubSnapshot(
            hub
        );

    const GLuint surfaceTexture =
        albedoTexture != 0
            ? albedoTexture
            : previewTexture;

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    // -----------------------------------------------------------------
// 1. Базовое тело планеты.
// Гладкая океаническая масса без текстурной грязи.
//
// ВАЖНО:
// Внутри GL_TRIANGLE_STRIP на каждый угол должно быть ровно две вершины:
//   - внутренняя окружность r0
//   - внешняя окружность r1
//
// Если оставить третью вершину, появятся треугольные "зубья"
// по всей планете.
// -----------------------------------------------------------------
m_owner.beginGpuStage(
    game::system_map::HubMapBackend::GpuStage::FallbackBody
);

if (surfaceTexture == 0)
{
    constexpr int bands = 32;
    constexpr int segments = 256;

    for (int band = 0; band < bands; ++band)
    {
        const double t0 =
            static_cast<double>(band) /
            static_cast<double>(bands);

        const double t1 =
            static_cast<double>(band + 1) /
            static_cast<double>(bands);

        const double r0 =
            visualRadiusPx *
            t0;

        const double r1 =
            visualRadiusPx *
            t1;

        const float c0 =
            static_cast<float>(t0);

        const float c1 =
            static_cast<float>(t1);

        const glm::vec4 color0 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c0
            );

        const glm::vec4 color1 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c1
            );

        glBegin(GL_TRIANGLE_STRIP);

        for (int i = 0; i <= segments; ++i)
        {
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double ca =
                std::cos(a);

            const double sa =
                std::sin(a);

            // Вершина внутренней окружности.
            glColor4f(
                color0.r,
                color0.g,
                color0.b,
                color0.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r0,
                visualPlanetCenterPx.y + sa * r0
            );

            // Вершина внешней окружности.
            glColor4f(
                color1.r,
                color1.g,
                color1.b,
                color1.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r1,
                visualPlanetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }
}




m_owner.endGpuStage();




// -----------------------------------------------------------------
    // 2. Поверхность родительского тела.
    // Renderer получает ориентацию из server snapshot и использует
    // отдельную Hub-map screen-space проекцию, а не Detail globe path.
    // -----------------------------------------------------------------
        /*
        Одно общее время для поверхности и облаков.
        */
/*
    Визуальное время используется только для движения
    procedural clouds.
*/
const double cloudVisualTimeSeconds =
    m_resources.visualEffectTimeSeconds(
        hub.universeTimeSeconds
    );

/*
    Геометрическая ориентация планеты берётся строго
    из текущего server snapshot.
*/
const glm::mat3 cameraToPlanetBody =
    hubCameraToParentPlanetBodyMatrix(
        hub
    );



    m_owner.beginGpuStage(
        game::system_map::HubMapBackend::GpuStage::Surface
    );


    if (surfaceTexture != 0)
    {
        /*
            Пока это screen-space свет.

            Позже вместо фиксированного направления передадим
            реальное направление к звезде системы.
        */
        const glm::vec3 lightDirection =
            glm::normalize(
                glm::vec3(
                    -0.42f,
                    0.34f,
                    0.84f
                )
            );

        m_resources.hubPlanetSurfaceRenderer().render(
            surfaceTexture,
            normalTexture,
            visualPlanetCenterPx,
            visualRadiusPx,
            cameraToPlanetBody,
            lightDirection
        );
    }


    m_owner.endGpuStage();

    /*
        Облака и атмосфера являются мягкими слоями. Рисуем их
        в половинном разрешении и один раз композим поверх
        full-resolution поверхности.
    */
    constexpr float hubSoftLayerResolutionScale =
        0.50f;

    const bool softLayerTargetActive =
        m_overlayRenderer.begin(
            viewport[2],
            viewport[3],
            hubSoftLayerResolutionScale
        );

    const double softLayerScale =
        softLayerTargetActive
            ? static_cast<double>(
                m_overlayRenderer.resolutionScale()
            )
            : 1.0;

    const glm::dvec2 softPlanetCenterPx =
        visualPlanetCenterPx *
        softLayerScale;

    const double softPlanetRadiusPx =
        visualRadiusPx *
        softLayerScale;


        m_owner.beginGpuStage(
            game::system_map::HubMapBackend::GpuStage::Clouds
        );

        const auto cloudStyles =
            hubPlanetCloudStylesForHub(
                hub
            );

        for (std::size_t layerIndex = 0;
            layerIndex < cloudStyles.size();
            ++layerIndex)
        {
            const auto& cloudStyle =
                cloudStyles[layerIndex];

            if (!cloudStyle.enabled)
                continue;

            /*
                ProceduralCloudLayer по-прежнему изменяет форму
                облаков и выполняет blending между состояниями.

                Новый mesh renderer только дешёво натягивает
                готовую динамическую текстуру на сферу.
            */
            const GLuint cloudTexture =
                m_resources.proceduralCloudLayer().textureForStyle(
                    cloudStyle,
                    cloudVisualTimeSeconds
                );

            if (cloudTexture == 0)
                continue;

            const double meanHeightMeters =
                (
                    static_cast<double>(
                        cloudStyle.baseHeightKm
                    ) +
                    static_cast<double>(
                        cloudStyle.topHeightKm
                    )
                ) *
                500.0;

            const double physicalRadiusScale =
                1.0 +
                meanHeightMeters /
                    std::max(
                        1.0,
                        hub.parentPlanetRadiusMeters
                    );

            /*
                На огромной cinematic-сфере физическая разница
                высот почти незаметна. Оставляем слабое визуальное
                разделение слоёв.
            */
            const double cloudRadiusScale =
                std::clamp(
                    physicalRadiusScale +
                        0.0025 *
                        static_cast<double>(
                            layerIndex + 1
                        ),
                    1.003,
                    1.055
                );

            /*
                Независимый дрейф текущего облачного слоя.
            */
            const double driftU =
                std::fmod(
                    cloudVisualTimeSeconds *
                        static_cast<double>(
                            cloudStyle.driftSpeed
                        ),
                    1.0
                );

            render::celestial::PlanetGlobeLayerDraw draw;

            draw.texture =
                cloudTexture;

            /*
                visualPlanetCenterPx задан относительно текущего
                viewport и использует начало координат слева сверху.
            */
            draw.centerPx =
                softPlanetCenterPx;

            draw.radiusPx =
                softPlanetRadiusPx *
                cloudRadiusScale;

            /*
                Hub backdrop является художественной экранной
                сферой, поэтому используем прямую ориентацию:

                    X — вправо;
                    Y — вверх;
                    Z — к камере.
            */
            /*
                cameraToPlanetBody является чистым вращением.
                Его inverse равен transpose.
            */
            draw.bodyToCamera =
                glm::transpose(
                    cameraToPlanetBody
                );

            /*
                Вращение самой планеты уже содержится
                в bodyToCamera.

                Здесь остаётся только относительный
                атмосферный дрейф облаков.
            */
            draw.longitudeUvOffset =
                static_cast<float>(
                    driftU
                );

            /*
                Hub backdrop textures use a top-left screen-space origin.
            */
            draw.flipV =
                true;

            draw.color =
                glm::vec4(
                    1.0f
                );

            /*
                Сохраняем прежнее художественное усиление alpha.
            */
            draw.opacity =
                std::clamp(
                    cloudStyle.opacity *
                        2.6f,
                    0.0f,
                    0.72f
                );

            draw.blending = true;
            draw.premultipliedTarget = softLayerTargetActive;

            /*
                Растворение облаков возле горизонта.
            */
            draw.useHorizonFade =
                true;

            draw.horizonFadeStart =
                0.05f;

            draw.horizonFadeEnd =
                0.32f;

            draw.usePolarFade =
                false;

            m_resources.planetGlobeMeshRenderer().render(
                draw
            );
        }



        m_owner.endGpuStage();


        m_owner.beginGpuStage(
            game::system_map::HubMapBackend::GpuStage::Atmosphere
        );



        drawLocalMapAtmosphereStack(
            softPlanetCenterPx,
            softPlanetRadiusPx,
            atmosphereStyle,
            softLayerTargetActive
        );

        if (softLayerTargetActive)
        {
            /*
                Composite учитываем в Atmosphere-stage:
                это один bilinear fullscreen pass.
            */
            m_overlayRenderer.endAndComposite();
        }

        m_owner.endGpuStage();






    // -----------------------------------------------------------------
    // 6. Тактический оверлей оставляем.
    // -----------------------------------------------------------------
    const glm::dvec3 planetCenterLocal =
        hub.parentPlanetCenterLocalMeters;






        /*
            Полная круговая орбита выбранного хаба вокруг
            родительской планеты.

            Hub-local convention:
                X = prograde;
                Y = radial;
                Z = orbital normal.

            Поэтому орбита лежит в локальной плоскости XY.
        */
        glColor4f(
            m_resources.hubVisuals().planetOrbitColor.r,
            m_resources.hubVisuals().planetOrbitColor.g,
            m_resources.hubVisuals().planetOrbitColor.b,
            m_resources.hubVisuals().planetOrbitColor.a
        );

        drawHubMapCircleLocalXY(
            planetCenterLocal,
            hub.hubOrbitRadiusMeters,
            scale,
            centerPx,
            256
        );










    const glm::dvec3 surfacePoint =
        planetCenterLocal +
        glm::dvec3(
            0.0,
            hub.parentPlanetRadiusMeters,
            0.0
        );

    glColor4f(
        m_resources.hubVisuals().hubOriginColor.r,
        m_resources.hubVisuals().hubOriginColor.g,
        m_resources.hubVisuals().hubOriginColor.b,
        m_resources.hubVisuals().hubOriginColor.a
    );

    drawLocalMapCross(
        m_owner.activeCamera().project(surfacePoint),
        5.0f
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}



GLuint HubMapPlanetPass::mapPreviewTextureForHubSnapshot(
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* asset =
        m_resources.generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (!asset)
        return 0;

    return m_resources.mapPreviewTextureForGeneratedAsset(*asset);
}



GLuint HubMapPlanetPass::globalNormalTextureForHubSnapshot(
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* asset =
        m_resources.generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (!asset)
        return 0;

    return m_resources.globalNormalTextureForGeneratedAsset(
        *asset
    );
}



LocalMapAtmosphereStyle
HubMapPlanetPass::hubPlanetAtmosphereStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    return m_resources.atmosphereStyleForBody(
        hub.systemId,
        hub.parentBodyId,
        hub.parentBodyId,
        hub.parentEnvironmentPresetId
    );
}



render::celestial::HubSphericalGridStyle
HubMapPlanetPass::hubSphericalGridStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    render::celestial::HubSphericalGridStyle style;

    std::string bodyKey =
        normalizeGeneratedIdentityToken(
            lastGeneratedIdentityPathPart(
                hub.parentBodyId
            )
        );

    const auto* asset =
        m_resources.generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (asset)
    {
        const std::string assetKey =
            normalizeGeneratedIdentityToken(
                asset->bodyFolderName
            );

        if (!assetKey.empty())
            bodyKey = assetKey;
    }

    // По умолчанию — холодная голубая сетка.
    style.radiusScale = 1.12;
    style.latitudeStepDeg =
        m_resources.hubVisuals().sphericalGridLatitudeStepDeg;
    style.longitudeStepDeg =
        m_resources.hubVisuals().sphericalGridLongitudeStepDeg;
    style.majorEvery =
        m_resources.hubVisuals().sphericalGridMajorEvery;
    style.samplesPerLine =
        m_resources.hubVisuals().sphericalGridSamplesPerLine;
    style.minorColor =
        m_resources.hubVisuals().sphericalGridMinorColor;
    style.majorColor =
        m_resources.hubVisuals().sphericalGridMajorColor;
    style.horizonFadeStart =
        m_resources.hubVisuals().sphericalGridHorizonFadeStart;
    style.horizonFadeEnd =
        m_resources.hubVisuals().sphericalGridHorizonFadeEnd;

    if (bodyKey == "mars" ||
        bodyKey == "ares")
    {
        style.minorColor =
            m_resources.hubVisuals().marsGridMinorColor;
        style.majorColor =
            m_resources.hubVisuals().marsGridMajorColor;
    }
    else if (bodyKey == "venus")
    {
        style.minorColor =
            m_resources.hubVisuals().venusGridMinorColor;
        style.majorColor =
            m_resources.hubVisuals().venusGridMajorColor;
    }
    else if (bodyKey == "titan")
    {
        style.minorColor =
            m_resources.hubVisuals().titanGridMinorColor;
        style.majorColor =
            m_resources.hubVisuals().titanGridMajorColor;
    }

    return style;
}



std::vector<
    render::celestial::ProceduralCloudStyle
>
HubMapPlanetPass::hubPlanetCloudStylesForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    auto styles =
        m_resources.cloudStylesForBody(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId,
            hub.parentEnvironmentPresetId,
            hub.parentPlanetRadiusMeters,
            1024,
            512
        );

    /*
        Hub — режим просмотра огромной планеты.
        Здесь разрешаем усиленное preview-движение,
        но сохраняем относительные скорости слоёв.
    */
    for (auto& style : styles)
    {
        style.driftSpeed *=
            2.5f;
    }

    return styles;
}

}
