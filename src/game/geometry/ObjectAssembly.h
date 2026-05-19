#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "MeshData.h"
#include "MeshGPU.h"
#include "src/world/types/ObjectType.h"

namespace game::ship::geometry
{

struct AssemblyMeshPartDesc
{
    std::string meshId;

    std::string lod0Path;
    std::string lod1Path;

    bool enabled = true;

    glm::vec3 localOffset {0.0f};
};

struct AssemblyModuleDesc
{
    std::string moduleId;
    std::string parentModuleId;
    std::string subsystemId;

    bool enabled = true;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot {0.0f};

    bool rotates = false;
    glm::vec3 rotationAxis {0.0f, 0.0f, 1.0f};
    float rotationSpeed = 0.0f;

    std::vector<AssemblyMeshPartDesc> meshes;
};

struct ObjectAssemblyDesc
{
    ObjectType typeId = ObjectType::None;
    std::vector<AssemblyModuleDesc> modules;
    float lodSwitchDistance = -1.0f;

    // Цельный mesh всего корабля для дальнего рендера.
    // Это НЕ part LOD, а whole-ship proxy.
    std::string wholeShipProxyPath;
};

struct AssemblyMeshPart
{
    std::string id;

    std::string lod0Path;
    std::string lod1Path;

    glm::vec3 rawMinBounds {0.0f};
    glm::vec3 rawMaxBounds {0.0f};
    glm::vec3 rawCenter    {0.0f};

    glm::vec3 localOffset  {0.0f};

    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};
    glm::vec3 boundCenter {0.0f};
    glm::vec3 boundHalfSize {0.5f};

    MeshData lod0Mesh;
    MeshData lod1Mesh;

    render::MeshGPU lod0Gpu;
    render::MeshGPU lod1Gpu;
};

struct AssemblyModule
{
    std::string id;
    std::string parentModuleId;
    std::string subsystemId;

    glm::vec3 localPosition    {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot            {0.0f};

    bool rotates = false;
    glm::vec3 rotationAxis {0.0f, 0.0f, 1.0f};
    float rotationSpeed = 0.0f;

    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};
    glm::vec3 boundCenter {0.0f};
    glm::vec3 boundHalfSize {0.5f};

    std::vector<AssemblyMeshPart> meshes;
};

struct ObjectAssembly
{
    ObjectType typeId = ObjectType::None;

    float lodSwitchDistance = 2500.0f;

    std::vector<AssemblyModule> modules;

    glm::vec3 minBounds   {0.0f};
    glm::vec3 maxBounds   {0.0f};
    glm::vec3 boundCenter {0.0f};
    float     boundRadius = 0.0f;

    // Дальний цельный mesh всего корабля.
    bool hasWholeShipProxy = false;
    std::string wholeShipProxyPath;

    MeshData wholeShipProxyMesh;
    render::MeshGPU wholeShipProxyGpu;

    glm::vec3 wholeShipProxyBoundCenter {0.0f};
    float wholeShipProxyBoundRadius = 1.0f;
};

} // namespace game::ship::geometry