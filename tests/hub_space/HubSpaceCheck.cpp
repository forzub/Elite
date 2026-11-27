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
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/simulation/CityPadPresence.h"
#include "src/game/network/ReplicationSnapshotMerge.h"

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

    // ---- A5 (re-audit): a berth's places in its RIGID model (HubExpansion: the model at toFrame(padOrigin), turned by
    // basisAt(padOrigin.x) * Rx(under 180) * Ry(yaw)) vs the place bent point by point (toFrame(touchdown in plan))
    {
        const auto& bend = ring.ring();
        double worst = 0.0, worstAfter = 0.0;
        std::string worstAt;
        for (const auto& m : ring.modules())
        {
            if (!m.pad || game::navigation::hubBerthPlaces(m) < 2)
                continue;
            const glm::dmat3 modelPlan = game::navigation::hubRotationXYZ(glm::dvec3(m.under ? 180.0 : 0.0, m.padYawDeg, 0.0));
            const glm::dmat3 modelFrame = bend.basisAt(m.padOrigin.x) * modelPlan;
            for (int bay = 0; bay < game::navigation::hubBerthPlaces(m); ++bay)
            {
                const glm::dvec3 tPlan = game::navigation::hubBerthTouchdown(m, bay);
                const glm::dvec3 local = glm::transpose(modelPlan) * (tPlan - m.padOrigin);   // the point IN the model
                const glm::dvec3 rigid = bend.toFrame(m.padOrigin) + modelFrame * local;
                const double dev = glm::length(bend.toFrame(tPlan) - rigid);
                if (dev > worst)
                {
                    worst = dev;
                    worstAt = m.address + " place " + std::to_string(bay);
                }
                const glm::dvec3 tRigid = game::navigation::hubBerthTouchdownOnStructure(m, bay, bend);
                worstAfter = std::max(worstAfter, glm::length(bend.toFrame(tRigid) - rigid));
            }
        }
        // the landed hull's up: the deck normal under the touchdown (the old rule) vs the rigid model's floor
        double tiltOld = 0.0, tiltNew = 0.0;
        for (const auto& m : ring.modules())
            if (m.pad && game::navigation::hubBerthPlaces(m) >= 2)
                for (int bay = 0; bay < 2; ++bay)
                {
                    const glm::dvec3 t = game::navigation::hubBerthTouchdownOnStructure(m, bay, bend);
                    const glm::dvec3 floorUp = game::navigation::hubBerthFloorUpFrame(m, bend);
                    const glm::dvec3 deckUp = bend.basisAt(t.x)[1] * (m.under ? -1.0 : 1.0);
                    const glm::dvec3 structureUp = bend.basisAt(m.padOrigin.x)[1] * (m.under ? -1.0 : 1.0);   // the new rule
                    tiltOld = std::max(tiltOld, glm::degrees(std::acos(std::clamp(glm::dot(deckUp, floorUp), -1.0, 1.0))));
                    tiltNew = std::max(tiltNew, glm::degrees(std::acos(std::clamp(glm::dot(structureUp, floorUp), -1.0, 1.0))));
                }
        std::printf("A5 landed attitude on the ring: the deck normal under the place is off the berth's floor by up to %.3f deg; "
                    "the structure's axes %.3g deg\n", tiltOld, tiltNew);
        check(tiltNew < 1.0e-6, "a landed hull stands on the berth's floor");
        std::printf("A5 rigid model, earth_ring_hub: a place bent point by point is off its rigid model by up to %.3f m (%s); "
                    "through the structure's pose %.3g m\n", worst, worstAt.c_str(), worstAfter);
        check(worstAfter < 1.0e-6, "berth places on the structure's rigid pose");
    }

    // ---- A8: the ring's plan sheet vs its physical extents (the map bridge's two sizes)
    {
        double under = 0.0;
        for (const auto& b : ring.blocks())
            under = std::max(under, b.underDepthMeters);
        for (const auto& m : ring.modules())   // the same rule as ClientHubMapBridge
            under = std::max(under, -std::min(0.0, std::min(m.y0, m.y1)));
        const glm::dvec4 r = ring.bounds();
        const double sheetX = r.z - r.x, outer = ring.ring().radius + under;
        // every plan point's physical distance from the axis stays within the wheel
        double maxR = 0.0;
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        for (const auto& m : ring.modules())
            for (const double y : {m.y0, m.y1})
            {
                const glm::dvec3 f = space.planToModule(glm::dvec3(0.5 * (m.rect.x + m.rect.z), y, 0.5 * (m.rect.y + m.rect.w)));
                maxR = std::max(maxR, std::hypot(f.x, f.y));
            }
        std::printf("A8 earth_ring_hub: plan sheet %.0f m long; the wheel %.0f m across (deck R %.0f + under %.0f); the "
                    "farthest building point %.0f m from the axis\n", sheetX, 2.0 * outer, ring.ring().radius, under, maxR);
        check(maxR <= outer + 1.0e-6, "every building inside the physical wheel the map now uses");
        check(sheetX > 2.5 * outer, "the sheet is NOT the physical size (the old map box)");
    }

    // ---- A3: the map's inverse - screen -> scene -> screen round trip on the ring's plan sheet (top-down plan camera)
    {
        game::system_map::LocalMapCameraSnapshot cam;
        cam.state.pitch = 1.5707963267948966;   // the plan view looks straight down
        cam.state.yaw = 0.0;
        cam.state.zoom = 1.0;
        cam.scale = 0.12;
        cam.centerPx = glm::dvec2(960.0, 540.0);
        cam.planMode = true;
        cam.planWarp.valid = true;
        cam.planWarp.origin = glm::dvec3(150.0, -40.0, 75.0);
        cam.planWarp.ring = ring.ring();
        cam.originMeters = cam.planWarp.origin;
        double worst = 0.0, worstOld = 0.0;
        for (int i = 0; i <= 20; ++i)
            for (int j = 0; j <= 20; ++j)
            {
                const glm::dvec2 px(960.0 + (i - 10) * 40.0, 540.0 + (j - 10) * 10.0);
                const glm::dvec3 scene = cam.unprojectPlane(px);   // a scene point (the city on the ring)
                worst = std::max(worst, glm::length(cam.project(scene) - px));
                const glm::dvec3 sheetAsScene = cam.unprojectSheet(px);   // the old answer: the sheet point as a scene point
                worstOld = std::max(worstOld, glm::length(cam.project(sheetAsScene) - px));
            }
        std::printf("A3 ring plan map: screen -> scene -> screen worst %.3g px (the old sheet-as-scene answer: %.1f px)\n",
                    worst, worstOld);
        check(worst < 1.0e-6, "the plan map's cursor point re-projects onto the cursor");
    }

    // ---- A2: the plan's domain - deck points yes, the spin axis (axial docks, the sync gate) no
    {
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        bool allBlocks = true;
        for (const auto& b : ring.blocks())
            allBlocks = allBlocks && space.moduleOnDeck(space.planToModule(glm::dvec3(b.centre.x, 0.0, b.centre.y)));
        bool tallest = true;
        for (const auto& m : ring.modules())
            tallest = tallest && space.moduleOnDeck(space.planToModule(glm::dvec3(0.5 * (m.rect.x + m.rect.z), m.y1, 0.5 * (m.rect.y + m.rect.w))));
        check(allBlocks && tallest, "A2: every block and every building top is in the plan's domain");
        check(!space.moduleOnDeck(glm::dvec3(0.0, 0.0, 0.0)) && !space.moduleOnDeck(ring.ring().syncGate) &&
                  !space.moduleOnDeck(glm::dvec3(0.0, 0.0, 3000.0)),
              "A2: the spin axis (the hub origin, the sync gate, an axial point 3 km out) is NOT a deck point");
        game::navigation::HubCitySpace flatSpace;
        check(flatSpace.moduleOnDeck(glm::dvec3(0.0, 5000.0, 0.0)), "A2: a flat city's plan covers its whole frame");
        std::printf("A2 plan domain: deck points in, the ring's axis out\n");
    }

    // ---- A4 (audit #6): a line across the seam on the ring's 2D plan breaks exactly once (the map's split rule)
    {
        game::system_map::LocalMapPlanWarp warp;
        warp.valid = true;
        warp.ring = ring.ring();
        const double L = ring.ring().length, x0 = ring.ring().x0;
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        // a deck route from 300 m before the seam to 300 m past it, in the scene (the module frame = the scene here)
        const glm::dvec3 a = space.planToModule(glm::dvec3(x0 + L - 300.0, 0.0, 100.0));
        const glm::dvec3 b = space.planToModule(glm::dvec3(x0 + 300.0, 0.0, 100.0));
        int breaks = 0;
        double last = 0.0, worstStep = 0.0;
        for (int s = 0; s <= 30; ++s)
        {
            const double sx = warp.apply(a + (b - a) * (s / 30.0)).x;
            if (s > 0)
            {
                if (std::abs(sx - last) > 0.5 * L)
                    ++breaks;
                else
                    worstStep = std::max(worstStep, std::abs(sx - last));
            }
            last = sx;
        }
        std::printf("A4 ring plan polyline across the seam: %d break(s), the longest drawn step %.0f m (the sheet is %.0f m)\n",
                    breaks, worstStep, L);
        check(breaks == 1 && worstStep < 100.0, "A4: one break at the seam, no line across the sheet");
    }

    // ---- B1 / B2 (audit #6): a RETAINED ship row (sparse publication) advanced in its turning frame - a ship standing on
    // the ring's deck must stay on its arc and world == origin + axes * local after the advance
    {
        ShipSnapshot row;
        row.referenceFrame.valid = true;
        row.referenceFrame.originMeters = glm::dvec3(1.0e9, 2.0e8, -3.0e8);
        row.referenceFrame.velocityMetersPerSecond = glm::dvec3(7600.0, 0.0, 0.0);
        const double omega = 6.283185307179586 / ring.ring().periodSeconds;
        row.referenceFrame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, omega);
        row.referenceFrame.localPositionMeters = glm::dvec3(ring.ring().radius, 0.0, 0.0);   // on the deck, at rest
        const glm::dmat3 B0(row.referenceFrame.progradeAxis, row.referenceFrame.radialAxis, row.referenceFrame.normalAxis);
        row.transform.setWorldPositionMeters(row.referenceFrame.originMeters + B0 * row.referenceFrame.localPositionMeters);
        row.transform.motion.worldVelocityMps =
            row.referenceFrame.velocityMetersPerSecond +
            glm::cross(row.referenceFrame.angularVelocityWorldRadPerSecond, B0 * row.referenceFrame.localPositionMeters);
        const double dt = 1.0;
        game::network::detail::advanceRetainedShipPresentation(row, dt);
        const glm::dmat3 B(row.referenceFrame.progradeAxis, row.referenceFrame.radialAxis, row.referenceFrame.normalAxis);
        const glm::dvec3 framed = row.referenceFrame.originMeters + B * row.referenceFrame.localPositionMeters;
        // the truth: the frame origin moved by v dt, the deck point turned by omega dt round it
        const glm::dvec3 origin1 = glm::dvec3(1.0e9, 2.0e8, -3.0e8) + glm::dvec3(7600.0, 0.0, 0.0) * dt;
        const glm::dvec3 truth = origin1 + ring.ring().radius * glm::dvec3(std::cos(omega * dt), std::sin(omega * dt), 0.0);
        const double consistency = glm::length(row.transform.fullWorldMeters() - framed);
        const double offArc = glm::length(row.transform.fullWorldMeters() - truth);
        // the old rule (the world point along its velocity, the axes not turned): for the record
        const glm::dvec3 oldWorld = glm::dvec3(1.0e9, 2.0e8, -3.0e8) + B0 * glm::dvec3(ring.ring().radius, 0.0, 0.0) +
                                    (glm::dvec3(7600.0, 0.0, 0.0) + glm::cross(glm::dvec3(0.0, 0.0, omega), B0 * glm::dvec3(ring.ring().radius, 0.0, 0.0))) * dt;
        const glm::dvec3 oldFramed = origin1 + B0 * glm::dvec3(ring.ring().radius, 0.0, 0.0);
        std::printf("B retained row on the turning deck, 1 s: world vs frame %.3g m, off the arc %.3g m (the old rule: %.1f m "
                    "inconsistent, %.2f m off the arc)\n", consistency, offArc, glm::length(oldWorld - oldFramed),
                    glm::length(oldWorld - truth));
        check(consistency < 1.0e-6 && offArc < 1.0e-6, "B: a retained row stays on its frame and on the deck's arc");
    }

    // ---- A2 (audit #8): the pad service TAKES a ship where it is - several radii (the exact axis, near it, the deck,
    // beyond the deck's domain) and angles: the world point unchanged; the velocity carried: kept to the city-relative
    // value, so the world velocity is continuous
    {
        game::navigation::HubCitySpace space;
        space.bend = ring.ring();
        space.cpos = glm::dvec3(4.0e8, -1.0e7, 2.0e6);
        space.R = glm::dmat3(glm::rotate(glm::dmat4(1.0), 0.7, glm::dvec3(0.3, 1.0, -0.2)));
        space.cvel = glm::dvec3(7400.0, 120.0, -30.0);
        space.omega = space.R * glm::dvec3(0.0, 0.0, 6.283185307179586 / ring.ring().periodSeconds);
        const glm::dvec3 pad(ring.ring().x0 + 0.37 * ring.ring().length, 0.0, 120.0);
        double worstPos = 0.0, worstVel = 0.0;
        int n = 0;
        for (double r : {0.0, 0.0004, 0.5, 40.0, 0.3 * ring.ring().radius, ring.ring().radius - 30.0, ring.ring().radius,
                         1.7 * ring.ring().radius})
            for (int k = 0; k < 12; ++k)
            {
                const double a = 6.283185307179586 * k / 12.0 + 0.1;
                const glm::dvec3 module(r * std::sin(a), -r * std::cos(a), -60.0 + 15.0 * k);
                const glm::dvec3 world = space.cpos + space.R * module;
                const glm::dvec3 plan = space.takePlanPoint(world, pad);
                worstPos = std::max(worstPos, glm::length(space.toWorld(plan) - world));
                // a ship moving 6 m/s relative to the city at that point: the service's start velocity (the residual it
                // carries, mapped back) equals it
                const glm::dvec3 vRel = space.R * glm::dvec3(4.0, -3.0, 2.0);
                const glm::dvec3 vWorld = space.cvel + glm::cross(space.omega, world - space.cpos) + vRel;
                const glm::dvec3 residual = space.velocityToStrip(plan, vWorld);
                // the service maps its strip velocity like velocityToWorld; near the axis (r < 1 m) the along scaling is
                // skipped on purpose (finite there) - so check where it applies
                if (r > 1.0)
                    worstVel = std::max(worstVel, glm::length(space.velocityToWorld(plan, residual) - vWorld));
                ++n;
            }
        std::printf("A2 takeover over %d points (r 0 .. 1.7 R, 12 angles): position step %.3g m, velocity step %.3g m/s\n", n,
                    worstPos, worstVel);
        check(worstPos < 1.0e-5, "A2: the service takes the ship where it is (no position step, axis included)");
        check(worstVel < 1.0e-6, "A2: the takeover keeps the ship's velocity (no velocity step)");
    }

    // ---- A7: the hub map's GPU line shader (hub_map_dynamic_lines.vert: origin + R(p) * scale) == the CPU fallback
    {
        game::system_map::LocalMapCameraSnapshot cam;
        cam.state.pitch = 1.5707963267948966;
        cam.state.yaw = 0.37;
        cam.state.zoom = 1.3;
        cam.state.pan = glm::dvec2(25.0, -40.0);
        cam.scale = 0.12;
        cam.centerPx = glm::dvec2(960.0, 540.0);
        cam.planMode = true;
        cam.planWarp.valid = true;
        cam.planWarp.origin = glm::dvec3(1500.0, -40.0, 900.0);
        cam.planWarp.ring = ring.ring();
        cam.originMeters = glm::dvec3(1400.0, 0.0, 850.0);   // the plan view's centre (not 0)
        const auto gpu = [&](const glm::dvec2& screenOrigin, const glm::dvec3& p) {
            const double cy = std::cos(cam.state.yaw), sy = std::sin(cam.state.yaw);
            const double cp = std::cos(cam.state.pitch), sp = std::sin(cam.state.pitch);
            const double rx = p.x * cy - p.z * sy, rz = p.x * sy + p.z * cy, py = p.y * cp - rz * sp;
            return screenOrigin + glm::dvec2(rx, -py) * (cam.scale * cam.state.zoom);
        };
        const glm::dvec2 newOrigin = cam.projectSheet(glm::dvec3(0.0));
        const glm::dvec2 oldOrigin = cam.centerPx + cam.state.pan;
        double worst = 0.0, worstOld = 0.0, worstOldCpu = 0.0;
        for (const auto& b : ring.blocks())
        {
            const glm::dvec3 sheet = cam.planWarp.origin + glm::dvec3(b.centre.x, 0.0, b.centre.y);   // as HubMapBackend gives
            worst = std::max(worst, glm::length(gpu(newOrigin, sheet) - cam.projectSheet(sheet)));
            worstOld = std::max(worstOld, glm::length(gpu(oldOrigin, sheet) - cam.projectSheet(sheet)));
            worstOldCpu = std::max(worstOldCpu, glm::length(cam.project(sheet) - cam.projectSheet(sheet)));   // warped twice
        }
        std::printf("A7 hub map: GPU vs CPU %.3g px (the old GPU origin %.1f px off; the old CPU path warping the sheet "
                    "again %.1f px off)\n", worst, worstOld, worstOldCpu);
        check(worst < 1.0e-6, "the GPU and CPU paths draw the same sheet geometry at the same pixels");
    }

    // ---- A6: a pad's actual existence = the package's volumes + the damage state (CityPadPresence.h)
    {
        struct V
        {
            std::string m_label;
            int assemblyNode;
        };
        const std::vector<V> package {{"pad_A1-03.hubl_garage2.0", 40}, {"pad_A1-03.hubl_garage2.1", 40}, {"b1.hubb_tower.0", 7},
                                      {"pad_B2-01.hubl_pad_roof.0", 41}};
        const auto nodes = game::simulation::cityPadNodes(package);
        std::vector<game::simulation::CityBuildingState> damage {{41, static_cast<std::uint8_t>(game::simulation::CityBuildingVariant::Gutted)}};
        const auto built = game::simulation::cityPadPresence(nodes, damage, "A1-03");
        const auto gutted = game::simulation::cityPadPresence(nodes, damage, "B2-01");
        const auto missing = game::simulation::cityPadPresence(nodes, damage, "C3-07");   // its building still going up
        check(nodes.size() == 2 && built.usable() && built.node == 40, "A6: a built pad is usable");
        check(gutted.built && !gutted.usable(), "A6: a gutted pad exists but is not usable");
        check(!missing.built && !missing.usable(), "A6: a pad not in the package does not exist");
        damage.push_back({40, static_cast<std::uint8_t>(game::simulation::CityBuildingVariant::Missile)});
        check(game::simulation::cityPadPresence(nodes, damage, "A1-03").usable(), "A6: a missile hit leaves the deck usable");
        std::printf("A6 pad presence rule: built / gutted / not built / hit - as specified\n");
    }

    if (g_failures)
    {
        std::printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("hub space contract OK\n");
    return 0;
}
