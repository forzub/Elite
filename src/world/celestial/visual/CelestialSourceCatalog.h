#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace world::celestial::visual
{

enum class CelestialBodyShape
{
    Sphere,
    IrregularMesh
};

enum class CelestialProjection
{
    Equirectangular,
    MeshUv
};

// -----------------------------------------------------------------------------
// Source assets: реальные входные файлы.
// required_assets / optional_assets.
// -----------------------------------------------------------------------------
struct CelestialSourceCatalogAssetGroup
{
    std::string albedo;
    std::string height;
    std::string clouds;
    std::string emission;
    std::string mesh;
};

// -----------------------------------------------------------------------------
// Derived outputs: канонические выходы baker'а.
// derived_outputs.
// -----------------------------------------------------------------------------
struct CelestialSourceCatalogOutputGroup
{
    std::string mesh;

    std::string albedo;
    std::string normal;
    std::string material;

    std::string clouds;
    std::string emission;

    std::string preview;
    std::string previewFlat;
    std::string meta;
};

struct CelestialSourceCatalog
{
    std::string systemId;
    std::string bodyId;
    std::string bodyName;
    std::string sourcePolicy;

    CelestialBodyShape bodyShape =
        CelestialBodyShape::Sphere;

    CelestialProjection projection =
        CelestialProjection::Equirectangular;

    CelestialSourceCatalogAssetGroup required;
    CelestialSourceCatalogAssetGroup optional;

    CelestialSourceCatalogOutputGroup outputs;

    std::filesystem::path catalogPath;
};

inline CelestialBodyShape parseBodyShape(const std::string& s)
{
    if (s == "irregular_mesh")
        return CelestialBodyShape::IrregularMesh;

    return CelestialBodyShape::Sphere;
}

inline CelestialProjection parseProjection(const std::string& s)
{
    if (s == "mesh_uv")
        return CelestialProjection::MeshUv;

    return CelestialProjection::Equirectangular;
}

inline const char* toString(CelestialBodyShape s)
{
    switch (s)
    {
        case CelestialBodyShape::Sphere:
            return "sphere";

        case CelestialBodyShape::IrregularMesh:
            return "irregular_mesh";

        default:
            return "sphere";
    }
}

inline const char* toString(CelestialProjection p)
{
    switch (p)
    {
        case CelestialProjection::Equirectangular:
            return "equirectangular";

        case CelestialProjection::MeshUv:
            return "mesh_uv";

        default:
            return "equirectangular";
    }
}

inline std::string readAssetString(
    const nlohmann::json& root,
    const char* groupName,
    const char* key
)
{
    if (!root.contains(groupName))
        return {};

    if (!root[groupName].is_object())
        return {};

    if (!root[groupName].contains(key))
        return {};

    if (!root[groupName][key].is_string())
        return {};

    return root[groupName][key].get<std::string>();
}

inline CelestialSourceCatalogAssetGroup readInputAssetGroup(
    const nlohmann::json& root,
    const char* groupName
)
{
    CelestialSourceCatalogAssetGroup out;

    out.albedo =
        readAssetString(root, groupName, "albedo");

    out.height =
        readAssetString(root, groupName, "height");

    out.clouds =
        readAssetString(root, groupName, "clouds");

    out.emission =
        readAssetString(root, groupName, "emission");

    out.mesh =
        readAssetString(root, groupName, "mesh");

    return out;
}

inline CelestialSourceCatalogOutputGroup readOutputAssetGroup(
    const nlohmann::json& root,
    const char* groupName
)
{
    CelestialSourceCatalogOutputGroup out;

    out.mesh =
        readAssetString(root, groupName, "mesh");

    out.albedo =
        readAssetString(root, groupName, "albedo");

    out.normal =
        readAssetString(root, groupName, "normal");

    out.material =
        readAssetString(root, groupName, "material");

    out.clouds =
        readAssetString(root, groupName, "clouds");

    out.emission =
        readAssetString(root, groupName, "emission");

    out.preview =
        readAssetString(root, groupName, "preview");

    out.previewFlat =
        readAssetString(root, groupName, "preview_flat");

    out.meta =
        readAssetString(root, groupName, "meta");

    return out;
}

inline bool loadCelestialSourceCatalog(
    const std::filesystem::path& path,
    CelestialSourceCatalog& out
)
{
    std::ifstream f(path);

    if (!f.is_open())
    {
        std::cerr
            << "[CelestialSourceCatalog] cannot open: "
            << path.generic_string()
            << "\n";

        return false;
    }

    nlohmann::json root;
    f >> root;

    out = CelestialSourceCatalog {};
    out.catalogPath = path;

    out.systemId =
        root.value("system_id", "");

    out.bodyId =
        root.value("body_id", "");

    out.bodyName =
        root.value("body_name", "");

    out.sourcePolicy =
        root.value("source_policy", "");

    out.bodyShape =
        parseBodyShape(root.value("body_shape", "sphere"));

    out.projection =
        parseProjection(root.value("projection", "equirectangular"));

    out.required =
        readInputAssetGroup(root, "required_assets");

    out.optional =
        readInputAssetGroup(root, "optional_assets");

    out.outputs =
        readOutputAssetGroup(root, "derived_outputs");

    return true;
}

} // namespace world::celestial::visual