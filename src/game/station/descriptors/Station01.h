#pragma once

#include <string>
#include "src/world/descriptors/StationDescriptor.h"

namespace Station1
{
    inline void apply(StationDescriptor& d)
    {
        d.m_meshId = "stantion-01";
        
        d.m_rotationSpeed = 0.05f;
        d.m_boundingRadius = 500.0f;
        
        d.meshSizeMeters = glm::vec3(
                500.f,
                500.0f,
               4000.0f
            );
    }
}



