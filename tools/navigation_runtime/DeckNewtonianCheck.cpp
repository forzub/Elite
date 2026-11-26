// DEV offline check: a NEWTONIAN ship over a TURNING DECK (a ring city's rim, DeckFrame.h) on the production ShipCore
// server step (the GameSimulation::update order, as EliteNewtonianReplay). Audit 2026-11-26 MEDIUM-HIGH: the deck's
// charge (omega x v to follow the rim; co-rotating also omega x u) was added AFTER the 24 nozzles fired - a free force,
// no gas, no watts. Now it is nozzle work (ShipCorePropulsion): this check flies a ship at rest on the deck with the
// stick released and reports how well its nozzles hold it there and what the gas did, against the same ship off the
// deck.
//
//   EliteDeckNewtonianCheck [seconds] [radius m] [period s]   (defaults: 20 s, the Earth ring: 1209.578 m, 69.781 s)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/DeckFrame.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/world/WorldParams.h"

namespace
{
using Model = game::ship::plant::PhysicalControlModelId;

struct Result
{
    double radialDriftM = 0.0;     // the largest |r - R| over the run
    double gasEnd01 = 1.0;
    double meanNozzleMps2 = 0.0;   // the RCS's executed acceleration, mean magnitude
    bool ok = false;
};

Result fly(const char* label, Model model, bool deck, bool coRotating, double seconds, double radius, double period)
{
    Result out;
    static std::string env;
    env = std::string("ELITE_SHIP_PHYSICAL_TEST_MODEL=") + label;
    putenv(env.data());
    game::ship::core::ShipCore core;
    core.init(ShipRole::Player, EliteCobraMk1::EliteCobraMk1Descriptor(), glm::vec3(0.0f), ShipInitData {}, glm::mat4(1.0f));
    if (!core.selectPhysicalConstructionForDiagnostic(model))
        return out;
    const double omega = 2.0 * 3.14159265358979323846 / period;
    // the ring's axis along the frame's Z through the origin; the ship on the rim at (0, -R, 0), its top toward the axis
    const glm::dvec3 axisPoint(0.0);
    const glm::dvec3 w(0.0, 0.0, omega);
    const glm::dvec3 start(0.0, -radius, 0.0);
    auto& tr = core.transform();
    tr.orientation = glm::mat4(1.0f);   // right +X, up +Y (toward the axis), nose -Z (along the axis)
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.systemId = 1;
    tr.motion.travelFrame.systemId = 1;
    tr.motion.travelFrame.frameId = "deck-newtonian-check";
    tr.motion.travelFrame.originMeters = glm::dvec3(0.0);
    tr.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    tr.motion.travelFrame.valid = true;
    tr.motion.localPositionMeters = start;
    const glm::dvec3 v0 = deck ? glm::cross(w, start - axisPoint) : glm::dvec3(0.0);
    tr.motion.localVelocityMps = v0;
    tr.motion.worldVelocityMps = v0;
    tr.motion.referenceVelocityMps = glm::dvec3(0.0);
    tr.setWorldPositionMeters(start);
    // the ship turns with the deck (as it would after the sync at the gate)
    tr.rollRate = deck ? static_cast<float>(-omega) : 0.0f;   // roll about forward (-Z): the deck's +Z turn
    core.refreshRuntimeState();
    constexpr double Dt = 0.02;
    const WorldParams world {};
    const int ticks = static_cast<int>(seconds / Dt);
    double nozzleSum = 0.0;
    std::uint64_t controlTick = 0;
    for (int tick = 0; tick < ticks; ++tick)
    {
        auto& m = tr.motion;
        m.deckActive = deck;
        m.deckCoRotating = deck && coRotating;
        m.deckAxisPointLocal = axisPoint;
        m.deckOmegaLocal = deck ? w : glm::dvec3(0.0);
        ShipControlState control {};
        control.controlTick = ++controlTick;
        control.deckCoRotating = deck && coRotating;
        core.control() = control;
        const float fdt = static_cast<float>(Dt);
        core.refreshRuntimeState();
        core.updateMotionControl(fdt, world);
        core.updateSystems(fdt);
        core.updateTranslationControl(fdt);
        core.propagateMotionOrientation(fdt);
        (void)core.propagateHubTacticalTranslation(Dt);
        const glm::dvec3 p = tr.motion.localPositionMeters - axisPoint;
        out.radialDriftM = std::max(out.radialDriftM, std::abs(std::hypot(p.x, p.y) - radius));
        nozzleSum += glm::length(tr.motion.manoeuvreAccelerationMps2);
        if (std::getenv("ELITE_DECK_TRACE") && deck && tick % 25 == 0)
        {
            const glm::dvec3 toAxis = -glm::normalize(glm::dvec3(p.x, p.y, 0.0));
            std::printf("  t %.1f r %.2f nozzles toward the axis %.3f (|a| %.3f) assisted %.3f engine %.3f gas %.3f u %.2f\n",
                        tick * Dt, std::hypot(p.x, p.y), glm::dot(tr.motion.manoeuvreAccelerationMps2, toAxis),
                        glm::length(tr.motion.manoeuvreAccelerationMps2),
                        glm::dot(tr.motion.assistedStabilizationAccelerationMps2, toAxis),
                        glm::dot(tr.motion.engineAccelerationMps2, toAxis), tr.motion.manoeuvreGasPressure01,
                        glm::length(tr.motion.localVelocityMps - game::navigation::deckVelocityLocal(tr.motion)));
        }
    }
    out.gasEnd01 = tr.motion.manoeuvreGasPressure01;
    out.meanNozzleMps2 = ticks > 0 ? nozzleSum / ticks : 0.0;
    out.ok = true;
    return out;
}
} // namespace

