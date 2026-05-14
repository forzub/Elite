
#include "MeshLibrary.h"
#include "ObjLoader.h"
#include <iostream>

#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"

using namespace render;

std::unordered_map<uint16_t, MeshGPU>
MeshLibrary::meshes;

std::unordered_map<uint16_t, game::ship::geometry::MeshData> 
MeshLibrary::meshData;




MeshGPU& MeshLibrary::get(ObjectType typeId)
{
    std::cout << "[MeshLibrary] type: " << static_cast<uint16_t>(typeId) << "\n";


    if (typeId == ObjectType::None)
        throw std::runtime_error("[MeshLibrary]: ObjectType::None requested");

    
        
    uint16_t key = static_cast<uint16_t>(typeId);

    auto it = meshes.find(key);

    if (it != meshes.end())
        return it->second;

    std::cout << "[MeshLibrary] LOAD MESH TYPE " << key << std::endl;

    // --- загружаем OBJ на клиенте ---
    game::ship::geometry::MeshData mesh;

    std::string objPath;

    switch (typeId)
    {
        case ObjectType::CobraMk1:
            objPath = "assets/models/cobra-mk1.obj";
            // objPath = "assets/models/cobramk1T.obj";
            // objPath = "assets/models/sphere.obj";
            // objPath = "assets/models/cube.obj";
            break;
        case ObjectType::Station:
            objPath = "assets/models/stantion-01.obj";
            break;

        default:
            std::cout << "[MeshLibrary] Unknown ship type\n";
            break;
    }

    if (!game::ship::geometry::ObjLoader::load(objPath, mesh))
    {
        std::cout << "[MeshLibrary] OBJ load failed\n";
    }


    // после загрузки OBJ
    // считаем исходные bounds
    mesh.computeBounds();
    mesh.computeBoundingSphere();

    // ----------------------------------------
    // МАСШТАБ ДО РЕАЛЬНЫХ РАЗМЕРОВ
    // ----------------------------------------

    // const ShipDescriptor& desc =
    //     ShipDescriptorRegistry::get(typeId);

    const IObjectDescriptor& desc =
        ObjectDescriptorRegistry::get(typeId);

    glm::vec3 currentSize =
        mesh.maxBounds - mesh.minBounds;

    glm::vec3 targetSize =
        desc.getMeshSizeMeters();

    std::cout
        << "[MeshLibrary] NORMALIZE currentSize = "
        << currentSize.x << ", " << currentSize.y << ", " << currentSize.z
        << " targetSize = "
        << targetSize.x << ", " << targetSize.y << ", " << targetSize.z
        << std::endl;



    
    const LogicalDimensions& dims = desc.logicalDimensions();

    float uniformScale = 1.0f;

    const float measuredWidth  = currentSize.x;
    const float measuredHeight = currentSize.y;
    const float measuredLength = currentSize.z;

    if (dims.enabled)
    {
        switch (dims.scaleReference)
        {
            case ScaleReference::Length:
            {
                if (measuredLength > 1e-6f)
                    uniformScale = dims.length / measuredLength;
                break;
            }

            case ScaleReference::Width:
            {
                if (measuredWidth > 1e-6f)
                    uniformScale = dims.width / measuredWidth;
                break;
            }

            case ScaleReference::Height:
            {
                if (measuredHeight > 1e-6f)
                    uniformScale = dims.height / measuredHeight;
                break;
            }

            case ScaleReference::None:
            default:
            {
                uniformScale = 1.0f;
                break;
            }
        }

        std::cout
            << "[MeshLibrary] NORMALIZE logical currentSize (W,H,L) = "
            << measuredWidth << ", " << measuredHeight << ", " << measuredLength
            << " target (W,H,L) = "
            << dims.width << ", " << dims.height << ", " << dims.length
            << " chosen = " << uniformScale
            << std::endl;
    }
    else
    {
        const glm::vec3 targetSize = desc.getMeshSizeMeters();

        float scaleX = targetSize.x / currentSize.x;
        float scaleY = targetSize.y / currentSize.y;
        float scaleZ = targetSize.z / currentSize.z;

        uniformScale = std::max(scaleX, std::max(scaleY, scaleZ));

        std::cout
            << "[MeshLibrary] NORMALIZE LEGACY scales = "
            << scaleX << ", " << scaleY << ", " << scaleZ
            << " chosen = " << uniformScale
            << std::endl;
    }



    for(auto& v : mesh.vertices)
    {
        v.position *= uniformScale;
    }

    for(auto& e : mesh.edges)
    {
        e.a *= uniformScale;
        e.b *= uniformScale;
    }


    // пересчитываем bounds
    mesh.computeBounds();
    mesh.computeBoundingSphere();

    glm::vec3 size =
    mesh.maxBounds - mesh.minBounds;

    std::cout
    << "[MeshLibrary] FINAL SIZE "
    << size.x << " "
    << size.y << " "
    << size.z
    << std::endl;



        meshData[key] = mesh;

        MeshGPU gpu;
        gpu.upload(mesh);
        meshes[key] = gpu;
        

        return meshes[key];
}









const game::ship::geometry::MeshData&
MeshLibrary::getMeshData(ObjectType typeId)
{
    uint16_t key = static_cast<uint16_t>(typeId);

    auto it = meshData.find(key);

    if (it == meshData.end())
    {
        // гарантируем загрузку
        get(typeId);

        it = meshData.find(key);

        if (it == meshData.end())
            throw std::runtime_error("MeshData not loaded");
    }

    return it->second;
}