#include "SceneRenderer.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <memory>

// #define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/DebugGrid.h"
#include "src/render/ship/ShipMeshRenderer.h"
#include "src/render/ShaderLibrary.h"  // Используем существующий ShaderLibrary
#include "src/game/geometry/MeshLibrary.h"
#include "src/render/renderers/DebugLineRenderer.h"

#include "src/render/frustum/Frustum.h"

#include "src/render/renderers/LightingParams.h"




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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    GLuint fillShader = ShaderLibrary::instance().get("mesh_fill");
    GLuint largeObjectShader = ShaderLibrary::instance().get("large_object_shader");
    GLuint edgeShader = ShaderLibrary::instance().get("edge_shader");

    
   
    
    // обновляем frustum
    Frustum frustum;
    glm::mat4 vp = proj * view;
    frustum.update(vp);

    // ============================================================
    // 1. РЕНДЕРИНГ КОРАБЛЕЙ (кроме игрока)
    // ============================================================
    for (const auto& pair : ships)
    {
        if (pair.first == playerId.value)
            continue;

        const auto& ship = pair.second;
        auto* gpu = ship.gpuMesh;

        if (!gpu)
            continue;

        glm::mat4 model =
            glm::translate(glm::mat4(1.0f), ship.renderTransform.position) *
            glm::mat4(ship.renderTransform.orientation);

        glm::mat4 mvp = proj * view * model;

        // Параметры освещения для кораблей
        LightingParams shipParams = LightingParams::ship();
        
        // Динамически меняем параметры в зависимости от расстояния
        float distToShip = glm::length(ship.renderTransform.position - cameraPos);
        if (distToShip > 200.0f) {
            shipParams.fog.startDistance = 100.0f;
            shipParams.fog.nearColor = glm::vec3(0.1f, 0.1f, 0.15f);
            shipParams.fog.farColor = glm::vec3(0.05f, 0.05f, 0.1f);
        }

        const auto& meshData = render::MeshLibrary::getMeshData(ship.descriptor->typeId);

        // Проверка видимости через фрустум
        bool visible = frustum.sphereVisible(meshData.boundCenter, meshData.boundRadius, model);

        // DEBUG Browser
        if(shouldDebug)
        {
            DebugShipInfo info;
            info.id = pair.first;
            info.position = glm::vec3(model * glm::vec4(meshData.boundCenter, 1));
            info.radius = meshData.boundRadius;
            info.visible = visible;
            info.distance = glm::length(info.position - debugData.camera.position);
            info.type = std::to_string(static_cast<int>(pair.second.descriptor->typeId));
            debugData.ships.push_back(info);
        }

        if(!visible)
            continue;

        m_debugRenderer.renderAxes(model, view, proj);

        m_meshRenderer.draw(
            *ship.gpuMesh,
            fillShader,
            edgeShader,
            mvp,
            model,
            shipParams,
            cameraPos
        );
    }


// ============================================================
// 2. РЕНДЕРИНГ СТАЦИОНАРНЫХ ОБЪЕКТОВ (станции, астероиды, планеты)
// ============================================================


// В цикле рендеринга объектов:
for (const auto& pair : objects)
{
    const auto& obj = pair.second;
    auto* gpu = obj.gpuMesh;

    if (!gpu)
        continue;

    // Создаем модель-матрицу для объекта
    glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.renderPosition);
    
    // Если есть вращение, добавляем его
    if (glm::length(obj.renderRotation) > 0.001f) {
        model = glm::rotate(model, obj.renderRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, obj.renderRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, obj.renderRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    glm::mat4 mvp = proj * view * model;

    // Получаем meshData для проверки видимости
    const auto& meshData = render::MeshLibrary::getMeshData(obj.type);

    // Проверка видимости через фрустум (оставляем как есть)
    bool visible = frustum.sphereVisible(meshData.boundCenter, meshData.boundRadius, model);
    
    if (!visible)
        continue;

    // Выбираем параметры освещения в зависимости от типа объекта
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
            // Для неизвестных объектов
      
            break;
    }



    // Рендерим объект
    m_meshRenderer.draw(
        *gpu,
        largeObjectShader,
        edgeShader,
        mvp,
        model,
        params,
        cameraPos
    );

    // DEBUG Browser для объектов
    if(shouldDebug)
    {
        DebugObjectInfo objInfo;
        objInfo.id = pair.first;
        objInfo.position = obj.renderPosition;
        objInfo.type = std::to_string(static_cast<int>(obj.type));
        objInfo.visible = true;
        debugData.objects.push_back(objInfo);
    }
}

    // ============================================================
    // 3. ОТЛАДОЧНАЯ ИНФОРМАЦИЯ ДЛЯ КОРАБЛЯ ИГРОКА
    // ============================================================
    auto it = ships.find(playerId.value);
    // if (it != ships.end() && m_debugLines->isInitialized())
    // {
    //     const auto& t = it->second.renderTransform;
        
    //     glm::mat4 debugModel = glm::translate(glm::mat4(1.0f), t.position) * 
    //                            glm::mat4(t.orientation) *
    //                            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -20.0f, -20.0f));

    //     m_debugRenderer.renderPlayerGrid(debugModel, view, proj);
    // }

    // Отправляем отладочные данные
    if(shouldDebug && m_debugCallback)
    {
        m_debugCallback(debugData);
    }
}