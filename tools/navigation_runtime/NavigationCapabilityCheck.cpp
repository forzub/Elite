// DEV check: the factual navigation capability and the route request never
// invent authority (step C). Production code only:
//  - NavigationControlCapability / applyRouteRequestCapability on a healthy
//    Newtonian ship (production ShipCore + its published control-authority
//    telemetry), then with main = 0, one RCS direction = 0, one angular
//    direction = 0;
//  - production ShipCore: propulsion reactors off and restarting -> the
//    Newtonian control-authority telemetry is withdrawn (the client waits,
//    nothing is planned), and returns after the real 10 s restart.
//
//   EliteNavigationCapabilityCheck        -> prints PASS/FAIL lines, exit 0/1

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/NavigationVehicleProfileAdapters.h"
#include "src/game/navigation/RouteRequestCapability.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"
#include "src/game/navigation/autopilot/NewtonianTurnModel.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"

namespace
{

using namespace game::navigation;
int failures = 0;

void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", what.c_str());
    if (!condition)
        ++failures;
}

// The client's Newtonian capability: server capability + published
// signed control authority (SpaceState's path).
ShipParams clientParams(const game::ship::core::ShipCore& core, bool& telemetryValid)
{
    const auto telemetry = core.controlAuthorityTelemetry();
    telemetryValid = telemetry.valid;
    return game::ship::telemetry::applyAuthorityToControlParams(
        core.capabilities().effectivePhysics, telemetry);
}

RouteRequestCapabilityResult request(const ShipParams& p, planner::RoutePlanRequest& r)
{
    VehicleGuidanceEnvelope envelope;
    envelope.lengthMeters = 22.2;
    envelope.widthMeters = 26.0;
    envelope.heightMeters = 5.0;
    envelope.valid = true;
    return applyRouteRequestCapability(
        r, makeNavigationVehicleProfile(p, envelope, LocalFlightControlLaw::Newtonian),
        LocalFlightControlLaw::Newtonian);
}

} // namespace

