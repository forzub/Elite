#include "SceneRenderer.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <utility>
#include <unordered_map>

// #define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/DebugGrid.h"
#include "src/render/ship/ShipMeshRenderer.h"
#include "src/render/ShaderLibrary.h"  // Используем существующий ShaderLibrary

#include "src/debug/render/DebugLineRenderer.h"
#include "src/debug/DebugSettings.h"

#include "src/render/frustum/Frustum.h"
#include "src/render/renderers/LightingParams.h"
#include "src/debug/render/DebugHitVolumeRenderer.h"
#include "src/debug/DebugSettings.h"
#include "src/debug/render/ServerHitVolumeRenderer.h"

#include "src/game/math/OrientationBasis.h"

#include "src/render/HUD/TextRenderer.h"
#include <cstdio>

#include "src/game/visual/VisualShip.h"
#include "src/world/coordinates/WorldFrame.h"

#include <fstream>
#include <iomanip>

namespace
{
    float findAssemblyModuleAngleRad(
        const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& modules,
        const std::string& moduleId
    )
    {
        for (const auto& m : modules)
        {
            if (m.moduleId == moduleId)
                return m.rotationAngleRad;
        }

        return 0.0f;
    }



        double renderProfileNowMs()
        {
            return glfwGetTime() * 1000.0;
        }



        void drawFramePacingMarker()
        {
            GLint viewport[4] = {0, 0, 1, 1};
            glGetIntegerv(GL_VIEWPORT, viewport);

            const float w = static_cast<float>(viewport[2]);
            const float h = static_cast<float>(viewport[3]);

            if (w <= 1.0f || h <= 1.0f)
                return;

            const float t =
                static_cast<float>(glfwGetTime());

            // Скорость бегущей точки в пикселях/сек.
            const float speedPxPerSec = 240.0f;

            const float x =
                std::fmod(t * speedPxPerSec, w);

            const float y =
                h - 24.0f;

            const float size = 8.0f;

            auto pxToNdc = [&](float px, float py) -> glm::vec2
            {
                return {
                    (px / w) * 2.0f - 1.0f,
                    1.0f - (py / h) * 2.0f
                };
            };

            const glm::vec2 a = pxToNdc(x - size, y - size);
            const glm::vec2 b = pxToNdc(x + size, y + size);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glBegin(GL_QUADS);
                glVertex2f(a.x, a.y);
                glVertex2f(b.x, a.y);
                glVertex2f(b.x, b.y);
                glVertex2f(a.x, b.y);
            glEnd();

            glPopMatrix();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            glMatrixMode(GL_MODELVIEW);

            glEnable(GL_DEPTH_TEST);
        }





        const game::simulation::ObjectModuleSnapshot* findModuleSnapshotById(
            const std::vector<game::simulation::ObjectModuleSnapshot>& modules,
            const std::string& moduleId
        )
        {
            for (const auto& m : modules)
            {
                if (m.moduleId == moduleId)
                    return &m;
            }

            return nullptr;
        }

        bool isDetachedVisualState(uint8_t state)
        {
            return state == 3 || state == 4; // Detached / Hanging
        }


        float safeRadiusFromHalfSize(const glm::vec3& halfSize)
        {
            const float r = glm::length(halfSize);
            return r > 0.001f ? r : 1.0f;
        }

        struct QueuedMeshDraw
        {
            const render::MeshGPU* gpu = nullptr;
            glm::mat4 mvp = glm::mat4(1.0f);
            glm::mat4 model = glm::mat4(1.0f);
        };







        constexpr float kCelestialNearPlane = 10000.0f;
        constexpr float kCelestialFarPlane  = 200000000000.0f;

        constexpr float kFarStationNearPlane = 1000.0f;
        constexpr float kFarStationFarPlane  = 20000000.0f;

        // Ближе этой дистанции станция рисуется полной модульной сборкой.
        // Дальше — только дальний proxy-силуэт.
        constexpr float kStationFullRenderDistance = 450000.0f;
        constexpr float kVisualShipProxyRenderDistance  = 450.0f;
        constexpr float kVisualShipFullPrepareDistance  = 520.0f;
        constexpr float kVisualShipEdgeFullAlphaDistance = 260.0f;
        constexpr float kVisualShipEdgeZeroAlphaDistance = 450.0f;

        void addCircleXz(
            DebugLineRenderer& lines,
            const glm::vec3& center,
            float radius,
            int segments,
            const glm::vec4& color
        )
        {
            constexpr float pi = 3.14159265358979323846f;

            for (int i = 0; i < segments; ++i)
            {
                const float a0 = 2.0f * pi * float(i) / float(segments);
                const float a1 = 2.0f * pi * float(i + 1) / float(segments);

                lines.addLine(
                    {
                        center.x + std::cos(a0) * radius,
                        center.y,
                        center.z + std::sin(a0) * radius
                    },
                    {
                        center.x + std::cos(a1) * radius,
                        center.y,
                        center.z + std::sin(a1) * radius
                    },
                    color
                );
            }
        }

        void addCircleXy(
            DebugLineRenderer& lines,
            const glm::vec3& center,
            float radius,
            int segments,
            const glm::vec4& color
        )
        {
            constexpr float pi = 3.14159265358979323846f;

            for (int i = 0; i < segments; ++i)
            {
                const float a0 = 2.0f * pi * float(i) / float(segments);
                const float a1 = 2.0f * pi * float(i + 1) / float(segments);

                lines.addLine(
                    {
                        center.x + std::cos(a0) * radius,
                        center.y + std::sin(a0) * radius,
                        center.z
                    },
                    {
                        center.x + std::cos(a1) * radius,
                        center.y + std::sin(a1) * radius,
                        center.z
                    },
                    color
                );
            }
        }

        void addCircleYz(
            DebugLineRenderer& lines,
            const glm::vec3& center,
            float radius,
            int segments,
            const glm::vec4& color
        )
        {
            constexpr float pi = 3.14159265358979323846f;

            for (int i = 0; i < segments; ++i)
            {
                const float a0 = 2.0f * pi * float(i) / float(segments);
                const float a1 = 2.0f * pi * float(i + 1) / float(segments);

                lines.addLine(
                    {
                        center.x,
                        center.y + std::cos(a0) * radius,
                        center.z + std::sin(a0) * radius
                    },
                    {
                        center.x,
                        center.y + std::cos(a1) * radius,
                        center.z + std::sin(a1) * radius
                    },
                    color
                );
            }
        }








        void drawQueuedMeshPasses(
            MeshRenderer& meshRenderer,
            const std::vector<QueuedMeshDraw>& draws,
            GLuint fillShader,
            GLuint edgeShader,
            const LightingParams& params,
            const glm::vec3& cameraLocalPosition
        )
        {
            // Важно: сначала все fill-проходы всех деталей пишут depth.
            // Только после этого рисуем edges. Иначе внутренние ребра двигателей/рам
            // успевают попасть в кадр до того, как обшивка закрыла их depth-буфером.
            for (const QueuedMeshDraw& draw : draws)
            {
                if (!draw.gpu)
                    continue;

                meshRenderer.draw(
                    *draw.gpu,
                    fillShader,
                    0,
                    draw.mvp,
                    draw.model,
                    params,
                    cameraLocalPosition
                );
            }

            for (const QueuedMeshDraw& draw : draws)
            {
                if (!draw.gpu)
                    continue;

                meshRenderer.draw(
                    *draw.gpu,
                    0,
                    edgeShader,
                    draw.mvp,
                    draw.model,
                    params,
                    cameraLocalPosition
                );
            }

        }




        void drawQueuedMeshFillOnly(
            MeshRenderer& meshRenderer,
            const std::vector<QueuedMeshDraw>& draws,
            GLuint fillShader,
            const LightingParams& params,
            const glm::vec3& cameraLocalPosition
        )
        {
            for (const QueuedMeshDraw& draw : draws)
            {
                if (!draw.gpu)
                    continue;

                meshRenderer.draw(
                    *draw.gpu,
                    fillShader,
                    0,
                    draw.mvp,
                    draw.model,
                    params,
                    cameraLocalPosition
                );
            }
        }



        float smoothStep01(float x)
        {
            x = std::clamp(x, 0.0f, 1.0f);
            return x * x * (3.0f - 2.0f * x);
        }

        float visualShipEdgeAlphaByDistance(float distance)
        {
            const float a = kVisualShipEdgeFullAlphaDistance;
            const float b = kVisualShipEdgeZeroAlphaDistance;

            if (distance <= a)
                return 1.0f;

            if (distance >= b)
                return 0.0f;

            const float t =
                (distance - a) / (b - a);

            return 1.0f - smoothStep01(t);
        }





        void drawQueuedVisualShipMeshPasses(
            MeshRenderer& meshRenderer,
            const std::vector<QueuedMeshDraw>& draws,
            GLuint fillShader,
            GLuint edgeShader,
            const LightingParams& params,
            const glm::vec3& cameraLocalPosition
        )
        {
            // Fill рисуем всегда.
            for (const QueuedMeshDraw& draw : draws)
            {
                if (!draw.gpu)
                    continue;

                meshRenderer.draw(
                    *draw.gpu,
                    fillShader,
                    0,
                    draw.mvp,
                    draw.model,
                    params,
                    cameraLocalPosition
                );
            }

            // Edges рисуем только в ближней зоне и с плавной прозрачностью.
            for (const QueuedMeshDraw& draw : draws)
            {
                if (!draw.gpu)
                    continue;

                const glm::vec3 localPos =
                    glm::vec3(draw.model * glm::vec4(0, 0, 0, 1));

                const float distance =
                    glm::length(localPos - cameraLocalPosition);

                const float alpha =
                    visualShipEdgeAlphaByDistance(distance);

                if (alpha <= 0.01f)
                    continue;

                LightingParams edgeParams = params;
                edgeParams.edgeAlpha *= alpha;

                meshRenderer.draw(
                    *draw.gpu,
                    0,
                    edgeShader,
                    draw.mvp,
                    draw.model,
                    edgeParams,
                    cameraLocalPosition
                );
            }
        }



