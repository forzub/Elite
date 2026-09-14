#include "src/game/assets/RuntimeModelAssetLibrary.h"

#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "src/game/assets/CompiledModelAssetReader.h"
#include "src/game/assets/LegacyAssemblyModelAdapter.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/model_asset/ModelAssetBinary.h"

namespace game::assets
{
namespace
{
using elite::model_asset::ModelAsset;
using game::ship::geometry::AssemblyMeshLibrary;

struct RuntimeModelAssetState
{
    std::mutex mutex;
    std::map<ObjectType, RuntimeModelAssetSource> sources;
    std::map<ObjectType, ModelAsset> cache;
};

RuntimeModelAssetState& runtimeState()
{
    static RuntimeModelAssetState state;
    return state;
}

RuntimeModelAssetSource sourceUnlocked(
    const RuntimeModelAssetState& state,
    ObjectType typeId)
{
    const auto it = state.sources.find(typeId);
    if (it != state.sources.end())
        return it->second;

    // Behavior-preserving migration default: old OBJ/ObjectAssembly remains the
    // source until a concrete object type is explicitly switched to .elmodel.
    return RuntimeModelAssetSource{};
}

ModelAsset loadLegacy(ObjectType typeId)
{
    if (typeId == ObjectType::None)
        throw std::runtime_error("runtime model asset legacy source requires ObjectType");

    if (!AssemblyMeshLibrary::has(typeId))
    {
        throw std::runtime_error(
            "no legacy ObjectAssembly registered for ObjectType " +
            std::to_string(static_cast<std::uint16_t>(typeId)));
    }

    ModelAsset asset = LegacyAssemblyModelAdapter::convert(
        typeId,
        AssemblyMeshLibrary::get(typeId));

    std::string validationError;
    if (!elite::model_asset::ModelAssetBinary::validate(asset, &validationError))
    {
        throw std::runtime_error(
            "legacy ObjectAssembly -> ModelAsset conversion failed validation: " +
            validationError);
    }

    return asset;
}

ModelAsset loadBinary(ObjectType typeId, const std::string& path)
{
    ModelAsset asset;
    std::string error;
    if (!CompiledModelAssetReader::load(path, typeId, asset, &error))
        throw std::runtime_error(error);
    return asset;
}

} // namespace

void RuntimeModelAssetLibrary::useLegacy(ObjectType typeId)
{
    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.sources[typeId] = RuntimeModelAssetSource{};
    state.cache.erase(typeId);
}

void RuntimeModelAssetLibrary::useBinary(ObjectType typeId, std::string path)
{
    if (path.empty())
        throw std::invalid_argument("RuntimeModelAssetLibrary::useBinary path is empty");

    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);

    RuntimeModelAssetSource source;
    source.kind = RuntimeModelAssetSourceKind::CompiledBinary;
    source.path = std::move(path);
    state.sources[typeId] = std::move(source);
    state.cache.erase(typeId);
}

RuntimeModelAssetSource RuntimeModelAssetLibrary::source(ObjectType typeId)
{
    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return sourceUnlocked(state, typeId);
}

bool RuntimeModelAssetLibrary::isLoaded(ObjectType typeId)
{
    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.cache.find(typeId) != state.cache.end();
}

const ModelAsset& RuntimeModelAssetLibrary::get(ObjectType typeId)
{
    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);

    const auto cached = state.cache.find(typeId);
    if (cached != state.cache.end())
        return cached->second;

    const RuntimeModelAssetSource selected = sourceUnlocked(state, typeId);
    ModelAsset loaded;

    switch (selected.kind)
    {
        case RuntimeModelAssetSourceKind::LegacyObjAssembly:
            loaded = loadLegacy(typeId);
            break;

        case RuntimeModelAssetSourceKind::CompiledBinary:
            loaded = loadBinary(typeId, selected.path);
            break;
    }

    auto [it, inserted] = state.cache.emplace(typeId, std::move(loaded));
    (void)inserted;
    return it->second;
}

void RuntimeModelAssetLibrary::clear(ObjectType typeId)
{
    auto& state = runtimeState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.cache.erase(typeId);
}

} // namespace game::assets
