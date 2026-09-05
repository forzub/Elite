#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

// Runtime LOD policy is expressed in screen pixels. The authored per-LOD value
// is scale independent: relativeGeometricError = omittedFeature / modelCharacteristic.
// The renderer supplies the *actual* projected characteristic size after every
// gameplay/world scale has been applied. Blender/source units never enter the
// runtime decision.
inline constexpr float DefaultLodVisibilityCutoffPx = 2.0f;
inline constexpr float DefaultLodCoarsenCutoffPx = 1.8f;
inline constexpr float DefaultLodRefineCutoffPx = 2.2f;

inline float lodCharacteristicSize(const glm::vec3& extents)
{
    float values[3] = {
        std::max(0.0f, extents.x),
        std::max(0.0f, extents.y),
        std::max(0.0f, extents.z)
    };
    std::sort(std::begin(values), std::end(values));
    return std::max(0.01f, values[1]);
}

inline float relativeGeometricErrorForFeature(float featureSize, float modelCharacteristic)
{
    if (!std::isfinite(featureSize) || !std::isfinite(modelCharacteristic) ||
        featureSize <= 0.0f || modelCharacteristic <= 0.0f)
        return -1.0f;
    return featureSize / modelCharacteristic;
}

inline bool hasRuntimeLodError(const RenderLod& lod)
{
    if (lod.level == 0) return true;
    return std::isfinite(lod.relativeGeometricError) && lod.relativeGeometricError > 0.0f;
}

inline float runtimeLodErrorRatio(const RenderLod& lod)
{
    if (lod.level == 0) return 0.0f;
    return hasRuntimeLodError(lod) ? lod.relativeGeometricError : -1.0f;
}

inline float projectedGeometricErrorPixels(
    const RenderLod& lod,
    float projectedCharacteristicPixels)
{
    const float ratio = runtimeLodErrorRatio(lod);
    if (ratio < 0.0f || !std::isfinite(projectedCharacteristicPixels) || projectedCharacteristicPixels < 0.0f)
        return std::numeric_limits<float>::infinity();
    return ratio * projectedCharacteristicPixels;
}

// Convenience for a perspective camera when the renderer has a real world-space
// characteristic size. A renderer that already projected final world-scaled
// bounds should pass that measured projected size directly to the selector.
inline float perspectiveProjectedCharacteristicPixels(
    float worldCharacteristicMeters,
    float distanceMeters,
    float verticalFovRadians,
    float viewportHeightPixels)
{
    if (!std::isfinite(worldCharacteristicMeters) || !std::isfinite(distanceMeters) ||
        !std::isfinite(verticalFovRadians) || !std::isfinite(viewportHeightPixels) ||
        worldCharacteristicMeters <= 0.0f || distanceMeters <= 0.0f ||
        verticalFovRadians <= 0.0f || verticalFovRadians >= 3.13f || viewportHeightPixels <= 0.0f)
        return 0.0f;
    const float focalPixels = viewportHeightPixels / (2.0f * std::tan(verticalFovRadians * 0.5f));
    return worldCharacteristicMeters * focalPixels / distanceMeters;
}

// Stateful selector with hysteresis. Unknown/non-authored SSE metadata is a hard
// conservative boundary: the selector never auto-coarsens into that LOD. If the
// current LOD has lost its metadata, it refines toward the nearest valid level.
inline std::size_t selectRenderLodScreenSpace(
    const std::vector<RenderLod>& lods,
    std::size_t currentLodIndex,
    float projectedCharacteristicPixels,
    float coarsenCutoffPx = DefaultLodCoarsenCutoffPx,
    float refineCutoffPx = DefaultLodRefineCutoffPx)
{
    if (lods.empty()) return 0;
    std::size_t selected = std::min(currentLodIndex, lods.size() - 1);
    if (!std::isfinite(projectedCharacteristicPixels) || projectedCharacteristicPixels < 0.0f)
        return 0;
    if (!std::isfinite(coarsenCutoffPx) || !std::isfinite(refineCutoffPx) ||
        coarsenCutoffPx <= 0.0f || refineCutoffPx <= coarsenCutoffPx)
    {
        coarsenCutoffPx = DefaultLodCoarsenCutoffPx;
        refineCutoffPx = DefaultLodRefineCutoffPx;
    }

    while (selected > 0)
    {
        if (!hasRuntimeLodError(lods[selected]) ||
            projectedGeometricErrorPixels(lods[selected], projectedCharacteristicPixels) > refineCutoffPx)
        {
            --selected;
            continue;
        }
        break;
    }

    while (selected + 1 < lods.size())
    {
        const auto& candidate = lods[selected + 1];
        if (!hasRuntimeLodError(candidate)) break;
        if (projectedGeometricErrorPixels(candidate, projectedCharacteristicPixels) > coarsenCutoffPx)
            break;
        ++selected;
    }
    return selected;
}

} // namespace elite::model_asset
