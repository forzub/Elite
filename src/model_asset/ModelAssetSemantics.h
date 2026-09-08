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
    std::size_t structuralLinks = 0;
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
    std::size_t removedStructuralLinks = 0;
};

// Conservative migration for the editor/runtime-assembly bootstrap used before
// semantic Nodes and RenderNodes were separated cleanly. The old importer built
// one module semantic Node plus a synthetic same-module visual child. This
// operation recognizes only that exact low-risk scaffold, rebinds its visual
// RenderNodes to the module Node, leaves the render transform graph intact, and
// removes the redundant semantic child. It deliberately does NOT flatten real
// transform parents and does NOT invent/delete structural graph links.
struct LegacySemanticCleanupResult
{
    std::vector<std::string> removedNodeIds;
    std::vector<std::size_t> affectedRenderLods;
    std::size_t reboundRenderNodes = 0;
    std::size_t clearedTransformOnlyBindings = 0;
};

bool isLegacySourceBootstrapCollision(const ModelAsset& asset, std::size_t nodeIndex, const CollisionVolume& collision);
SemanticNodeUsage inspectSemanticNodeUsage(const ModelAsset& asset, std::size_t nodeIndex);
SemanticEraseResult eraseSemanticNode(ModelAsset& asset, std::size_t nodeIndex, bool removeOwnedPayload);
LegacySemanticCleanupResult cleanLegacySyntheticVisualSemanticNodes(ModelAsset& asset);

} // namespace elite::model_asset
