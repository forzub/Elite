// DEV offline harness: replay one dumped Newtonian autopilot start.
//
//   EliteNewtonianReplay <case file> <model label> <seconds> [delay ticks]
//
// The case file is written by the game (ELITE_AUTOPILOT_CASE_DUMP). The
// replay flies it with the PRODUCTION route autopilot
// (NewtonianClientRouteAutopilot) against the PRODUCTION authoritative ship
// step (ShipCore, the same call order as GameSimulation::update for a
// HubTactical ship with a valid travel frame). Control reaches the ship
// after `delay ticks` server ticks (default 1, as in the game: the client
// samples a snapshot, the server consumes the command on a later tick).
// The pilot's DEV traces (ELITE_NEWTONIAN_POLICY_TRACE / _EVENT_TRACE)
// work unchanged, so the same analysis scripts apply.
// ELITE_REPLAY_STATE_TRACE=<tsv path> writes the authoritative ship state
// and the applied control every tick (hull/volume checks of matrix.py).
//
// ELITE_REPLAY_DAMAGE="T MODULE TARGET01[;T MODULE TARGET01...]" injects a
// real equipment failure (ObjectModuleRuntime::injectEquipmentFailure, the
// server DEV fault path) into the authoritative ship at T seconds. From the
// first injection on, the autopilot receives the LIVE control params (server
// capability + published signed authority, as the game client does);
// before it, the dumped case physics (healthy runs stay bit-identical).
//
// Not modelled: replanning, other ships, gravity context, network jitter.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <sstream>
#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/DedicatedClientRouteAutopilotLaws.h"
#include "src/game/navigation/diagnostics/AutopilotCaseFile.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/world/WorldParams.h"