        glm::mat4 buildAssemblyModuleBaseModel(
            const glm::mat4& ownerModel,
            const game::ship::geometry::AssemblyModule& module
        )
        {
            glm::mat4 moduleBaseModel = ownerModel;

            moduleBaseModel = glm::translate(
                moduleBaseModel,
                module.localPosition
            );

            moduleBaseModel = glm::rotate(
                moduleBaseModel,
                glm::radians(module.localRotationDeg.x),
                glm::vec3(1.0f, 0.0f, 0.0f)
            );

            moduleBaseModel = glm::rotate(
                moduleBaseModel,
                glm::radians(module.localRotationDeg.y),
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            moduleBaseModel = glm::rotate(
                moduleBaseModel,
                glm::radians(module.localRotationDeg.z),
                glm::vec3(0.0f, 0.0f, 1.0f)
            );

            return moduleBaseModel;
        }






        glm::mat4 buildAssemblyModuleModel(
            const glm::mat4& ownerModel,
            const game::ship::geometry::AssemblyModule& module,
            const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& assemblyModules
        )
        {
            glm::mat4 moduleModel =
                buildAssemblyModuleBaseModel(ownerModel, module);

            if (module.rotates)
            {
                float angle = findAssemblyModuleAngleRad(
                    assemblyModules,
                    module.id
                );

                moduleModel =
                    moduleModel *
                    glm::rotate(glm::mat4(1.0f), angle, module.rotationAxis);
                    }

            return moduleModel;
        }








        template<typename TOwner>
        void renderDetachedAssemblyModules(
            MeshRenderer& meshRenderer,
            DebugLineRenderer* debugLines,
            const TOwner& owner,
            const glm::mat4& ownerModel,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::vec3& cameraLocalPosition,
            const world::coordinates::WorldFrame& frame,
            GLuint largeObjectShader,
            GLuint edgeShader,
            const LightingParams& params
        )
        {
            const glm::mat4 renderView =
                        world::coordinates::makeRenderView(view);

            if (!owner.assembly)
                return;

            for (const auto& fragment : owner.detachedFragments)
            {
                const game::ship::geometry::AssemblyModule* detachedModule = nullptr;

                for (const auto& module : owner.assembly->modules)
                {
                    const std::string meshModuleId =
                        fragment.originalModuleId.empty()
                            ? fragment.moduleId
                            : fragment.originalModuleId;

                    if (module.id == meshModuleId)
                    {
                        detachedModule = &module;
                        break;
                    }
                }

                if (!detachedModule)
                    continue;
                
                

                glm::mat4 moduleModel =
                    world::coordinates::makeRenderModelMatrix(
                        fragment.worldPosition,
                        fragment.orientation,
                        frame
                    );

                const glm::vec3 moduleWorldPos =
                    glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

                const float distToModule =
                    glm::length(moduleWorldPos);

                const bool useLod1 =
                    distToModule >= owner.assembly->lodSwitchDistance;

                for (const auto& part : detachedModule->meshes)
                {
                    const glm::mat4 partModel =
                        glm::translate(moduleModel, part.localOffset);

                    const glm::mat4 partMvp =
                        proj * renderView * partModel;

                    const render::MeshGPU& gpu =
                        useLod1 ? part.lod1Gpu : part.lod0Gpu;

                    meshRenderer.draw(
                        gpu,
                        largeObjectShader,
                        edgeShader,
                        partMvp,
                        partModel,
                        params,
                        cameraLocalPosition
                    );
                }

                if (debugLines)
                {
                    debug::render::ServerHitVolumeRenderer::render(
                        *debugLines,
                        fragment.debugHitVolumes,
                        moduleModel,
                        renderView,
                        proj
                    );
                }


            }
        }





}




SceneRenderer::SceneRenderer()
    : m_debugLines(std::make_unique<DebugLineRenderer>())
    , m_debugRenderer(m_debugLines.get())
    , m_initialized(false)
{
}




bool SceneRenderer::initializeStaticResources()
{
    bool ok = true;

    if (!m_debugLines || !m_debugLines->isInitialized())
    {
        m_initialized = m_debugLines->initialize();
        ok = ok && m_initialized;
    }

    if (!m_starfieldRenderer.isInitialized())
    {
        ok = ok && m_starfieldRenderer.initialize();
    }

    if (!m_planetRenderer.isInitialized())
    {
        m_planetRenderer.initialize();
    }

    // Прогрев shader-путей. Шейдеры уже должны быть загружены InitShaders().
    // glUseProgram заставляет драйвер подготовить программу до первого игрового кадра.
    const GLuint galaxyHaze =
        ShaderLibrary::instance().get("galaxy_haze");

    if (galaxyHaze != 0)
    {
        glUseProgram(galaxyHaze);
        glUseProgram(0);
    }

    const GLuint galaxyStarfield =
        ShaderLibrary::instance().get("galaxy_starfield");

    if (galaxyStarfield != 0)
    {
        glUseProgram(galaxyStarfield);
        glUseProgram(0);
    }

    return ok;
}







glm::mat4 SceneRenderer::makePerspectiveForCurrentViewport(
    float fovDeg,
    float nearPlane,
    float farPlane
) const
{
    GLint viewport[4] = {0, 0, 1, 1};
    glGetIntegerv(GL_VIEWPORT, viewport);

    const float aspect =
        viewport[3] > 0
            ? float(viewport[2]) / float(viewport[3])
            : 16.0f / 9.0f;

    return glm::perspective(
        glm::radians(fovDeg),
        aspect,
        nearPlane,
        farPlane
    );
}

void SceneRenderer::renderCelestialPass(
    const glm::mat4& view,
    const world::coordinates::WorldFrame& frame,
    float timeSeconds
)
{   
    static int enterCelestialCounter = 0;

    if ((enterCelestialCounter++ % 120) == 0)
    {
        std::cerr
            << "[CelestialPass] ENTER initialized="
            << m_planetRenderer.isInitialized()
            << std::endl;
    }




    if (!m_planetRenderer.isInitialized())
        return;

    const glm::mat4 celestialProj =
        makePerspectiveForCurrentViewport(
            70.0f,
            kCelestialNearPlane,
            kCelestialFarPlane
        );

    const glm::mat4 renderView =
        world::coordinates::makeRenderView(view);

    const double AU = 149597870700.0;
    const double MoonDistanceM = 384400000.0;

    const float EarthRadiusM = 6371000.0f;
    const float MoonRadiusM = 1737400.0f;
    const float SunRadiusM = 696340000.0f;

    const world::coordinates::WorldPosition sunWorldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(0.0, 0.0, 0.0)
        );

    const world::coordinates::WorldPosition earthWorldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(AU, 0.0, 0.0)
        );

    const world::coordinates::WorldPosition moonWorldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(AU, 0.0, MoonDistanceM)
        );

    const glm::vec3 sunCenterLocal =
        world::coordinates::toRenderLocal(
            sunWorldPosition,
            frame
        );

    const glm::vec3 earthCenterLocal =
        world::coordinates::toRenderLocal(
            earthWorldPosition,
            frame
        );

    const glm::vec3 moonCenterLocal =
        world::coordinates::toRenderLocal(
            moonWorldPosition,
            frame
        );

    static int celestialLogCounter = 0;

    if ((celestialLogCounter++ % 120) == 0)
    {
        std::cout
            << "[CelestialPass] "
            << "sunLocal=("
            << sunCenterLocal.x << ", "
            << sunCenterLocal.y << ", "
            << sunCenterLocal.z << ") "
            << "earthLocal=("
            << earthCenterLocal.x << ", "
            << earthCenterLocal.y << ", "
            << earthCenterLocal.z << ") "
            << "moonLocal=("
            << moonCenterLocal.x << ", "
            << moonCenterLocal.y << ", "
            << moonCenterLocal.z << ") "
            << "sunDist=" << glm::length(sunCenterLocal)
            << " moonDist=" << glm::length(moonCenterLocal)
            << std::endl;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // Солнце в 0,0,0.
    m_planetRenderer.renderSphereOnly(
        renderView,
        celestialProj,
        sunCenterLocal,
        SunRadiusM / EarthRadiusM,
        glm::vec3(1.0f, 0.92f, 0.70f),
        glm::vec3(1.0f, 0.98f, 0.88f),
        1.5f
    );

    // Земля на 1 AU.
    m_planetRenderer.render(
        renderView,
        celestialProj,
        earthCenterLocal,
        timeSeconds
    );

    // Луна рядом с Землёй.
   m_planetRenderer.renderSphereOnly(
        renderView,
        celestialProj,
        moonCenterLocal,
        MoonRadiusM / EarthRadiusM,
        glm::vec3(0.025f, 0.025f, 0.028f),
        glm::vec3(0.18f, 0.18f, 0.19f),
        0.0f
    );

    m_lastStats.drawCalls += 4;
}







void SceneRenderer::renderFarStationProxyPass(
    const ClientWorldState& world,
    const glm::mat4& view,
    const glm::vec3& cameraLocalPosition,
    const world::coordinates::WorldFrame& frame
)
{
    if (!m_debugLines || !m_debugLines->isInitialized())
        return;

    const glm::mat4 farStationProj =
        makePerspectiveForCurrentViewport(
            70.0f,
            kFarStationNearPlane,
            kFarStationFarPlane
        );

    const glm::mat4 renderView =
        world::coordinates::makeRenderView(view);

    const glm::mat4 vp =
        farStationProj * renderView;

    const glm::vec4 stationColor {0.20f, 0.70f, 1.00f, 0.42f};

    m_debugLines->begin();

    for (const auto& pair : world.objects())
    {
        const auto& obj = pair.second;

        if (obj.type != ObjectType::Station)
            continue;

        if (!obj.assembly)
            continue;

        const glm::vec3 objectLocalPosition =
            world::coordinates::toRenderLocal(
                obj.renderWorldPosition,
                frame
            );

const float distance =
    glm::length(objectLocalPosition - cameraLocalPosition);

        if (distance <= kStationFullRenderDistance)
            continue;



    const glm::vec3 center = 
        objectLocalPosition + obj.assembly->boundCenter;

        const float radius =
            std::max(obj.assembly->boundRadius, 1000.0f);

        addCircleXz(*m_debugLines, center, radius, 96, stationColor);
        addCircleXy(*m_debugLines, center, radius, 96, stationColor);
        addCircleYz(*m_debugLines, center, radius, 96, stationColor);

        const float cross = radius * 0.20f;

        m_debugLines->addLine(
            center + glm::vec3(-cross, 0.0f, 0.0f),
            center + glm::vec3( cross, 0.0f, 0.0f),
            stationColor
        );

        m_debugLines->addLine(
            center + glm::vec3(0.0f, -cross, 0.0f),
            center + glm::vec3(0.0f,  cross, 0.0f),
            stationColor
        );

        m_debugLines->addLine(
            center + glm::vec3(0.0f, 0.0f, -cross),
            center + glm::vec3(0.0f, 0.0f,  cross),
            stationColor
        );

        m_lastStats.drawCalls += 1;
    }

    m_debugLines->end(vp);
}















