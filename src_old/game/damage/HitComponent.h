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

        int bestLayer = std::numeric_limits<int>::max();
        int bestPriority = std::numeric_limits<int>::min();

        for (const auto& v : volumes)
        {
            if (v.destroyed)
                continue;

            if (!v.contains(event.position))
                continue;

            // Сначала внешний слой
            if (v.layerIndex < bestLayer)
            {
                bestLayer = v.layerIndex;
                bestPriority = v.priority;
                best = &v;
                continue;
            }

            if (v.layerIndex == bestLayer && v.priority > bestPriority)
            {
                bestPriority = v.priority;
                best = &v;
            }
        }

        return { &event, best };
    }

    int findVolume(const glm::vec3& point) const
    {
        int best = -1;
        int bestLayer = std::numeric_limits<int>::max();
        int bestPriority = std::numeric_limits<int>::min();

        for (size_t i = 0; i < volumes.size(); ++i)
        {
            const auto& v = volumes[i];

            if (v.destroyed)
                continue;

            if (!v.contains(point))
                continue;

            if (v.layerIndex < bestLayer)
            {
                bestLayer = v.layerIndex;
                bestPriority = v.priority;
                best = (int)i;
                continue;
            }

            if (v.layerIndex == bestLayer && v.priority > bestPriority)
            {
                bestPriority = v.priority;
                best = (int)i;
            }
        }

        return best;
    }

    void load(const std::vector<HitVolume>& v)
    {
        volumes = v;
    }
};

} // namespace game::damage