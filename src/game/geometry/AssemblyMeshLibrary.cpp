#include "AssemblyMeshLibrary.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// #define TINYOBJLOADER_IMPLEMENTATION
#include "render/tiny_obj_loader.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ObjectAssemblyRegistry.h"
#include "ObjLoader.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/world/descriptors/LogicalDimensions.h"

using namespace game::ship::geometry;


namespace
{
    glm::vec3 transformPoint(const glm::mat4& m, const glm::vec3& p)
    {
        return glm::vec3(m * glm::vec4(p, 1.0f));
    }

    glm::vec3 transformDirection(const glm::mat4& m, const glm::vec3& d)
    {
        glm::vec3 out = glm::vec3(m * glm::vec4(d, 0.0f));
        if (glm::length(out) < 1e-6f)
            return glm::vec3(0.0f, 0.0f, 1.0f);
        return glm::normalize(out);
    }

    void applyBasisToMesh(MeshData& mesh, const glm::mat4& basis)
    {
        for (auto& v : mesh.vertices)
        {
            v.position = transformPoint(basis, v.position);
            v.normal   = transformDirection(basis, v.normal);
        }

        for (auto& e : mesh.edges)
        {
            e.a = transformPoint(basis, e.a);
            e.b = transformPoint(basis, e.b);
        }

        mesh.computeBounds();
        mesh.computeBoundingSphere();
    }

    void rotateBoundsByCorners(
        const glm::vec3& inMin,
        const glm::vec3& inMax,
        const glm::mat4& basis,
        glm::vec3& outMin,
        glm::vec3& outMax
    )
    {
        const glm::vec3 corners[8] =
        {
            {inMin.x, inMin.y, inMin.z},
            {inMax.x, inMin.y, inMin.z},
            {inMin.x, inMax.y, inMin.z},
            {inMax.x, inMax.y, inMin.z},
            {inMin.x, inMin.y, inMax.z},
            {inMax.x, inMin.y, inMax.z},
            {inMin.x, inMax.y, inMax.z},
            {inMax.x, inMax.y, inMax.z}
        };

        outMin = glm::vec3( 1e9f);
        outMax = glm::vec3(-1e9f);

        for (const auto& c : corners)
        {
            const glm::vec3 p = transformPoint(basis, c);
            outMin = glm::min(outMin, p);
            outMax = glm::max(outMax, p);
        }
    }
}





std::unordered_map<uint16_t, ObjectAssembly>
AssemblyMeshLibrary::s_cache;

bool AssemblyMeshLibrary::has(ObjectType typeId)
{
    return ObjectAssemblyRegistry::has(typeId);
}

const ObjectAssembly& AssemblyMeshLibrary::get(ObjectType typeId)
{
    uint16_t key = static_cast<uint16_t>(typeId);

    auto it = s_cache.find(key);
    if (it != s_cache.end())
        return it->second;

    s_cache[key] = loadAssembly(typeId);
    return s_cache[key];
}