PreparedScene SceneRenderer::prepareScene(
    const ClientWorldState& world,
    EntityId playerId
)
{
    PreparedScene prepared;
    prepared.world = &world;
    prepared.playerId = playerId;
    prepared.valid = false;

    const auto& ships = world.ships();

    auto itPlayer = ships.find(playerId.value);
    if (itPlayer == ships.end())
        return prepared;

    prepared.frame =
        world::coordinates::makeRenderFrameFromCamera(
            itPlayer->second.renderTransform.worldPosition
        );


        prepared.realShipMeshes.clear();

        for (const auto& pair : ships)
        {
            const auto& ship = pair.second;

            if (!ship.assembly)
                continue;

            glm::mat4 shipModel =
                world::coordinates::makeRenderModelMatrix(
                    ship.renderTransform.worldPosition,
                    glm::mat4(ship.renderTransform.orientation),
                    prepared.frame
                );

            for (const auto& module : ship.assembly->modules)
            {
                glm::mat4 moduleModel =
                    buildAssemblyModuleModel(
                        shipModel,
                        module,
                        ship.assemblyModules
                    );

                for (const auto& part : module.meshes)
                {
                    if (ship.hiddenPartIds.find(part.id) != ship.hiddenPartIds.end())
                        continue;

                    PreparedScene::RealShipMeshItem item;
                    item.gpuLod0 = &part.lod0Gpu;
                    item.gpuLod1 = &part.lod1Gpu;
                    item.model = glm::translate(moduleModel, part.localOffset);
                    item.boundCenter = part.boundCenter;
                    item.boundHalfSize = part.boundHalfSize;
                    item.lodSwitchDistance = ship.assembly->lodSwitchDistance;

                    prepared.realShipMeshes.push_back(item);
                }
            }
        }



        prepared.objectMeshes.clear();

        for (const auto& pair : world.objects())
        {
            const auto& obj = pair.second;

            if (!obj.assembly)
                continue;

            glm::mat4 objectBaseModel =
                world::coordinates::makeRenderModelMatrix(
                    obj.renderWorldPosition,
                    obj.renderOrientation,
                    prepared.frame
                );

            const auto& renderAssemblyModules =
                obj.renderAssemblyModules.empty()
                    ? obj.assemblyModules
                    : obj.renderAssemblyModules;

            for (const auto& module : obj.assembly->modules)
            {
                glm::mat4 moduleModel =
                    buildAssemblyModuleModel(
                        objectBaseModel,
                        module,
                        renderAssemblyModules
                    );

                for (const auto& part : module.meshes)
                {
                    if (obj.hiddenPartIds.find(part.id) != obj.hiddenPartIds.end())
                        continue;

                    PreparedScene::ObjectMeshItem item;
                    item.type = obj.type;
                    item.gpuLod0 = &part.lod0Gpu;
                    item.gpuLod1 = &part.lod1Gpu;
                    item.model = glm::translate(moduleModel, part.localOffset);
                    item.boundCenter = part.boundCenter;
                    item.boundHalfSize = part.boundHalfSize;
                    item.lodSwitchDistance = obj.assembly->lodSwitchDistance;

                    prepared.objectMeshes.push_back(item);
                }
            }
        }




        prepared.visualShips.clear();
        prepared.visualShipParts.clear();

        prepared.visualShips.reserve(world.visualShips().size());
        prepared.visualShipParts.reserve(512);

        for (const auto& ship : world.visualShips())
        {
            if (!ship.visible)
                continue;

            if (!ship.descriptor || !ship.assembly)
                continue;

            glm::mat4 shipModel =
                world::coordinates::makeRenderModelMatrix(
                    ship.renderTransform.worldPosition,
                    glm::mat4(ship.renderTransform.orientation),
                    prepared.frame
                );

            if (ship.visualScale != 1.0f)
            {
                shipModel =
                    glm::scale(
                        shipModel,
                        glm::vec3(ship.visualScale)
                    );
            }

            PreparedScene::VisualShipItem shipItem;
            shipItem.wholeShipProxyGpu = &ship.assembly->wholeShipProxyGpu;
            shipItem.model = shipModel;
            shipItem.boundCenter = ship.assembly->boundCenter;
            shipItem.boundRadius = ship.assembly->boundRadius;
            shipItem.hasWholeShipProxy = ship.assembly->hasWholeShipProxy;
            shipItem.lodSwitchDistance = ship.assembly->lodSwitchDistance;

            const int shipIndex =
                static_cast<int>(prepared.visualShips.size());

            prepared.visualShips.push_back(shipItem);

            // ВАЖНО:
            // если корабль далеко и у него есть wholeShipProxy,
            // НЕ собираем его модули и части в PreparedScene.
            // Иначе proxy рисуется, но CPU уже сжёг время на полный корабль.
            const glm::vec3 shipLocalPosition =
                glm::vec3(shipModel * glm::vec4(0, 0, 0, 1));

            const float distToShip =
                glm::length(shipLocalPosition);

            

            const bool willUseWholeShipProxy =
                ship.assembly->hasWholeShipProxy &&
                distToShip > kVisualShipFullPrepareDistance;

            if (willUseWholeShipProxy)
                continue;

            for (const auto& module : ship.assembly->modules)
            {
                glm::mat4 moduleModel =
                    buildAssemblyModuleBaseModel(
                        shipModel,
                        module
                    );

                const glm::vec3 moduleLocalPosition =
                    glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

                const float moduleDistanceBias =
                    glm::length(moduleLocalPosition);

                for (const auto& part : module.meshes)
                {
                    glm::mat4 partModel =
                        glm::translate(moduleModel, part.localOffset);

                    PreparedScene::VisualShipPartItem partItem;
                    partItem.shipIndex = shipIndex;
                    partItem.gpuLod0 = &part.lod0Gpu;
                    partItem.gpuLod1 = &part.lod1Gpu;
                    partItem.model = partModel;
                    partItem.boundCenter = part.boundCenter;
                    partItem.boundHalfSize = part.boundHalfSize;
                    partItem.moduleDistanceBias = moduleDistanceBias;

                    prepared.visualShipParts.push_back(partItem);
                }
            }
        }














    prepared.valid = true;
    return prepared;
}

void SceneRenderer::renderPrepared(
    const PreparedScene& prepared,
    const SceneCameraParams& camera,
    const SceneRenderPolicy& policy
)
{
    if (!prepared.valid || !prepared.world)
        return;

    renderInternal(
        prepared,
        camera.view,
        camera.proj,
        camera.cameraId,
        camera.cameraName,
        policy
    );
}

void SceneRenderer::render(
    const ClientWorldState& world,
    EntityId playerId,
    const glm::mat4& view,
    const glm::mat4& proj,
    int cameraId,
    const std::string& cameraName,
    const SceneRenderPolicy& policy
)
{
    PreparedScene prepared = prepareScene(world, playerId);

    SceneCameraParams camera;
    camera.view = view;
    camera.proj = proj;
    camera.cameraId = cameraId;
    camera.cameraName = cameraName;

    renderPrepared(
        prepared,
        camera,
        policy
    );
}



















