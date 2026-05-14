#pragma once
#include <glm/glm.hpp>



class Frustum
{
public:

    void update(const glm::mat4& vp);

    bool boxVisible(
        const glm::vec3& min,
        const glm::vec3& max,
        const glm::mat4& model) const;

    bool sphereVisible(
    const glm::vec3& center,
    float radius,
    const glm::mat4& model) const;



private:

    glm::vec4 planes[6];
};


