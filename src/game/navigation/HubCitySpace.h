#pragma once

// THE city space (spatial integrity stage A2, 2026-11-27): one owner for every conversion between a hub city's PLAN and the
// world. It is not a new coordinate system: the bend is HubRingBend (the ring's strip <-> frame math, identity for a flat
// city), the pose is the city object's kinematic pose (origin, axes, velocity, spin). Promoted from the pad service's
// CityStrip (GameSimulationPadLanding.cpp), unchanged in its math, so the server, the HUD and the maps share it.
//
// The three spaces (names used in every caller):
//   PLAN   - the hub plan's strip metres: x along the city (along the ring), y up from the deck, z across. A block /
//            building / pad / street position of HubPlanCatalog, as the 2D plan stores it ("plan point").
//   MODULE - the city object's own frame (metres): the ring bent round its +Z axis (HubRingBend::toFrame).
//   WORLD  - system / world metres: origin + R * module.
// A plan point is NEVER a module offset: on a ring the same numbers are 1.4-4 km apart (earth_ring_hub, audit A1).
//
// The EPOCH is the pose's: build it from the authoritative pose (server StaticObject, a client object's worldPosition /
// orientation) for physics, from the render pose (renderWorldPosition / renderOrientation) for presentation - never mix
// the two inside one conversion.
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/HubRingBend.h"

namespace game::navigation
{
struct HubCitySpace
{
    // the pose (WORLD): the module origin, its axes (columns: module +X, +Y, +Z), its velocity, its spin
    glm::dvec3 cpos {0.0};
    glm::dvec3 cvel {0.0};
    glm::dvec3 omega {0.0};
    glm::dmat3 R {1.0};
    HubRingBend bend;

    // PLAN <-> MODULE
    glm::dvec3 planToModule(const glm::dvec3& plan) const { return bend.toFrame(plan); }
    glm::dvec3 moduleToPlan(const glm::dvec3& module) const { return bend.toStrip(module); }
    // a RIGID piece of the plan (a block's box, a building, a pad model) placed at a plan point: its MODULE position and
    // axes (columns: along, up, across) - the map's block boxes, their occluders and the HUD's use this one placement
    struct ModulePose
    {
        glm::dvec3 position {0.0};
        glm::dmat3 axes {1.0};
    };
    ModulePose planPoseToModule(const glm::dvec3& plan) const { return {bend.toFrame(plan), bend.basisAt(plan.x)}; }
    // PLAN <-> WORLD (the pad service's historical names)
    glm::dvec3 toWorld(const glm::dvec3& plan) const { return cpos + R * bend.toFrame(plan); }
    glm::dvec3 toStrip(const glm::dvec3& world) const { return bend.toStrip(glm::transpose(R) * (world - cpos)); }
    glm::dvec3 planToWorld(const glm::dvec3& plan) const { return toWorld(plan); }
    glm::dvec3 worldToPlan(const glm::dvec3& world) const { return toStrip(world); }
    // the plan's axes at a plan point, in WORLD (columns: along, up = the surface normal, across)
    glm::dmat3 axes(const glm::dvec3& plan) const { return R * bend.basisAt(plan.x); }
    glm::dvec3 surfaceUpWorld(const glm::dvec3& plan) const { return axes(plan)[1]; }
    // a plan direction at a plan point -> WORLD (no frame motion: a direction, not a velocity)
    glm::dvec3 planVectorToWorld(const glm::dvec3& plan, const glm::dvec3& v) const { return R * bend.vectorToFrame(plan, v); }
    // a velocity relative to the plan at a plan point -> WORLD: the city's velocity + its spin at that point + the motion
    glm::dvec3 velocityToWorld(const glm::dvec3& plan, const glm::dvec3& v) const
    {
        return cvel + glm::cross(omega, toWorld(plan) - cpos) + R * bend.vectorToFrame(plan, v);
    }
    // the WORLD velocity of a point fixed in the city (a pad, a building)
    glm::dvec3 pointVelocityWorld(const glm::dvec3& plan) const { return velocityToWorld(plan, glm::dvec3(0.0)); }
    // where a service TAKES a ship (audit #8 A2): its own plan point - the world point unchanged, wherever it is (on the
    // deck, near the axis, beyond the deck's domain); periodic `along` unwrapped near `nearPlan`. ONLY on the spin axis
    // itself (r below 1 micron), where the angle is undefined and every along maps to the same world point, the along is
    // taken from `nearPlan` (the pad) - no jump there by construction
    glm::dvec3 takePlanPoint(const glm::dvec3& world, const glm::dvec3& nearPlan) const
    {
        glm::dvec3 plan = unwrapNear(toStrip(world), nearPlan);
        if (bend.valid)
        {
            const glm::dvec3 module = glm::transpose(R) * (world - cpos);
            if (std::hypot(module.x, module.y) < 1.0e-6)
                plan.x = nearPlan.x;
        }
        return plan;
    }
    glm::dvec3 velocityToStrip(const glm::dvec3& plan, const glm::dvec3& vWorld) const
    {
        glm::dvec3 local = glm::transpose(axes(plan)) * (vWorld - cvel - glm::cross(omega, toWorld(plan) - cpos));
        if (bend.valid && bend.radius - plan.y > 1.0)
            local.x *= bend.radius / (bend.radius - plan.y);
        return local;
    }

    // THE PLAN'S DOMAIN (audit A2): a ring's strip coordinates describe the deck and what stands on it - not the spin
    // axis (the axial docks, the zero-g industry, the sync gate): there r -> 0, the angle is undefined and "height over
    // the deck" is ~R. A module point belongs to the plan when it is within `reach` of the deck along the radius (the
    // tallest building + room; default half the radius); a flat city: always.
    bool moduleOnDeck(const glm::dvec3& module, double reachMeters = -1.0) const
    {
        if (!bend.valid)
            return true;
        const double reach = reachMeters > 0.0 ? reachMeters : 0.5 * bend.radius;
        const double r = std::hypot(module.x, module.y);
        return r > bend.radius - reach && r < bend.radius + reach;
    }
    bool worldOnDeck(const glm::dvec3& world, double reachMeters = -1.0) const
    {
        return moduleOnDeck(glm::transpose(R) * (world - cpos), reachMeters);
    }

    // the along-city difference b - a (A4: the ring is periodic - the shortest way round, never across the seam the long
    // way; a flat city: the plain difference)
    double alongDelta(double a, double b) const
    {
        double d = b - a;
        if (bend.valid && bend.length > 0.0)
        {
            d = std::fmod(d, bend.length);
            if (d > 0.5 * bend.length)
                d -= bend.length;
            else if (d < -0.5 * bend.length)
                d += bend.length;
        }
        return d;
    }
    // a plan point with its along coordinate moved by whole turns to lie nearest `reference` (a ring: the ship just past
    // the seam from its pad is 25 m away, not L - 25); positions beyond [x0, x0 + L) are valid - the bend is periodic
    glm::dvec3 unwrapNear(const glm::dvec3& plan, const glm::dvec3& reference) const
    {
        return {reference.x + alongDelta(reference.x, plan.x), plan.y, plan.z};
    }
    // the plan difference b - a with the along part periodic
    glm::dvec3 planDelta(const glm::dvec3& a, const glm::dvec3& b) const
    {
        return {alongDelta(a.x, b.x), b.y - a.y, b.z - a.z};
    }
};
} // namespace game::navigation