int main(int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof(argv[1]) : 20.0;
    const double radius = argc > 2 ? std::atof(argv[2]) : 1209.578;
    const double period = argc > 3 ? std::atof(argv[3]) : 69.781;
    const double omega = 2.0 * 3.14159265358979323846 / period;
    std::printf("deck: R %.1f m, period %.2f s, omega^2 R = %.2f m/s2; %.0f s, stick released\n", radius, period,
                omega * omega * radius, seconds);
    const struct { const char* label; Model model; } models[] = {
        {"Newtonian-I", Model::NewtonianI}, {"Newtonian-Y", Model::NewtonianY}, {"Newtonian-X", Model::NewtonianX}};
    int failures = 0;
    for (const auto& md : models)
    {
        const Result off = fly(md.label, md.model, false, false, seconds, radius, period);
        const Result on = fly(md.label, md.model, true, true, seconds, radius, period);
        if (!off.ok || !on.ok)
        {
            std::printf("%s: model override failed\n", md.label);
            ++failures;
            continue;
        }
        std::printf("%s: off the deck  - drift %.2f m, gas %.3f, nozzles %.3f m/s2\n", md.label, off.radialDriftM, off.gasEnd01,
                    off.meanNozzleMps2);
        std::printf("%s: on the deck   - drift %.2f m, gas %.3f, nozzles %.3f m/s2 (the deck asks %.2f)\n", md.label,
                    on.radialDriftM, on.gasEnd01, on.meanNozzleMps2, omega * omega * radius);
        // the deck's charge must be NOZZLE work: the nozzles fire about the deck's ask, and the gas pays for it
        if (!(on.meanNozzleMps2 > 0.5 * std::min(omega * omega * radius, 1.0)))
        {
            std::printf("  FAIL: the nozzles did not fire for the deck\n");
            ++failures;
        }
        if (!(on.gasEnd01 < off.gasEnd01 - 1.0e-3))
        {
            std::printf("  FAIL: the deck's charge cost no gas\n");
            ++failures;
        }
    }
    std::printf("deck newtonian check: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
