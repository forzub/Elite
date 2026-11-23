#pragma once

// FAR LIGHTS of model assets (src/model_asset/ModelAssetLights.h): every light source of a drawn asset as a point with
// a physical brightness, independent of the geometry LOD (author 2026-11-23: «огни видны очень далеко»). One static
// vertex buffer per asset (its lights in asset space), one GL_POINTS draw per drawn object; additive, depth-tested,
// no depth write. ELITE_FAR_LIGHTS=0 turns it off (A/B), ELITE_FAR_LIGHTS_GAIN scales the brightness.
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/model_asset/ModelAssetLights.h"

namespace render::geometry
{
class FarLightsRenderer
{
public:
    ~FarLightsRenderer();
    static bool enabled();
    // queue a drawn object's lights (key: a stable id of the light set, e.g. its ModelAssetGpu); modelView = asset ->
    // camera-relative view space
    void queue(const void* key, const std::vector<elite::model_asset::AssetLight>& lights, const glm::mat4& modelView);
    void flush(const glm::mat4& proj, float focalPx, float nearPlane, float timeSeconds);
    std::size_t lastPoints() const { return m_lastPoints; }

private:
    struct Set
    {
        GLuint vao = 0, vbo = 0;
        GLsizei count = 0;
    };
    struct Item
    {
        const Set* set = nullptr;
        glm::mat4 modelView {1.0f};
    };
    const Set& setFor(const void* key, const std::vector<elite::model_asset::AssetLight>& lights);
    GLuint m_program = 0;
    std::unordered_map<const void*, Set> m_sets;
    std::vector<Item> m_items;
    std::size_t m_lastPoints = 0;
};
} // namespace render::geometry
