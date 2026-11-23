#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace world::navigation
{
// THE OBSTACLE GRID (2026-11-23, the route-draft net's dataset ran at 0.3 scenes/s): a candidate is ~200 sampled
// segments and each was tested against EVERY obstacle (a city leg: ~3000 -> 600 000 tests, ~12 ms an evaluation).
// Built once per optimize() call: every obstacle's inflated bounding box goes into the cells it covers; a segment
// tests only the obstacles of its cells (exactly the same per-obstacle test - the answer does not change).
class NavigationObstacleGrid
{
public:
    void build(const std::vector<NavigationObstacle>& obstacles, double hullRadius)
    {
        m_obstacles = &obstacles;
        m_hull = hullRadius;
        m_cells.clear();
        m_big.clear();
        m_stamp.assign(obstacles.size(), 0);
        m_epoch = 0;
        for (std::size_t i = 0; i < obstacles.size(); ++i)
        {
            const auto& o = obstacles[i];
            const double r = o.conservativeRadiusMeters() +
                             navigationObstacleInflationMeters(o, hullRadius) + 1.0;
            const glm::dvec3 lo = o.centerMeters - glm::dvec3(r), hi = o.centerMeters + glm::dvec3(r);
            const glm::ivec3 a = cellOf(lo), b = cellOf(hi);
            const glm::ivec3 span = b - a + glm::ivec3(1);
            if (static_cast<long long>(span.x) * span.y * span.z > 4096)
            {
                m_big.push_back(static_cast<int>(i));   // a huge one (a floor slab): always tested
                continue;
            }
            for (int x = a.x; x <= b.x; ++x)
                for (int y = a.y; y <= b.y; ++y)
                    for (int z = a.z; z <= b.z; ++z)
                        m_cells[key(x, y, z)].push_back(static_cast<int>(i));
        }
    }

    bool segmentClear(const glm::dvec3& p, const glm::dvec3& q) const
    {
        const auto& obs = *m_obstacles;
        if (++m_epoch == 0)
        {
            std::fill(m_stamp.begin(), m_stamp.end(), 0);
            m_epoch = 1;
        }
        const auto test = [&](int i) {
            if (m_stamp[static_cast<std::size_t>(i)] == m_epoch)
                return true;
            m_stamp[static_cast<std::size_t>(i)] = m_epoch;
            return !segmentIntersectsNavigationObstacle(p, q, obs[static_cast<std::size_t>(i)], m_hull);
        };
        for (const int i : m_big)
            if (!test(i))
                return false;
        const glm::ivec3 a = cellOf(glm::min(p, q)), b = cellOf(glm::max(p, q));
        for (int x = a.x; x <= b.x; ++x)
            for (int y = a.y; y <= b.y; ++y)
                for (int z = a.z; z <= b.z; ++z)
                {
                    const auto it = m_cells.find(key(x, y, z));
                    if (it == m_cells.end())
                        continue;
                    for (const int i : it->second)
                        if (!test(i))
                            return false;
                }
        return true;
    }

    const std::vector<NavigationObstacle>* source() const { return m_obstacles; }

private:
    static constexpr double Cell = 150.0;
    static glm::ivec3 cellOf(const glm::dvec3& p)
    {
        return glm::ivec3(static_cast<int>(std::floor(p.x / Cell)), static_cast<int>(std::floor(p.y / Cell)),
                          static_cast<int>(std::floor(p.z / Cell)));
    }
    static std::uint64_t key(int x, int y, int z)
    {
        const auto u = [](int v) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(v + (1 << 20)) & 0x1FFFFFu); };
        return (u(x) << 42) | (u(y) << 21) | u(z);
    }
    const std::vector<NavigationObstacle>* m_obstacles = nullptr;
    double m_hull = 0.0;
    std::unordered_map<std::uint64_t, std::vector<int>> m_cells;
    std::vector<int> m_big;
    mutable std::vector<std::uint32_t> m_stamp;
    mutable std::uint32_t m_epoch = 0;
};

} // namespace world::navigation
