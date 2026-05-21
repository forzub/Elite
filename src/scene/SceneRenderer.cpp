#include "SceneRenderer.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <utility>

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
constexpr float kCelestialFarPlane  = 500000000.0f;

constexpr float kFarStationNearPlane = 1000.0f;
constexpr float kFarStationFarPlane  = 20000000.0f;

// Ближе этой дистанции станция рисуется полной модульной сборкой.
// Дальше — только дальний proxy-силуэт.
constexpr float kStationFullRenderDistance = 450000.0f;

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
    const glm::vec3& cameraPos
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
            cameraPos
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
            cameraPos
        );
    }
}



glm::mat4 buildAssemblyModuleModel(
    const glm::mat4& ownerModel,
    const game::ship::geometry::AssemblyModule& module,
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& assemblyModules
)
{
    glm::mat4 moduleBaseModel = ownerModel;
    moduleBaseModel = glm::translate(moduleBaseModel, module.localPosition);

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

    glm::mat4 moduleModel = moduleBaseModel;

    if (module.rotates)
    {
        const float angle = findAssemblyModuleAngleRad(
            assemblyModules,
            module.id
        );

        moduleModel =
            moduleModel *
            glm::translate(glm::mat4(1.0f), module.pivot) *
            glm::rotate(glm::mat4(1.0f), angle, module.rotationAxis) *
            glm::translate(glm::mat4(1.0f), -module.pivot);
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
    const glm::vec3& cameraPos,
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
                cameraPos
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
    if (!m_planetRenderer.isInitialized())
    {
        m_planetRenderer.initialize();
    }

    const glm::mat4 celestialProj =
        makePerspectiveForCurrentViewport(
            70.0f,
            kCelestialNearPlane,
            kCelestialFarPlane
        );

    const glm::mat4 renderView =
        world::coordinates::makeRenderView(view);

    const float earthRadiusM = 6371000.0f;
    const float stationAltitudeM = 1200000.0f;

    const world::coordinates::WorldPosition earthWorldPosition =
    world::coordinates::makeWorldPositionFromMeters(
        glm::dvec3(
            0.0,
            -static_cast<double>(earthRadiusM + stationAltitudeM),
            2450.0
        )
    );

    const glm::vec3 earthCenterLocal =
        world::coordinates::toRenderLocal(
            earthWorldPosition,
            frame
        );

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    m_planetRenderer.render(
        renderView,
        celestialProj,
        earthCenterLocal,
        timeSeconds
    );

    m_lastStats.drawCalls += 2;
}

void SceneRenderer::renderFarStationProxyPass(
    const ClientWorldState& world,
    const glm::mat4& view,
    const glm::vec3& cameraPos,
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
    glm::length(objectLocalPosition - cameraPos);

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







void SceneRenderer::render(
    const ClientWorldState& world,
    EntityId playerId,
    const glm::mat4& view,
    const glm::mat4& proj,
    int cameraId,          
    const std::string& cameraName
)
{
    const auto& ships = world.ships();
    const auto& objects = world.objects();


    // --- browser DEBUG ---
    const auto& dbg = debug::get().render;
    m_frameCounter++;

    m_lastStats.reset();

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

    // Получаем позицию камеры
glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);



auto itPlayer = ships.find(playerId.value);
if (itPlayer == ships.end())
    return;

const world::coordinates::WorldFrame frame =
    world::coordinates::makeRenderFrameFromCamera(
        itPlayer->second.renderTransform.worldPosition
    );

const glm::mat4 renderView =
    world::coordinates::makeRenderView(view);
   


    
    // Включаем необходимые состояния
    glDepthFunc(GL_LESS);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ============================================================
    // 0. BACKGROUND: STARFIELD
    // ============================================================
    // Рисуем первым. Он не пишет в depth buffer.
    // Потом станция/корабли просто перерисуют его поверх.
    if (dbg.shouldRenderStarfield())
    {
        if (!m_starfieldRenderer.isInitialized())
        {
            m_starfieldRenderer.initialize();
        }

        m_starfieldRenderer.render(renderView, proj);
    }

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
    renderCelestialPass(
        view,
        frame,
        static_cast<float>(glfwGetTime())
    );

    // Дальний proxy станции можно отключать только для маленького rear-monitor,
    // но не для обычного главного вида, даже если он смотрит назад.
    if (!isRearMonitor)
    {
        renderFarStationProxyPass(
            world,
            view,
            cameraPos,
            frame
        );
    }

    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);














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



    if (cameraName == "mainCam")
    {
        renderStarSystemLabels(renderView, proj);
        renderConstellationHoverOverlay(renderView, proj);
    }







    // ============================================================
    // 1. РЕНДЕРИНГ КОРАБЛЕЙ (кроме игрока)
    // ============================================================
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
                shipInfo.distance = glm::length(shipLocalPosition - cameraPos);
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
                glm::mat4 moduleBaseModel = shipModel;
                moduleBaseModel = glm::translate(moduleBaseModel, module.localPosition);

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

                glm::mat4 moduleModel = moduleBaseModel;

                if (module.rotates)
                {
                    float angle = findAssemblyModuleAngleRad(
                        ship.assemblyModules,
                        module.id
                    );

                    moduleModel =
                        moduleModel *
                        glm::translate(glm::mat4(1.0f), module.pivot) *
                        glm::rotate(glm::mat4(1.0f), angle, module.rotationAxis) *
                        glm::translate(glm::mat4(1.0f), -module.pivot);
                }

                glm::vec3 moduleWorldPos = glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));
                float distToModule = glm::length(moduleWorldPos - cameraPos);
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
    //     cameraPos
    // );

    drawQueuedMeshPasses(
        m_meshRenderer,
        queuedShipMeshDraws,
        fillShader,
        edgeShader,
        shipParams,
        cameraPos
    );



    renderDetachedAssemblyModules(
        m_meshRenderer,
        m_debugLines.get(),
        ship,
        shipModel,
        view,
        proj,
        cameraPos,
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
    

    

    renderVisualShips(
        world,
        frustum,
        view,
        proj,
        cameraPos,
        frame,
        fillShader,
        edgeShader,
        isRearMonitor ? 64 : -1
    );


// ============================================================
// 1.6. РЕНДЕРИНГ Дронов
// ============================================================


renderVisualDrones(
    world,
    frustum,
    view,
    proj,
    cameraPos,
    frame,
    fillShader,
    edgeShader
);







// ============================================================
// 2. РЕНДЕРИНГ СТАЦИОНАРНЫХ ОБЪЕКТОВ (станции, астероиды, планеты)
// ============================================================



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
        glm::length(objectLocalPosition - cameraPos);

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
            glm::mat4 moduleBaseModel = objectBaseModel;
            moduleBaseModel = glm::translate(moduleBaseModel, module.localPosition);

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

            glm::mat4 moduleModel = moduleBaseModel;

            if (module.rotates)
            {
                float angle = findAssemblyModuleAngleRad(
                    obj.assemblyModules,
                    module.id
                );

                moduleModel =
                    moduleModel *
                    glm::translate(glm::mat4(1.0f), module.pivot) *
                    glm::rotate(glm::mat4(1.0f), angle, module.rotationAxis) *
                    glm::translate(glm::mat4(1.0f), -module.pivot);
            }

            glm::vec3 moduleWorldPos = glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));
            float distToModule = glm::length(moduleWorldPos - cameraPos);
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
        cameraPos
    );

    renderDetachedAssemblyModules(
    m_meshRenderer,
    m_debugLines.get(),
    obj,
        objectBaseModel,
        view,
        proj,
        cameraPos,
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






    // ============================================================
    // 4. ОТЛАДОЧНАЯ ИНФОРМАЦИЯ ДЛЯ КОРАБЛЯ ИГРОКА
    // ============================================================
    auto it = ships.find(playerId.value);

    // Отправляем отладочные данные
    if(shouldDebug && m_debugCallback)
    {
        m_debugCallback(debugData);
    }
}