ObjectAssembly AssemblyMeshLibrary::loadAssembly(ObjectType typeId)
{
    std::cout << "[AssemblyMeshLibrary] LOAD ASSEMBLY TYPE "
              << static_cast<uint16_t>(typeId) << std::endl;

    const auto& desc = ObjectAssemblyRegistry::get(typeId);
    const auto& objectDesc = ObjectDescriptorRegistry::get(typeId);
    const glm::mat4 meshToLogical = objectDesc.meshToLogicalBasis4();

    constexpr float DEFAULT_ASSEMBLY_LOD_SWITCH_DISTANCE = 2500.0f;

    ObjectAssembly assembly;
    assembly.typeId = typeId;
    assembly.lodSwitchDistance =
        (desc.lodSwitchDistance > 0.0f)
            ? desc.lodSwitchDistance
            : DEFAULT_ASSEMBLY_LOD_SWITCH_DISTANCE;

    for (const auto& modDesc : desc.modules)
    {
        if (!modDesc.enabled)
            continue;

        AssemblyModule module;
        module.id = modDesc.moduleId;
        module.subsystemId = modDesc.subsystemId;

        // module.localPosition = modDesc.localPosition;
        // module.localRotationDeg = modDesc.localRotationDeg;
        // module.pivot = modDesc.pivot;
        // module.rotates = modDesc.rotates;
        // module.rotationAxis = modDesc.rotationAxis;
        // module.rotationSpeed = modDesc.rotationSpeed;

        module.localPosition = transformPoint(meshToLogical, modDesc.localPosition);
        module.localRotationDeg = modDesc.localRotationDeg; // пока оставляем как есть
        module.pivot = transformPoint(meshToLogical, modDesc.pivot);
        module.rotationAxis = transformDirection(meshToLogical, modDesc.rotationAxis);
        module.rotates = modDesc.rotates;
        module.rotationSpeed = modDesc.rotationSpeed;

        for (const auto& meshDesc : modDesc.meshes)
        {
            if (!meshDesc.enabled)
                continue;

            AssemblyMeshPart part;
            part.id = meshDesc.meshId;
            part.lod0Path = meshDesc.lod0Path;
            part.lod1Path = meshDesc.lod1Path;
            // part.localOffset = meshDesc.localOffset;
            part.localOffset = transformPoint(meshToLogical, meshDesc.localOffset);

            computeRawBoundsFromObj(
                part.lod0Path,
                part.rawMinBounds,
                part.rawMaxBounds,
                part.rawCenter
            );


            {
                glm::vec3 rotatedMin(0.0f);
                glm::vec3 rotatedMax(0.0f);

                rotateBoundsByCorners(
                    part.rawMinBounds,
                    part.rawMaxBounds,
                    meshToLogical,
                    rotatedMin,
                    rotatedMax
                );

                part.rawMinBounds = rotatedMin;
                part.rawMaxBounds = rotatedMax;
                part.rawCenter = (rotatedMin + rotatedMax) * 0.5f;
            }



            if (!ObjLoader::load(part.lod0Path, part.lod0Mesh, false))
            {
                throw std::runtime_error(
                    "[AssemblyMeshLibrary] failed to load lod0 mesh part: " + part.lod0Path
                );
            }

            if (!ObjLoader::load(part.lod1Path, part.lod1Mesh, false))
            {
                throw std::runtime_error(
                    "[AssemblyMeshLibrary] failed to load lod1 mesh part: " + part.lod1Path
                );
            }

            applyBasisToMesh(part.lod0Mesh, meshToLogical);
            applyBasisToMesh(part.lod1Mesh, meshToLogical);




            module.meshes.push_back(std::move(part));
        }

        if (!module.meshes.empty())
        {
            computeModuleBounds(module);
            assembly.modules.push_back(std::move(module));
        }
    }

    computeAssemblyBounds(assembly);
    normalizeAssemblyToDescriptorSize(assembly);
    computeAssemblyBounds(assembly);
    computeAssemblyBoundingSphere(assembly);

    for (auto& module : assembly.modules)
    {
        for (auto& part : module.meshes)
        {
            part.lod0Gpu.upload(part.lod0Mesh);
            part.lod1Gpu.upload(part.lod1Mesh);
        }
    }

    glm::vec3 finalSize = assembly.maxBounds - assembly.minBounds;
    std::cout << "[AssemblyMeshLibrary] FINAL SIZE "
              << finalSize.x << " "
              << finalSize.y << " "
              << finalSize.z << std::endl;

    return assembly;
}






void AssemblyMeshLibrary::computeRawBoundsFromObj(
    const std::string& path,
    glm::vec3& outMin,
    glm::vec3& outMax,
    glm::vec3& outCenter
)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ok = tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warn,
        &err,
        path.c_str(),
        nullptr,
        false
    );

    // if (!warn.empty())
    //     std::cout << "[AssemblyMeshLibrary] OBJ WARNING: " << warn << std::endl;

    if (!err.empty())
        std::cout << "[AssemblyMeshLibrary] OBJ ERROR: " << err << std::endl;

    if (!ok)
        throw std::runtime_error(
            "[AssemblyMeshLibrary] raw bounds load failed: " + path
        );

    if (attrib.vertices.empty())
        throw std::runtime_error(
            "[AssemblyMeshLibrary] OBJ has no vertices: " + path
        );

    outMin = glm::vec3( 1e9f);
    outMax = glm::vec3(-1e9f);

    for (size_t i = 0; i < attrib.vertices.size() / 3; ++i)
    {
        glm::vec3 p(
            attrib.vertices[i * 3 + 0],
            attrib.vertices[i * 3 + 1],
            attrib.vertices[i * 3 + 2]
        );

        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }

    outCenter = (outMin + outMax) * 0.5f;
}


void AssemblyMeshLibrary::computeModuleBounds(AssemblyModule& module)
{
    bool first = true;
    glm::vec3 moduleMin(0.0f);
    glm::vec3 moduleMax(0.0f);

    for (auto& part : module.meshes)
    {
        part.lod0Mesh.computeBounds();

        part.minBounds = part.lod0Mesh.minBounds + module.localPosition + part.localOffset;
        part.maxBounds = part.lod0Mesh.maxBounds + module.localPosition + part.localOffset;
        part.boundCenter = (part.minBounds + part.maxBounds) * 0.5f;
        part.boundHalfSize = (part.maxBounds - part.minBounds) * 0.5f;

        if (first)
        {
            moduleMin = part.minBounds;
            moduleMax = part.maxBounds;
            first = false;
        }
        else
        {
            moduleMin = glm::min(moduleMin, part.minBounds);
            moduleMax = glm::max(moduleMax, part.maxBounds);
        }
    }

    if (first)
    {
        module.minBounds = glm::vec3(0.0f);
        module.maxBounds = glm::vec3(0.0f);
        module.boundCenter = glm::vec3(0.0f);
        module.boundHalfSize = glm::vec3(0.5f);
        return;
    }

    module.minBounds = moduleMin;
    module.maxBounds = moduleMax;
    module.boundCenter = (moduleMin + moduleMax) * 0.5f;
    module.boundHalfSize = (moduleMax - moduleMin) * 0.5f;
}




