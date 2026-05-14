#include "SceneRenderer.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>

// #define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/DebugGrid.h"
#include "src/render/ship/ShipMeshRenderer.h"
#include "src/render/ShaderLibrary.h"  // Используем существующий ShaderLibrary
// #include "src/game/geometry/MeshLibrary.h"

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
    GLuint largeObjectShader,
    GLuint edgeShader,
    const LightingParams& params
)
{
    if (!owner.assembly)
        return;

    for (const auto& fragment : owner.detachedFragments)
    {
        const game::ship::geometry::AssemblyModule* detachedModule = nullptr;

        for (const auto& module : owner.assembly->modules)
        {
            if (module.id == fragment.moduleId)
            {
                detachedModule = &module;
                break;
            }
        }

        if (!detachedModule)
            continue;

        glm::mat4 moduleModel =
            glm::translate(glm::mat4(1.0f), fragment.position) *
            fragment.orientation;

        const glm::vec3 moduleWorldPos =
            glm::vec3(moduleModel * glm::vec4(0, 0, 0, 1));

        const float distToModule =
            glm::length(moduleWorldPos - cameraPos);

        const bool useLod1 =
            distToModule >= owner.assembly->lodSwitchDistance;

        for (const auto& part : detachedModule->meshes)
        {
            const glm::mat4 partModel =
                glm::translate(moduleModel, part.localOffset);

            const glm::mat4 partMvp =
                proj * view * partModel;

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
                view,
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




void SceneRenderer::render(
    const ClientWorldState& world,
    EntityId playerId,
    const glm::mat4& view,
    const glm::mat4& proj,
    int cameraId,          
    const std::string& cameraName
)
{
    // --- browser DEBUG ---
    const auto& dbg = debug::get().render;
    m_frameCounter++;

    static float debugTimer = 0;
    debugTimer += 0.016f; // примерно dt, но лучше передавать dt параметром
    
    bool shouldDebug = false;
    if(m_debugCallback)
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


    const auto& ships = world.ships();
    const auto& objects = world.objects(); // Добавить этот метод в ClientWorldState
    
    auto itPlayer = ships.find(playerId.value);
    if (itPlayer == ships.end())
        return;
    
    // Включаем необходимые состояния
    glDepthFunc(GL_LESS);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ============================================================
    // 0. BACKGROUND: STARFIELD
    // ============================================================
    // Рисуем первым. Он не пишет в depth buffer.
    // Потом станция/корабли просто перерисуют его поверх.
    if (dbg.shouldRenderStarfield() && cameraName == "mainCam")
    {
        if (!m_starfieldRenderer.isInitialized())
        {
            m_starfieldRenderer.initialize();
        }

        m_starfieldRenderer.render(view, proj);
    }

    // Возвращаем нормальный state для мира.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Starfield is rendered later, after world geometry, as a depth-tested
    // background pass. That makes it immune to accidental clears/background
    // passes and prevents it from covering ships or large objects.
    
    GLuint fillShader = ShaderLibrary::instance().get("mesh_fill");
    GLuint largeObjectShader = ShaderLibrary::instance().get("large_object_shader");
    GLuint edgeShader = ShaderLibrary::instance().get("edge_shader");

    
   
    
    // обновляем frustum
    Frustum frustum;
    glm::mat4 vp = proj * view;
    frustum.update(vp);


    const bool drawWorldAxes = dbg.shouldDrawWorldAxes();
    const bool drawObjectAxes = dbg.shouldDrawObjectAxes();

    if (drawWorldAxes)
    {
        m_debugRenderer.renderAxes(
            glm::mat4(1.0f),
            view,
            proj,
            dbg.worldAxisLength
        );
    }



    if (cameraName == "mainCam")
    {
        renderStarSystemLabels(view, proj);
    }







    // ============================================================
    // 1. РЕНДЕРИНГ КОРАБЛЕЙ (кроме игрока)
    // ============================================================
    for (const auto& pair : ships)
    {
        // if (pair.first == playerId.value)
        //     continue;


        const auto& ship = pair.second;

        glm::mat4 shipModel =
            glm::translate(glm::mat4(1.0f), ship.renderTransform.position) *
            glm::mat4(ship.renderTransform.orientation);


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
                shipInfo.position = ship.renderTransform.position;
                shipInfo.forward = shipForward;
                shipInfo.up = shipUp;
                shipInfo.right = shipRight;
                shipInfo.radius = ship.assembly->boundRadius;
                shipInfo.visible = visible;
                shipInfo.distance = glm::length(ship.renderTransform.position - cameraPos);
                shipInfo.type = "Ship";

                debugData.ships.push_back(shipInfo);
            }




            if (!visible)
                continue;


            if (drawObjectAxes)
            {
                m_debugRenderer.renderAxes(
                    shipModel,
                    view,
                    proj,
                    dbg.shipAxisLength
                );
            }

            

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

                if (dbg.shouldDrawMeshes())
                {
                    for (const auto& part : module.meshes)
                    {
                        if (ship.hiddenPartIds.find(part.id) != ship.hiddenPartIds.end())
                            continue;

                        glm::mat4 partModel = glm::translate(moduleModel, part.localOffset);
                        glm::mat4 partMvp   = proj * view * partModel;

                        const render::MeshGPU& gpu = useLod1 ? part.lod1Gpu : part.lod0Gpu;

                        m_meshRenderer.draw(
                            gpu,
                            largeObjectShader,
                            edgeShader,
                            partMvp,
                            partModel,
                            shipParams,
                            cameraPos
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
                            view,
                            proj
                        );

                        m_debugRenderer.renderCross(
                            pivotWorld,
                            dbg.moduleCrossSize,
                            dbg.modulePivotColor,
                            view,
                            proj
                        );

                        m_debugRenderer.renderAxes(
                            pivotAxesModel,
                            view,
                            proj,
                            dbg.moduleAxisLength
                        );

                        m_debugRenderer.renderLine(
                            pivotWorld,
                            pivotWorld + rotationAxisWorld * dbg.rotAxisLength,
                            dbg.rotationAxisColor,
                            view,
                            proj
                        );
                    }
            }



if (dbg.shouldDrawMeshes())
{
    renderDetachedAssemblyModules(
    m_meshRenderer,
    m_debugLines.get(),
    ship,
        shipModel,
        view,
        proj,
        cameraPos,
        largeObjectShader,
        edgeShader,
        shipParams
    );
}



            debug::render::ServerHitVolumeRenderer::render(
                *m_debugLines,
                ship.debugHitVolumes,
                shipModel,
                view,
                proj
            );




for (const auto& job : ship.repairJobs)
{
    const glm::vec4 droneColor(0.2f, 1.0f, 0.2f, 1.0f);
    const glm::vec4 towColor(0.2f, 0.8f, 1.0f, 1.0f);
    const glm::vec4 homeColor(1.0f, 0.8f, 0.2f, 1.0f);

    m_debugRenderer.renderCross(
        job.dronePosition,
        0.6f,
        droneColor,
        view,
        proj
    );

    m_debugRenderer.renderLine(
        job.dronePosition,
        job.fragmentPosition,
        towColor,
        view,
        proj
    );

    m_debugRenderer.renderLine(
        job.fragmentPosition,
        job.homePosition,
        homeColor,
        view,
        proj
    );

    m_debugRenderer.renderCross(
        job.homePosition,
        0.4f,
        homeColor,
        view,
        proj
    );
}







            continue;
        }
    }


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
        glm::mat4 objectBaseModel =
            glm::translate(glm::mat4(1.0f), obj.renderPosition) *
            obj.renderOrientation;

        const glm::vec3 objectForward = game::math::forwardFromOrientation(obj.renderOrientation);
        const glm::vec3 objectUp      = game::math::upFromOrientation(obj.renderOrientation);
        const glm::vec3 objectRight   = game::math::rightFromOrientation(obj.renderOrientation);

        // glm::mat4 objectBaseModel = glm::translate(glm::mat4(1.0f), obj.renderPosition);

        // if (glm::length(obj.renderRotation) > 0.001f)
        // {
        //     objectBaseModel = glm::rotate(objectBaseModel, obj.renderRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        //     objectBaseModel = glm::rotate(objectBaseModel, obj.renderRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        //     objectBaseModel = glm::rotate(objectBaseModel, obj.renderRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        // }

        // if (obj.descriptor)
        // {
        //     objectBaseModel = objectBaseModel * buildVisualBasisFix(obj.descriptor->visualBasisRotationDeg());
        // }

        bool visible = frustum.sphereVisible(
            obj.assembly->boundCenter,
            obj.assembly->boundRadius,
            objectBaseModel
        );

        if (shouldDebug && dbg.publishObjectOrientation)
        {
            DebugObjectInfo objInfo;
            objInfo.id = pair.first;
            objInfo.position = obj.renderPosition;
            objInfo.forward = objectForward;
            objInfo.up = objectUp;
            objInfo.right = objectRight;
            objInfo.type = std::to_string(static_cast<int>(obj.type));
            objInfo.distance = glm::length(obj.renderPosition - cameraPos);
            objInfo.visible = visible;

            debugData.objects.push_back(objInfo);
        }

        if (!visible)
            continue;

        if (drawObjectAxes)
        {
            m_debugRenderer.renderAxes(
                objectBaseModel,
                view,
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

            


            if (dbg.shouldDrawMeshes())
            {
                for (const auto& part : module.meshes)
                {
                    glm::mat4 partModel = glm::translate(moduleModel, part.localOffset);
                    glm::mat4 partMvp   = proj * view * partModel;

                    if (obj.hiddenPartIds.find(part.id) != obj.hiddenPartIds.end())
                        continue;

                    const render::MeshGPU& gpu = useLod1 ? part.lod1Gpu : part.lod0Gpu;

                    m_meshRenderer.draw(
                        gpu,
                        largeObjectShader,
                        edgeShader,
                        partMvp,
                        partModel,
                        params,
                        cameraPos
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
                    view,
                    proj
                );

                m_debugRenderer.renderCross(
                    pivotWorld,
                    dbg.moduleCrossSize,
                    dbg.modulePivotColor,
                    view,
                    proj
                );

                m_debugRenderer.renderAxes(
                    pivotAxesModel,
                    view,
                    proj,
                    dbg.moduleAxisLength
                );

                m_debugRenderer.renderLine(
                    pivotWorld,
                    pivotWorld + rotationAxisWorld * dbg.rotAxisLength,
                    dbg.rotationAxisColor,
                    view,
                    proj
                );
            }

        }



if (dbg.shouldDrawMeshes())
{
    renderDetachedAssemblyModules(
    m_meshRenderer,
    m_debugLines.get(),
    obj,
        objectBaseModel,
        view,
        proj,
        cameraPos,
        largeObjectShader,
        edgeShader,
        params
    );
}

       
            debug::render::ServerHitVolumeRenderer::render(
                *m_debugLines,
                obj.debugHitVolumes,
                objectBaseModel,
                view,
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