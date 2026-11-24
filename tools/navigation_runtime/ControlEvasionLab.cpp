// EliteControlEvasionLab - evasion by the PILOT'S CONTROLS judged by the REAL ship physics (author 2026-11-23: «надо
// задавать сети на выход - ручное управление кораблем ... это будет уже проблема физического слоя»).
// A control program = the pilot's inputs (strafe / lift / forward RCS, pitch / yaw, the speed command) held for
// holdSeconds, then hands off (the flight law keeps flying); its trajectory comes from the same chain the game runs
// (SharedShipPhysics::integrate -> DynamicMotionSystem). The trajectory depends on the ship only, not on the threats:
// fly every program once per ship state, then judge it against any threats cheaply.
//   EliteControlEvasionLab bench [assisted|newtonian]   the cost of the physics + what each program does
//   EliteControlEvasionLab compare <scenes> [seed]      the point-mass reflex (as in the game) vs the best program on the
//                                                       real physics, both judged on the real trajectories
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/avoidance/ControlProgramEvasion.h"
#include "src/game/navigation/avoidance/MultiThreatEvasion.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

using namespace game::navigation::avoidance;

namespace
{
struct Program
{
    float strafe = 0, lift = 0, forward = 0, pitch = 0, yaw = 0, speed = 0;
    std::string name() const
    {
        char b[96];
        std::snprintf(b, sizeof b, "s%+.0f l%+.0f f%+.0f p%+.0f y%+.0f v%+.0f", strafe, lift, forward, pitch, yaw, speed);
        return b;
    }
};

// 27 RCS translations + 4 turns x 3 speed commands + 2 speed commands alone = 41 programs
std::vector<Program> programs()
{
    std::vector<Program> out;
    for (int s = -1; s <= 1; ++s)
        for (int l = -1; l <= 1; ++l)
            for (int f = -1; f <= 1; ++f)
                out.push_back({float(s), float(l), float(f), 0, 0, 0});
    const float turns[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& t : turns)
        for (int v = -1; v <= 1; ++v)
            out.push_back({0, 0, 0, t[0], t[1], float(v)});
    out.push_back({0, 0, 0, 0, 0, 1});
    out.push_back({0, 0, 0, 0, 0, -1});
    return out;
}

ShipParams testShip()
{
    // the navigation scenario tests' nominal hull (tests/navigation_runtime/NavigationScenarioRunnerTests.cpp)
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;
    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;
    p.strafeAccel = 20.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    return p;
}

struct Flight
{
    std::vector<glm::dvec3> pos, vel;   // every sampleSeconds, the initial body frame (+X right, +Y up, -Z nose)
};

constexpr double SampleSeconds = 0.5;

Flight fly(const ShipParams& ship, game::navigation::LocalFlightControlLaw law, double speed0, const Program& pr,
           double holdSeconds, double horizonSeconds, double dt)
{
    ShipTransform tr {};
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.systemId = 1;
    tr.motion.localControlLaw = law;
    tr.motion.localPositionMeters = glm::dvec3(0.0);
    tr.motion.localVelocityMps = glm::dvec3(0.0, 0.0, -speed0);
    tr.motion.targetForwardSpeedMps = speed0;
    tr.setWorldPositionMeters(glm::dvec3(0.0));
    tr.orientation = glm::mat4(1.0f);   // right +X, up +Y, nose -Z
    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "control-evasion-lab";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;
    tr.motion.travelFrame = frame;
    WorldParams world {};
    Flight out;
    const int perSample = std::max(1, static_cast<int>(std::lround(SampleSeconds / dt)));
    const int steps = static_cast<int>(std::ceil(horizonSeconds / dt));
    for (int i = 0; i <= steps; ++i)
    {
        if (i % perSample == 0)
        {
            out.pos.push_back(tr.motion.localPositionMeters);
            out.vel.push_back(tr.motion.localVelocityMps);
        }
        ShipControlState c {};
        if (i * dt < holdSeconds)
        {
            c.strafeInput = pr.strafe;
            c.liftInput = pr.lift;
            c.forwardInput = pr.forward;
            c.pitchInput = pr.pitch;
            c.yawInput = pr.yaw;
            c.targetSpeedRate = pr.speed;
            c.rcsTranslationPerAxisAuthority = true;
        }
        SharedShipPhysics::integrate(tr, ship, c, world, static_cast<float>(dt));
        game::navigation::DynamicMotionSystem::applyLocalFrameInput(tr.motion, frame, ship, static_cast<float>(dt),
                                                                     c.targetSpeedRate, c.cruiseActive, c.forwardInput,
                                                                     c.liftInput, c.strafeInput, tr.forward(), tr.right(),
                                                                     tr.up());
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(tr.motion, tr.worldPosition, frame, ship, dt);
    }
    return out;
}

// a flight judged against straight-moving threats (positions / velocities relative to the ship's INITIAL state, i.e.
// the threat's absolute motion in the initial body frame = its relative motion + our initial velocity)
EvasionChoice judge(const Flight& f, const EvasionScene& sc, const glm::dvec3& ourV0)
{
    EvasionChoice out;
    out.minMarginMeters = 1.0e30;
    for (std::size_t k = 0; k < f.pos.size(); ++k)
    {
        const double t = k * SampleSeconds;
        for (const auto& th : sc.threats)
        {
            const glm::dvec3 vAbs = th.velocity + ourV0;
            const glm::dvec3 q = th.position + vAbs * t;
            const double dist = glm::length(q - f.pos[k]);
            const double margin = dist - th.radiusMeters - sc.caps.radiusMeters - sc.marginMeters;
            if (margin < out.minMarginMeters)
            {
                out.minMarginMeters = margin;
                out.timeOfMinSeconds = t;
            }
            if (!out.contact && dist < th.radiusMeters + sc.caps.radiusMeters)
            {
                out.contact = true;
                const glm::dvec3 n = dist > 1.0e-6 ? (f.pos[k] - q) / dist : glm::dvec3(0.0, 0.0, 1.0);
                const glm::dvec3 rel = f.vel[k] - vAbs;
                out.impactNormalSpeedMps = std::max(0.0, -glm::dot(rel, n));
                out.impactSpeedMps = glm::length(rel);
            }
        }
    }
    out.score = out.contact ? -1000.0 - 5.0 * out.impactNormalSpeedMps : std::min(out.minMarginMeters, 30.0);
    return out;
}

glm::dvec3 randomUnit(std::mt19937& rng)
{
    std::normal_distribution<double> n(0.0, 1.0);
    glm::dvec3 v(n(rng), n(rng), n(rng));
    const double m = glm::length(v);
    return m > 1.0e-9 ? v / m : glm::dvec3(0.0, 0.0, 1.0);
}

// threats aimed at the ship flying straight on at speed0 (relative tracks, as the radar gives them)
EvasionScene aimedScene(std::mt19937& rng)
{
    std::uniform_real_distribution<double> u(0.0, 1.0);
    EvasionScene sc;
    sc.forward = glm::dvec3(0.0, 0.0, -1.0);
    sc.caps.radiusMeters = 25.0;
    sc.horizonSeconds = 12.0;
    sc.holdSeconds = 4.0;
    sc.marginMeters = 25.0;
    const int n = 1 + static_cast<int>(u(rng) * 3.0);
    for (int i = 0; i < n; ++i)
    {
        EvasionThreatTrack t;
        t.radiusMeters = 15.0 + 45.0 * u(rng);
        const double speed = 10.0 + 140.0 * u(rng);
        const glm::dvec3 vdir = randomUnit(rng);
        t.velocity = vdir * speed;
        const double tc = 3.0 + 9.0 * u(rng);
        glm::dvec3 miss = randomUnit(rng);
        miss -= vdir * glm::dot(miss, vdir);
        miss = glm::length(miss) > 1.0e-6 ? glm::normalize(miss) : glm::dvec3(1.0, 0.0, 0.0);
        t.position = miss * (60.0 * u(rng)) - t.velocity * tc;
        sc.threats.push_back(t);
    }
    return sc;
}
} // namespace