void SceneRenderer::renderInternal(
    const PreparedScene& prepared,
    const glm::mat4& view,
    const glm::mat4& proj,
    int cameraId,
    const std::string& cameraName,
    const SceneRenderPolicy& policy
)
{
    if (!prepared.valid || !prepared.world)
        return;

    const ClientWorldState& world = *prepared.world;
    EntityId playerId = prepared.playerId;
    const world::coordinates::WorldFrame& frame = prepared.frame;



    const auto& ships = world.ships();
    const auto& objects = world.objects();


    // --- browser DEBUG ---
    const auto& dbg = debug::get().render;
    m_frameCounter++;

    m_lastStats.reset();



    const double profileStartMs = renderProfileNowMs();

    double profileAfterSetupMs = profileStartMs;
    double profileAfterStarfieldMs = profileStartMs;
    double profileAfterFarPassesMs = profileStartMs;
    double profileAfterLabelsMs = profileStartMs;
    double profileAfterRealShipsMs = profileStartMs;
    double profileAfterVisualShipsMs = profileStartMs;
    double profileAfterVisualDronesMs = profileStartMs;
    double profileAfterObjectsMs = profileStartMs;
    double profileAfterDebugCallbackMs = profileStartMs;









    static float debugTimer = 0;
    debugTimer += 0.016f; // примерно dt, но лучше передавать dt параметром
    
    bool shouldDebug = false;
    if(m_debugCallback  && cameraName == "mainCam")
    {
        // Отправляем раз в 5 кадров для производительности
        shouldDebug = (m_frameCounter % 5 == 0);
    }
    
    DebugFrameData debugData;
    if(shouldDebug)
    {
        debugData.frameNumber = m_frameCounter;
        debugData.timestamp = glfwGetTime();
        
        // Заполняем информацию о камере
        debugData.camera.cameraId = cameraId;          
        debugData.camera.cameraName = cameraName;
        glm::mat4 invView = glm::inverse(view);
        debugData.camera.position = glm::vec3(invView[3]);
        debugData.camera.direction = -glm::vec3(invView[2]); // forward в OpenGL
        debugData.camera.up = glm::vec3(invView[1]);
        
        // Извлекаем параметры проекции (упрощенно)
        debugData.camera.fov = 45.0f; // временно
        debugData.camera.aspect = 16.0f/9.0f;
        debugData.camera.nearPlane = 0.1f;
        debugData.camera.farPlane = 100000.0f;
        
        // Вычисляем углы фрустума в world space
        glm::mat4 invViewProj = glm::inverse(proj * view);
        glm::vec3 ndc[8] = {
            {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
            {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}
        };
        
        for(int i = 0; i < 8; i++)
        {
            glm::vec4 p = invViewProj * glm::vec4(ndc[i], 1.0f);
            debugData.camera.frustumCorners[i] = glm::vec3(p) / p.w;
        }
    }
    // --- browser END DEBUG ---

    // Инициализируем отладчик при первом рендере
    if (!m_initialized) {
        m_initialized = m_debugLines->initialize();
        if (!m_initialized) {
            std::cerr << "Failed to initialize debug renderer" << std::endl;
        }
    }

// В render-frame камера находится в локальном нуле.
// Все world objects уже переведены через WorldFrame -> render-local.
const glm::vec3 cameraLocalPosition(0.0f);


// auto itPlayer = ships.find(playerId.value);

// if (itPlayer == ships.end())
//     return;

// const world::coordinates::WorldFrame frame =
//     world::coordinates::makeRenderFrameFromCamera(
//         itPlayer->second.renderTransform.worldPosition
//     );


   
glm::mat4 renderView =
    world::coordinates::makeRenderView(view);


profileAfterSetupMs = renderProfileNowMs();
    
    // Включаем необходимые состояния
    glDepthFunc(GL_LESS);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ============================================================
    // 0. BACKGROUND: STARFIELD
    // ============================================================
    // Рисуем первым. Он не пишет в depth buffer.
    // Потом станция/корабли просто перерисуют его поверх.
    
    if (policy.drawStarfield &&
        dbg.shouldRenderStarfield() &&
        m_starfieldRenderer.isInitialized())
    {
        m_starfieldRenderer.render(renderView, proj);
    }

    profileAfterStarfieldMs = renderProfileNowMs();



    // Возвращаем нормальный state для мира.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);





    // ============================================================
    // 0.5. FAR PASSES
    // ============================================================
    // Дальние объекты рисуются отдельными projection-матрицами.
    // Потом depth очищается, чтобы они не портили точность ближнего мира.
    const bool isRearMonitor = (cameraName == "secondCam");

    // Планеты / sky / celestial objects должны быть видны во всех камерах,
    // включая задний монитор. Иначе при взгляде назад пропадает планета.
    

    // Дальний proxy станции можно отключать только для маленького rear-monitor,
    // но не для обычного главного вида, даже если он смотрит назад.
    if (policy.drawFarStationProxy)
    {
        renderFarStationProxyPass(
            world,
            view,
            cameraLocalPosition,
            frame
        );
    }

    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);



    profileAfterFarPassesMs = renderProfileNowMs();










    // Starfield is rendered later, after world geometry, as a depth-tested
    // background pass. That makes it immune to accidental clears/background
    // passes and prevents it from covering ships or large objects.
    
    GLuint fillShader = ShaderLibrary::instance().get("mesh_fill");
    GLuint largeObjectShader = ShaderLibrary::instance().get("large_object_shader");
    GLuint edgeShader = ShaderLibrary::instance().get("edge_shader");

    
   
    
    // обновляем frustum
    Frustum frustum;
    glm::mat4 vp = proj * renderView;
    frustum.update(vp);


    const bool drawWorldAxes = dbg.shouldDrawWorldAxes();
    const bool drawObjectAxes = dbg.shouldDrawObjectAxes();

    if (drawWorldAxes)
    {
        m_debugRenderer.renderAxes(
            glm::mat4(1.0f),
            renderView,
            proj,
            dbg.worldAxisLength
        );
    }



    if (policy.drawLabels)
    {
        renderStarSystemLabels(renderView, proj);
        renderConstellationHoverOverlay(renderView, proj);
    }



    profileAfterLabelsMs = renderProfileNowMs();



    // ============================================================
    // 1. РЕНДЕРИНГ КОРАБЛЕЙ (кроме игрока)
    // ============================================================
    
    #if 0

    for (const auto& pair : ships)
    {
        // if (pair.first == playerId.value)
        //     continue;


        const auto& ship = pair.second;

        const glm::vec3 shipLocalPosition =
            world::coordinates::toRenderLocal(
                ship.renderTransform.worldPosition,
                frame
            );

        glm::mat4 shipModel =
            world::coordinates::makeRenderModelMatrix(
                ship.renderTransform.worldPosition,
                glm::mat4(ship.renderTransform.orientation),
                frame
            );


        const glm::vec3 shipForward = ship.renderTransform.forward();
        const glm::vec3 shipUp      = ship.renderTransform.up();
        const glm::vec3 shipRight   = ship.renderTransform.right();

        // Параметры освещения
        LightingParams shipParams = LightingParams::ship();
            

        // ===== НОВОЕ: модульная сборка =====
                // ===== НОВОЕ: модульная сборка =====
        if (ship.assembly)
        {
            bool visible = frustum.sphereVisible(
                ship.assembly->boundCenter,
                ship.assembly->boundRadius,
                shipModel
            );


           if (shouldDebug && dbg.publishObjectOrientation)
            {
                DebugShipInfo shipInfo;
                shipInfo.id = pair.first;
                shipInfo.position = shipLocalPosition;
                shipInfo.forward = shipForward;
                shipInfo.up = shipUp;
                shipInfo.right = shipRight;
                shipInfo.radius = ship.assembly->boundRadius;
                shipInfo.visible = visible;
                shipInfo.distance = glm::length(shipLocalPosition - cameraLocalPosition);
                shipInfo.type = "Ship";

                debugData.ships.push_back(shipInfo);
            }




            if (!visible)
                continue;


            m_lastStats.realShipsDrawn++;

            if (drawObjectAxes)
            {
                m_debugRenderer.renderAxes(
                    shipModel,
                    renderView,
                    proj,
                    dbg.shipAxisLength
                );
            }

            

            std::vector<QueuedMeshDraw> queuedShipMeshDraws;

            for (const auto& module : ship.assembly->modules)
            {
                glm::mat4 moduleBaseModel =
                    buildAssemblyModuleBaseModel(
                        shipModel,
                        module
                    );

                glm::mat4 moduleModel =
                    buildAssemblyModuleModel(
                        shipModel,
                        module,
                        ship.assemblyModules
                    );





                   

                
                

                glm::vec3 moduleWorldPos = glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

                float distToModule = glm::length(moduleWorldPos - cameraLocalPosition);
                bool useLod1 = (distToModule >= ship.assembly->lodSwitchDistance);

                const float moduleRadius = safeRadiusFromHalfSize(module.boundHalfSize);

                if (!frustum.sphereVisible(module.boundCenter, moduleRadius, moduleModel))
                {
                    m_lastStats.modulesCulled++;
                    continue;
                }

                m_lastStats.modulesDrawn++;



                if (dbg.shouldDrawMeshes())
                {
                    for (const auto& part : module.meshes)
                    {
                        if (ship.hiddenPartIds.find(part.id) != ship.hiddenPartIds.end())
                            continue;

                        glm::mat4 partModel = glm::translate(moduleModel, part.localOffset);

                        const float partRadius = safeRadiusFromHalfSize(part.boundHalfSize);

                        if (!frustum.sphereVisible(part.boundCenter, partRadius, partModel))
                        {
                            m_lastStats.partsCulled++;
                            continue;
                        }

                        m_lastStats.partsDrawn++;
                        m_lastStats.realShipPartsDrawn++;
                        m_lastStats.drawCalls += 2; // fill + edge

                        glm::mat4 partMvp = proj * renderView * partModel;

                        const render::MeshGPU& gpu = useLod1 ? part.lod1Gpu : part.lod0Gpu;

                        queuedShipMeshDraws.push_back(
                            QueuedMeshDraw{
                                &gpu,
                                partMvp,
                                partModel
                            }
                        );
                    }
                }



                    if (dbg.drawModulePivots)
                    {
                        glm::vec3 moduleOriginWorld = glm::vec3(moduleBaseModel[3]);

                        glm::mat4 pivotAxesModel =
                            moduleModel * glm::translate(glm::mat4(1.0f), module.pivot);

                        glm::vec3 pivotWorld = glm::vec3(pivotAxesModel[3]);

                        glm::vec3 rotationAxisWorld =
                            glm::normalize(glm::mat3(moduleModel) * module.rotationAxis);

                        m_debugRenderer.renderCross(
                            moduleOriginWorld,
                            dbg.moduleCrossSize,
                            dbg.moduleOriginColor,
                            renderView,
                            proj
                        );

                        m_debugRenderer.renderCross(
                            pivotWorld,
                            dbg.moduleCrossSize,
                            dbg.modulePivotColor,
                            renderView,
                            proj
                        );

                        m_debugRenderer.renderAxes(
                            pivotAxesModel,
                            renderView,
                            proj,
                            dbg.moduleAxisLength
                        );

                        m_debugRenderer.renderLine(
                            pivotWorld,
                            pivotWorld + rotationAxisWorld * dbg.rotAxisLength,
                            dbg.rotationAxisColor,
                            renderView,
                            proj
                        );
                    }

                    





            }



        if (dbg.shouldDrawMeshes())
        {
            // drawQueuedMeshPasses(
            //     m_meshRenderer,
            //     queuedShipMeshDraws,
            //     largeObjectShader,
            //     edgeShader,
            //     shipParams,
            //     cameraLocalPosition
            // );

            drawQueuedMeshPasses(
                m_meshRenderer,
                queuedShipMeshDraws,
                fillShader,
                edgeShader,
                shipParams,
                cameraLocalPosition
            );



        renderDetachedAssemblyModules(
            m_meshRenderer,
            m_debugLines.get(),
            ship,
            shipModel,
            view,
            proj,
            cameraLocalPosition,
            frame,
            fillShader,
            edgeShader,
            shipParams
        );
    
    }



            debug::render::ServerHitVolumeRenderer::render(
                *m_debugLines,
                ship.debugHitVolumes,
                shipModel,
                renderView,
                proj
            );



        if (dbg.drawModulePivots)
        {
            for (const auto& job : ship.repairJobs)
            {
                const glm::vec4 droneColor(0.2f, 1.0f, 0.2f, 1.0f);
                const glm::vec4 towColor(0.2f, 0.8f, 1.0f, 1.0f);
                const glm::vec4 homeColor(1.0f, 0.8f, 0.2f, 1.0f);

                const glm::vec3 droneLocal =
                    world::coordinates::toRenderLocal(job.droneWorldPosition, frame);

                const glm::vec3 fragmentLocal =
                    world::coordinates::toRenderLocal(job.fragmentWorldPosition, frame);

                const glm::vec3 homeLocal =
                    world::coordinates::toRenderLocal(job.homeWorldPosition, frame);

                m_debugRenderer.renderCross(
                    droneLocal,
                    0.6f,
                    droneColor,
                    renderView,
                    proj
                );

                m_debugRenderer.renderLine(
                    droneLocal,
                    fragmentLocal,
                    towColor,
                    renderView,
                    proj
                );

                m_debugRenderer.renderLine(
                    fragmentLocal,
                    homeLocal,
                    homeColor,
                    renderView,
                    proj
                );

                m_debugRenderer.renderCross(
                    homeLocal,
                    0.4f,
                    homeColor,
                    renderView,
                    proj
                );
            }
        }







            continue;
        }
    }

// ============================================================
    // 1.5. РЕНДЕРИНГ ЛЁГКИХ ВИЗУАЛЬНЫХ КОРАБЛЕЙ
    // ============================================================
    
    profileAfterRealShipsMs = renderProfileNowMs();
    
#endif






// ============================================================
// 1B. РЕНДЕРИНГ PREPARED REAL SHIPS
// ============================================================
{
    LightingParams shipParams = LightingParams::ship();

    std::vector<QueuedMeshDraw> queuedPreparedShipDraws;
    queuedPreparedShipDraws.reserve(prepared.realShipMeshes.size());

    if (dbg.shouldDrawMeshes())
    {
        for (const auto& item : prepared.realShipMeshes)
        {
            const float partRadius = safeRadiusFromHalfSize(item.boundHalfSize);

            if (!frustum.sphereVisible(item.boundCenter, partRadius, item.model))
            {
                m_lastStats.partsCulled++;
                continue;
            }

            const glm::vec3 partLocalPos =
                glm::vec3(item.model * glm::vec4(0, 0, 0, 1));

            const float distToPart =
                glm::length(partLocalPos - cameraLocalPosition);

            const bool useLod1 =
                distToPart >= item.lodSwitchDistance;

            const render::MeshGPU* gpu =
                useLod1 ? item.gpuLod1 : item.gpuLod0;

            if (!gpu)
                continue;

            glm::mat4 mvp = proj * renderView * item.model;

            queuedPreparedShipDraws.push_back(
                QueuedMeshDraw{
                    gpu,
                    mvp,
                    item.model
                }
            );

            m_lastStats.partsDrawn++;
            m_lastStats.realShipPartsDrawn++;
            m_lastStats.drawCalls += 2;
        }

        drawQueuedMeshPasses(
            m_meshRenderer,
            queuedPreparedShipDraws,
            fillShader,
            edgeShader,
            shipParams,
            cameraLocalPosition
        );
    }

    profileAfterRealShipsMs = renderProfileNowMs();
}















        if (policy.drawVisualShips)
        {
            const auto& dbg = debug::get().render;

            if (dbg.shouldDrawMeshes())
            {
                LightingParams shipParams = LightingParams::ship();

                const glm::mat4 renderViewForVisual =
                    world::coordinates::makeRenderView(view);

                int visualShipsActuallyDrawn = 0;

                std::vector<char> visualShipVisible;
                visualShipVisible.resize(prepared.visualShips.size(), 0);

                std::vector<char> visualShipUseProxy;
                visualShipUseProxy.resize(prepared.visualShips.size(), 0);

                std::unordered_map<const render::MeshGPU*, std::vector<glm::mat4>> proxyInstanceGroups;
                proxyInstanceGroups.reserve(8);

                // Proxy edges временно отключены.
                // Полная edge-сетка proxy-меша на дальних кораблях даёт "медуз".
                // Для proxy пока оставляем только instanced fill.
                // std::unordered_map<const render::MeshGPU*, std::vector<glm::mat4>> proxyEdgeInstanceGroups;
                // proxyEdgeInstanceGroups.reserve(8);

                // Дальше этого расстояния edges для proxy ships вообще не рисуем.
                // Не делаем коротким: пусть силуэт держится достаточно долго.
                constexpr float kProxyEdgeFadeStart = 900.0f;
                constexpr float kProxyEdgeFadeEnd   = 1800.0f;


                for (size_t i = 0; i < prepared.visualShips.size(); ++i)
                {
                    const auto& ship = prepared.visualShips[i];

                    if (policy.maxVisualShipsToDraw >= 0 &&
                        visualShipsActuallyDrawn >= policy.maxVisualShipsToDraw)
                    {
                        m_lastStats.visualShipsCulled++;
                        continue;
                    }

                    if (!frustum.sphereVisible(
                            ship.boundCenter,
                            ship.boundRadius,
                            ship.model
                        ))
                    {
                        m_lastStats.visualShipsCulled++;
                        continue;
                    }

                    const glm::vec3 shipLocalPosition =
                        glm::vec3(ship.model * glm::vec4(0, 0, 0, 1));

                    const float distToShip =
                        glm::length(shipLocalPosition - cameraLocalPosition);

                    

                    const bool useWholeShipProxy =
                        ship.hasWholeShipProxy &&
                        distToShip > kVisualShipProxyRenderDistance;

                    visualShipVisible[i] = 1;
                    visualShipUseProxy[i] = useWholeShipProxy ? 1 : 0;

                    m_lastStats.visualShipsDrawn++;
                    visualShipsActuallyDrawn++;

                    if (useWholeShipProxy)
                    {
                        m_lastStats.visualProxyShipsDrawn++;

                        proxyInstanceGroups[ship.wholeShipProxyGpu].push_back(ship.model);
                        
                        // Proxy edge-pass отключён намеренно.
                        // Дальние proxy с полной edge-сеткой выглядят как артефактные "медузы".
                        // if (distToShip <= kProxyEdgeFadeEnd)
                        // {
                        //     proxyEdgeInstanceGroups[ship.wholeShipProxyGpu].push_back(ship.model);
                        // }

                            m_lastStats.partsDrawn++;
                            m_lastStats.visualShipPartsDrawn++;

                            // Пока считаем логически как fill+edge.
                            // Edge-pass позже тоже надо будет сделать instanced.
                            m_lastStats.drawCalls += 2;
                    }
                    else
                    {
                        m_lastStats.visualFullShipsDrawn++;
                    }
                }





                GLuint instancedFillShader =
                    ShaderLibrary::instance().get("mesh_fill_instanced");

                const glm::mat4 vpInstanced =
                    proj * renderViewForVisual;

                if (instancedFillShader != 0)
                {
                    for (const auto& group : proxyInstanceGroups)
                    {
                        const render::MeshGPU* gpu = group.first;
                        const std::vector<glm::mat4>& models = group.second;

                        if (!gpu || models.empty())
                            continue;

                        m_meshRenderer.drawInstanced(
                            *gpu,
                            instancedFillShader,
                            vpInstanced,
                            models,
                            shipParams,
                            cameraLocalPosition
                        );
                    }





                        // GLuint instancedEdgeShader =
                        //     ShaderLibrary::instance().get("edge_shader_instanced");

                        // if (instancedEdgeShader != 0)
                        // {
                        //     for (const auto& group : proxyEdgeInstanceGroups)
                        //     {
                        //         const render::MeshGPU* gpu = group.first;
                        //         const std::vector<glm::mat4>& models = group.second;

                        //         if (!gpu || models.empty())
                        //             continue;

                        //         m_meshRenderer.drawEdgesInstanced(
                        //             *gpu,
                        //             instancedEdgeShader,
                        //             vpInstanced,
                        //             models,
                        //             cameraLocalPosition,
                        //             kProxyEdgeFadeStart,
                        //             kProxyEdgeFadeEnd,
                        //             glm::vec3(0.18f, 0.78f, 1.0f),
                        //             0.72f
                        //         );
                        //     }
                        // }
                }
                else
                {
                    // Fallback, если shader не загрузился.
                    std::vector<QueuedMeshDraw> queuedProxyDraws;

                    for (const auto& group : proxyInstanceGroups)
                    {
                        const render::MeshGPU* gpu = group.first;

                        if (!gpu)
                            continue;

                        for (const glm::mat4& model : group.second)
                        {
                            queuedProxyDraws.push_back(
                                QueuedMeshDraw{
                                    gpu,
                                    proj * renderViewForVisual * model,
                                    model
                                }
                            );
                        }
                    }

                    drawQueuedMeshFillOnly(
                        m_meshRenderer,
                        queuedProxyDraws,
                        fillShader,
                        shipParams,
                        cameraLocalPosition
                    );
                }









                std::vector<QueuedMeshDraw> queuedVisualShipMeshDraws;
                queuedVisualShipMeshDraws.reserve(prepared.visualShipParts.size()); 

                for (const auto& part : prepared.visualShipParts)
                {
                    if (part.shipIndex < 0 ||
                        part.shipIndex >= static_cast<int>(visualShipVisible.size()))
                    {
                        continue;
                    }

                    if (!visualShipVisible[part.shipIndex])
                        continue;

                    if (visualShipUseProxy[part.shipIndex])
                        continue;

                    const float partRadius =
                        safeRadiusFromHalfSize(part.boundHalfSize);

                    if (!frustum.sphereVisible(
                            part.boundCenter,
                            partRadius,
                            part.model
                        ))
                    {
                        m_lastStats.partsCulled++;
                        continue;
                    }

                    const glm::vec3 partLocalPosition =
                        glm::vec3(part.model * glm::vec4(0, 0, 0, 1));

                    const float distToPart =
                        glm::length(partLocalPosition - cameraLocalPosition);

                    bool useLod1 =
                        distToPart >= prepared.visualShips[part.shipIndex].lodSwitchDistance;

                    if (distToPart > 350.0f)
                        useLod1 = true;

                    const render::MeshGPU* gpu =
                        useLod1 ? part.gpuLod1 : part.gpuLod0;

                    if (!gpu)
                        continue;

                    glm::mat4 partMvp =
                        proj * renderViewForVisual * part.model;

                    queuedVisualShipMeshDraws.push_back(
                        QueuedMeshDraw{
                            gpu,
                            partMvp,
                            part.model
                        }
                    );

                    m_lastStats.partsDrawn++;
                    m_lastStats.visualShipPartsDrawn++;
                    m_lastStats.drawCalls += 2;
                }

                drawQueuedVisualShipMeshPasses(
                    m_meshRenderer,
                    queuedVisualShipMeshDraws,
                    fillShader,
                    edgeShader,
                    shipParams,
                    cameraLocalPosition
                );
            }
        }


    profileAfterVisualShipsMs = renderProfileNowMs();



// ============================================================
// 1.6. РЕНДЕРИНГ Дронов
// ============================================================


if (policy.drawVisualDrones)
{
    renderVisualDrones(
        world,
        frustum,
        view,
        proj,
        cameraLocalPosition,
        frame,
        fillShader,
        edgeShader
    );
}

profileAfterVisualDronesMs = renderProfileNowMs();





// ============================================================
// 2. РЕНДЕРИНГ СТАЦИОНАРНЫХ ОБЪЕКТОВ (станции, астероиды, планеты)
// ============================================================
#if 0


if (policy.drawObjects)
{

    for (const auto& pair : objects)
    {
        const auto& obj = pair.second;

        // --------------------------------------------------------
        // 1. OBJECT ASSEMBLY PATH
        // --------------------------------------------------------
        if (obj.assembly)
        {

        const glm::vec3 objectLocalPosition =
            world::coordinates::toRenderLocal(
                obj.renderWorldPosition,
                frame
            );

        glm::mat4 objectBaseModel =
            world::coordinates::makeRenderModelMatrix(
                obj.renderWorldPosition,
                obj.renderOrientation,
                frame
            );

        const float objectDistance =
            glm::length(objectLocalPosition - cameraLocalPosition);

            // if (
            //     obj.type == ObjectType::Station &&
            //     objectDistance > kStationFullRenderDistance
            // )
            // {
            //     // Дальняя станция уже нарисована в far station proxy pass.
            //     // Полную модульную сборку на таком расстоянии не рисуем.
            //     continue;
            // }

            const glm::vec3 objectForward = game::math::forwardFromOrientation(obj.renderOrientation);
            const glm::vec3 objectUp      = game::math::upFromOrientation(obj.renderOrientation);
            const glm::vec3 objectRight   = game::math::rightFromOrientation(obj.renderOrientation);


            bool visible = frustum.sphereVisible(
                obj.assembly->boundCenter,
                obj.assembly->boundRadius,
                objectBaseModel
            );

            if (shouldDebug && dbg.publishObjectOrientation)
            {
                DebugObjectInfo objInfo;
                objInfo.id = pair.first;
                objInfo.position = objectLocalPosition;
                objInfo.forward = objectForward;
                objInfo.up = objectUp;
                objInfo.right = objectRight;
                objInfo.type = std::to_string(static_cast<int>(obj.type));
                objInfo.distance = objectDistance;
                objInfo.visible = visible;

                debugData.objects.push_back(objInfo);
            }

            if (!visible)
                continue;

            if (drawObjectAxes)
            {
                m_debugRenderer.renderAxes(
                    objectBaseModel,
                    renderView,
                    proj,
                    dbg.objectAxisLength
                );
            }


            LightingParams params;
            switch (obj.type)
            {
                case ObjectType::Station:
                    params = LightingParams::station();
                    break;
                case ObjectType::Asteroid:
                    params = LightingParams::asteroid();
                    break;
                case ObjectType::Planet:
                    params = LightingParams::planet();
                    break;
                default:
                    break;
            }

            std::vector<QueuedMeshDraw> queuedObjectMeshDraws;

            for (const auto& module : obj.assembly->modules)
            {
                glm::mat4 moduleBaseModel =
                    buildAssemblyModuleBaseModel(
                        objectBaseModel,
                        module
                    );

                const auto& renderAssemblyModules =
                    obj.renderAssemblyModules.empty()
                        ? obj.assemblyModules
                        : obj.renderAssemblyModules;

                glm::mat4 moduleModel =
                    buildAssemblyModuleModel(
                        objectBaseModel,
                        module,
                        renderAssemblyModules
                    );


                

                glm::vec3 moduleWorldPos = glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));



                





                float distToModule = glm::length(moduleWorldPos - cameraLocalPosition);
                bool useLod1 = (distToModule >= obj.assembly->lodSwitchDistance);

                const float moduleRadius = safeRadiusFromHalfSize(module.boundHalfSize);

                if (!frustum.sphereVisible(module.boundCenter, moduleRadius, moduleModel))
                {
                    m_lastStats.modulesCulled++;
                    continue;
                }

                m_lastStats.modulesDrawn++;

                


                if (dbg.shouldDrawMeshes())
                {
                    for (const auto& part : module.meshes)
                    {
                        glm::mat4 partModel = glm::translate(moduleModel, part.localOffset);

                        const float partRadius = safeRadiusFromHalfSize(part.boundHalfSize);

                        if (!frustum.sphereVisible(part.boundCenter, partRadius, partModel))
                        {
                            m_lastStats.partsCulled++;
                            continue;
                        }

                        m_lastStats.partsDrawn++;
                        m_lastStats.drawCalls += 2; // fill + edge

                        glm::mat4 partMvp = proj * renderView * partModel;

                        if (obj.hiddenPartIds.find(part.id) != obj.hiddenPartIds.end())
                            continue;

                        const render::MeshGPU& gpu = useLod1 ? part.lod1Gpu : part.lod0Gpu;

                        queuedObjectMeshDraws.push_back(
                            QueuedMeshDraw{
                                &gpu,
                                partMvp,
                                partModel
                            }
                        );
                    }
                }



                if (dbg.drawModulePivots)
                {
                    glm::vec3 moduleOriginWorld = glm::vec3(moduleBaseModel[3]);

                    glm::mat4 pivotAxesModel =
                        moduleModel * glm::translate(glm::mat4(1.0f), module.pivot);

                    glm::vec3 pivotWorld = glm::vec3(pivotAxesModel[3]);

                    glm::vec3 rotationAxisWorld =
                        glm::normalize(glm::mat3(moduleModel) * module.rotationAxis);

                    m_debugRenderer.renderCross(
                        moduleOriginWorld,
                        dbg.moduleCrossSize,
                        dbg.moduleOriginColor,
                        renderView,
                        proj
                    );

                    m_debugRenderer.renderCross(
                        pivotWorld,
                        dbg.moduleCrossSize,
                        dbg.modulePivotColor,
                        renderView,
                        proj
                    );

                    m_debugRenderer.renderAxes(
                        pivotAxesModel,
                        renderView,
                        proj,
                        dbg.moduleAxisLength
                    );

                    m_debugRenderer.renderLine(
                        pivotWorld,
                        pivotWorld + rotationAxisWorld * dbg.rotAxisLength,
                        dbg.rotationAxisColor,
                        renderView,
                        proj
                    );
                }

            }



            if (dbg.shouldDrawMeshes())
            {
                drawQueuedMeshPasses(
                    m_meshRenderer,
                    queuedObjectMeshDraws,
                    largeObjectShader,
                    edgeShader,
                    params,
                    cameraLocalPosition
                );

                renderDetachedAssemblyModules(
                m_meshRenderer,
                m_debugLines.get(),
                obj,
                    objectBaseModel,
                    view,
                    proj,
                    cameraLocalPosition,
                    frame,
                    largeObjectShader,
                    edgeShader,
                    params
                );
            }

        
                debug::render::ServerHitVolumeRenderer::render(
                    *m_debugLines,
                    obj.debugHitVolumes,
                    objectBaseModel,
                    renderView,
                    proj
                );
            

            continue;
        }

    

    
    }


}


