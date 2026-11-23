#include "src/game/scene/LayoutCityHitVolumes.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/damage/HitComponent.h"
#include "src/model_asset/HubCityAssembly.h"
#include "src/model_asset/HubExpansion.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/world/modules/HitVolumeSnapshotBuilder.h"

namespace game::scene
{
void collisionToHitVolumes(ObjectType type, const elite::model_asset::ModelAsset& asset,
                           std::vector<game::damage::HitVolume>& out)
{
    namespace ma = elite::model_asset;
    const glm::mat4 basis = ObjectDescriptorRegistry::get(type).meshToLogicalBasis4();
    for (const auto& c : asset.collisionVolumes)
    {
        if (!c.enabled ||
            (!c.activeStates.empty() &&
             std::find(c.activeStates.begin(), c.activeStates.end(), "intact") == c.activeStates.end()))
            continue;
        glm::vec3 half;
        switch (c.shape)
        {
            case ma::CollisionShape::Box: half = c.halfSize; break;
            case ma::CollisionShape::Sphere: half = glm::vec3(c.radius); break;
            case ma::CollisionShape::Capsule: half = glm::vec3(c.radius, c.halfHeight + c.radius, c.radius); break;
            case ma::CollisionShape::Cylinder:
            {
                const float r = std::max(c.radius, c.radiusTop);
                half = glm::vec3(r, c.halfHeight, r);
                break;
            }
            default: continue;
        }
        glm::mat4 m = glm::translate(glm::mat4(1.0f), c.localPosition);
        m = glm::rotate(m, glm::radians(c.localRotationDeg.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(c.localRotationDeg.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(c.localRotationDeg.z), glm::vec3(0, 0, 1));
        m = basis * m;
        game::damage::HitVolume v;
        v.zone = game::damage::HitZoneType::Hull;
        v.center = glm::vec3(m[3]);
        glm::mat3 r(m);
        for (int k = 0; k < 3; ++k)
        {
            const float len = glm::length(r[k]);
            half[k] *= len;
            if (len > 1.0e-6f)
                r[k] /= len;
        }
        v.orientation = r;
        v.halfSize = half;
        v.m_label = c.id;
        v.moduleId = c.moduleId;
        v.assemblyNode = c.parentNodeIndex;   // the building (city damage: game::simulation::CityDamage.h)
        // the node's module group (a block, a street run) - the route planner merges a group far from the route
        if (c.parentNodeIndex >= 0 && static_cast<std::size_t>(c.parentNodeIndex) < asset.nodes.size())
            v.subsystemId = asset.nodes[static_cast<std::size_t>(c.parentNodeIndex)].moduleId;
        out.push_back(std::move(v));
    }
}

bool layoutCityHitVolumesOnClient(ObjectType type, const std::string& hubId)
{
    // ELITE_REPLICATE_CITY_HIT_VOLUMES=1: the old way (the server sends them)
    static const bool replicate = [] {
        const char* v = std::getenv("ELITE_REPLICATE_CITY_HIT_VOLUMES");
        return v && v[0] == '1';
    }();
    if (replicate || type != ObjectType::HubBeltCity || hubId.empty())
        return false;
    namespace ma = elite::model_asset;
    // cities with construction stages too: the client assembles the stages by its own clock, as its renderer already
    // does (271 of the 294 layout cities have an expansion plan - excluding them left the bootstrap at 63 MB)
    static std::mutex mutex;
    static std::map<std::string, bool> known;
    std::lock_guard<std::mutex> lock(mutex);
    const auto [it, inserted] = known.try_emplace(hubId, false);
    if (inserted)
        it->second = std::filesystem::exists(ma::hubCityLayoutPath(hubId));
    return it->second;
}

namespace
{
struct CityEntry
{
    std::atomic<bool> ready {false};
    std::uint64_t stageKey = 0;
    std::vector<game::simulation::DebugHitVolumeSnapshot> volumes;
};
} // namespace

const std::vector<game::simulation::DebugHitVolumeSnapshot>* clientLayoutCityHitVolumes(ObjectType type,
                                                                                       const std::string& hubId)
{
    static std::mutex mutex;
    // per hub: the current entry and the one being built for a new construction stage (the old one stays in use
    // until the new one is ready)
    static std::map<std::string, std::shared_ptr<CityEntry>> cache, building;
    const std::uint64_t key = elite::model_asset::hubExpansionStageKey(hubId);
    std::shared_ptr<CityEntry> entry;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto& e = cache[hubId];
        auto& b = building[hubId];
        if (b && b->ready)
        {
            e = b;
            b.reset();
        }
        const bool stale = e && e->ready && e->stageKey != key;
        if ((!e || stale) && !b)
        {
            auto fresh = std::make_shared<CityEntry>();
            fresh->stageKey = key;
            if (!e)
                e = fresh;
            else
                b = fresh;
            std::shared_ptr<CityEntry> job = fresh;
            const std::string id = hubId;
            std::cerr << "[CityHitVolumes] " << hubId << ": assembling on the client (stage key " << key << ")" << std::endl;
            std::thread([job, id, type] {
                namespace ma = elite::model_asset;
                ma::ModelAsset city;
                game::damage::HitComponent hit;
                if (ma::assembleHubCityCollision(id, "assets/compiled/hub_modules", city))
                    collisionToHitVolumes(type, city, hit.volumes);
                job->volumes = world::modules::HitVolumeSnapshotBuilder::build(hit);
                std::cerr << "[CityHitVolumes] " << id << ": " << job->volumes.size()
                          << " hit volumes assembled on the client" << std::endl;
                job->ready = true;
            }).detach();
        }
        entry = e;
    }
    return entry->ready ? &entry->volumes : nullptr;
}
} // namespace game::scene
