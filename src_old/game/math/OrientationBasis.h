#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <cmath>

namespace game::math
{

inline glm::mat4 makeOrientationFromForwardUp(
    const glm::vec3& forwardWorld,
    const glm::vec3& upHintWorld
)
{
    glm::vec3 f = glm::normalize(forwardWorld);
    glm::vec3 upHint = glm::normalize(upHintWorld);

    if (glm::length2(f) < 1e-8f)
        return glm::mat4(1.0f);

    if (glm::length2(upHint) < 1e-8f)
        upHint = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 right = glm::cross(f, upHint);

    if (glm::length2(right) < 1e-8f)
    {
        const glm::vec3 fallbackUp =
            (std::abs(f.y) < 0.99f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);

        right = glm::cross(f, fallbackUp);
    }

    right = glm::normalize(right);
    glm::vec3 up = glm::normalize(glm::cross(right, f));
    glm::vec3 back = -f; // local +Z = back, local -Z = forward

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(right, 0.0f);
    m[1] = glm::vec4(up,    0.0f);
    m[2] = glm::vec4(back,  0.0f);
    m[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return m;
}

inline glm::vec3 forwardFromOrientation(const glm::mat4& orientation)
{
    return glm::normalize(glm::vec3(orientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
}

inline glm::vec3 upFromOrientation(const glm::mat4& orientation)
{
    return glm::normalize(glm::vec3(orientation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
}

inline glm::vec3 rightFromOrientation(const glm::mat4& orientation)
{
    return glm::normalize(glm::vec3(orientation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
}

} // namespace game::math