#endif



    // ============================================================
    // 2B. РЕНДЕРИНГ PREPARED OBJECTS / STATIONS
    // ============================================================

    if (policy.drawObjects && dbg.shouldDrawMeshes())
    {
        std::vector<QueuedMeshDraw> queuedPreparedStationDraws;
        std::vector<QueuedMeshDraw> queuedPreparedAsteroidDraws;
        std::vector<QueuedMeshDraw> queuedPreparedPlanetDraws;
        std::vector<QueuedMeshDraw> queuedPreparedDefaultDraws;

        for (const auto& item : prepared.objectMeshes)
        {
            const float partRadius = safeRadiusFromHalfSize(item.boundHalfSize);

            if (!frustum.sphereVisible(item.boundCenter, partRadius, item.model))
            {
                m_lastStats.partsCulled++;
                continue;
            }

            const glm::vec3 partLocalPos =
                glm::vec3(item.model * glm::vec4(0, 0, 0, 1));

            const float distToPart =
                glm::length(partLocalPos - cameraLocalPosition);

            const bool useLod1 =
                distToPart >= item.lodSwitchDistance;

            const render::MeshGPU* gpu =
                useLod1 ? item.gpuLod1 : item.gpuLod0;

            if (!gpu)
                continue;

            glm::mat4 mvp = proj * renderView * item.model;

            QueuedMeshDraw draw{
                gpu,
                mvp,
                item.model
            };

            switch (item.type)
            {
                case ObjectType::Station:
                    queuedPreparedStationDraws.push_back(draw);
                    break;

                case ObjectType::Asteroid:
                    queuedPreparedAsteroidDraws.push_back(draw);
                    break;

                case ObjectType::Planet:
                    queuedPreparedPlanetDraws.push_back(draw);
                    break;

                default:
                    queuedPreparedDefaultDraws.push_back(draw);
                    break;
            }

            m_lastStats.partsDrawn++;
            m_lastStats.drawCalls += 2;
        }

        if (!queuedPreparedStationDraws.empty())
        {
            drawQueuedMeshPasses(
                m_meshRenderer,
                queuedPreparedStationDraws,
                largeObjectShader,
                edgeShader,
                LightingParams::station(),
                cameraLocalPosition
            );
        }

        if (!queuedPreparedAsteroidDraws.empty())
        {
            drawQueuedMeshPasses(
                m_meshRenderer,
                queuedPreparedAsteroidDraws,
                largeObjectShader,
                edgeShader,
                LightingParams::asteroid(),
                cameraLocalPosition
            );
        }

        if (!queuedPreparedPlanetDraws.empty())
        {
            drawQueuedMeshPasses(
                m_meshRenderer,
                queuedPreparedPlanetDraws,
                largeObjectShader,
                edgeShader,
                LightingParams::planet(),
                cameraLocalPosition
            );
        }

        if (!queuedPreparedDefaultDraws.empty())
        {
            drawQueuedMeshPasses(
                m_meshRenderer,
                queuedPreparedDefaultDraws,
                largeObjectShader,
                edgeShader,
                LightingParams::station(),
                cameraLocalPosition
            );
        }
    }

    profileAfterObjectsMs = renderProfileNowMs();









    // ============================================================
    // 4. ОТЛАДОЧНАЯ ИНФОРМАЦИЯ ДЛЯ КОРАБЛЯ ИГРОКА
    // ============================================================


    profileAfterObjectsMs = renderProfileNowMs();






    auto it = ships.find(playerId.value);

    // Отправляем отладочные данные
    if(policy.drawDebug && shouldDebug && m_debugCallback)
    {
        m_debugCallback(debugData);
    }





        profileAfterDebugCallbackMs = renderProfileNowMs();

    const double setupMs =
        profileAfterSetupMs - profileStartMs;

    const double starfieldMs =
        profileAfterStarfieldMs - profileAfterSetupMs;

    const double farPassesMs =
        profileAfterFarPassesMs - profileAfterStarfieldMs;

    const double labelsMs =
        profileAfterLabelsMs - profileAfterFarPassesMs;

    const double realShipsMs =
        profileAfterRealShipsMs - profileAfterLabelsMs;

    const double visualShipsMs =
        profileAfterVisualShipsMs - profileAfterRealShipsMs;

    const double visualDronesMs =
        profileAfterVisualDronesMs - profileAfterVisualShipsMs;

    const double objectsMs =
        profileAfterObjectsMs - profileAfterVisualDronesMs;

    const double debugCallbackMs =
        profileAfterDebugCallbackMs - profileAfterObjectsMs;

    const double totalMs =
        profileAfterDebugCallbackMs - profileStartMs;

    const bool suspicious =
        totalMs > 16.0 ||
        realShipsMs > 6.0 ||
        visualShipsMs > 6.0 ||
        visualDronesMs > 4.0 ||
        objectsMs > 6.0 ||
        farPassesMs > 6.0 ||
        labelsMs > 4.0 ||
        debugCallbackMs > 4.0;

    if (suspicious)
    {
        static bool initialized = false;
        static int rows = 0;

        if (rows < 800)
        {
            std::ofstream out(
                "scene_render_breakdown_log.csv",
                initialized ? std::ios::app : std::ios::out
            );

            if (!initialized)
            {
                out << "row,"
                    << "frame,cameraName,totalMs,"
                    << "setupMs,starfieldMs,farPassesMs,labelsMs,"
                    << "realShipsMs,visualShipsMs,visualDronesMs,objectsMs,debugCallbackMs,"
                    << "drawCalls,"
                    << "modulesDrawn,modulesCulled,"
                    << "partsDrawn,partsCulled,"
                    << "realShips,realShipParts,"
                    << "visualShipsDrawn,visualShipsCulled,"
                    << "visualProxy,visualFull,visualShipParts\n";

                initialized = true;
            }

            out << rows++ << ","
                << m_frameCounter << ","
                << cameraName << ","
                << std::fixed << std::setprecision(4)
                << totalMs << ","
                << setupMs << ","
                << starfieldMs << ","
                << farPassesMs << ","
                << labelsMs << ","
                << realShipsMs << ","
                << visualShipsMs << ","
                << visualDronesMs << ","
                << objectsMs << ","
                << debugCallbackMs << ","
                << m_lastStats.drawCalls << ","
                << m_lastStats.modulesDrawn << ","
                << m_lastStats.modulesCulled << ","
                << m_lastStats.partsDrawn << ","
                << m_lastStats.partsCulled << ","
                << m_lastStats.realShipsDrawn << ","
                << m_lastStats.realShipPartsDrawn << ","
                << m_lastStats.visualShipsDrawn << ","
                << m_lastStats.visualShipsCulled << ","
                << m_lastStats.visualProxyShipsDrawn << ","
                << m_lastStats.visualFullShipsDrawn << ","
                << m_lastStats.visualShipPartsDrawn
                << "\n";
        }
    }










}