namespace
{

using game::navigation::autopilot::AutopilotAgentState;
using game::navigation::autopilot::NewtonianClientRouteAutopilot;
using Model = game::ship::plant::PhysicalControlModelId;

bool modelFromLabel(const std::string& label, Model& out)
{
    if (label == "Newtonian-I") out = Model::NewtonianI;
    else if (label == "Newtonian-Y") out = Model::NewtonianY;
    else if (label == "Newtonian-X") out = Model::NewtonianX;
    else return false;
    return true;
}

AutopilotAgentState agentOf(const ShipTransform& transform)
{
    AutopilotAgentState out;
    out.positionMapMeters = transform.motion.localPositionMeters;
    out.velocityMapMetersPerSecond = transform.motion.localVelocityMps;
    out.forwardMap = glm::normalize(glm::dvec3(transform.forward()));
    out.rightMap = glm::normalize(glm::dvec3(transform.right()));
    out.upMap = glm::normalize(glm::dvec3(transform.up()));
    out.pitchRateRadPerSec = transform.pitchRate;
    out.yawRateRadPerSec = transform.yawRate;
    out.rollRateRadPerSec = transform.rollRate;
    return out;
}

glm::mat4 orientationOf(const AutopilotAgentState& agent)
{
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(glm::vec3(glm::normalize(agent.rightMap)), 0.0f);
    m[1] = glm::vec4(glm::vec3(glm::normalize(agent.upMap)), 0.0f);
    m[2] = glm::vec4(glm::vec3(-glm::normalize(agent.forwardMap)), 0.0f);
    return m;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr,
            "usage: EliteNewtonianReplay <case> <Newtonian-I|Y|X> <seconds> "
            "[delay ticks]\n");
        return 2;
    }
    const std::string label = argv[2];
    const double seconds = std::atof(argv[3]);
    const int delayTicks = argc > 4 ? std::max(0, std::atoi(argv[4])) : 1;

    Model model {};
    if (!modelFromLabel(label, model))
    {
        std::fprintf(stderr, "unknown model %s\n", label.c_str());
        return 2;
    }

    game::navigation::diagnostics::AutopilotCase replayCase;
    if (!game::navigation::diagnostics::readAutopilotCase(argv[1], replayCase))
    {
        std::fprintf(stderr, "cannot read case %s\n", argv[1]);
        return 2;
    }

    // DEV gimbal sweep: the server-side N1 passport reads
    // ELITE_DEV_N1_GIMBAL_DEG itself; apply the same travel to the client
    // physics the autopilot was compiled with.
    if (const char* deg = std::getenv("ELITE_DEV_N1_GIMBAL_DEG"))
    {
        const float rad =
            static_cast<float>(std::atof(deg) * 3.14159265358979323846 / 180.0);
        replayCase.physics.mainGimbalPitchLimitRad = rad;
        replayCase.physics.mainGimbalYawLimitRad = rad;
    }

    // The production descriptor reads the model label at first use.
    static std::string modelEnv = "ELITE_SHIP_PHYSICAL_TEST_MODEL=" + label;
    putenv(modelEnv.data());

    game::ship::core::ShipCore core;
    core.init(
        ShipRole::Player,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        glm::vec3(0.0f),
        ShipInitData {},
        orientationOf(replayCase.agent)
    );
    if (!core.selectPhysicalConstructionForDiagnostic(model))
    {
        std::fprintf(stderr, "model override failed\n");
        return 1;
    }

    auto& tr = core.transform();
    tr.orientation = orientationOf(replayCase.agent);
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.systemId = 1;
    tr.motion.travelFrame.systemId = 1;
    tr.motion.travelFrame.frameId = "newtonian-replay";
    tr.motion.travelFrame.originMeters = glm::dvec3(0.0);
    tr.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    tr.motion.travelFrame.valid = true;
    tr.motion.localPositionMeters = replayCase.agent.positionMapMeters;
    tr.motion.localVelocityMps = replayCase.agent.velocityMapMetersPerSecond;
    tr.motion.worldVelocityMps = replayCase.agent.velocityMapMetersPerSecond;
    tr.motion.referenceVelocityMps = glm::dvec3(0.0);
    tr.referenceVelocityMetersPerSecond = glm::dvec3(0.0);
    tr.setWorldPositionMeters(replayCase.agent.positionMapMeters);
    tr.pitchRate = static_cast<float>(replayCase.agent.pitchRateRadPerSec);
    tr.yawRate = static_cast<float>(replayCase.agent.yawRateRadPerSec);
    tr.rollRate = static_cast<float>(replayCase.agent.rollRateRadPerSec);
    // ELITE_REPLAY_GAS01=<fill>: the RCS gas tank's fill at the start.
    if (const char* gas = std::getenv("ELITE_REPLAY_GAS01"))
        if (*gas)
            tr.motion.manoeuvreGasPressure01 = std::atof(gas);
    core.refreshRuntimeState();

    NewtonianClientRouteAutopilot::State autopilot;
    std::string failure;
    if (!NewtonianClientRouteAutopilot::start(
            autopilot,
            replayCase.plan,
            agentOf(tr),
            replayCase.physics,
            replayCase.acceptedAtUniverseTimeSeconds,
            replayCase.requestSerial,
            replayCase.trackingToleranceMeters,
            replayCase.routeUpReference,
            replayCase.holdAtTerminal,
            &failure))
    {
        std::fprintf(stderr, "autopilot start failed: %s\n", failure.c_str());
        return 1;
    }

    constexpr double Dt = 0.02;
    const WorldParams world {};
    std::deque<ShipControlState> pending;
    double maxCrossTrack = 0.0;
    double completedAt = -1.0;
    double progress = 0.0;
    double settledAt = -1.0;
    const int ticks = static_cast<int>(seconds / Dt);
    std::uint64_t controlTick = 0;
    const char* stopStill = std::getenv("ELITE_REPLAY_STOP_STILL_S");
    const double stopStillSeconds = stopStill ? std::atof(stopStill) : 0.0;
    double stillSince = -1.0;
    std::FILE* stateTrace = nullptr;
    if (const char* path = std::getenv("ELITE_REPLAY_STATE_TRACE"))
        if (*path && (stateTrace = std::fopen(path, "w")))
            std::fputs("t\tpx\tpy\tpz\tvx\tvy\tvz\tfx\tfy\tfz\tux\tuy\tuz\t"
                "wp\twy\twr\tmain\trcs_f\trcs_s\trcs_l\n", stateTrace);
    // ELITE_REPLAY_KICK="T VX VY VZ": an external velocity change (m/s, map
    // frame) at T seconds - a disturbance for the safety monitor checks.
    double kickAt = -1.0;
    glm::dvec3 kick {0.0};
    if (const char* k = std::getenv("ELITE_REPLAY_KICK"))
        if (std::sscanf(k, "%lf %lf %lf %lf", &kickAt, &kick.x, &kick.y, &kick.z) != 4)
            kickAt = -1.0;
    struct DamageEvent { double at; std::string module; double target01; };
    std::vector<DamageEvent> damage;
    if (const char* d = std::getenv("ELITE_REPLAY_DAMAGE"))
    {
        std::stringstream all(d);
        std::string item;
        while (std::getline(all, item, ';'))
        {
            std::stringstream one(item);
            DamageEvent e {};
            if (one >> e.at >> e.module >> e.target01)
                damage.push_back(e);
        }
    }
    // ELITE_REPLAY_LIVE (non-empty): live params from the first tick (check
    // that the live path matches the dumped case on a healthy ship).
    const char* liveEnv = std::getenv("ELITE_REPLAY_LIVE");
    bool liveParams = liveEnv && *liveEnv;
    ShipParams params = replayCase.physics;
    for (int tick = 0; tick < ticks; ++tick)
    {
        const double time = replayCase.acceptedAtUniverseTimeSeconds + tick * Dt;
        for (auto& e : damage)
            if (e.at >= 0.0 && tick * Dt >= e.at)
            {
                const bool applied =
                    core.moduleRuntime().injectEquipmentFailure(e.module, e.target01);
                core.refreshRuntimeState();
                std::fprintf(stderr, "[REPLAY] damage t=%.2f module=%s target=%.2f %s\n",
                    tick * Dt, e.module.c_str(), e.target01,
                    applied ? "applied" : "NOT applied");
                e.at = -1.0;
                liveParams = true;
            }
        if (liveParams)
            params = game::ship::telemetry::applyAuthorityToControlParams(
                core.effectivePhysics(), core.controlAuthorityTelemetry());
        if (kickAt >= 0.0 && tick * Dt >= kickAt)
        {
            tr.motion.localVelocityMps += kick;
            tr.motion.worldVelocityMps += kick;
            kickAt = -1.0;
        }
        if (stateTrace)
        {
            const auto a = agentOf(tr);
            const auto& c = core.control();
            std::fprintf(stateTrace,
                "%.2f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.6f\t%.6f\t%.6f\t"
                "%.6f\t%.6f\t%.6f\t%.5f\t%.5f\t%.5f\t%.4f\t%.4f\t%.4f\t%.4f\n",
                tick * Dt, a.positionMapMeters.x, a.positionMapMeters.y,
                a.positionMapMeters.z, a.velocityMapMetersPerSecond.x,
                a.velocityMapMetersPerSecond.y, a.velocityMapMetersPerSecond.z,
                a.forwardMap.x, a.forwardMap.y, a.forwardMap.z, a.upMap.x,
                a.upMap.y, a.upMap.z, a.pitchRateRadPerSec, a.yawRateRadPerSec,
                a.rollRateRadPerSec, c.targetSpeedRate, c.forwardInput,
                c.strafeInput, c.liftInput);
        }
        const auto output = NewtonianClientRouteAutopilot::update(
            autopilot, agentOf(tr), params, time, Dt);
        if (!output.valid)
        {
            std::fprintf(stderr, "autopilot output invalid at t=%.2f (%s)\n",
                tick * Dt,
                output.invalidReason ? output.invalidReason : "follower");
            break;
        }
        maxCrossTrack = std::max(maxCrossTrack, output.crossTrackErrorMeters);
        progress = output.exactRemainingRouteMeters;
        // With holdAtTerminal (as in the game) success is a settled
        // terminal hold: at the route end, slower than the precision-stop
        // entry speed.
        if (settledAt < 0.0 && output.terminalHold &&
            glm::length(tr.motion.localVelocityMps) <= 0.5)
            settledAt = tick * Dt;
        // ELITE_REPLAY_STOP_STILL_S=N ends the replay after N seconds of a
        // still terminal hold (matrix runs; DONE needs 1 s of it).
        if (stopStillSeconds > 0.0)
        {
            const bool still = output.terminalHold &&
                glm::length(tr.motion.localVelocityMps) <= 0.05 &&
                std::abs(tr.pitchRate) + std::abs(tr.yawRate) +
                        std::abs(tr.rollRate) <= 0.01;
            stillSince = still ? (stillSince < 0.0 ? tick * Dt : stillSince) : -1.0;
            if (stillSince >= 0.0 && tick * Dt - stillSince >= stopStillSeconds)
            {
                completedAt = tick * Dt;
                break;
            }
        }
        if (output.complete)
        {
            completedAt = tick * Dt;
            break;
        }

        ShipControlState control = output.control;
        control.controlTick = ++controlTick;
        pending.push_back(control);
        if (static_cast<int>(pending.size()) > delayTicks)
        {
            core.control() = pending.front();
            pending.pop_front();
        }

        // GameSimulation::update order for a HubTactical ship with a valid
        // travel frame and per-tick motion-control cadence.
        const float fdt = static_cast<float>(Dt);
        core.refreshRuntimeState();
        core.updateMotionControl(fdt, world);
        core.updateSystems(fdt);
        core.updateTranslationControl(fdt);
        core.propagateMotionOrientation(fdt);
        (void)core.propagateHubTacticalTranslation(Dt);
    }

    if (stateTrace)
        std::fclose(stateTrace);
    std::printf(
        "[REPLAY] model=%s delay_ticks=%d flown_s=%.1f complete=%s "
        "remaining_m=%.0f settled_s=%.1f final_speed_mps=%.2f "
        "max_cross_track_m=%.1f tol_m=%.1f\n",
        label.c_str(), delayTicks,
        completedAt >= 0.0 ? completedAt : seconds,
        completedAt >= 0.0 ? "yes" : "no", progress, settledAt,
        glm::length(tr.motion.localVelocityMps), maxCrossTrack,
        replayCase.trackingToleranceMeters);
    return 0;
}