void SceneRenderer::renderVisualShips(
    const ClientWorldState& world,
    const Frustum& frustum,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cameraPos,
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

static int frameCounter = 0;
frameCounter++;

if (frameCounter % 120 == 0)
{
    std::cerr
        << "[VisualShips] total=" << world.visualShips().size()
        << " traffic=" << trafficCount
        << " promo=" << promoCount
        << " other=" << otherCount
        << std::endl;
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
            glm::length(shipLocalPosition - cameraPos);

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

            drawQueuedMeshPasses(
                m_meshRenderer,
                proxyDraws,
                fillShader,
                edgeShader,
                shipParams,
                cameraPos
            );

            continue;
        }


        m_lastStats.visualFullShipsDrawn++;

        std::vector<QueuedMeshDraw> queuedVisualShipMeshDraws;

        for (const auto& module : ship.assembly->modules)
        {
            glm::mat4 moduleBaseModel = shipModel;

            moduleBaseModel =
                glm::translate(
                    moduleBaseModel,
                    module.localPosition
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.x),
                    glm::vec3(1.0f, 0.0f, 0.0f)
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.y),
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.z),
                    glm::vec3(0.0f, 0.0f, 1.0f)
                );

            glm::mat4 moduleModel = moduleBaseModel;

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
                glm::length(moduleLocalPosition - cameraPos);

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

        drawQueuedMeshPasses(
            m_meshRenderer,
            queuedVisualShipMeshDraws,
            fillShader,
            edgeShader,
            shipParams,
            cameraPos
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
    const glm::vec3& cameraPos,
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
            glm::mat4 moduleBaseModel = droneModel;

            moduleBaseModel =
                glm::translate(
                    moduleBaseModel,
                    module.localPosition
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.x),
                    glm::vec3(1.0f, 0.0f, 0.0f)
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.y),
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );

            moduleBaseModel =
                glm::rotate(
                    moduleBaseModel,
                    glm::radians(module.localRotationDeg.z),
                    glm::vec3(0.0f, 0.0f, 1.0f)
                );

            glm::mat4 moduleModel = moduleBaseModel;

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
            cameraPos
        );
    }
}