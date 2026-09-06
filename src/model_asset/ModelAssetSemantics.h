#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

struct SemanticNodeUsage
{
    std::size_t renderBindings = 0;
    std::size_t children = 0;
    std::size_t collisions = 0;
    std::size_t sockets = 0;
    std::size_t stateVariants = 0;
    std::size_t hitRegions = 0;
    std::size_t openings = 0;
    std::size_t repairTargets = 0;
    std::size_t legacySourceBootstrapCollisions = 0;
    bool physicsEnabled = false;

    bool hasRuntimePayload() const;
    bool isOrphanCandidate() const;
};

struct SemanticEraseResult
{
    std::string deletedId;
    std::vector<std::size_t> affectedRenderLods;
    std::size_t unboundRenderNodes = 0;
    std::size_t removedCollisions = 0;
    std::size_t removedSockets = 0;
    std::size_t removedStateVariants = 0;
    std::size_t removedHitRegions = 0;
    std::size_t removedOpenings = 0;
    std::size_t removedRepairTargets = 0;
};

bool isLegacySourceBootstrapCollision(const ModelAsset& asset, std::size_t nodeIndex, const CollisionVolume& collision);
SemanticNodeUsage inspectSemanticNodeUsage(const ModelAsset& asset, std::size_t nodeIndex);
SemanticEraseResult eraseSemanticNode(ModelAsset& asset, std::size_t nodeIndex, bool removeOwnedPayload);

} // namespace elite::model_asset
