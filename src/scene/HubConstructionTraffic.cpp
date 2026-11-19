#include "src/scene/HubConstructionTraffic.h"

#include "src/model_asset/HubExpansion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace scene
{
namespace
{
namespace ma = elite::model_asset;

// the claw ship's grip point: the held element's centre in the ship frame (tools/models/units/hub_construction.py)
constexpr float ClawGripZ = 32.0f;

float hash01(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

std::uint32_t idHash(const std::string& s)
{
    std::uint32_t h = 2166136261u;
    for (const char c : s)
        h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return h;
}

float smooth(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// +Z along fwd, +Y as close to up as possible
glm::mat4 frameOf(const glm::vec3& pos, glm::vec3 fwd)
{
    if (glm::dot(fwd, fwd) < 1e-8f)
        fwd = glm::vec3(0, 0, 1);
    fwd = glm::normalize(fwd);
    glm::vec3 up(0, 1, 0);
    glm::vec3 x = glm::cross(up, fwd);
    if (glm::dot(x, x) < 1e-6f)
        x = glm::vec3(1, 0, 0);
    x = glm::normalize(x);
    const glm::vec3 y = glm::cross(fwd, x);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(x, 0);
    m[1] = glm::vec4(y, 0);
    m[2] = glm::vec4(fwd, 0);
    m[3] = glm::vec4(pos, 1);
    return m;
}

glm::vec3 bezier(const glm::vec3& a, const glm::vec3& m, const glm::vec3& b, float t)
{
    const float u = 1.0f - t;
    return u * u * a + 2.0f * u * t * m + t * t * b;
}

glm::vec3 bezierTangent(const glm::vec3& a, const glm::vec3& m, const glm::vec3& b, float t)
{
    return 2.0f * (1.0f - t) * (m - a) + 2.0f * t * (b - m);
}

struct Emitter
{
    const glm::vec3& camera;
    float radius;
    std::vector<ConstructionTrafficInstance>& out;
    void operator()(ConstructionCraft model, const glm::mat4& local) const
    {
        const float d = glm::length(glm::vec3(local[3]) - camera);
        if (d <= radius)
            out.push_back({model, local, d});
    }
};

void cranes(const ma::ExpansionProject& p, double t, bool working, const Emitter& emit)
{
    for (const auto& pr : p.props)
    {
        const bool tall = pr.folder == "hubc_crane_mast";
        if (!tall && pr.folder != "hubc_crane_mast_s")
            continue;
        const std::uint32_t k = idHash(pr.id);
        const float a0 = hash01(k, 1) * 6.2831853f;
        float a = a0;
        if (working)
        {
            // a lift: slew to the drop point, hold while it is set, slew back, hold while the next one is slung
            const float period = 200.0f + 120.0f * hash01(k, 2);
            const float ph = static_cast<float>(std::fmod(t / period + hash01(k, 3), 1.0));
            const float sweep = (0.4f + 0.6f * hash01(k, static_cast<std::uint32_t>(t / period))) * 3.14159265f;
            const float s = ph < 0.25f ? smooth(ph / 0.25f) : ph < 0.6f ? 1.0f : ph < 0.85f ? 1.0f - smooth((ph - 0.6f) / 0.25f) : 0.0f;
            a = a0 + sweep * s;
        }
        const glm::vec3 pivot = pr.position + glm::vec3(0.0f, tall ? 330.0f : 120.0f, 0.0f);
        emit(ConstructionCraft::CraneJib, frameOf(pivot, glm::vec3(std::sin(a), 0.0f, std::cos(a))));
    }
}

void working(const ma::ExpansionProject& p, double t, const Emitter& emit)
{
    const std::uint32_t pk = idHash(p.id);
    const auto outward = [](const glm::vec4& pl) {
        const float yaw = glm::radians(pl.w);
        return glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw));
    };
    // the cargo haulers: one per temporary deck - in, unloaded on the deck, out, away a while
    for (std::size_t i = 0; i < p.platforms.size(); ++i)
    {
        const glm::vec4& pl = p.platforms[i];
        const glm::vec3 out = outward(pl);
        const glm::vec3 hover = glm::vec3(pl) + glm::vec3(0.0f, 9.0f, 0.0f);
        const glm::vec3 far = hover + out * 900.0f + glm::vec3(0.0f, 350.0f, 0.0f);
        const double period = 1500.0;
        const float ph = static_cast<float>(std::fmod(t / period + hash01(pk, static_cast<std::uint32_t>(i) + 11u), 1.0));
        glm::vec3 pos;
        if (ph < 0.15f)
            pos = glm::mix(far, hover, smooth(ph / 0.15f));
        else if (ph < 0.7f)
            pos = hover + glm::vec3(0.0f, 0.4f * std::sin(static_cast<float>(t) * 0.5f), 0.0f);
        else if (ph < 0.85f)
            pos = glm::mix(hover, far, smooth((ph - 0.7f) / 0.15f));
        else
            continue;
        emit(ConstructionCraft::Hauler, frameOf(pos, -out));
    }
    if (p.platforms.empty())
        return;
    // the claw ships with their elements and escort tugs
    const std::size_t fronts = std::max<std::size_t>(1, p.front.size());
    const std::size_t ships = std::min<std::size_t>(3, std::max<std::size_t>(1, p.front.size()));
    for (std::size_t j = 0; j < ships; ++j)
    {
        const double period = 480.0 + 60.0 * hash01(pk, static_cast<std::uint32_t>(j) + 21u);
        const double u = t / period + hash01(pk, static_cast<std::uint32_t>(j) + 31u);
        const std::uint32_t cycle = static_cast<std::uint32_t>(std::floor(u));
        const float ph = static_cast<float>(u - std::floor(u));
        const glm::vec4& pl = p.platforms[(j + cycle) % p.platforms.size()];
        const glm::vec3 F = p.front.empty() ? glm::vec3(pl) - outward(pl) * 150.0f + glm::vec3(0.0f, 40.0f, 0.0f)
                                            : p.front[(j * 7u + cycle * 3u) % fronts];
        const glm::vec3 A = glm::vec3(pl) + glm::vec3(0.0f, 30.0f, 0.0f);
        glm::vec3 dirFA = A - F;
        dirFA.y *= 0.3f;
        dirFA = glm::length(dirFA) > 1.0f ? glm::normalize(dirFA) : glm::vec3(0, 0, 1);
        const glm::vec3 B = F + dirFA * (ClawGripZ + 70.0f);       // the approach gate
        const glm::vec3 H = F + dirFA * ClawGripZ;                 // the hold: the element's centre at F
        const glm::vec3 M1 = 0.5f * (A + B) + glm::vec3(0.0f, 150.0f, 0.0f);
        glm::vec3 pos, fwd;
        bool held = true, installed = false;
        float push = 0.0f;
        if (ph < 0.08f)
        {
            pos = A;
            fwd = F - A;
        }
        else if (ph < 0.45f)
        {
            const float s = smooth((ph - 0.08f) / 0.37f);
            pos = bezier(A, M1, B, s);
            fwd = glm::mix(bezierTangent(A, M1, B, s), -dirFA * glm::length(B - A), smooth((ph - 0.35f) / 0.1f));
        }
        else if (ph < 0.55f)
        {
            pos = glm::mix(B, H, smooth((ph - 0.45f) / 0.1f));
            fwd = -dirFA;
            push = smooth((ph - 0.45f) / 0.1f);
        }
        else if (ph < 0.72f)
        {
            pos = H + glm::vec3(0.0f, 0.15f * std::sin(static_cast<float>(t) * 0.7f), 0.0f);
            fwd = -dirFA;
            push = 1.0f;
        }
        else if (ph < 0.78f)
        {
            pos = glm::mix(H, B, smooth((ph - 0.72f) / 0.06f));
            fwd = -dirFA;
            held = false;
            installed = true;
            push = 1.0f - smooth((ph - 0.72f) / 0.06f);
        }
        else
        {
            const float s = smooth((ph - 0.78f) / 0.22f);
            pos = bezier(B, M1, A, s);
            fwd = glm::mix(-dirFA, bezierTangent(B, M1, A, s), smooth(s / 0.15f));
            held = false;
            installed = ph < 0.95f;
        }
        const glm::mat4 ship = frameOf(pos, fwd);
        emit(ConstructionCraft::ClawShip, ship);
        const ConstructionCraft element = ((cycle + j) & 1u) ? ConstructionCraft::Panel : ConstructionCraft::Beam;
        if (held)
            emit(element, ship * glm::mat4(glm::vec4(1, 0, 0, 0), glm::vec4(0, 1, 0, 0), glm::vec4(0, 0, 1, 0),
                                           glm::vec4(0, 0, ClawGripZ, 1)));
        else if (installed)
            emit(element, frameOf(F, -dirFA));
        // two tugs: escort beside the ship, then pushing the element's ends while it is held at the front
        for (int side = -1; side <= 1; side += 2)
        {
            const float wob = std::sin(static_cast<float>(t) * 0.31f + static_cast<float>(side + 2 * j));
            const glm::vec3 local = glm::mix(glm::vec3(side * 18.0f, -5.0f + wob, -8.0f),
                                             glm::vec3(side * 14.0f, 6.0f, ClawGripZ - 3.0f), push);
            const glm::vec3 tp = glm::vec3(ship * glm::vec4(local, 1.0f));
            emit(ConstructionCraft::Tug, frameOf(tp, glm::vec3(ship[2])));
        }
    }
    // tethered worker pods drifting around the build front
    for (std::size_t f = 0; f < std::min<std::size_t>(6, p.front.size()); ++f)
        for (std::uint32_t k = 0; k < 2; ++k)
        {
            const float r = 22.0f + 18.0f * hash01(pk, static_cast<std::uint32_t>(f) * 2u + k + 41u);
            const float w = (k ? -1.0f : 1.0f) * (0.02f + 0.02f * hash01(pk, static_cast<std::uint32_t>(f) + 51u));
            const float a = static_cast<float>(std::fmod(t * w, 6.2831853)) + 3.0f * static_cast<float>(k);
            const glm::vec3 c = p.front[f] + glm::vec3(0.0f, 8.0f + 6.0f * std::sin(static_cast<float>(t) * 0.05f + k), 0.0f);
            const glm::vec3 pos = c + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
            emit(ConstructionCraft::Pod, frameOf(pos, glm::vec3(-std::sin(a), 0.0f, std::cos(a)) * w));
        }
}
} // namespace

const char* constructionCraftModelId(ConstructionCraft model)
{
    switch (model)
    {
    case ConstructionCraft::Tug: return "hubc_tug";
    case ConstructionCraft::ClawShip: return "hubc_clawship";
    case ConstructionCraft::Beam: return "hubc_element_beam";
    case ConstructionCraft::Panel: return "hubc_element_panel";
    case ConstructionCraft::Pod: return "hubc_pod";
    case ConstructionCraft::Hauler: return "hubc_hauler";
    case ConstructionCraft::CraneJib: return "hubc_crane_jib";
    default: return "hubc_tug";
    }
}

void hubConstructionTraffic(const std::string& hubId, double timeSeconds, const glm::vec3& cameraLocal, float radius,
                            std::vector<ConstructionTrafficInstance>& out)
{
    const ma::HubExpansionPlan* plan = ma::hubExpansionPlan(hubId);
    if (!plan)
        return;
    const int day = ma::hubExpansionDay(*plan, timeSeconds);
    const Emitter emit {cameraLocal, radius, out};
    for (const auto& p : plan->projects)
    {
        ma::ConstructionStatus s;
        ma::projectProgress(p, day, &s);
        if (s == ma::ConstructionStatus::Planned || s == ma::ConstructionStatus::Done)
            continue;
        const bool active = s == ma::ConstructionStatus::Building;
        if (plan->gravity)
            cranes(p, timeSeconds, active, emit);
        if (active)
        {
            // a site far from the camera: its craft would not pass the radius anyway - skip the work
            const glm::vec3 c = p.platforms.empty() ? glm::vec3(0.0f) : glm::vec3(p.platforms.front());
            if (!p.platforms.empty() && glm::length(c - cameraLocal) > radius + 6000.0f && p.kind != "build")
                continue;
            working(p, timeSeconds, emit);
        }
        else
            // stopped: a tug parked on every temporary deck, nothing moves
            for (const auto& pl : p.platforms)
            {
                const float yaw = glm::radians(pl.w);
                emit(ConstructionCraft::Tug, frameOf(glm::vec3(pl) + glm::vec3(0.0f, 4.0f, 0.0f),
                                                    glm::vec3(-std::sin(yaw), 0.0f, -std::cos(yaw))));
            }
    }
}
} // namespace scene
