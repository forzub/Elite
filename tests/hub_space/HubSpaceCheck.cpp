// Spatial integrity stage A (2026-11-27): the city space contract (src/game/navigation/HubCitySpace.h) on the REAL plans.
// Run from the repository root. Exit 0 = every check passed.
//  A1  every block / building of earth_ring_hub: the plan point resolved through HubCitySpace == the 2D map's point (one
//      formula), and the old "module offset" reading (R * plan, no bend) is measured - it is the defect being removed.
//  A2  plan -> world -> plan round trip (ring + flat) under arbitrary orientations, through a full revolution of the ring;
//      the world velocity of a city point = v + omega x r; the surface up is the bend's.
//  A4  the along-ring difference is periodic (the seam): the shortest way, both directions.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/HubCitySpace.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/HubPlanPads.h"

namespace
{
int g_failures = 0;
void check(bool ok, const std::string& what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

glm::dmat3 randomRotation(std::mt19937& rng)
{
    std::normal_distribution<double> n(0.0, 1.0);
    glm::dvec3 a(n(rng), n(rng), n(rng));
    a = glm::normalize(a);
    const double angle = std::uniform_real_distribution<double>(0.0, 6.283185307179586)(rng);
    const double c = std::cos(angle), s = std::sin(angle), t = 1.0 - c;
    return glm::dmat3(glm::dvec3(t * a.x * a.x + c, t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y),
                      glm::dvec3(t * a.x * a.y - s * a.z, t * a.y * a.y + c, t * a.y * a.z + s * a.x),
                      glm::dvec3(t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c));
}

glm::dmat3 aboutZ(double angle)
{
    const double c = std::cos(angle), s = std::sin(angle);
    return glm::dmat3(glm::dvec3(c, s, 0.0), glm::dvec3(-s, c, 0.0), glm::dvec3(0.0, 0.0, 1.0));
}

void roundTrips(const game::navigation::HubPlan& plan, const char* name, std::mt19937& rng)
{
    game::navigation::HubCitySpace space;
    space.bend = plan.ring();
    double worst = 0.0;
    for (int pose = 0; pose < 8; ++pose)
    {
        space.R = randomRotation(rng);
        space.cpos = glm::dvec3(1.0e9 * (pose - 4), -3.0e8, 7.0e7 * pose);   // far from the origin as a real hub is
        for (const auto& b : plan.blocks())
        {
            const glm::dvec3 p(b.centre.x, 0.0, b.centre.y);
            const glm::dvec3 back = space.worldToPlan(space.planToWorld(p));
            worst = std::max(worst, glm::length(space.planDelta(p, back)));
        }
        for (const auto& m : plan.modules())
        {
            const glm::dvec3 p(0.5 * (m.rect.x + m.rect.z), m.y1, 0.5 * (m.rect.y + m.rect.w));
            const glm::dvec3 back = space.worldToPlan(space.planToWorld(p));
            worst = std::max(worst, glm::length(space.planDelta(p, back)));
        }
    }
    // 1e9 m origins: double resolution there is ~1e-7 m; the round trip must stay at the millimetre
    std::printf("%s: plan->world->plan worst %.3g m over %zu blocks, %zu buildings, 8 poses\n", name, worst,
                plan.blocks().size(), plan.modules().size());
    check(worst < 1.0e-3, std::string(name) + " round trip");
}
} // namespace

int main()
{
    std::mt19937 rng(20261127);
    game::navigation::HubPlan ring, flat;
    if (!ring.load("src/assets/data/navigation/hub_plans/earth_ring_hub.json") ||
        !flat.load("src/assets/data/navigation/hub_plans/earth_orbital_hub.json"))
    {
        std::printf("FAIL: plans not loaded (run from the repository root)\n");
        return 2;
    }
    check(ring.ring().valid && !flat.ring().valid, "earth_ring_hub is a ring, earth_orbital_hub flat");

    // ---- A1: the block / building targets of the ring city
    {
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        space.R = randomRotation(rng);
        space.cpos = glm::dvec3(1.2e10, -4.0e9, 3.0e8);
        double oldMin = 1e300, oldMax = 0.0, newMax = 0.0;
        for (const auto& b : ring.blocks())
        {
            const glm::dvec3 p(b.centre.x, 0.0, b.centre.y);
            // the 2D / 3D map's point (SystemMapRendererNavigation: cpos + R * toFrame) == the HUD's (planToWorld)
            const glm::dvec3 map = space.cpos + space.R * ring.ring().toFrame(p);
            const glm::dvec3 hud = space.planToWorld(p);
            const glm::dvec3 oldHud = space.cpos + space.R * p;   // the removed reading: a plan point as a module offset
            newMax = std::max(newMax, glm::length(hud - map));
            const double err = glm::length(oldHud - map);
            oldMin = std::min(oldMin, err);
            oldMax = std::max(oldMax, err);
        }
        std::printf("A1 earth_ring_hub %zu blocks: old HUD offset error %.2f .. %.2f m, now %.3g m\n", ring.blocks().size(),
                    oldMin, oldMax, newMax);
        check(ring.blocks().size() == 16, "earth_ring_hub has 16 blocks");
        check(oldMin > 1000.0, "the old reading is the measured defect (> 1 km on every ring block)");
        check(newMax < 1.0e-6, "HUD == map for every ring block");
        // a flat city: the plan IS the module frame (no change for it)
        game::navigation::HubCitySpace f;
        f.bend = flat.ring();
        f.R = randomRotation(rng);
        double flatMax = 0.0;
        for (const auto& b : flat.blocks())
        {
            const glm::dvec3 p(b.centre.x, 0.0, b.centre.y);
            flatMax = std::max(flatMax, glm::length(f.planToWorld(p) - (f.cpos + f.R * p)));
        }
        check(flatMax < 1.0e-9, "flat city: plan point == module offset");
    }

    // ---- A2: round trips, any orientation
    roundTrips(ring, "earth_ring_hub", rng);
    roundTrips(flat, "earth_orbital_hub", rng);

    // ---- A2: a full revolution of the ring: a city point turns WITH the city (the plan point does not move)
    {
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        const glm::dmat3 base = randomRotation(rng);
        const glm::dvec3 p(ring.blocks()[5].centre.x, 0.0, ring.blocks()[5].centre.y);
        double worst = 0.0;
        for (int k = 0; k <= 72; ++k)
        {
            const double a = 6.283185307179586 * k / 72.0;
            space.R = base * aboutZ(a);   // the ring spins about its module +Z
            const glm::dvec3 w = space.planToWorld(p);
            worst = std::max(worst, glm::length(space.planDelta(p, space.worldToPlan(w))));
            // the same point by rotating the module point: identical
            worst = std::max(worst, glm::length(w - (space.cpos + base * (aboutZ(a) * ring.ring().toFrame(p)))));
        }
        check(worst < 1.0e-6, "full revolution: the plan point stays the same, the world point turns with the ring");
        // the velocity of a fixed city point: v + omega x r (the deck speed at the rim ~ 2 pi R / period)
        space.R = base;
        space.cvel = glm::dvec3(30000.0, 0.0, 0.0);
        const double w = 6.283185307179586 / ring.ring().periodSeconds;
        space.omega = base * glm::dvec3(0.0, 0.0, w);
        const glm::dvec3 rim(0.0, 0.0, 0.0);
        const glm::dvec3 v = space.pointVelocityWorld(rim) - space.cvel;
        std::printf("A2 deck speed at the rim %.2f m/s (2 pi R / T = %.2f)\n", glm::length(v), w * ring.ring().radius);
        check(std::abs(glm::length(v) - w * ring.ring().radius) < 1.0e-6, "deck velocity = omega x r");
        check(std::abs(glm::dot(glm::normalize(space.surfaceUpWorld(rim)), glm::normalize(space.cpos - space.planToWorld(rim))) - 1.0) < 1.0e-9 ||
                  glm::length(space.cpos - space.planToWorld(rim)) < 1.0,
              "the surface up points to the axis on a ring");
    }

    // ---- A4: the seam
    {
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        const double L = ring.ring().length, x0 = ring.ring().x0;
        const double a = x0 + L - 10.0, b = x0 + 15.0;   // 25 m apart across the seam
        check(std::abs(space.alongDelta(a, b) - 25.0) < 1.0e-9, "across the seam forward: +25 m");
        check(std::abs(space.alongDelta(b, a) + 25.0) < 1.0e-9, "across the seam backward: -25 m");
        check(std::abs(space.alongDelta(x0 + 100.0, x0 + 400.0) - 300.0) < 1.0e-9, "inside the strip: the plain difference");
        const glm::dvec3 pa(a, 0.0, 3.0), pb(b, 0.0, 3.0);
        check(std::abs(glm::length(space.planToWorld(pa) - space.planToWorld(pb)) - 25.0) < 0.1,
              "the world chord across the seam is the short one (~25 m)");
        game::navigation::HubCitySpace flatSpace;
        check(std::abs(flatSpace.alongDelta(a, b) - (b - a)) < 1.0e-9, "a flat city has no seam");
        // the pad service across the seam: a ship 150 m past the seam from the pad nearest to it
        const game::navigation::HubPlanModule* nearSeam = nullptr;
        double best = 1e300;
        for (const auto& m : ring.modules())
            if (m.pad && !m.under && std::abs(space.alongDelta(x0, m.padPosition.x)) < best)
            {
                best = std::abs(space.alongDelta(x0, m.padPosition.x));
                nearSeam = &m;
            }
        check(nearSeam != nullptr, "a ring pad exists");
        if (nearSeam)
        {
            const double side = space.alongDelta(x0, nearSeam->padPosition.x) >= 0.0 ? -1.0 : 1.0;   // the other side
            const glm::dvec3 rawShip(x0 + (side < 0.0 ? L - 150.0 : 150.0), 300.0, nearSeam->padPosition.z);   // as toStrip gives
            const glm::dvec3 ship = space.unwrapNear(rawShip, nearSeam->padPosition);
            const auto route = game::navigation::hubPadRoute(ring, *nearSeam, ship, 300.0, true);
            double length = 0.0;
            for (std::size_t k = 1; k < route.size(); ++k)
                length += glm::length(route[k] - route[k - 1]);
            const double chord = glm::length(space.planToWorld(rawShip) - space.planToWorld(nearSeam->padPosition));
            const auto rawRoute = game::navigation::hubPadRoute(ring, *nearSeam, rawShip, 300.0, true);
            double rawLength = 0.0;
            for (std::size_t k = 1; k < rawRoute.size(); ++k)
                rawLength += glm::length(rawRoute[k] - rawRoute[k - 1]);
            std::printf("A4 pad %s %.0f m from the seam: service route %.0f m (straight %.0f m); without the unwrap %.0f m\n",
                        nearSeam->address.c_str(), best, length, chord, rawLength);
            check(length < chord + 1500.0, "the service route across the seam is the short one");
            check(rawLength > 0.5 * L, "the old (non-periodic) start goes the long way (the defect)");
        }
    }

    // ---- A5: every multi-place berth lands its places apart, on its own model's marked places
    for (const auto* plan : {&ring, &flat})
    {
        int garages2 = 0, hangars = 0, under = 0;
        for (const auto& m : plan->modules())
        {
            if (!m.pad || game::navigation::hubBerthPlaces(m) < 2)
                continue;
            const bool garage = m.padKind == game::navigation::HubPlanModule::PadKind::Garage;
            garages2 += garage;
            hangars += !garage;
            under += m.under;
            const glm::dvec3 t0 = game::navigation::hubBerthTouchdown(m, 0), t1 = game::navigation::hubBerthTouchdown(m, 1);
            const double expect = 2.0 * (garage ? game::navigation::HubGarage2PlaceX : game::navigation::HubHangarBayX);
            check(std::abs(glm::length(t1 - t0) - expect) < 1.0e-9,
                  plan->hubId() + " " + m.address + ": the two places " + std::to_string(glm::length(t1 - t0)) + " m apart");
            // both on the model's +-X line through its centre (HubExpansion: R = Rx(under 180) * Ry(yaw)), at its deck
            const glm::dvec3 rightModel = game::navigation::hubRotationXYZ(glm::dvec3(m.under ? 180.0 : 0.0, m.padYawDeg, 0.0)) *
                                          glm::dvec3(1.0, 0.0, 0.0);
            check(std::abs(std::abs(glm::dot(glm::normalize(t1 - t0), rightModel)) - 1.0) < 1.0e-9,
                  plan->hubId() + " " + m.address + ": the places lie along the model's own X axis");
            check(std::abs(t0.y - m.padPosition.y) < 1.0e-9 && std::abs(t1.y - m.padPosition.y) < 1.0e-9,
                  plan->hubId() + " " + m.address + ": the places at the deck height");
            // the door / apron front is the model's +Z
            const glm::dvec3 fwdModel = game::navigation::hubRotationXYZ(glm::dvec3(m.under ? 180.0 : 0.0, m.padYawDeg, 0.0)) *
                                        glm::dvec3(0.0, 0.0, 1.0);
            check(glm::length(game::navigation::hubPadAxisForward(m) - fwdModel) < 1.0e-9,
                  plan->hubId() + " " + m.address + ": the door axis is the model's +Z");
        }
        std::printf("A5 %s: %d two-place garages, %d hangars (%d under the deck) - places apart on the model's axes\n",
                    plan->hubId().c_str(), garages2, hangars, under);
    }

    if (g_failures)
    {
        std::printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("hub space contract OK\n");
    return 0;
}