void SceneRenderer::renderVisualShips(
    const ClientWorldState& world,
    const Frustum& frustum,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cameraLocalPosition,
    const world::coordinates::WorldFrame& frame,
    unsigned int fillShader,
    unsigned int edgeShader,
    int maxVisualShipsToDraw
)
{
    const auto& dbg = debug::get().render;

    if (!dbg.shouldDrawMeshes())
        return;


int trafficCount = 0;
int promoCount = 0;
int otherCount = 0;






int visualShipsActuallyDrawn = 0;

for (const auto& ship : world.visualShips())
{
    if (ship.id >= 800000u && ship.id < 900000u)
        trafficCount++;
    else if (ship.id >= 900000u && ship.id < 910000u)
        promoCount++;
    else
        otherCount++;
}




    LightingParams shipParams = LightingParams::ship();

    

    const glm::mat4 renderView =
        world::coordinates::makeRenderView(view);

    

    for (const auto& ship : world.visualShips())
    {
        if (!ship.visible)
            continue;

        if (!ship.descriptor || !ship.assembly)
            continue;

        if (maxVisualShipsToDraw >= 0 &&
            visualShipsActuallyDrawn >= maxVisualShipsToDraw)
        {
            m_lastStats.visualShipsCulled++;
            continue;
        }

        const glm::vec3 shipLocalPosition =
            world::coordinates::toRenderLocal(
                ship.renderTransform.worldPosition,
                frame
            );

        glm::mat4 shipModel =
            world::coordinates::makeRenderModelMatrix(
                ship.renderTransform.worldPosition,
                glm::mat4(ship.renderTransform.orientation),
                frame
            );

        if (ship.visualScale != 1.0f)
        {
            shipModel =
                glm::scale(
                    shipModel,
                    glm::vec3(ship.visualScale)
                );
        }

        const bool visible =
            frustum.sphereVisible(
                ship.assembly->boundCenter,
                ship.assembly->boundRadius,
                shipModel
            );

        if (!visible)
        {
            m_lastStats.visualShipsCulled++;
            continue;
        }

        m_lastStats.visualShipsDrawn++;
        visualShipsActuallyDrawn++;


        const float distToShip =
            glm::length(shipLocalPosition - cameraLocalPosition);

        const bool useWholeShipProxy =
            ship.assembly->hasWholeShipProxy &&
            distToShip > 250.0f;

        // const bool useWholeShipProxy =
        //     ship.assembly->hasWholeShipProxy;



        if (useWholeShipProxy)
        {
            m_lastStats.visualProxyShipsDrawn++;

            glm::mat4 proxyMvp = proj * renderView * shipModel;

            std::vector<QueuedMeshDraw> proxyDraws;
            proxyDraws.push_back(
                QueuedMeshDraw{
                    &ship.assembly->wholeShipProxyGpu,
                    proxyMvp,
                    shipModel
                }
            );

                m_lastStats.partsDrawn++;
                m_lastStats.visualShipPartsDrawn++;
                m_lastStats.drawCalls += 2;

            drawQueuedMeshFillOnly(
                m_meshRenderer,
                proxyDraws,
                fillShader,
                shipParams,
                cameraLocalPosition
            );

            continue;
        }


        m_lastStats.visualFullShipsDrawn++;

        std::vector<QueuedMeshDraw> queuedVisualShipMeshDraws;
        

        for (const auto& module : ship.assembly->modules)
        {
            glm::mat4 moduleBaseModel =
                buildAssemblyModuleBaseModel(
                    shipModel,
                    module
                );

            glm::mat4 moduleModel =
                buildAssemblyModuleBaseModel(
                    shipModel,
                    module
                );

            // VisualShip не имеет runtime assemblyModules.
            // Если модуль вращающийся — пока рисуем его в базовом положении.
            // Позже можно добавить visual animation time отдельно.
            const float moduleRadius =
                safeRadiusFromHalfSize(module.boundHalfSize);

            if (!frustum.sphereVisible(
                    module.boundCenter,
                    moduleRadius,
                    moduleModel
                ))
            {
                m_lastStats.modulesCulled++;
                continue;
            }

            m_lastStats.modulesDrawn++;

            const glm::vec3 moduleLocalPosition =
                glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

            const float distToModule =
                glm::length(moduleLocalPosition - cameraLocalPosition);

            bool useLod1 =
                distToModule >= ship.assembly->lodSwitchDistance;

            // Для visual ships можно быть агрессивнее:
            // если не крупный план — сразу LOD1.
            if (distToModule > 350.0f)
                useLod1 = true;

            for (const auto& part : module.meshes)
            {
                glm::mat4 partModel =
                    glm::translate(moduleModel, part.localOffset);

                const float partRadius =
                    safeRadiusFromHalfSize(part.boundHalfSize);

                if (!frustum.sphereVisible(
                        part.boundCenter,
                        partRadius,
                        partModel
                    ))
                {
                    m_lastStats.partsCulled++;
                    continue;
                }

                m_lastStats.partsDrawn++;
                m_lastStats.drawCalls += 2;

                glm::mat4 partMvp = proj * renderView * partModel;

                const render::MeshGPU& gpu =
                    useLod1 ? part.lod1Gpu : part.lod0Gpu;

                queuedVisualShipMeshDraws.push_back(
                    QueuedMeshDraw{
                        &gpu,
                        partMvp,
                        partModel
                    }
                );
            }
        }

        drawQueuedMeshFillOnly(
            m_meshRenderer,
            queuedVisualShipMeshDraws,
            fillShader,
            shipParams,
            cameraLocalPosition
        );
    }
}







