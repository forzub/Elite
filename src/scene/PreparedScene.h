#pragma once

#include <string>
#include <glm/glm.hpp>
#include <vector>

#include "src/game/client/ClientWorldState.h"
#include "src/scene/EntityID.h"
#include "src/world/coordinates/WorldFrame.h"

#include "src/world/types/ObjectType.h"



namespace render
{
    class MeshGPU;
}




struct SceneCameraParams
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);

    int cameraId = 0;
    std::string cameraName = "mainCam";
};

struct PreparedScene
{
    const ClientWorldState* world = nullptr;
    EntityId playerId{0};

    world::coordinates::WorldFrame frame;



    struct RealShipMeshItem
    {
        const render::MeshGPU* gpuLod0 = nullptr;
        const render::MeshGPU* gpuLod1 = nullptr;

        glm::mat4 model = glm::mat4(1.0f);

        glm::vec3 boundCenter = glm::vec3(0.0f);
        glm::vec3 boundHalfSize = glm::vec3(1.0f);

        float lodSwitchDistance = 0.0f;
    };


    struct ObjectMeshItem
    {
        ObjectType type = ObjectType::None;

        const render::MeshGPU* gpuLod0 = nullptr;
        const render::MeshGPU* gpuLod1 = nullptr;

        glm::mat4 model = glm::mat4(1.0f);

        glm::vec3 boundCenter = glm::vec3(0.0f);
        glm::vec3 boundHalfSize = glm::vec3(1.0f);

        float lodSwitchDistance = 0.0f;
    };

    std::vector<RealShipMeshItem> realShipMeshes;
    std::vector<ObjectMeshItem> objectMeshes;




    struct VisualShipItem
    {
        const render::MeshGPU* wholeShipProxyGpu = nullptr;

        glm::mat4 model = glm::mat4(1.0f);

        glm::vec3 boundCenter = glm::vec3(0.0f);
        float boundRadius = 1.0f;

        bool hasWholeShipProxy = false;
        float lodSwitchDistance = 0.0f;

        const void* assembly = nullptr;
    };

    struct VisualShipPartItem
    {
        int shipIndex = -1;

        const render::MeshGPU* gpuLod0 = nullptr;
        const render::MeshGPU* gpuLod1 = nullptr;

        glm::mat4 model = glm::mat4(1.0f);

        glm::vec3 boundCenter = glm::vec3(0.0f);
        glm::vec3 boundHalfSize = glm::vec3(1.0f);

        float moduleDistanceBias = 0.0f;
    };

    std::vector<VisualShipItem> visualShips;
    std::vector<VisualShipPartItem> visualShipParts;





    bool valid = false;
};