#include "Frustum.h"
#include <iostream>


void Frustum::update(const glm::mat4& m)
{
    glm::mat4 t = glm::transpose(m);

    planes[0] = t[3] - t[0]; // left  - это ПРАВИЛЬНО для способа Б
    planes[1] = t[3] + t[0]; // right - это ПРАВИЛЬНО для способа Б
    planes[2] = t[3] - t[1]; // bottom - это ПРАВИЛЬНО для способа Б
    planes[3] = t[3] + t[1]; // top - это ПРАВИЛЬНО для способа Б
    planes[4] = t[3] - t[2]; // near - это ПРАВИЛЬНО для способа Б
    planes[5] = t[3] + t[2]; // far - это ПРАВИЛЬНО для способа Б

    for(auto& p : planes)
    {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }
}




bool Frustum::boxVisible(
    const glm::vec3& min,
    const glm::vec3& max,
    const glm::mat4& model) const
{
    glm::vec3 corners[8] =
    {
        {min.x,min.y,min.z},
        {max.x,min.y,min.z},
        {min.x,max.y,min.z},
        {max.x,max.y,min.z},
        {min.x,min.y,max.z},
        {max.x,min.y,max.z},
        {min.x,max.y,max.z},
        {max.x,max.y,max.z}
    };

    for(auto& c : corners)
        c = glm::vec3(model * glm::vec4(c,1));

    for(const auto& p : planes)
    {
        int out = 0;

        for(const auto& c : corners)
        {
            if(glm::dot(glm::vec3(p),c) + p.w < 0)
                out++;
        }

        if(out == 8)
            return false;
    }

    return true;
}





bool Frustum::sphereVisible(
    const glm::vec3& center,
    float radius,
    const glm::mat4& model) const
{
    glm::vec3 worldCenter = glm::vec3(model * glm::vec4(center,1));

    for(int i=0;i<6;i++)
    {
        const auto& p = planes[i];

        float dist = glm::dot(glm::vec3(p), worldCenter) + p.w;

        // std::cout
        //     << "plane " << i
        //     << " dist=" << dist
        //     << " radius=" << radius
        //     << std::endl;

        if(dist < -radius)
        {
            // std::cout << "CULLED by plane " << i << std::endl;
            return false;
        }
    }

    return true;
}