void SceneRenderer::renderStarSystemLabels(
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    const auto& dbg = debug::get().render;

    if (!dbg.showStarLabels)
        return;

    if (!m_starfieldRenderer.isInitialized())
        return;

    if (!m_starLabelFont)
    {
        m_starLabelFont = std::make_unique<Font>(
            "assets/fonts/Roboto-Light.ttf",
            12
        );
    }

    const auto& stars = m_starfieldRenderer.getRealStars();
    const glm::vec3 observerLy = m_starfieldRenderer.getObserverPositionLy();

    const glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    const float skyRadius = m_starfieldRenderer.renderRadius();

    GLint viewport[4] = {0, 0, 1, 1};
    glGetIntegerv(GL_VIEWPORT, viewport);

    const float screenW = static_cast<float>(viewport[2]);
    const float screenH = static_cast<float>(viewport[3]);

    const glm::vec2 screenCenter(screenW * 0.5f, screenH * 0.5f);

    // Радиус "прицела" в пикселях.
    // Чем меньше — тем строже надо навести центр экрана.
    constexpr float kFocusRadiusPx = 70.0f;

    struct LabelCandidate
    {
        const GalaxyStarfieldRenderer::RealStar* star = nullptr;
        glm::vec2 screen {0.0f};
        float distLy = 0.0f;
        float centerDistPx = 999999.0f;
    };

    std::vector<LabelCandidate> candidates;
    candidates.reserve(64);

    for (const auto& star : stars)
    {
        if (!star.isGameSystem)
            continue;

        const glm::vec3 rel = star.positionLy - observerLy;
        const float distLy = glm::length(rel);

        if (distLy < 0.001f)
            continue;

        const glm::vec3 dir = rel / distLy;
        const glm::vec3 skyPos = dir * skyRadius;

        const glm::vec4 clip =
            proj * viewNoTranslation * glm::vec4(skyPos, 1.0f);

        if (clip.w <= 0.0001f)
            continue;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;

        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f)
            continue;

        const float screenX = (ndc.x * 0.5f + 0.5f) * screenW;
        const float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenH;

        const glm::vec2 screenPos(screenX, screenY);
        const float centerDist = glm::length(screenPos - screenCenter);

        LabelCandidate candidate;
        candidate.star = &star;
        candidate.screen = screenPos;
        candidate.distLy = distLy;
        candidate.centerDistPx = centerDist;

        candidates.push_back(candidate);
    }

    if (candidates.empty())
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto drawCandidate = [&](const LabelCandidate& candidate, float extraAlpha)
    {
        const auto& star = *candidate.star;

        char text[160];
        std::snprintf(
            text,
            sizeof(text),
            "%s  %.1f ly",
            star.name.empty() ? star.id.c_str() : star.name.c_str(),
            candidate.distLy
        );

        float alpha =
            std::max(0.18f, std::min(0.62f, 1.0f - candidate.distLy / 90.0f));

        alpha *= extraAlpha;

        TextRenderer::instance().textDraw(
            *m_starLabelFont,
            text,
            candidate.screen.x + 8.0f,
            candidate.screen.y - 6.0f,
            glm::vec4(0.65f, 0.82f, 1.0f, alpha)
        );
    };

    if (dbg.showAllStarLabels)
    {
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const LabelCandidate& a, const LabelCandidate& b)
            {
                if (a.distLy != b.distLy)
                    return a.distLy < b.distLy;

                return a.centerDistPx < b.centerDistPx;
            }
        );

        constexpr int kMaxLabels = 24;

        int rendered = 0;
        for (const LabelCandidate& candidate : candidates)
        {
            drawCandidate(candidate, 1.0f);

            ++rendered;
            if (rendered >= kMaxLabels)
                break;
        }
    }
    else
    {
        const auto bestIt = std::min_element(
            candidates.begin(),
            candidates.end(),
            [](const LabelCandidate& a, const LabelCandidate& b)
            {
                return a.centerDistPx < b.centerDistPx;
            }
        );

        if (bestIt != candidates.end() && bestIt->centerDistPx <= kFocusRadiusPx)
        {
            const float focusAlpha =
                1.0f - std::min(1.0f, bestIt->centerDistPx / kFocusRadiusPx);

            drawCandidate(*bestIt, 0.35f + focusAlpha * 0.65f);
        }
    }

    glEnable(GL_DEPTH_TEST);
}


