#pragma once

#include "HitVolume.h"
#include "DamageResult.h"
#include <vector>
#include <limits>

namespace game::damage
{

class HitComponent
{
public:

    std::vector<HitVolume> volumes;



    const HitVolume* findHit(const glm::vec3& point) const
    {
        for (const auto& v : volumes)
        {
            if (v.contains(point))
            {
                return &v;
            }
        }

        return nullptr;
    }



    DamageResult resolve(const DamageEvent& event) const
{
    const HitVolume* best = nullptr;

    int bestPriority = std::numeric_limits<int>::min();

    for (const auto& v : volumes)
    {
        if (v.destroyed)
            continue;

        if (!v.contains(event.position))
            continue;

        if (v.priority > bestPriority)
        {
            bestPriority = v.priority;
            best = &v;
        }
    }

    return { &event, best };
}




    void load(const std::vector<HitVolume>& v)
    {
        volumes = v;
    }



};


}

