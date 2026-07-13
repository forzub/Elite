#pragma once

#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/world/celestial/SystemMapTypes.h"

namespace render::celestial::rings
{

enum class PlanetRingRenderPart
{
    Back = 0,
    Front = 1
};

struct PlanetRingRenderContext
{
    glm::dvec2 planetCenterPx {0.0};

    glm::dvec2 ringAxisXPx {0.0};
    glm::dvec2 ringAxisYPx {0.0};

    glm::dvec2 ringDepthCoefficients {0.0};

    double planetRotationPhaseRad = 0.0;

    world::celestial::SystemMapRingVisualProfile visual;

    const std::vector<
        world::celestial::SystemMapRing
    >* bands = nullptr;
};

class PlanetRingRenderer
{
public:
    void render(
        const PlanetRingRenderContext& context,
        PlanetRingRenderPart part
    );

private:
    void ensureResources();

private:
    static constexpr int MaxRingBands =
        16;

    GLuint m_shader = 0;
    GLuint m_fullscreenVao = 0;

    GLint m_viewportOriginLocation = -1;
    GLint m_viewportSizeLocation = -1;

    GLint m_planetCenterLocation = -1;
    GLint m_ringAxisXLocation = -1;
    GLint m_ringAxisYLocation = -1;
    GLint m_ringDepthCoefficientsLocation = -1;

    GLint m_renderPartLocation = -1;
    GLint m_rotationPhaseLocation = -1;

    GLint m_visualModeLocation = -1;
    GLint m_visualBandEmphasisLocation = -1;
    GLint m_visualStructureLocation = -1;
    GLint m_visualParticleLocation = -1;
    GLint m_visualParticleShapeLocation = -1;
    GLint m_visualOcclusionLocation = -1;
    GLint m_visualLodLocation = -1;

    GLint m_bandCountLocation = -1;
    GLint m_bandGeometryLocation = -1;
    GLint m_bandColorOpticalLocation = -1;
    GLint m_bandAppearanceLocation = -1;
    GLint m_bandExtraLocation = -1;
    GLint m_bandParticleLocation = -1;

    bool m_missingShaderWarningPrinted = false;
};

} // namespace render::celestial::rings