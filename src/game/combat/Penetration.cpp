#include "src/game/combat/Penetration.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::combat
{
namespace
{
// a support-link volume the hit builder has not placed (centre at the origin):
// no geometry to hit
bool unplaced(const game::damage::HitVolume& v)
{
    return v.supportLinkVolume && glm::length(v.center) < 1.0e-4f;
}

bool rayObb(const game::damage::HitVolume& v, const glm::vec3& o, const glm::vec3& d, float& t0, float& t1)
{
    const glm::mat3 rt = glm::transpose(v.orientation);
    const glm::vec3 lo = rt * (o - v.center);
    const glm::vec3 ld = rt * d;
    t0 = -std::numeric_limits<float>::max();
    t1 = std::numeric_limits<float>::max();
    for (int a = 0; a < 3; ++a)
    {
        const float h = std::max(v.halfSize[a], 0.02f);
        if (std::abs(ld[a]) < 1.0e-8f)
        {
            if (std::abs(lo[a]) > h)
                return false;
            continue;
        }
        float ta = (-h - lo[a]) / ld[a];
        float tb = (h - lo[a]) / ld[a];
        if (ta > tb)
            std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1)
            return false;
    }
    return true;
}

void addLoss(BurnState& burn, const std::string& key, double health)
{
    burn.pendingHealthLoss[key] += health;
}
} // namespace

std::vector<RayHit> castRay(const game::damage::HitComponent& hit, const glm::vec3& o, const glm::vec3& d, float maxT)
{
    std::vector<RayHit> out;
    for (std::size_t i = 0; i < hit.volumes.size(); ++i)
    {
        const auto& v = hit.volumes[i];
        if (v.destroyed || unplaced(v))
            continue;
        float t0 = 0.0f, t1 = 0.0f;
        if (!rayObb(v, o, d, t0, t1) || t1 < 0.0f || t0 > maxT)
            continue;
        out.push_back(RayHit {static_cast<int>(i), std::max(t0, 0.0f), t1});
    }
    // by entry; at the same entry the outer layer first, then the higher priority
    std::sort(out.begin(), out.end(), [&](const RayHit& a, const RayHit& b) {
        if (std::abs(a.tEnter - b.tEnter) > 0.02f)
            return a.tEnter < b.tEnter;
        const auto& va = hit.volumes[a.volume];
        const auto& vb = hit.volumes[b.volume];
        if (va.layerIndex != vb.layerIndex)
            return va.layerIndex < vb.layerIndex;
        return va.priority > vb.priority;
    });
    return out;
}

std::string volumeKey(const game::damage::HitVolume& v)
{
    return v.supportLinkVolume ? "link::" + v.supportLinkId : "module::" + v.moduleId;
}

double burnThroughMJ(const game::damage::HitVolume& v, const MaterialConstants& m)
{
    return v.penetrationResistance > 0.0f ? v.penetrationResistance * m.burnMJPerPen : 0.5;
}

bool BurnState::holed(const std::string& key, const glm::vec3& p) const
{
    for (const auto& h : holes)
        if (h.key == key && glm::length(h.p - p) < h.radiusM)
            return true;
    return false;
}

void BurnState::cool(double dt, const MaterialConstants& m)
{
    for (auto it = spots.begin(); it != spots.end();)
    {
        it->second.heatMJ -= m.coolingMW * dt;
        if (it->second.heatMJ <= 0.0)
            it = spots.erase(it);
        else
            ++it;
    }
}

BeamHit stepBeam(const game::damage::HitComponent& hit, BurnState& burn, const glm::vec3& o, const glm::vec3& d, float maxT,
                 double powerMW, double dt, double beamRadiusM, const MaterialConstants& m)
{
    BeamHit out;
    double energy = powerMW * dt;
    const auto hits = castRay(hit, o, d, maxT);
    for (const RayHit& rh : hits)
    {
        if (energy <= 0.0)
            break;
        const auto& v = hit.volumes[rh.volume];
        const glm::vec3 p = o + d * rh.tEnter;
        const std::string key = volumeKey(v);
        // through an existing hole: the beam goes on
        if (burn.holed(key, p))
            continue;
        const double reflect = v.armor / (v.armor + m.reflectArmourScale);
        const double absorbed = energy * (1.0 - reflect);
        BurnState::Spot& spot = burn.spots[key];
        if (glm::length(spot.p - p) > m.spotMergeM)
        {
            // the beam has moved: a new spot (the old one's heat stays behind)
            spot.p = p;
            spot.heatMJ = 0.0;
        }
        spot.heatMJ += absorbed;
        addLoss(burn, key, absorbed * m.healthPerMJ);
        out.hit = true;
        out.pointLocal = p;
        out.distanceM = rh.tEnter;
        out.key = key;
        out.moduleId = v.moduleId;
        out.absorbedMJ += absorbed;
        out.spotHeatMJ = spot.heatMJ;
        out.burnMJ = burnThroughMJ(v, m);
        if (spot.heatMJ < out.burnMJ)
            break;
        // burned through: a hole the size of the beam's heated zone; the heat
        // beyond the cost goes on with the beam
        burn.holes.push_back(BurnState::Hole {key, p, static_cast<float>(std::max(beamRadiusM * 4.0, 0.25))});
        ++out.burnedThrough;
        const double surplus = spot.heatMJ - out.burnMJ;
        burn.spots.erase(key);
        energy = surplus / std::max(1.0 - reflect, 0.05);
    }
    return out;
}

PenetrationResult penetrate(const game::damage::HitComponent& hit, BurnState& burn, const glm::vec3& o, const glm::vec3& d,
                            float maxT, double energyMJ, WarheadKind kind, double slugKg, const MaterialConstants& m)
{
    PenetrationResult out;
    double energy = energyMJ;
    const bool slug = kind == WarheadKind::Slug;
    const float holeR = slug ? 0.12f : 0.18f;
    const auto hits = castRay(hit, o, d, maxT);
    for (const RayHit& rh : hits)
    {
        if (energy <= 0.0)
            break;
        const auto& v = hit.volumes[rh.volume];
        const glm::vec3 p = o + d * rh.tEnter;
        const std::string key = volumeKey(v);
        if (burn.holed(key, p))
            continue;
        out.hitHull = true;
        PenetrationResult::Layer layer;
        layer.key = key;
        layer.moduleId = v.moduleId;
        layer.entry = p;
        const double cost = burnThroughMJ(v, m) * (slug ? m.slugPenFactor : 1.0);
        if (energy >= cost)
        {
            // through: the cost plus the heat / spall of the passage (30 %)
            layer.spentMJ = cost;
            layer.holed = true;
            addLoss(burn, key, cost * 1.3 * m.healthPerMJ);
            burn.holes.push_back(BurnState::Hole {key, p, holeR});
            energy -= cost;
            out.layers.push_back(layer);
            out.stopLocal = o + d * rh.tExit;
            continue;
        }
        // spent inside this volume
        layer.spentMJ = energy;
        addLoss(burn, key, energy * m.healthPerMJ);
        const float depth = static_cast<float>(cost > 0.0 ? energy / cost : 0.0) * (rh.tExit - rh.tEnter);
        out.stopLocal = p + d * depth;
        energy = 0.0;
        out.layers.push_back(layer);
        if (slug)
        {
            out.stuck = true;
            burn.stuck.push_back(BurnState::Stuck {key, out.stopLocal, slugKg});
        }
        break;
    }
    out.residualMJ = energy;
    out.exited = out.hitHull && energy > 0.0;
    return out;
}
} // namespace game::combat
