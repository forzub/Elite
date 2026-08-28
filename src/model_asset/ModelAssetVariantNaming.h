#pragma once

#include <string>

namespace elite::model_asset
{

inline constexpr const char* SourceRenderVariantPrefix = "source_variant.";
inline constexpr const char* LegacyRenderVariantMarker = ".variant.";

struct RenderVariantIdentity
{
    bool isVariant = false;
    std::string variantId;
    // v0.9.4 encoded the replacement target into the geometry id. Keep this
    // only so already-written editor checkpoints remain readable. Current
    // authoring identity is an opaque workspace id independent of filenames.
    std::string legacyBaseGeometryId;
};

inline std::string makeRenderVariantGeometryId(const std::string& variantId)
{
    return std::string(SourceRenderVariantPrefix) + variantId;
}

inline RenderVariantIdentity renderVariantIdentity(const std::string& geometryId)
{
    RenderVariantIdentity result;
    const std::string prefix(SourceRenderVariantPrefix);
    if (geometryId.rfind(prefix, 0) == 0 && geometryId.size() > prefix.size())
    {
        result.isVariant = true;
        result.variantId = geometryId.substr(prefix.size());
        return result;
    }

    const std::string marker(LegacyRenderVariantMarker);
    const auto pos = geometryId.find(marker);
    if (pos == std::string::npos || pos == 0 || pos + marker.size() >= geometryId.size())
        return result;

    result.isVariant = true;
    result.legacyBaseGeometryId = geometryId.substr(0, pos);
    result.variantId = geometryId.substr(pos + marker.size());
    return result;
}

inline bool isRenderVariantGeometryId(const std::string& geometryId)
{
    return renderVariantIdentity(geometryId).isVariant;
}

} // namespace elite::model_asset