int main()
{
    static std::string modelEnv = "ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I";
    putenv(modelEnv.data());
    game::ship::core::ShipCore core;
    core.init(ShipRole::Player, EliteCobraMk1::EliteCobraMk1Descriptor(),
        glm::vec3(0.0f), ShipInitData {}, glm::mat4(1.0f));
    if (!core.selectPhysicalConstructionForDiagnostic(
            game::ship::plant::PhysicalControlModelId::NewtonianI))
    {
        std::printf("FAIL model override\n");
        return 1;
    }
    core.refreshRuntimeState();

    bool telemetryValid = false;
    const ShipParams healthy = clientParams(core, telemetryValid);
    check(telemetryValid, "healthy: Newtonian control-authority telemetry published");
    const auto facts = makeNavigationControlCapability(healthy, LocalFlightControlLaw::Newtonian);
    std::printf("     main %.2f m/s2, gimbal %.2f deg, RCS +x %.3f -x %.3f +y %.3f -y %.3f "
                "fwd %.3f aft %.3f m/s2, angular %.3f rad/s2, speed %.0f m/s\n",
        facts.mainForwardMps2, facts.gimbalYawRad * 57.29578,
        facts.rcsDirectionMps2(0, 1), facts.rcsDirectionMps2(0, -1),
        facts.rcsDirectionMps2(1, 1), facts.rcsDirectionMps2(1, -1),
        facts.rcsDirectionMps2(2, 1), facts.rcsDirectionMps2(2, -1),
        facts.angularScalarRadPerSec2, facts.controlledSpeedLimitMps);
    check(facts.valid && facts.mainForwardMps2 > 0.0 && facts.rcsPerAxisKnown &&
              facts.angularScalarRadPerSec2 > 0.0 && facts.canManoeuvre(),
        "healthy: facts present (main, signed RCS, attitude)");
    check(facts.mainForwardMps2 == game::ship::forwardMainAccelerationLimitMps2(healthy) &&
              facts.rcsScalarMps2 == game::ship::manoeuvreAccelerationLimitMps2(healthy) &&
              facts.angularScalarRadPerSec2 ==
                  game::ship::angularAccelerationLimitRadPerSec2(healthy),
        "healthy: facts equal the ShipParams adapter (no second interpretation)");
    {
        planner::RoutePlanRequest r;
        const auto result = request(healthy, r);
        check(result.ok && r.acceleratingMps2 > 1.0 && r.brakingMps2 > 1.0 &&
                  r.lateralMps2 > 1.0,
            "healthy: route request accepted");
    }

    {
        ShipParams p = healthy;
        p.forwardMainEngineAvailable = false;
        p.reverseMainEngineAvailable = false;
        planner::RoutePlanRequest r;
        const auto result = request(p, r);
        check(!result.ok &&
                  result.failureCode == planner::RoutePlanFailureCode::PropulsionInsufficient &&
                  r.acceleratingMps2 == 0.0 && r.brakingMps2 == 0.0,
            "main = 0: request refused, acceleration/braking stay 0 (no 0.1), reason: " +
                result.reason);
    }

    {
        // One RCS direction lost: the server telemetry shape with -X = 0.
        auto t = core.controlAuthorityTelemetry();
        t.negative[0] = 0.0;
        const ShipParams p = game::ship::telemetry::applyAuthorityToControlParams(
            core.capabilities().effectivePhysics, t);
        const auto f = makeNavigationControlCapability(p, LocalFlightControlLaw::Newtonian);
        check(f.rcsDirectionMps2(0, -1) == 0.0 && f.rcsDirectionMps2(0, 1) > 0.0 &&
                  f.rcsDirectionMps2(1, 1) > 0.0 && f.rcsScalarMps2 == 0.0,
            "RCS -X = 0: that direction is 0, the others stay, the scalar bound is 0");
    }

    {
        auto t = core.controlAuthorityTelemetry();
        t.negative[4] = 0.0; // yaw, one sign
        const ShipParams p = game::ship::telemetry::applyAuthorityToControlParams(
            core.capabilities().effectivePhysics, t);
        const auto f = makeNavigationControlCapability(p, LocalFlightControlLaw::Newtonian);
        planner::RoutePlanRequest r;
        const auto result = request(p, r);
        check(f.angularNegativeRadPerSec2.y == 0.0 && f.angularPositiveRadPerSec2.y > 0.0 &&
                  f.angularScalarRadPerSec2 == 0.0 && !f.canManoeuvre() && !result.ok &&
                  r.maxAngularAccelerationRadPerSecond2 == 0.0,
            "yaw- = 0: signed angular kept, scalar 0, request refused (" + result.reason + ")");
    }

    // ER-5 power is part of the published capability (same law as the executor).
    {
        double mainShare = 0.0, rcsShare = 0.0;
        core.newtonianPropulsionPowerShares(mainShare, rcsShare);
        std::printf("     healthy power shares: main %.4f rcs %.4f (ER-5 available %.0f MW)\n", mainShare, rcsShare,
                    core.engineReactorAvailableMW(0));
        check(mainShare == 1.0 && rcsShare == 1.0, "healthy: ER-5 power does not limit the published capability");
        const double healthyMain = core.controlAuthorityTelemetry().workingForwardAccelerationMps2;
        auto* reactor = core.moduleRuntime().findModule(
            EliteCobraMk1::EliteCobraMk1Descriptor().powerThermalInstallation.engineReactorModuleIds[0]);
        const double savedK = reactor->engineReactorOperating.temperatureK;
        // Overheated ER-5: halfway between its safe and critical temperatures
        // (installed passport) -> half power.
        const auto& desc = EliteCobraMk1::EliteCobraMk1Descriptor();
        const auto type = std::find_if(desc.modules.begin(), desc.modules.end(), [&](const auto& m) {
            return m.moduleId == desc.powerThermalInstallation.engineReactorModuleIds[0];
        });
        reactor->engineReactorOperating.temperatureK =
            0.5 * (type->engineReactor->safeTemperatureK + type->engineReactor->criticalTemperatureK);
        core.newtonianPropulsionPowerShares(mainShare, rcsShare);
        const double hotMain = core.controlAuthorityTelemetry().workingForwardAccelerationMps2;
        std::printf("     overheated ER-5 (%.0f K): available %.0f MW, main share %.4f, published main %.2f -> %.2f m/s2\n",
                    reactor->engineReactorOperating.temperatureK, core.engineReactorAvailableMW(0), mainShare,
                    healthyMain, hotMain);
        check(mainShare > 0.0 && mainShare < 1.0 && hotMain < healthyMain && std::abs(hotMain - healthyMain * mainShare) < 1.0e-9,
              "overheated ER-5: published main authority = installed x ER-5 power share");
        reactor->engineReactorOperating.temperatureK = savedK;
    }

    {
        // Damaged ER-5 (70 % equipment damage): what the generators can power.
        const auto id = EliteCobraMk1::EliteCobraMk1Descriptor().powerThermalInstallation.engineReactorModuleIds[0];
        core.moduleRuntime().injectEquipmentFailure(id, 0.7);
        core.refreshRuntimeState();
        double mainShare = 0.0, rcsShare = 0.0;
        core.newtonianPropulsionPowerShares(mainShare, rcsShare);
        std::printf("     ER-5 70%% damaged: available %.0f MW, main share %.4f, rcs share %.4f\n",
                    core.engineReactorAvailableMW(0), mainShare, rcsShare);
        bool valid = false;
        const ShipParams damaged = clientParams(core, valid);
        std::printf("     ER-5 70%%: telemetry working forward %.2f, client capability main %.2f (valid %d)%s",
                    core.controlAuthorityTelemetry().workingForwardAccelerationMps2,
                    makeNavigationControlCapability(damaged, LocalFlightControlLaw::Newtonian).mainForwardMps2,
                    valid ? 1 : 0, "\n");
        check(valid && std::abs(makeNavigationControlCapability(damaged, LocalFlightControlLaw::Newtonian).mainForwardMps2 -
                                core.controlAuthorityTelemetry().workingForwardAccelerationMps2) < 1.0e-4,
              "ER-5 70 %: client main capability == published live main (was the cached installed value)");
    }

    // Combined two-axis RCS demand (measurement; per-axis limits are single-axis).
    {
        std::printf("     RCS pair feasibility, healthy: %.3f\n", core.newtonianRcsPairFeasibility01());
        const auto& ids = game::ship::presets::cobra::newtonianRcsNozzles();
        core.moduleRuntime().markModuleDestroyed(std::string(ids[0].nozzleId), 0.0f);
        core.refreshRuntimeState();
        std::printf("     RCS pair feasibility, nozzle %s destroyed: %.3f\n", ids[0].nozzleId,
                    core.newtonianRcsPairFeasibility01());
        bool valid = false;
        const auto facts =
            makeNavigationControlCapability(clientParams(core, valid), LocalFlightControlLaw::Newtonian);
        const double planned = game::navigation::autopilot::newtonianPreferredTurnAccelerationRadPerSec2(facts);
        check(valid && std::abs(facts.rcsCombinedShare01 - core.newtonianRcsPairFeasibility01()) < 1.0e-9 &&
                  planned <= facts.rcsCombinedShare01 * facts.angularScalarRadPerSec2 + 1.0e-12,
              "combined RCS contract: published share reaches the client; planned turns use at most that share");
    }

    // Production ShipCore: reactors off, restarting, back.
    core.togglePropulsionReactors();
    core.refreshRuntimeState();
    clientParams(core, telemetryValid);
    check(!telemetryValid, "reactors off: control-authority telemetry withdrawn (client waits)");
    core.togglePropulsionReactors();
    for (int i = 0; i < 250; ++i) // 5 s of the 10 s restart
    {
        core.refreshRuntimeState();
        core.updateSystems(0.02f);
    }
    clientParams(core, telemetryValid);
    check(!telemetryValid, "reactors restarting (5 s of 10 s): telemetry still withdrawn");
    for (int i = 0; i < 300; ++i)
    {
        core.refreshRuntimeState();
        core.updateSystems(0.02f);
    }
    const ShipParams restarted = clientParams(core, telemetryValid);
    check(telemetryValid &&
              makeNavigationControlCapability(restarted, LocalFlightControlLaw::Newtonian)
                  .canManoeuvre(),
        "reactors restarted (11 s): telemetry and capability back");

    // Assisted A3: one aft main engine lost. The server must keep publishing
    // its control authority and one working thrust axis.
    {
        game::ship::core::ShipCore assisted;
        assisted.init(ShipRole::Player, EliteCobraMk1::EliteCobraMk1Descriptor(), glm::vec3(0.0f), ShipInitData {},
                      glm::mat4(1.0f));
        assisted.selectPhysicalConstructionForDiagnostic(game::ship::plant::PhysicalControlModelId::AssistedI);
        assisted.refreshRuntimeState();
        for (int i = 0; i < 50; ++i)
            assisted.updateSystems(0.02f);
        const auto healthyTelemetry = assisted.controlAuthorityTelemetry();
        assisted.moduleRuntime().injectEquipmentFailure("ship_engine_L", 0.95);
        assisted.refreshRuntimeState();
        for (int i = 0; i < 50; ++i)
            assisted.updateSystems(0.02f);
        const auto telemetry = assisted.controlAuthorityTelemetry();
        const auto caps = assisted.capabilities();
        std::printf("     Assisted engine L lost: telemetry valid %d (healthy %d), working forward %.2f -> %.2f, "
                    "axis (%.3f %.3f %.3f) emergency %d\n",
                    telemetry.valid ? 1 : 0, healthyTelemetry.valid ? 1 : 0,
                    healthyTelemetry.workingForwardAccelerationMps2, telemetry.workingForwardAccelerationMps2,
                    caps.effectivePhysics.assistedWorkingThrustAxisBody.x, caps.effectivePhysics.assistedWorkingThrustAxisBody.y,
                    caps.effectivePhysics.assistedWorkingThrustAxisBody.z, caps.effectivePhysics.assistedWorkingThrustAxisEmergency ? 1 : 0);
        check(healthyTelemetry.valid && telemetry.valid && telemetry.workingForwardAccelerationMps2 > 0.0,
              "Assisted engine L lost: authority telemetry stays published, forward authority > 0");
    }

    std::printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
