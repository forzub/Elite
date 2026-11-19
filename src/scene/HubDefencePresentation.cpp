#include "src/scene/HubDefencePresentation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/traffic/HubSecurity.h"

namespace scene
{
namespace sec = game::traffic::security;

const char* hubDefenceModelId(HubDefenceModel model)
{
    switch (model)
    {
    case HubDefenceModel::PdlBase: return "hubd_pdl_base";
    case HubDefenceModel::PdlHead: return "hubd_pdl_head";
    case HubDefenceModel::CoilBase: return "hubd_coil_base";
    case HubDefenceModel::CoilHead: return "hubd_coil_head";
    case HubDefenceModel::Vls: return "hubd_vls";
    case HubDefenceModel::RadarMast: return "hubd_radar_mast";
    case HubDefenceModel::RadarDish: return "hubd_radar_dish";
    case HubDefenceModel::Picket: return "hubd_picket";
    case HubDefenceModel::Rock: return "hubd_rock";
    case HubDefenceModel::Beam: return "hubd_fx_beam";
    case HubDefenceModel::Flash: return "hubd_fx_flash";
    default: return "hubd_pdl_base";
    }
}

namespace
{
constexpr double Pi = 3.14159265358979323846;
// the heads' pivots over their decks (tools/models/units/hub_defence.py)
constexpr double PivotPdl = 6.5, PivotCoil = 5.0, PivotPicket = 8.0, PivotRadar = 61.0;
constexpr double MuzzlePdlZ = 6.6, MuzzlePdlY = 2.4, MuzzleCoilZ = 24.0, MuzzleCoilY = 2.6;
constexpr double TurretDrawMeters = 6000.0;   // a 14 m turret is ~2 px beyond

double hash01(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0x1000000u);
}

glm::dmat3 siteBasis(const sec::DefenceSite& s)
{
    // +Y up, or upside down under the plane (a half turn about X)
    return s.up.y >= 0.0 ? glm::dmat3(1.0) : glm::dmat3(glm::dvec3(1, 0, 0), glm::dvec3(0, -1, 0), glm::dvec3(0, 0, -1));
}

glm::dmat3 rotY(double a)
{
    const double c = std::cos(a), s = std::sin(a);
    return glm::dmat3(glm::dvec3(c, 0, -s), glm::dvec3(0, 1, 0), glm::dvec3(s, 0, c));
}

glm::dmat3 rotX(double a)
{
    const double c = std::cos(a), s = std::sin(a);
    return glm::dmat3(glm::dvec3(1, 0, 0), glm::dvec3(0, c, s), glm::dvec3(0, -s, c));
}

glm::mat4 compose(const glm::dvec3& t, const glm::dmat3& R, const glm::dvec3& scale = glm::dvec3(1.0))
{
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(glm::vec3(R[0] * scale.x), 0.0f);
    m[1] = glm::vec4(glm::vec3(R[1] * scale.y), 0.0f);
    m[2] = glm::vec4(glm::vec3(R[2] * scale.z), 0.0f);
    m[3] = glm::vec4(glm::vec3(t), 1.0f);
    return m;
}

glm::dmat3 lookAlong(const glm::dvec3& d)
{
    const glm::dvec3 z = glm::normalize(d);
    glm::dvec3 x = glm::cross(glm::dvec3(0, 1, 0), z);
    if (glm::length(x) < 1.0e-6)
        x = glm::dvec3(1, 0, 0);
    x = glm::normalize(x);
    return glm::dmat3(x, glm::cross(z, x), z);
}

struct Slew
{
    double az = 0.0, el = 0.0, t = -1.0;
};

const sec::DefenceLayout& layoutFor(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork* lanes)
{
    static std::unordered_map<const game::navigation::HubPlan*, sec::DefenceLayout> cache;
    auto it = cache.find(&plan);
    if (it == cache.end())
        it = cache.emplace(&plan, sec::buildDefenceLayout(plan, lanes)).first;
    return it->second;
}
} // namespace

