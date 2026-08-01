#pragma once

#include <glm/glm.hpp>

namespace game::system_map
{
struct LocalMapAtmosphereStyle
{
    bool enabled = false;

    float visualIntensity = 1.0f;
    float radiusScale = 1.018f;

    glm::vec4 oceanInner {
        0.006f,
        0.035f,
        0.090f,
        0.96f
    };

    glm::vec4 oceanOuter {
        0.025f,
        0.095f,
        0.170f,
        0.96f
    };

    glm::vec4 surfaceHaze {
        0.68f,
        0.84f,
        1.00f,
        0.22f
    };

    glm::vec4 limbCore {
        0.88f,
        0.97f,
        1.00f,
        0.16f
    };

    glm::vec4 nearAtmosphere {
        0.30f,
        0.64f,
        1.00f,
        0.16f
    };

    glm::vec4 outerAtmosphere {
        0.12f,
        0.34f,
        0.78f,
        0.075f
    };
};
}
