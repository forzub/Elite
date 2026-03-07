#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game::math
{

struct MathTransform
{
    glm::vec3 position {0.0f,0.0f,0.0f};

    glm::mat4 orientation {1.0f};


    glm::vec3 worldToLocal(const glm::vec3& world) const
    {
        glm::vec3 relative = world - position;

        glm::mat4 invOrientation = glm::inverse(orientation);

        return glm::vec3(invOrientation * glm::vec4(relative,1.0f));
    }



    glm::vec3 localToWorld(const glm::vec3& local) const
    {
        return glm::vec3(orientation * glm::vec4(local,1.0f)) + position;
    }



    
    glm::vec3 directionToLocal(const glm::vec3& dir) const
    {
        glm::mat4 invOrientation = glm::inverse(orientation);

        return glm::vec3(invOrientation * glm::vec4(dir,0.0f));
    }
};

}