void AssemblyMeshLibrary::computeAssemblyBounds(ObjectAssembly& assembly)
{
    bool first = true;
    glm::vec3 globalMin(0.0f);
    glm::vec3 globalMax(0.0f);

    for (auto& module : assembly.modules)
    {
        computeModuleBounds(module);

        if (first)
        {
            globalMin = module.minBounds;
            globalMax = module.maxBounds;
            first = false;
        }
        else
        {
            globalMin = glm::min(globalMin, module.minBounds);
            globalMax = glm::max(globalMax, module.maxBounds);
        }
    }

    if (first)
    {
        assembly.minBounds = glm::vec3(0.0f);
        assembly.maxBounds = glm::vec3(0.0f);
        assembly.boundCenter = glm::vec3(0.0f);
        return;
    }

    assembly.minBounds = globalMin;
    assembly.maxBounds = globalMax;
    assembly.boundCenter = (globalMin + globalMax) * 0.5f;
}





void AssemblyMeshLibrary::normalizeAssemblyToDescriptorSize(ObjectAssembly& assembly)
{
    const auto& desc = ObjectDescriptorRegistry::get(assembly.typeId);

    
    
    const glm::vec3 currentSize = assembly.maxBounds - assembly.minBounds;
    const LogicalDimensions& dims = desc.logicalDimensions();

    if (!dims.enabled)
    {
        std::cout
            << "[AssemblyMeshLibrary] normalize skipped: logicalDimensions disabled"
            << std::endl;
        return;
    }

    const float measuredWidth  = currentSize.x;
    const float measuredHeight = currentSize.y;
    const float measuredLength = currentSize.z;

    std::cout
        << "[AssemblyMeshLibrary] NORMALIZE "
        << " logical currentSize (W,H,L) = "
        << measuredWidth << ", " << measuredHeight << ", " << measuredLength
        << " target (W,H,L) = "
        << dims.width << ", " << dims.height << ", " << dims.length
        << " scaleRef=" << static_cast<int>(dims.scaleReference)
        << " enabled=" << (dims.enabled ? 1 : 0)
        << std::endl;

    float uniformScale = 1.0f;

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
        << "[AssemblyMeshLibrary] NORMALIZE chosen scale = "
        << uniformScale
        << std::endl;

    


    

    


  


    for (auto& module : assembly.modules)
    {
        module.localPosition *= uniformScale;
        module.pivot *= uniformScale;

        for (auto& part : module.meshes)
        {
            part.localOffset *= uniformScale;

            for (auto& v : part.lod0Mesh.vertices)
                v.position *= uniformScale;

            for (auto& e : part.lod0Mesh.edges)
            {
                e.a *= uniformScale;
                e.b *= uniformScale;
            }

            for (auto& v : part.lod1Mesh.vertices)
                v.position *= uniformScale;

            for (auto& e : part.lod1Mesh.edges)
            {
                e.a *= uniformScale;
                e.b *= uniformScale;
            }

            part.lod0Mesh.computeBounds();
            part.lod0Mesh.computeBoundingSphere();

            part.lod1Mesh.computeBounds();
            part.lod1Mesh.computeBoundingSphere();
        }
    }

     for (auto& module : assembly.modules)
    {
        computeModuleBounds(module);
    }

    computeAssemblyBounds(assembly);

    const glm::vec3 finalSize = assembly.maxBounds - assembly.minBounds;

    std::cout
        << "[AssemblyMeshLibrary] FINAL logical size (W,H,L) = "
        << finalSize.x << ", " << finalSize.y << ", " << finalSize.z
        << std::endl;


}







void AssemblyMeshLibrary::computeAssemblyBoundingSphere(ObjectAssembly& assembly)
{
    glm::vec3 center = (assembly.minBounds + assembly.maxBounds) * 0.5f;
    assembly.boundCenter = center;

    float r2 = 0.0f;

    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            for (const auto& v : part.lod0Mesh.vertices)
            {
                glm::vec3 p =
                    v.position +
                    module.localPosition +
                    part.localOffset;

                float d2 = glm::dot(p - center, p - center);
                r2 = std::max(r2, d2);
            }
        }
    }

    assembly.boundRadius = std::sqrt(r2);
}