void hubDefencePresentation(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork* lanes, double t,
                            const glm::vec3& cameraLocal, float radius, const std::vector<glm::dvec3>& lockedTargets,
                            std::vector<HubDefenceInstance>& out, HubDefenceStats* stats)
{
    out.clear();
    const sec::DefenceLayout& L = layoutFor(plan, lanes);
    if (L.sites.empty())
        return;
    const glm::dvec3 camera(cameraLocal);
    const auto push = [&](HubDefenceModel m, const glm::mat4& local, float r) {
        const float d = static_cast<float>(glm::length(glm::dvec3(local[3]) - camera));
        if (d > radius + r)
            return;
        out.push_back({m, local, d, r});
    };
    // the bodies of the zone now (and those killed in the last 3 s: their fragments)
    static std::vector<sec::StrayBody> bodies;
    sec::strayBodiesAt(L, t, 3.0, bodies);
    static std::unordered_map<std::uint64_t, Slew> slews;
    const std::uint64_t planKey = reinterpret_cast<std::uintptr_t>(&plan) * 1315423911ull;
    int shots = 0, lockedCount = 0;

    for (std::size_t i = 0; i < L.sites.size(); ++i)
    {
        const auto& s = L.sites[i];
        const double dist = glm::length(s.position - camera);
        const bool picket = s.kind == sec::DefenceKind::Picket;
        const bool radar = s.kind == sec::DefenceKind::RadarMast;
        if (dist > radius + 100.0 || (!picket && !radar && dist > TurretDrawMeters))
            continue;
        const glm::dmat3 B = siteBasis(s);
        HubDefenceModel baseModel = HubDefenceModel::PdlBase;
        double pivot = PivotPdl;
        switch (s.kind)
        {
        case sec::DefenceKind::Coilgun: baseModel = HubDefenceModel::CoilBase; pivot = PivotCoil; break;
        case sec::DefenceKind::MissileCells: baseModel = HubDefenceModel::Vls; pivot = 0.0; break;
        case sec::DefenceKind::RadarMast: baseModel = HubDefenceModel::RadarMast; pivot = PivotRadar; break;
        case sec::DefenceKind::Picket: baseModel = HubDefenceModel::Picket; pivot = PivotPicket; break;
        default: break;
        }
        push(baseModel, compose(s.position, B), picket ? 30.0f : radar ? 40.0f : 15.0f);
        if (s.kind == sec::DefenceKind::MissileCells)
            continue;
        const glm::dvec3 pivotPos = s.position + B * glm::dvec3(0.0, pivot, 0.0);
        if (radar)
        {
            // 10 rpm search
            push(HubDefenceModel::RadarDish, compose(pivotPos, B * rotY(std::fmod(t, 6.0) / 6.0 * 2.0 * Pi)), 10.0f);
            continue;
        }
        // ---- what this turret aims at: its engagement, a locked ship, else the idle scan
        const bool coil = s.kind == sec::DefenceKind::Coilgun;
        const sec::StrayBody* engaged = nullptr;
        for (const auto& b : bodies)
            if (b.inbound && b.shooter == static_cast<int>(i) && t >= b.fireSeconds - 8.0 && t <= b.killSeconds)
                engaged = &b;
        glm::dvec3 aimPoint(0.0);
        bool aim = false;
        if (engaged)
        {
            aimPoint = sec::strayPosition(*engaged, t);
            aim = true;
        }
        else
        {
            double best = 8000.0;
            for (const auto& target : lockedTargets)
                if (glm::length(target - pivotPos) < best)
                {
                    best = glm::length(target - pivotPos);
                    aimPoint = target;
                    aim = true;
                }
            lockedCount += aim ? 1 : 0;
        }
        double wantAz, wantEl;
        if (aim)
        {
            const glm::dvec3 d = glm::transpose(B) * glm::normalize(aimPoint - pivotPos);
            wantAz = std::atan2(d.x, d.z);
            wantEl = std::asin(std::clamp(d.y, -1.0, 1.0));
            wantEl = std::max(wantEl, -0.15);   // the deck limits the depression
        }
        else
        {
            wantAz = s.idlePhase + 0.6 * std::sin(t * 0.05 + s.idlePhase);
            wantEl = 0.25 + 0.15 * std::sin(t * 0.07 + 2.0 * s.idlePhase);
        }
        // the drives slew at a finite rate (40 deg/s in azimuth, 25 deg/s in elevation)
        Slew& sl = slews[planKey ^ (static_cast<std::uint64_t>(s.id) * 2654435761ull)];
        if (sl.t < 0.0)
        {
            sl.az = wantAz;
            sl.el = wantEl;
        }
        const double dt = std::clamp(t - sl.t, 0.0, 0.25);
        sl.t = t;
        double daz = std::remainder(wantAz - sl.az, 2.0 * Pi);
        const double maxAz = glm::radians(40.0) * dt, maxEl = glm::radians(25.0) * dt;
        sl.az += std::clamp(daz, -maxAz, maxAz);
        sl.el += std::clamp(wantEl - sl.el, -maxEl, maxEl);
        const glm::dmat3 H = B * rotY(sl.az) * rotX(-sl.el);
        push(coil ? HubDefenceModel::CoilHead : HubDefenceModel::PdlHead, compose(pivotPos, H), coil ? 25.0f : 8.0f);
        // ---- the shot
        if (!engaged || t < engaged->fireSeconds)
            continue;
        const glm::dvec3 muzzle = pivotPos + H * (coil ? glm::dvec3(0.0, MuzzleCoilY, MuzzleCoilZ) : glm::dvec3(0.0, MuzzlePdlY, MuzzlePdlZ));
        const glm::dvec3 target = sec::strayPosition(*engaged, t);
        const double camD = glm::length(muzzle - camera);
        ++shots;
        if (!engaged->laser || coil)
        {
            // the slug at 3 km/s from the muzzle to the body; the muzzle flash for 0.3 s
            const double flight = std::max(1.0e-3, engaged->killSeconds - engaged->fireSeconds);
            const double f = std::clamp((t - engaged->fireSeconds) / flight, 0.0, 1.0);
            const glm::dvec3 slug = muzzle + (engaged->killPoint - muzzle) * f;
            const double w = std::max(1.5, glm::length(slug - camera) * 0.003);
            push(HubDefenceModel::Beam, compose(slug - glm::normalize(engaged->killPoint - muzzle) * (w * 20.0),
                                                lookAlong(engaged->killPoint - muzzle), glm::dvec3(w, w, w * 20.0)),
                 static_cast<float>(w * 20.0));
            if (t - engaged->fireSeconds < 0.3)
                push(HubDefenceModel::Flash, compose(muzzle, glm::dmat3(1.0), glm::dvec3(std::max(4.0, camD * 0.004))), 6.0f);
            continue;
        }
        // the laser: a beam from the director to the body (drawn as its glow: at least ~2 px wide), the hot spot on it
        const double len = glm::length(target - muzzle);
        const double w = std::max(0.8, camD * 0.0015) * (0.85 + 0.15 * std::sin(t * 60.0));
        push(HubDefenceModel::Beam, compose(muzzle, lookAlong(target - muzzle), glm::dvec3(w, w, len)), static_cast<float>(len));
        const double spot = std::max(1.5 + engaged->sizeMeters, glm::length(target - camera) * 0.004) * (0.8 + 0.2 * std::sin(t * 45.0));
        push(HubDefenceModel::Flash, compose(target, rotY(t * 7.0), glm::dvec3(spot)), static_cast<float>(spot));
    }

    // ---- the bodies, the kills, the fragments
    int bodyCount = 0;
    for (const auto& b : bodies)
    {
        const bool killed = b.inbound && t > b.killSeconds;
        if (!killed)
        {
            const glm::dvec3 p = sec::strayPosition(b, t);
            const double d = glm::length(p - camera);
            const double scale = std::max(b.sizeMeters, d * 0.0025);   // never below ~1-2 px: the eye follows the tracked body
            push(HubDefenceModel::Rock, compose(p, rotY(t * (0.3 + b.sizeMeters * 0.1) + b.index) * rotX(t * 0.21 + b.index * 0.7), glm::dvec3(scale)),
                 static_cast<float>(scale));
            ++bodyCount;
            continue;
        }
        const double since = t - b.killSeconds;
        if (since > 3.0)
            continue;
        const double d = glm::length(b.killPoint - camera);
        if (since < 1.0)
            push(HubDefenceModel::Flash, compose(b.killPoint, rotY(b.index), glm::dvec3(std::max(3.0 + b.sizeMeters * 5.0, d * 0.006) * (1.0 - since))),
                 20.0f);
        // the fragments: 8 pieces flying apart at ~25 m/s with the body's drift
        for (int k = 0; k < 8; ++k)
        {
            const double az = hash01(b.index, static_cast<std::uint32_t>(k)) * 2.0 * Pi;
            const double el = (hash01(b.index + 99u, static_cast<std::uint32_t>(k)) - 0.5) * Pi;
            const glm::dvec3 dir(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
            const glm::dvec3 p = b.killPoint + b.velocity * since + dir * (25.0 * since);
            const double sz = std::max(b.sizeMeters * 0.3 * (1.0 - since / 3.0), d * 0.0012 * (1.0 - since / 3.0));
            push(HubDefenceModel::Rock, compose(p, rotY(since * 3.0 + k) * rotX(since * 2.0 + k), glm::dvec3(std::max(0.05, sz))), 2.0f);
        }
    }
    if (stats)
    {
        stats->sites = static_cast<int>(L.sites.size());
        stats->bodies = bodyCount;
        stats->shots = shots;
        stats->locked = lockedCount;
    }
}
} // namespace scene
