#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

void writeMeta(Writer& w, const ModelAsset& a)
{
    w.string(a.assetId);
    w.string(a.displayName);
    w.pod(a.sourceObjectType);
    w.pod(a.lodSwitchDistance);
    w.string(a.sourceBasis.preset);
    w.pod(static_cast<std::int8_t>(a.sourceBasis.right));
    w.pod(static_cast<std::int8_t>(a.sourceBasis.up));
    w.pod(static_cast<std::int8_t>(a.sourceBasis.forward));
    w.pod(static_cast<std::uint8_t>(a.sourceBasis.canonicalized ? 1 : 0));
    w.vec3(a.minBounds);
    w.vec3(a.maxBounds);
}

void readMeta(Reader& r, ModelAsset& a)
{
    r.string(a.assetId);
    r.string(a.displayName);
    r.pod(a.sourceObjectType);
    r.pod(a.lodSwitchDistance);
    r.string(a.sourceBasis.preset);
    std::int8_t right = 0, up = 0, forward = 0;
    r.pod(right); r.pod(up); r.pod(forward);
    a.sourceBasis.right = static_cast<AxisDirection>(right);
    a.sourceBasis.up = static_cast<AxisDirection>(up);
    a.sourceBasis.forward = static_cast<AxisDirection>(forward);
    std::uint8_t canonicalized = 0;
    r.pod(canonicalized);
    a.sourceBasis.canonicalized = canonicalized != 0;
    r.vec3(a.minBounds);
    r.vec3(a.maxBounds);
}

void writeMaterials(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.materials.size()));
    for (const auto& m : a.materials)
    {
        w.string(m.id); w.string(m.sourceName);
        w.vec4(m.baseColor); w.vec3(m.emissiveColor);
        w.pod(m.emissiveStrength); w.pod(m.metallic); w.pod(m.roughness);
        w.pod(static_cast<std::uint8_t>(m.twoSided ? 1 : 0));
        w.string(m.baseColorTexture); w.string(m.emissiveTexture);
    }
}

void readMaterials(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.materials.resize(count);
    for (auto& m : a.materials)
    {
        r.string(m.id); r.string(m.sourceName);
        r.vec4(m.baseColor); r.vec3(m.emissiveColor);
        r.pod(m.emissiveStrength); r.pod(m.metallic); r.pod(m.roughness);
        std::uint8_t twoSided = 0;
        r.pod(twoSided);
        m.twoSided = twoSided != 0;
        r.string(m.baseColorTexture); r.string(m.emissiveTexture);
    }
}

void writePhysicalSizeV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint8_t>(a.physicalSize.enabled ? 1 : 0));
    w.pod(static_cast<std::uint8_t>(a.physicalSize.axis));
    w.pod(a.physicalSize.targetMeters);
    w.pod(static_cast<std::uint8_t>(0));
    w.pod(a.physicalSize.sourceExtent);
    w.pod(a.physicalSize.sourceToMeters);
    w.pod(static_cast<std::uint8_t>(a.physicalSize.gameLinked ? 1 : 0));
    w.vec3(a.physicalSize.gameDimensionsMeters);
    w.pod(static_cast<std::uint8_t>(a.physicalSize.geometrySpace));
}

void readPhysicalSizeV4(Reader& r, ModelAsset& a)
{
    std::uint8_t enabled = 0, axis = 2, legacyAutoApply = 0;
    r.pod(enabled); r.pod(axis); r.pod(a.physicalSize.targetMeters); r.pod(legacyAutoApply);
    a.physicalSize.enabled = enabled != 0;
    a.physicalSize.axis = static_cast<PhysicalSizeAxis>(axis);
    a.physicalSize.autoApplyOnSourceImport = false;

    constexpr std::size_t ExtendedBytes = sizeof(float) * 2 + sizeof(std::uint8_t) + sizeof(float) * 3 + sizeof(std::uint8_t);
    if (r.remaining() >= ExtendedBytes)
    {
        std::uint8_t gameLinked = 0, geometrySpace = 0;
        r.pod(a.physicalSize.sourceExtent);
        r.pod(a.physicalSize.sourceToMeters);
        r.pod(gameLinked);
        r.vec3(a.physicalSize.gameDimensionsMeters);
        r.pod(geometrySpace);
        a.physicalSize.gameLinked = gameLinked != 0;
        a.physicalSize.geometrySpace = static_cast<PhysicalGeometrySpace>(geometrySpace);
    }
    else
    {
        a.physicalSize.sourceExtent = 0.0f;
        a.physicalSize.sourceToMeters = 1.0f;
        a.physicalSize.gameLinked = false;
        a.physicalSize.gameDimensionsMeters = glm::vec3(0.0f);
        a.physicalSize.geometrySpace = a.physicalSize.enabled
            ? PhysicalGeometrySpace::LegacyUnknown
            : PhysicalGeometrySpace::Authoring;
        a.physicalSize.autoApplyOnSourceImport = legacyAutoApply != 0;
    }
}

} // namespace elite::model_asset::binary
