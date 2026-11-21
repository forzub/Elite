#include "src/render/cockpit/HubCorridorHudRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/HubPlanPads.h"
#include <vector>

namespace render::cockpit
{

void HubCorridorHudRenderer::init()
{
    m_batch.init();
}

bool HubCorridorHudRenderer::project(const glm::dvec3& relative, const glm::mat4& view, const glm::mat4& proj,
                                     const Viewport& vp, glm::vec2& outPx) const
{
    // relative-to-player with w = 0 (no large-coordinate precision loss), as the guidance corridor
    const glm::vec4 v = view * glm::vec4(glm::vec3(relative), 0.0f);
    if (v.z >= -0.05f)
        return false;
    const glm::vec4 clip = proj * glm::vec4(v.x, v.y, v.z, 1.0f);
    if (std::abs(clip.w) <= 1.0e-6f)
        return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || std::abs(ndc.x) > 4.0f || std::abs(ndc.y) > 4.0f)
        return false;
    outPx = {(ndc.x * 0.5f + 0.5f) * static_cast<float>(vp.width), (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(vp.height)};
    return true;
}

void HubCorridorHudRenderer::render(const ClientWorldState& world, const ClientShipState& player, const glm::mat4& viewMatrix,
                                    const glm::mat4& projectionMatrix, const Viewport& viewport)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    // in a dock / flown by the station: no lanes - the dock walls hide them anyway and the pilot does
    // not fly them now (author 2026-11-21: «трассы видны даже когда я нахожусь в доке»)
    if (player.dockDispatcher.active && player.dockDispatcher.stationControlled)
        return;
    const glm::dvec3 playerMeters = world::coordinates::fullMeters(player.renderTransform.worldPosition);
    // the player's direction of travel (velocity relative to the frame; the nose when nearly at rest): a lane running
    // against it is drawn RED (author 2026-11-17: «тунели для встречного движения делать красными зонами в
    // зависимости от направления движения игрока»)
    glm::dvec3 travel(player.renderTransform.relativeVelocityMetersPerSecond);
    travel = glm::length(travel) > 5.0 ? glm::normalize(travel) : glm::dvec3(player.renderTransform.forward());
    bool begun = false;
    const auto alphaAt = [&](const glm::dvec3& w, double peak) {
        const double d = glm::length(w - playerMeters);
        return d >= FadeMeters ? 0.0 : peak * (1.0 - d / FadeMeters);
    };
    for (const auto& [oid, object] : world.objects())
    {
        // the module whose pose is a lane network's frame (HubLaneCatalog: frame_module_id of the hub's lane file)
        if (!object.hubAttachment.valid || object.systemId != player.transform.motion.systemId)
            continue;
        const auto* network = game::navigation::traffic::HubLaneCatalog::forFrameModule(object.hubAttachment.moduleId);
        if (!network)
            continue;
        const glm::dvec3 origin = world::coordinates::fullMeters(object.renderWorldPosition);
        if (glm::length(origin - playerMeters) > 60000.0)
            continue;
        const glm::dmat3 R(glm::mat3(object.renderOrientation));
        const auto toWorld = [&](const glm::dvec3& local) { return origin + R * local; };
        const glm::dvec3 up = R * glm::dvec3(0.0, 1.0, 0.0);
        // what hides the lanes / zones (author 2026-11-21: «если между зоной и игроком находится помеха, за помехой зону не
        // должно быть видно»): the planar city's blocks as boxes in this frame; a sample is hidden when the segment from it
        // to the pilot meets one (a ring by its centre, a connector / zone edge by its middle)
        struct Box
        {
            glm::dvec3 lo, hi;
        };
        std::vector<Box> blockers;
        // each building of the plan by its own box (a block as one box hid everything from a ship flying low over it,
        // live 2026-11-21); a box the pilot is inside does not count
        const glm::dmat3 Rt = glm::transpose(R);
        const glm::dvec3 eyeLocal = Rt * (playerMeters - origin);
        if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId))
            for (const auto& m : plan->modules())
            {
                const glm::dvec3 lo(std::min(m.rect.x, m.rect.z), std::min(m.y0, m.y1), std::min(m.rect.y, m.rect.w));
                const glm::dvec3 hi(std::max(m.rect.x, m.rect.z), std::max(m.y0, m.y1), std::max(m.rect.y, m.rect.w));
                const bool inside = eyeLocal.x > lo.x && eyeLocal.x < hi.x && eyeLocal.y > lo.y && eyeLocal.y < hi.y &&
                                    eyeLocal.z > lo.z && eyeLocal.z < hi.z;
                if (!inside && hi.y - lo.y > 1.0)
                    blockers.push_back({lo, hi});
            }
        const auto hidden = [&](const glm::dvec3& world) {
            const glm::dvec3 p = Rt * (world - origin);
            const glm::dvec3 d = eyeLocal - p;
            const glm::dvec3 segLo = glm::min(p, eyeLocal), segHi = glm::max(p, eyeLocal);
            for (const auto& bx : blockers)
            {
                if (bx.hi.x < segLo.x || bx.lo.x > segHi.x || bx.hi.y < segLo.y || bx.lo.y > segHi.y || bx.hi.z < segLo.z ||
                    bx.lo.z > segHi.z)
                    continue;   // not on the way
                double t0 = 1.0e-3, t1 = 1.0;   // strictly between the sample and the eye
                bool miss = false;
                for (int k = 0; k < 3 && !miss; ++k)
                {
                    if (std::abs(d[k]) < 1.0e-9)
                    {
                        miss = p[k] < bx.lo[k] || p[k] > bx.hi[k];
                        continue;
                    }
                    double a = (bx.lo[k] - p[k]) / d[k], b = (bx.hi[k] - p[k]) / d[k];
                    if (a > b)
                        std::swap(a, b);
                    t0 = std::max(t0, a);
                    t1 = std::min(t1, b);
                    miss = t0 > t1;
                }
                if (!miss)
                    return true;
            }
            return false;
        };
        if (!begun)
        {
            m_batch.begin(viewport.width, viewport.height);
            begun = true;
        }
        const auto seg = [&](const glm::dvec3& a, const glm::dvec3& b, const glm::vec4& c, float w) {
            glm::vec2 pa, pb;
            if (project(a - playerMeters, viewMatrix, projectionMatrix, viewport, pa) &&
                project(b - playerMeters, viewMatrix, projectionMatrix, viewport, pb))
                m_batch.line(pa, pb, w, c);
        };
        // GREEN lanes: rings + four rails
        constexpr int RingSides = 12;
        for (const auto& lane : network->lanes())
        {
            for (std::size_t k = 0; k + 1 < lane.stationsLocal.size(); ++k)
            {
                const glm::dvec3 a = toWorld(lane.stationsLocal[k]);
                const glm::dvec3 b = toWorld(lane.stationsLocal[k + 1]);
                const double len = glm::length(b - a);
                if (len < 1.0)
                    continue;
                // skip the segment entirely when its nearest point is beyond the fade distance
                const glm::dvec3 ab = (b - a) / len;
                const double t = std::clamp(glm::dot(playerMeters - a, ab), 0.0, len);
                if (glm::length(a + ab * t - playerMeters) > FadeMeters + lane.radiusMeters)
                    continue;
                glm::dvec3 side = glm::cross(ab, up);
                side = glm::length(side) > 1.0e-6 ? glm::normalize(side) : glm::dvec3(1.0, 0.0, 0.0);
                const glm::dvec3 vert = glm::normalize(glm::cross(side, ab));
                const double r = lane.radiusMeters;
                const int steps = std::max(1, static_cast<int>(std::ceil(len / RingStepMeters)));
                std::array<glm::dvec3, RingSides> prevRing {};
                bool havePrev = false;
                for (int s = 0; s <= steps; ++s)
                {
                    const glm::dvec3 c = a + ab * (len * s / steps);
                    const double alpha = hidden(c) ? 0.0 : alphaAt(c, 0.18);
                    std::array<glm::dvec3, RingSides> ring {};
                    for (int i = 0; i < RingSides; ++i)
                    {
                        const double ang = 2.0 * 3.14159265358979323846 * i / RingSides;
                        ring[i] = c + side * (r * std::cos(ang)) + vert * (r * std::sin(ang));
                    }
                    if (alpha > 0.005)
                    {
                        const bool oncoming = glm::dot(ab, travel) < -0.3;
                        const glm::vec3 tint = oncoming    ? glm::vec3(1.00f, 0.32f, 0.26f)
                                               : lane.blue ? glm::vec3(0.35f, 0.62f, 1.00f)
                                                           : glm::vec3(0.40f, 1.00f, 0.50f);
                        const glm::vec4 col(tint, static_cast<float>(alpha * 0.55));
                        for (int i = 0; i < RingSides; ++i)
                            seg(ring[i], ring[(i + 1) % RingSides], col, 1.0f);
                        if (havePrev)
                        {
                            const glm::vec4 rail(tint, static_cast<float>(alpha * 0.35));
                            for (int i = 0; i < RingSides; i += RingSides / 4)
                                seg(prevRing[i], ring[i], rail, 1.0f);
                            // the direction: a faint chevron on the lane floor every ring
                            const glm::dvec3 tip = c - vert * (r * 0.9);
                            const glm::vec4 chev(tint, static_cast<float>(alpha * 0.6));
                            seg(tip, tip - ab * 25.0 + side * 15.0, chev, 1.2f);
                            seg(tip, tip - ab * 25.0 - side * 15.0, chev, 1.2f);
                        }
                    }
                    prevRing = ring;
                    havePrev = alpha > 0.005;
                }
            }
        }
        // CONNECTORS (pre-written turns, plaza merges): a faint centre line only
        for (const auto& cn : network->connectors())
        {
            const glm::dvec3 mid = toWorld(cn.pointsLocal[cn.pointsLocal.size() / 2]);
            if (glm::length(mid - playerMeters) > FadeMeters + cn.lengthMeters)
                continue;
            for (std::size_t i = 0; i + 1 < cn.pointsLocal.size(); ++i)
            {
                const glm::dvec3 a = toWorld(cn.pointsLocal[i]);
                const glm::dvec3 b = toWorld(cn.pointsLocal[i + 1]);
                const double alpha = hidden((a + b) * 0.5) ? 0.0 : alphaAt(a, 0.18);
                if (alpha <= 0.005)
                    continue;
                const double len = glm::length(b - a);
                const bool oncoming = len > 1.0e-6 && glm::dot((b - a) / len, travel) < -0.3;
                const glm::vec3 tint = oncoming ? glm::vec3(1.00f, 0.32f, 0.26f) : glm::vec3(0.40f, 1.00f, 0.50f);
                seg(a, b, glm::vec4(tint, static_cast<float>(alpha * 0.45)), 1.0f);
            }
        }
        // the PAD capture zones (author 2026-11-21: «прозрачный куб с белёсыми рёбрами, который появляется, когда до площадки
        // остаётся метров 100»): an automatic pad's zone whitish, a hand-flown pad's amber; the deck outline under it
        if (const auto* cityPlan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId))
            for (const auto& m : cityPlan->modules())
            {
                if (!m.pad)
                    continue;
                const glm::dvec3 deck = toWorld(m.padPosition);
                const double d = glm::length(deck - playerMeters);
                if (d > game::navigation::HubPadZoneShowMeters)
                    continue;
                const double fade = std::clamp((game::navigation::HubPadZoneShowMeters - d) / 30.0, 0.0, 1.0);
                const glm::vec3 tint = m.autoLanding ? glm::vec3(0.90f, 0.95f, 1.00f) : glm::vec3(1.00f, 0.75f, 0.35f);
                const glm::vec4 edge(tint, static_cast<float>(0.22 * fade));   // translucent (author: «слишком яркая»)
                const double s = game::navigation::HubPadZoneHalf, hgt = game::navigation::HubPadZoneHeight;
                const double sy = m.under ? -1.0 : 1.0;
                std::array<glm::dvec3, 8> c {};
                for (int i = 0; i < 8; ++i)
                    c[i] = toWorld(m.padPosition + glm::dvec3((i & 1) ? s : -s, (i & 2) ? sy * hgt : 0.0, (i & 4) ? s : -s));
                for (int i = 0; i < 8; ++i)
                    for (int bit = 1; bit < 8; bit <<= 1)
                        if (!(i & bit) && !hidden((c[i] + c[i | bit]) * 0.5))
                            seg(c[i], c[i | bit], edge, 1.0f);
                const double half = 0.5 * game::navigation::HubPadDeckSide;
                const glm::vec4 deckLine(tint, static_cast<float>(0.35 * fade));
                const glm::dvec3 lift(0.0, sy * 0.3, 0.0);
                const glm::dvec3 q0 = toWorld(m.padPosition + lift + glm::dvec3(-half, 0, -half)),
                                 q1 = toWorld(m.padPosition + lift + glm::dvec3(half, 0, -half)),
                                 q2 = toWorld(m.padPosition + lift + glm::dvec3(half, 0, half)),
                                 q3 = toWorld(m.padPosition + lift + glm::dvec3(-half, 0, half));
                seg(q0, q1, deckLine, 2.0f);
                seg(q1, q2, deckLine, 2.0f);
                seg(q2, q3, deckLine, 2.0f);
                seg(q3, q0, deckLine, 2.0f);
            }
        // RED zones: faint box edges
        for (const auto& zone : network->redZones())
        {
            glm::dmat4 rm(1.0);
            rm = glm::rotate(rm, glm::radians(zone.rotationDeg.x), glm::dvec3(1, 0, 0));
            rm = glm::rotate(rm, glm::radians(zone.rotationDeg.y), glm::dvec3(0, 1, 0));
            rm = glm::rotate(rm, glm::radians(zone.rotationDeg.z), glm::dvec3(0, 0, 1));
            const glm::dmat3 zr(rm);
            const glm::dvec3 centre = toWorld(zone.centerLocal);
            const double alpha = alphaAt(centre, 0.22);
            if (alpha <= 0.005)
                continue;
            const glm::vec4 col(1.00f, 0.30f, 0.25f, static_cast<float>(alpha * 0.6));
            const glm::dvec3 h = zone.halfExtentsMeters;
            std::array<glm::dvec3, 8> c {};
            for (int i = 0; i < 8; ++i)
                c[i] = toWorld(zone.centerLocal + zr * glm::dvec3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z));
            for (int i = 0; i < 8; ++i)
                for (int bit = 1; bit < 8; bit <<= 1)
                    if (!(i & bit) && !hidden((c[i] + c[i | bit]) * 0.5))
                        seg(c[i], c[i | bit], col, 1.0f);
        }
    }
    if (begun)
        m_batch.flush();
}

} // namespace render::cockpit