int main(int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "bench";
    const ShipParams ship = testShip();
    const auto law = (argc > 2 && std::string(argv[2]) == "newtonian") ? game::navigation::LocalFlightControlLaw::Newtonian
                                                                        : game::navigation::LocalFlightControlLaw::Assisted;
    const auto progs = programs();
    const double dt = 0.05, hold = 4.0, horizon = 12.0, speed0 = 60.0;
    if (mode == "bench")
    {
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<Flight> flights;
        for (const auto& p : progs)
            flights.push_back(fly(ship, law, speed0, p, hold, horizon, dt));
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        const int steps = static_cast<int>(progs.size()) * static_cast<int>(std::ceil(horizon / dt));
        std::printf("[ControlLab] %zu programs x %.0f s at dt %.2f: %.2f ms (%.2f us per physics step)\n", progs.size(),
                    horizon, dt, ms, 1000.0 * ms / steps);
        const glm::dvec3 straight = flights[13].pos.back();   // s0 l0 f0: hands off
        for (std::size_t i = 0; i < progs.size(); ++i)
        {
            const glm::dvec3 d4 = flights[i].pos[static_cast<std::size_t>(hold / SampleSeconds)] -
                                  flights[13].pos[static_cast<std::size_t>(hold / SampleSeconds)];
            const glm::dvec3 d12 = flights[i].pos.back() - straight;
            std::printf("  %-26s off the straight line at 4 s (%6.1f %6.1f %6.1f)  at 12 s (%7.1f %7.1f %7.1f)\n",
                        progs[i].name().c_str(), d4.x, d4.y, d4.z, d12.x, d12.y, d12.z);
        }
        return 0;
    }
    if (mode == "freerot")
    {
        // the main drive in Assisted free rotation (Backspace): the speed key held 4 s, nose fixed -> the push along it
        for (const bool free : {false, true})
        {
            ShipTransform tr {};
            tr.motion.mode = game::navigation::MotionMode::HubTactical;
            tr.motion.systemId = 1;
            tr.motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
            tr.motion.assistedFreeRotation = free;
            tr.orientation = glm::mat4(1.0f);
            game::navigation::KinematicFrame frame;
            frame.systemId = 1;
            frame.frameId = "freerot";
            frame.localToWorldBasis = glm::dmat3(1.0);
            frame.valid = true;
            tr.motion.travelFrame = frame;
            WorldParams world {};
            for (int i = 0; i < 80; ++i)
            {
                ShipControlState c {};
                c.targetSpeedRate = 1.0f;
                SharedShipPhysics::integrate(tr, ship, c, world, 0.05f);
                game::navigation::DynamicMotionSystem::applyLocalFrameInput(tr.motion, frame, ship, 0.05f, c.targetSpeedRate,
                                                                             false, 0.0f, 0.0f, 0.0f, tr.forward(), tr.right(),
                                                                             tr.up());
                game::navigation::DynamicMotionSystem::updateLocalFrameMotion(tr.motion, tr.worldPosition, frame, ship, 0.05);
            }
            const glm::dvec3 v = tr.motion.localVelocityMps;
            std::printf("[ControlLab] Assisted%s, speed key 4 s: velocity (%.1f %.1f %.1f) m/s, free rotation still %d\n",
                        free ? " FREE ROTATION" : "", v.x, v.y, v.z, tr.motion.assistedFreeRotation ? 1 : 0);
        }
        return 0;
    }
    if (mode == "compare")
    {
        const int scenes = argc > 3 ? std::atoi(argv[3]) : 2000;
        std::mt19937 rng(argc > 4 ? static_cast<unsigned>(std::atoi(argv[4])) : 3u);
        // the programs' flights: once (the ship state is the same in every scene here)
        std::vector<Flight> flights;
        for (const auto& p : progs)
            flights.push_back(fly(ship, law, speed0, p, hold, horizon, dt));
        const glm::dvec3 v0(0.0, 0.0, -speed0);
        // the RCS acceleration the point-mass reflex assumes: measured from the strafe program over the first second
        const double lateral = std::abs(flights[22].pos[2].x - flights[13].pos[2].x) * 2.0;   // s+1: x = a t^2 / 2, t = 1
        std::printf("[ControlLab] measured RCS lateral ~%.2f m/s2 (point-mass model input)\n", lateral);
        int safeNone = 0, safeReflex = 0, safeBest = 0, touchReflex = 0, touchBest = 0, solvable = 0;
        double impReflex = 0.0, impBest = 0.0;
        int nImpReflex = 0, nImpBest = 0;
        for (int s = 0; s < scenes; ++s)
        {
            EvasionScene sc = aimedScene(rng);
            sc.caps.lateralAccelMps2 = std::max(0.05, lateral);
            sc.caps.mainAccelMps2 = sc.caps.lateralAccelMps2;
            // the game's reflex: the point-mass teacher -> RCS inputs (EvasionReflex.h), flown on the real physics
            const EvasionChoice pm = teachEvasion(sc);
            const glm::dvec3 a = pm.accel / sc.caps.lateralAccelMps2;
            Program rp {static_cast<float>(std::clamp(a.x, -1.0, 1.0)), static_cast<float>(std::clamp(a.y, -1.0, 1.0)),
                        static_cast<float>(std::clamp(-a.z, -1.0, 1.0)), 0, 0, 0};
            const Flight rf = fly(ship, law, speed0, rp, hold, horizon, dt);
            const EvasionChoice reflex = judge(rf, sc, v0);
            EvasionChoice best = judge(flights[13], sc, v0);
            const EvasionChoice none = best;
            for (const auto& f : flights)
            {
                const EvasionChoice c = judge(f, sc, v0);
                if (c.score > best.score)
                    best = c;
            }
            safeNone += !none.contact && none.minMarginMeters >= 0.0;
            safeReflex += !reflex.contact && reflex.minMarginMeters >= 0.0;
            safeBest += !best.contact && best.minMarginMeters >= 0.0;
            solvable += !best.contact;
            touchReflex += reflex.contact;
            touchBest += best.contact;
            if (best.contact)
            {
                impBest += best.impactNormalSpeedMps;
                ++nImpBest;
                if (reflex.contact)
                {
                    impReflex += reflex.impactNormalSpeedMps;
                    ++nImpReflex;
                }
            }
        }
        std::printf("[ControlLab] %d aimed scenes (%s): no touch possible in %d\n", scenes,
                    law == game::navigation::LocalFlightControlLaw::Assisted ? "Assisted" : "Newtonian", solvable);
        std::printf("  safe (no touch, margin >= 0): hands off %.1f %%, the game's reflex %.1f %%, the best program %.1f %%\n",
                    100.0 * safeNone / scenes, 100.0 * safeReflex / scenes, 100.0 * safeBest / scenes);
        std::printf("  touches: the reflex %d, the best program %d; normal impact where a touch is unavoidable: reflex %.1f m/s, best %.1f m/s\n",
                    touchReflex, touchBest, nImpReflex ? impReflex / nImpReflex : 0.0, nImpBest ? impBest / nImpBest : 0.0);
        return 0;
    }
    if (mode == "oracle")
    {
        // THE OLD vs THE NEW JUDGE against an INDEPENDENT ORACLE (audit 8e3c49007 NAV-1: this lab used the same sampled
        // judge as the game, so it could not see a touch falling between samples): every program flown once on the real
        // physics; the oracle = the same physics stored every 2 ms against the straight threats; each judge's touch /
        // no-touch per (scene, program) against the oracle's - false negatives (a real touch called clear) and false
        // positives (a clear pass called a touch), and the CPU cost of judging. Threat speeds 10 .. <max> m/s.
        const int scenes = argc > 3 ? std::atoi(argv[3]) : 400;
        const double maxSpeed = argc > 4 ? std::atof(argv[4]) : 500.0;
        std::mt19937 rng(11u);
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        game::navigation::avoidance::ControlShipState st;
        st.params = ship;
        st.law = law;
        st.velocityBody = glm::dvec3(0.0, 0.0, -speed0);
        st.targetForwardSpeedMps = speed0;
        // two oracles: SAMPLING (the predicted flight itself - dt steps, every step stored, each step's straight motion
        // checked every 1 ms: the judge's own question, exactly) and PHYSICS (the flight integrated at 2 ms: how far the
        // dt-step prediction is from finer physics - a model error the judge cannot remove)
        std::vector<game::navigation::avoidance::ControlFlight> coarse, fine, steps;
        std::vector<Flight> oldFlights;
        for (const auto& p : progs)
        {
            game::navigation::avoidance::ControlProgram cp;
            cp.strafe = p.strafe;
            cp.lift = p.lift;
            cp.forward = p.forward;
            cp.pitch = p.pitch;
            cp.yaw = p.yaw;
            cp.speed = p.speed;
            coarse.push_back(game::navigation::avoidance::flyControlProgram(st, cp, hold, horizon, dt));
            fine.push_back(game::navigation::avoidance::flyControlProgram(st, cp, hold, horizon, 0.002, 0.002));
            steps.push_back(game::navigation::avoidance::flyControlProgram(st, cp, hold, horizon, dt, dt));
            oldFlights.push_back(fly(ship, law, speed0, p, hold, horizon, dt));
        }
        const glm::dvec3 v0(0.0, 0.0, -speed0);
        long long pairs = 0, oracleTouches = 0, oldFn = 0, oldFp = 0, newFn = 0, newFp = 0;
        long long sampTouches = 0, sampOldFn = 0, sampNewFn = 0, sampNewFp = 0;
        double oldMs = 0.0, newMs = 0.0;
        for (int s = 0; s < scenes; ++s)
        {
            EvasionScene sc = aimedScene(rng);
            for (auto& t : sc.threats)
            {
                // re-speed the threats over the requested range (the aimed geometry kept: same closest-approach time)
                const double sp = 10.0 + (maxSpeed - 10.0) * u01(rng);
                const double old = glm::length(t.velocity);
                const double tc = old > 1.0e-6 ? -glm::dot(t.position, t.velocity) / (old * old) : 6.0;
                const glm::dvec3 miss = t.position + t.velocity * tc;
                t.velocity = t.velocity / std::max(1.0e-6, old) * sp;
                t.position = miss - t.velocity * tc;
                t.radiusMeters = 1.0 + 60.0 * u01(rng);   // debris .. large hulls
            }
            sc.marginMeters = 0.0;   // physical touches only: the judges against the oracle on the same question
            for (std::size_t p = 0; p < progs.size(); ++p)
            {
                // the oracle
                bool touch = false;
                for (std::size_t k = 0; k < fine[p].pos.size() && !touch; ++k)
                {
                    const double t = static_cast<double>(k) * fine[p].sampleSeconds;
                    for (const auto& th : sc.threats)
                        if (glm::length(th.position + (th.velocity + v0) * t - fine[p].pos[k]) <
                            th.radiusMeters + sc.caps.radiusMeters)
                            touch = true;
                }
                // the sampling oracle: the predicted flight's own steps, straight within each, every 1 ms
                bool stepTouch = false;
                const auto& sf = steps[p];
                for (std::size_t k = 0; k + 1 < sf.pos.size() && !stepTouch; ++k)
                    for (int m = 0; m <= 100 && !stepTouch; ++m)
                    {
                        const double a = m / 100.0;
                        const double t = (static_cast<double>(k) + a) * sf.sampleSeconds;
                        const glm::dvec3 ours = sf.pos[k] + (sf.pos[k + 1] - sf.pos[k]) * a;
                        for (const auto& th : sc.threats)
                            if (glm::length(th.position + (th.velocity + v0) * t - ours) < th.radiusMeters + sc.caps.radiusMeters)
                                stepTouch = true;
                    }
                const auto t0 = std::chrono::steady_clock::now();
                const EvasionChoice oldC = judge(oldFlights[p], sc, v0);
                const auto t1 = std::chrono::steady_clock::now();
                const EvasionChoice newC = game::navigation::avoidance::judgeControlFlight(coarse[p], sc, v0, 30.0);
                const auto t2 = std::chrono::steady_clock::now();
                oldMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
                newMs += std::chrono::duration<double, std::milli>(t2 - t1).count();
                ++pairs;
                oracleTouches += touch;
                oldFn += touch && !oldC.contact;
                oldFp += !touch && oldC.contact;
                newFn += touch && !newC.contact;
                newFp += !touch && newC.contact;
                sampTouches += stepTouch;
                sampOldFn += stepTouch && !oldC.contact;
                sampNewFn += stepTouch && !newC.contact;
                sampNewFp += !stepTouch && newC.contact;
            }
        }
        std::printf("[ControlLab] oracle: %d scenes x %zu programs = %lld flights, threats 10..%.0f m/s, %lld real touches\n",
                    scenes, progs.size(), pairs, maxSpeed, oracleTouches);
        std::printf("  OLD sampled judge:     missed touches %lld (%.1f %%), false touches %lld; %.2f us a flight\n", oldFn,
                    oracleTouches ? 100.0 * oldFn / oracleTouches : 0.0, oldFp, 1000.0 * oldMs / std::max(1LL, pairs));
        std::printf("  NEW continuous judge:  missed touches %lld (%.1f %%), false touches %lld; %.2f us a flight\n", newFn,
                    oracleTouches ? 100.0 * newFn / oracleTouches : 0.0, newFp, 1000.0 * newMs / std::max(1LL, pairs));
        std::printf("  SAMPLING oracle (the predicted flight, 1 ms inside its steps): %lld touches; old judge missed %lld, NEW missed %lld, new false %lld\n",
                    sampTouches, sampOldFn, sampNewFn, sampNewFp);
        return sampNewFn == 0 ? 0 : 1;
    }
    std::fprintf(stderr, "usage: EliteControlEvasionLab bench [assisted|newtonian] | compare [assisted|newtonian] <scenes> [seed] | oracle [assisted|newtonian] <scenes> <max threat m/s>\n");
    return 2;
}
