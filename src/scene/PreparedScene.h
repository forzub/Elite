#pragma once

#include <string>
#include <glm/glm.hpp>
#include <vector>

#include "src/game/client/ClientWorldState.h"
#include "src/scene/EntityID.h"
#include "src/world/coordinates/WorldFrame.h"

#include "src/world/types/ObjectType.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/visual/VisualShipKind.h"



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
    int activeSystemId = -1;

    world::coordinates::WorldFrame frame;

    // Absolute observer position used only by galaxy-scale presentation
    // (starfield/parallax). Object rendering remains player-relative via
    // WorldFrame and never consumes this large coordinate directly.
    glm::dvec3 observerGalacticPositionLy {0.0};
    bool observerGalacticPositionValid = false;


    struct RealShipMeshItem
    {
        EntityId entityId{0};
        ShipRole role = ShipRole::NPC;

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
        game::visual::VisualShipKind kind = game::visual::VisualShipKind::Generic;
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


    struct DebugAssemblyItem
    {
        enum class Kind
        {
            RealShip,
            Object
        };

        Kind kind = Kind::Object;
        EntityId entityId{0};
        ShipRole shipRole = ShipRole::NPC;
        ObjectType objectType = ObjectType::None;

        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
        float boundRadius = 0.0f;

        const game::ship::geometry::ObjectAssembly* assembly = nullptr;
        const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>* assemblyModules = nullptr;
        const std::vector<game::simulation::DebugHitVolumeSnapshot>* debugHitVolumes = nullptr;
    };

    // Debug geometry is prepared from the exact same presentation transforms
    // as meshes. This prevents diagnostic axes/volumes from drifting back to
    // stale authoritative/world-space paths when render frames evolve.
    std::vector<DebugAssemblyItem> debugAssemblies;

    bool valid = false;
};