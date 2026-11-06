#pragma once

// Flight rules around celestial bodies - ONE source for the server (J flight, route legs, landing
// checks), the client route planner, the map and the HUD (architecture audit 2026-11-06, items 2.1,
// 2.2, 2.4). Pure functions, no game / render dependencies.
//
//   dominantBody    - the body whose Laplace sphere of influence holds a point: r = d(parent) *
//                     (mu / mu_parent)^0.4; the parent is the catalog parent (parentId) when it is
//                     a body, else the nearest enclosing id ("...Земля.Луна" -> "...Земля"), else the
//                     root (the largest mu, the star); a parent lighter than the body is the root.
//   floorAltitudeM  - the J floor (see JDrive.h): max(top of the atmosphere + 50 km, 5 % R, 50 km)
//   clearanceM      - how close a route may pass: the star 20 radii (heat), a planet / moon
//                     max(R + 150 km, atmosphere + 50 km)
//   isAirless       - no atmosphere (only then a ship that is not a shuttle may land)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

#include <glm/glm.hpp>

namespace game::navigation::rules
{

inline double floorAltitudeM(double radiusM, double atmosphereRadiusM)
{
    const double atmosphereTop = atmosphereRadiusM > radiusM ? atmosphereRadiusM - radiusM : 0.0;
    return std::max({atmosphereTop + 50000.0, radiusM * 0.05, 50000.0});
}

inline bool isAirless(double radiusM, double atmosphereRadiusM)
{
    return atmosphereRadiusM <= radiusM * 1.0001;
}

inline double clearanceM(double radiusM, double atmosphereRadiusM, bool isStar)
{
    if (isStar)
        return 20.0 * radiusM;
    return std::max(radiusM + 150000.0, std::max(radiusM, atmosphereRadiusM) + 50000.0);
}

// The dominant body over any body list. The accessors give a body's id, catalog parent id (may be
// empty), mu and centre. Returns the index (npos when the list is empty); isRoot = the star holds it.
template <class Bodies, class GetId, class GetParent, class GetMu, class GetCenter>
std::size_t dominantBody(const Bodies& bodies, const glm::dvec3& point, GetId id, GetParent parentOf, GetMu mu,
                         GetCenter center, bool* isRoot = nullptr)
{
    constexpr std::size_t npos = static_cast<std::size_t>(-1);
    const std::size_t n = bodies.size();
    std::size_t root = npos;
    for (std::size_t i = 0; i < n; ++i)
        if (mu(bodies[i]) > 0.0 && (root == npos || mu(bodies[i]) > mu(bodies[root])))
            root = i;
    const auto find = [&](const std::string& key) {
        for (std::size_t i = 0; i < n; ++i)
            if (id(bodies[i]) == key)
                return i;
        return npos;
    };
    std::size_t best = root;
    double bestSphere = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i)
    {
        if (i == root || mu(bodies[i]) <= 0.0)
            continue;
        std::size_t parent = npos;
        const std::string declared = parentOf(bodies[i]);
        if (!declared.empty())
            parent = find(declared);
        for (std::string key = id(bodies[i]); parent == npos;)
        {
            const auto dot = key.rfind('.');
            if (dot == std::string::npos)
                break;
            key.resize(dot);
            parent = find(key);
        }
        if (parent == npos || mu(bodies[parent]) <= mu(bodies[i]))
            parent = root;
        if (parent == npos)
            continue;
        const double sphere = glm::length(center(bodies[parent]) - center(bodies[i])) * std::pow(mu(bodies[i]) / mu(bodies[parent]), 0.4);
        if (sphere < bestSphere && glm::length(point - center(bodies[i])) < sphere)
        {
            bestSphere = sphere;
            best = i;
        }
    }
    if (isRoot)
        *isRoot = best == root;
    return best;
}

} // namespace game::navigation::rules