void SceneRenderer::renderConstellationHoverOverlay(
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    const auto& dbg = debug::get().render;

    if (!dbg.showConstellationHover)
        return;

    if (!m_starfieldRenderer.isInitialized())
        return;

    if (!m_debugLines || !m_debugLines->isInitialized())
        return;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return;

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    GLint viewport[4] = {0, 0, 1, 1};
    glGetIntegerv(GL_VIEWPORT, viewport);

    const float screenW = static_cast<float>(viewport[2]);
    const float screenH = static_cast<float>(viewport[3]);

    if (screenW <= 1.0f || screenH <= 1.0f)
        return;

    if (mouseX < 0.0 || mouseY < 0.0 || mouseX > screenW || mouseY > screenH)
        return;

    struct ConstellationDef
    {
        const char* id;
        const char* label;
        std::vector<std::pair<const char*, const char*>> edges;
    };

    static const std::vector<ConstellationDef> kConstellations =
    {
        {
            "Ori",
            "Orion",
            {
                {"Betelgeuse", "Bellatrix"},
                {"Bellatrix", "Mintaka"},
                {"Mintaka", "Alnilam"},
                {"Alnilam", "Alnitak"},
                {"Alnitak", "Saiph"},
                {"Saiph", "Rigel"},
                {"Rigel", "Mintaka"}
            }
        },
        {
            "UMa",
            "Ursa Major",
            {
                {"Dubhe", "Merak"},
                {"Merak", "Phecda"},
                {"Phecda", "Megrez"},
                {"Megrez", "Alioth"},
                {"Alioth", "Mizar"},
                {"Mizar", "Alkaid"}
            }
        },
        {
            "Cyg",
            "Cygnus",
            {
                {"Deneb", "Sadr"},
                {"Sadr", "Aljanah"},
                {"Sadr", "Fawaris"},
                {"Sadr", "Albireo"}
            }
        },
        {
            "Cas",
            "Cassiopeia",
            {
                {"Caph", "Schedar"},
                {"Schedar", "Cih"},
                {"Cih", "Ruchbah"},
                {"Ruchbah", "Segin"}
            }
        },
        {
            "Cru",
            "Crux",
            {
                {"Gacrux", "Acrux"},
                {"Mimosa", "Imai"}
            }
        },
        {
            "Leo",
            "Leo",
            {
                {"Regulus", "Algieba"},
                {"Algieba", "Zosma"},
                {"Zosma", "Denebola"},
                {"Zosma", "Chertan"}
            }
        },
        {
            "Gem",
            "Gemini",
            {
                {"Castor", "Pollux"},
                {"Castor", "Mebsuta"},
                {"Mebsuta", "Alhena"},
                {"Pollux", "Wasat"},
                {"Wasat", "Alhena"}
            }
        },
        {
            "Sco",
            "Scorpius",
            {
                {"Dschubba", "Antares"},
                {"Antares", "Larawag"},
                {"Larawag", "Shaula"},
                {"Shaula", "Lesath"},
                {"Shaula", "Sargas"}
            }
        }
    };

    struct ScreenStar
    {
        const GalaxyStarfieldRenderer::RealStar* star = nullptr;
        glm::vec2 screen {0.0f};
    };

    const auto& stars = m_starfieldRenderer.getRealStars();
    const glm::vec3 observerLy = m_starfieldRenderer.getObserverPositionLy();
    const glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    const float skyRadius = m_starfieldRenderer.renderRadius();

    std::vector<ScreenStar> projected;
    projected.reserve(stars.size());

    auto projectStar = [&](const GalaxyStarfieldRenderer::RealStar& star, glm::vec2& outScreen) -> bool
    {
        const glm::vec3 rel = star.positionLy - observerLy;
        const float distLy = glm::length(rel);

        if (distLy < 0.001f)
            return false;

        const glm::vec3 dir = rel / distLy;
        const glm::vec3 skyPos = dir * skyRadius;

        const glm::vec4 clip =
            proj * viewNoTranslation * glm::vec4(skyPos, 1.0f);

        if (clip.w <= 0.0001f)
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;

        if (std::abs(ndc.x) > 1.15f || std::abs(ndc.y) > 1.15f)
            return false;

        outScreen.x = (ndc.x * 0.5f + 0.5f) * screenW;
        outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenH;

        return true;
    };

    for (const auto& star : stars)
    {
        glm::vec2 screen;
        if (!projectStar(star, screen))
            continue;

        projected.push_back(ScreenStar{&star, screen});
    }

    auto findByName = [&](const char* name) -> const ScreenStar*
    {
        for (const auto& s : projected)
        {
            if (!s.star)
                continue;

            if (s.star->name == name)
                return &s;
        }

        return nullptr;
    };

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    const ConstellationDef* bestConstellation = nullptr;
    float bestDistance = 999999.0f;

    for (const auto& c : kConstellations)
    {
        float minDistance = 999999.0f;
        int visibleEdges = 0;

        for (const auto& edge : c.edges)
        {
            const ScreenStar* a = findByName(edge.first);
            const ScreenStar* b = findByName(edge.second);

            if (!a || !b)
                continue;

            const glm::vec2 ab = b->screen - a->screen;
            const float abLen2 = glm::dot(ab, ab);

            if (abLen2 < 0.0001f)
                continue;

            const float t =
                std::max(
                    0.0f,
                    std::min(
                        1.0f,
                        glm::dot(mouse - a->screen, ab) / abLen2
                    )
                );

            const glm::vec2 closest = a->screen + ab * t;
            const float d = glm::length(mouse - closest);

            minDistance = std::min(minDistance, d);
            ++visibleEdges;
        }

        if (visibleEdges <= 0)
            continue;

        if (minDistance < bestDistance)
        {
            bestDistance = minDistance;
            bestConstellation = &c;
        }
    }

    if (!bestConstellation)
        return;

    if (bestDistance > dbg.constellationHoverRadiusPx)
        return;

    if (!m_starLabelFont)
    {
        m_starLabelFont = std::make_unique<Font>(
            "assets/fonts/Roboto-Light.ttf",
            12
        );
    }

    auto pxToNdc = [&](const glm::vec2& p) -> glm::vec3
    {
        return glm::vec3(
            (p.x / screenW) * 2.0f - 1.0f,
            1.0f - (p.y / screenH) * 2.0f,
            0.0f
        );
    };

    m_debugLines->begin();

    const glm::vec4 lineColor(0.45f, 0.78f, 1.0f, 0.72f);

    glm::vec2 labelAnchor(0.0f);
    int labelCount = 0;

    for (const auto& edge : bestConstellation->edges)
    {
        const ScreenStar* a = findByName(edge.first);
        const ScreenStar* b = findByName(edge.second);

        if (!a || !b)
            continue;

        m_debugLines->addLine(
            pxToNdc(a->screen),
            pxToNdc(b->screen),
            lineColor
        );

        labelAnchor += a->screen;
        labelAnchor += b->screen;
        labelCount += 2;
    }

    m_debugLines->endOverlay(glm::mat4(1.0f));

    if (labelCount > 0)
        labelAnchor /= static_cast<float>(labelCount);
    else
        labelAnchor = mouse;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TextRenderer::instance().textDraw(
        *m_starLabelFont,
        bestConstellation->label,
        labelAnchor.x + 12.0f,
        labelAnchor.y - 12.0f,
        glm::vec4(0.65f, 0.86f, 1.0f, 0.92f)
    );

    glEnable(GL_DEPTH_TEST);
}



void SceneRenderer::renderVisualDrones(
    const ClientWorldState& world,
    const Frustum& frustum,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cameraLocalPosition,
    const world::coordinates::WorldFrame& frame,
    unsigned int fillShader,
    unsigned int edgeShader
)
{
    const auto& dbg = debug::get().render;

    if (!dbg.shouldDrawMeshes())
        return;

    const glm::mat4 renderView =
        world::coordinates::makeRenderView(view);

    LightingParams droneParams = LightingParams::ship();

    for (const auto& drone : world.visualDrones())
    {
        if (!drone.visible || !drone.assembly)
            continue;

        glm::mat4 droneModel =
            world::coordinates::makeRenderModelMatrix(
                drone.renderTransform.worldPosition,
                glm::mat4(drone.renderTransform.orientation),
                frame
            );

        if (drone.visualScale != 1.0f)
        {
            droneModel =
                glm::scale(
                    droneModel,
                    glm::vec3(drone.visualScale)
                );
        }

        if (!frustum.sphereVisible(
                drone.assembly->boundCenter,
                drone.assembly->boundRadius,
                droneModel
            ))
        {
            continue;
        }

        std::vector<QueuedMeshDraw> queuedDroneDraws;

        for (const auto& module : drone.assembly->modules)
        {
            glm::mat4 moduleBaseModel =
                buildAssemblyModuleBaseModel(
                    droneModel,
                    module
                );

            glm::mat4 moduleModel =
                buildAssemblyModuleBaseModel(
                    droneModel,
                    module
                );

            const float moduleRadius =
                safeRadiusFromHalfSize(module.boundHalfSize);

            if (!frustum.sphereVisible(
                    module.boundCenter,
                    moduleRadius,
                    moduleModel
                ))
            {
                continue;
            }

            const glm::vec3 moduleWorldPos =
                glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

            const float distToModule =
                glm::length(moduleWorldPos);

            const bool useLod1 =
                distToModule >= drone.assembly->lodSwitchDistance;

            for (const auto& part : module.meshes)
            {
                const glm::mat4 partModel =
                    glm::translate(moduleModel, part.localOffset);

                const float partRadius =
                    safeRadiusFromHalfSize(part.boundHalfSize);

                if (!frustum.sphereVisible(
                        part.boundCenter,
                        partRadius,
                        partModel
                    ))
                {
                    continue;
                }

                const glm::mat4 partMvp =
                    proj * renderView * partModel;

                const render::MeshGPU& gpu =
                    useLod1 ? part.lod1Gpu : part.lod0Gpu;

                queuedDroneDraws.push_back(
                    QueuedMeshDraw{
                        &gpu,
                        partMvp,
                        partModel
                    }
                );
            }
        }

        drawQueuedMeshPasses(
            m_meshRenderer,
            queuedDroneDraws,
            fillShader,
            edgeShader,
            droneParams,
            cameraLocalPosition
        );
    }
}