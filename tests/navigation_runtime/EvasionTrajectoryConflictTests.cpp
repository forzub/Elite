// The avoidance layer's collision / safety prediction against an INDEPENDENT oracle (audit 8e3c49007 NAV-1 / NAV-2 /
// NAV-3; the reviewer's pass: «тест должен быть независимым от алгоритма»). The oracle steps the true motion finely
// (every step shorter than a tenth of the smallest radius at that speed) with its own code; the predictor must never
// miss a touch the oracle sees, wherever the touch falls between the old 0.5 s samples and whatever the speed.
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/damage/HitVolumeBounds.h"
#include "src/game/navigation/avoidance/ControlProgramEvasion.h"
#include "src/game/navigation/avoidance/EvasionReflex.h"
#include "src/game/navigation/avoidance/TrajectoryConflict.h"
#include "src/world/WorldParams.h"

namespace av = game::navigation::avoidance;

namespace
{
int g_failures = 0;
int g_checks = 0;
void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// the oracle: two linear motions, the least centre distance found by fine stepping
double oracleMinDistance(const glm::dvec3& p0, const glm::dvec3& u, const glm::dvec3& q0, const glm::dvec3& v, double T,
                         double smallestRadius)
{
    const double rel = std::max(1.0e-9, glm::length(v - u));
    const double step = std::min(T / 10.0, 0.1 * smallestRadius / rel);
    double best = 1.0e300;
    for (double t = 0.0; t <= T + 1.0e-12; t += step)
        best = std::min(best, glm::length((q0 + v * t) - (p0 + u * t)));
    return best;
}

// the old judge: distances only at the 0.5 s samples
bool oldSampledContact(const glm::dvec3& p0, const glm::dvec3& u, const glm::dvec3& q0, const glm::dvec3& v, double T,
                       double R)
{
    for (double t = 0.0; t <= T + 1.0e-12; t += 0.5)
        if (glm::length((q0 + v * t) - (p0 + u * t)) < R)
            return true;
    return false;
}

// the predictor over [0, T] in 0.5 s intervals (straight motion of ours: chord bound 0)
av::ConflictResult predict(const glm::dvec3& p0, const glm::dvec3& u, const glm::dvec3& q0, const glm::dvec3& v, double T,
                           double ownR, double theirR)
{
    av::ConflictResult c;
    av::ConflictBody body;
    body.physicalRadiusMeters = theirR;
    av::SafetyEnvelopePolicy policy;
    for (double t0 = 0.0; t0 < T - 1.0e-12; t0 += 0.5)
        av::accumulateConflictInterval(c, t0, 0.5, p0 + u * t0, p0 + u * (t0 + 0.5), 0.0, q0 + v * t0, v, ownR, body, policy);
    return c;
}

// 1. CONTINUOUS CROSSINGS at 100 .. 1000 m/s and 0.1 c: head-on, perpendicular, glancing, near miss, exact hit; debris,
//    small / wide / large hulls; the touch placed strictly BETWEEN the old 0.5 s samples
void testContinuousCrossings()
{
    const double speeds[] = {100.0, 200.0, 300.0, 500.0, 1000.0, 0.1 * 299792458.0};
    struct Size
    {
        const char* name;
        double r;
    };
    const Size sizes[] = {{"debris 2 m", 1.7}, {"small ship", 11.0}, {"Cobra (wide)", 17.3}, {"large transport", 114.6}};
    const double ownR = 17.3;
    int betweenSamples = 0, oldMissed = 0;
    for (const double speed : speeds)
        for (const auto& size : sizes)
            for (int geometry = 0; geometry < 3; ++geometry)        // head-on, perpendicular, oblique
                for (const double impact : {0.0, 0.5, 0.95, 1.05}) // exact hit .. glancing .. near miss (x the touch radius)
                {
                    const double R = ownR + size.r;
                    // the touch time placed between samples: 3.27 s (not a multiple of 0.5)
                    const double tTouch = 3.27;
                    const glm::dvec3 u(0.0, 0.0, -speed * 0.5);   // ours
                    const glm::dvec3 dir = geometry == 0 ? glm::dvec3(0, 0, 1) : geometry == 1 ? glm::dvec3(1, 0, 0)
                                                                                                    : glm::normalize(glm::dvec3(1, 0.3, 1));
                    const glm::dvec3 v = u + dir * speed;          // theirs: relative speed `speed` along dir
                    // their closest approach to us at tTouch with miss `impact * R` perpendicular to dir
                    const glm::dvec3 side = glm::normalize(glm::cross(dir, glm::dvec3(0.13, 1, 0.07)));
                    const glm::dvec3 p0(0.0);
                    const glm::dvec3 q0 = p0 + u * tTouch + side * (impact * R) - v * tTouch;
                    const double T = 12.0;
                    const double oracle = oracleMinDistance(p0, u, q0, v, T, std::min(ownR, size.r));
                    const bool oracleHit = oracle < R;
                    const auto c = predict(p0, u, q0, v, T, ownR, size.r);
                    char what[256];
                    std::snprintf(what, sizeof(what), "crossing %.0f m/s %s geom %d impact %.2f: oracle %.2f m vs predicted %.2f m",
                                  speed, size.name, geometry, impact, oracle, c.closestApproachMeters);
                    // no miss of a real touch; the closest approach exact (to the oracle's step)
                    check(!oracleHit || c.physicalCollisionPredicted, what);
                    check(oracleHit || !c.physicalCollisionPredicted, what);
                    check(std::abs(c.closestApproachMeters - oracle) <= 0.02 * R + 1.0e-6, what);
                    if (oracleHit)
                    {
                        ++betweenSamples;
                        if (!oldSampledContact(p0, u, q0, v, T, R))
                            ++oldMissed;
                        check(std::abs(c.firstContactSeconds - tTouch) <= R / speed + 1.0e-6, what);
                    }
                }
    std::printf("crossings: %d touches between the samples, the old 0.5 s judge missed %d of them, the new one 0\n",
                betweenSamples, oldMissed);
    check(oldMissed > 0, "the scenario set must contain touches the old sampled judge misses");
}

// 2. RANDOM linear encounters at any speed: never a missed touch
void testRandomEncounters()
{
    std::mt19937_64 g(7);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    int touches = 0;
    for (int i = 0; i < 4000; ++i)
    {
        const double speed = std::pow(10.0, 1.0 + 4.0 * (0.5 + 0.5 * uni(g)));   // 10 m/s .. 100 km/s
        const glm::dvec3 u(uni(g) * speed, uni(g) * speed, uni(g) * speed);
        const glm::dvec3 v(uni(g) * speed, uni(g) * speed, uni(g) * speed);
        const double ownR = 5.0 + 60.0 * (0.5 + 0.5 * uni(g)), theirR = 1.0 + 100.0 * (0.5 + 0.5 * uni(g));
        const double T = 12.0;
        const double tc = 6.0 * (1.0 + uni(g));
        const glm::dvec3 miss = glm::dvec3(uni(g), uni(g), uni(g)) * (ownR + theirR) * 1.5;
        const glm::dvec3 q0 = u * tc + miss - v * tc;
        const double oracle = oracleMinDistance(glm::dvec3(0.0), u, q0, v, T, std::min(ownR, theirR));
        const auto c = predict(glm::dvec3(0.0), u, q0, v, T, ownR, theirR);
        if (oracle < ownR + theirR)
            ++touches;
        check(!(oracle < ownR + theirR) || c.physicalCollisionPredicted, "random encounter: a real touch missed");
    }
    std::printf("random encounters: 4000, %d touches, none missed\n", touches);
}

// 3. OUR NON-LINEAR flight (a dodge program on the real physics) against a fast crosser: the judge's chord bound keeps
//    it conservative - the oracle integrates the same program finely and measures the true least distance
void testNonLinearFlight()
{
    av::ControlShipState ship;
    ship.params = ShipParams {};
    ship.params.maxPitchRate = ship.params.maxYawRate = ship.params.maxRollRate = 1.0f;
    ship.params.angularAccel = 2.0f;
    ship.params.angularDamping = 2.0f;
    ship.params.maxCombatSpeed = ship.params.maxCruiseSpeed = 300.0f;
    ship.params.throttleAccel = 30.0f;
    ship.params.forwardMainEngineAccelerationMps2 = 30.0f;
    ship.law = game::navigation::LocalFlightControlLaw::Assisted;
    ship.velocityBody = glm::dvec3(0.0, 0.0, -120.0);
    ship.targetForwardSpeedMps = 120.0;
    const auto& programs = av::controlPrograms();
    int misses = 0, judged = 0;
    for (std::size_t p = 0; p < programs.size(); p += 5)
    {
        const av::ControlFlight coarse = av::flyControlProgram(ship, programs[p], 2.0, 12.0, 0.1);
        // the oracle's flight: the same physics stepped and STORED every 1 ms
        const av::ControlFlight fine = av::flyControlProgram(ship, programs[p], 2.0, 12.0, 0.001, 0.001);
        // a 300 m/s crosser aimed at our hands-off position at 4.27 s
        av::EvasionScene sc;
        sc.caps.radiusMeters = 17.3;
        sc.marginMeters = 0.0;
        av::EvasionThreatTrack th;
        th.radiusMeters = 11.0;
        const glm::dvec3 vAbs(300.0, 0.0, 0.0);
        const glm::dvec3 hit = glm::dvec3(0.0, 0.0, -120.0 * 4.27);
        th.velocity = vAbs - ship.velocityBody;
        th.position = hit - vAbs * 4.27;
        sc.threats.push_back(th);
        const auto choice = av::judgeControlFlight(coarse, sc, ship.velocityBody, 30.0);
        // the oracle: the fine flight vs the straight threat, 0.5 ms steps
        double least = 1.0e300;
        for (std::size_t k = 0; k < fine.pos.size(); ++k)
        {
            const double t = static_cast<double>(k) * fine.sampleSeconds;
            least = std::min(least, glm::length((th.position + vAbs * t) - fine.pos[k]));
        }
        ++judged;
        const bool oracleTouch = least < sc.caps.radiusMeters + th.radiusMeters;
        if (oracleTouch && !choice.contact)
            ++misses;
    }
    std::printf("non-linear flights: %d programs judged against the fine oracle, %d touches missed\n", judged, misses);
    check(misses == 0, "a dodge program's real touch missed by the continuous judge");
}

// 4. GEOMETRY: the size comes from the hull's real volumes / dimensions, not 30 m
void testGeometry()
{
    struct Volume
    {
        glm::vec3 center;
        glm::vec3 halfSize;
        bool destroyed = false;
    };
    // a Cobra-like hull: 26 m wide, 22 m long, 5 m tall (wider than long)
    const std::vector<Volume> cobra = {{{0, 0, 0}, {6, 2.5, 11}, false}, {{-9.5f, 0, 3}, {3.5f, 2, 5}, false}, {{9.5f, 0, 3}, {3.5f, 2, 5}, false}};
    const double rc = game::damage::hitVolumesBoundingRadius(cobra);
    check(rc > 0.5 * 22.0, "a wide hull's radius must exceed half its length");
    check(rc >= 13.0, "a wide hull's radius covers its half width");
    // a 220 m transport
    const double rt = game::damage::dimensionsBoundingRadius(220.0, 60.0, 40.0);
    check(rt > 100.0, "a large transport is not a 30 m sphere");
    // a torn-off wing shrinks the body
    auto torn = cobra;
    torn[1].destroyed = torn[2].destroyed = true;
    check(game::damage::hitVolumesBoundingRadius(torn) < rc, "a destroyed volume no longer counts");
    // the reflex takes the track's size, not the kind's guess
    game::radar::RadarScanReport scan;
    scan.measuredAtUniverseTimeSeconds = 100.0;
    game::radar::RadarTrackReport big;
    big.relativePositionMeters = glm::dvec3(0, 0, -500);
    big.relativeVelocityMps = glm::dvec3(0, 0, 50);
    big.kind = game::radar::RadarTrackKindShip;
    big.boundingRadiusMeters = 114.6;
    scan.tracks.push_back(big);
    av::EvasionReflex reflex;
    const auto sc = reflex.buildScene(scan, 100.0, 17.3, nullptr);
    check(sc.threats.size() == 1 && std::abs(sc.threats[0].radiusMeters - 114.6) < 1.0e-9, "the track's real size used");
    std::printf("geometry: wide hull %.1f m (length/2 = 11), transport %.1f m, track size %.1f m\n", rc, rt,
                sc.threats.empty() ? -1.0 : sc.threats[0].radiusMeters);
}

// 5. RADAR AGE + OUR ROTATION: a scan 0.58 s old while we yaw at 0.3 rad/s - the scene must be in our CURRENT frame
void testScanRotation()
{
    const glm::vec3 rates(0.0f, 0.3f, 0.0f);
    const double age = 0.58;
    // the truth: our orientation after `age` by the physics' own propagation (fine steps)
    ShipTransform turn {};
    turn.orientation = glm::mat4(1.0f);
    turn.pitchRate = rates.x;
    turn.yawRate = rates.y;
    turn.rollRate = rates.z;
    for (int i = 0; i < 580; ++i)
        SharedShipPhysics::propagateOrientation(turn, 0.001f);
    const glm::dmat3 bodyNowInMeasured(glm::mat3(turn.orientation));
    // a target at rest in the measured (inertial over the age) frame, 300 m ahead-right; we do not translate
    const glm::dvec3 target(80.0, 0.0, -300.0);
    const glm::dvec3 expectedNow = glm::transpose(bodyNowInMeasured) * target;
    game::radar::RadarScanReport scan;
    scan.measuredAtUniverseTimeSeconds = 10.0;
    game::radar::RadarTrackReport t;
    t.relativePositionMeters = target;
    t.relativeVelocityMps = glm::dvec3(0.0);
    t.kind = game::radar::RadarTrackKindShip;
    t.boundingRadiusMeters = 11.0;
    scan.tracks.push_back(t);
    av::EvasionReflex reflex;
    const auto sc = reflex.buildScene(scan, 10.0 + age, 17.3, &rates);
    const auto unrotated = reflex.buildScene(scan, 10.0 + age, 17.3, nullptr);
    const double err = sc.threats.empty() ? 1.0e9 : glm::length(sc.threats[0].position - expectedNow);
    const double oldErr = unrotated.threats.empty() ? 1.0e9 : glm::length(unrotated.threats[0].position - expectedNow);
    std::printf("scan rotation: 0.3 rad/s x 0.58 s at 310 m: error %.3f m (without the rotation %.1f m)\n", err, oldErr);
    check(err < 0.5, "the old scan turned into the current body frame");
    check(oldErr > 20.0, "the scenario must matter (the untransformed error is large)");
}

// 6. UNCERTAINTY: the same nominal pass - exact track: clear; uncertain track: a safety conflict
void testUncertainty()
{
    const glm::dvec3 p0(0.0), u(0.0, 0.0, -100.0);
    const glm::dvec3 v(0.0, 0.0, 100.0);
    const double R = 17.3 + 11.0;
    const glm::dvec3 q0 = glm::dvec3(R + 20.0, 0.0, 0.0) - (v - u) * 5.0;   // passes 20 m outside the hulls at 5 s
    av::SafetyEnvelopePolicy policy;
    const auto run = [&](double pu, double vu) {
        av::ConflictBody body;
        body.physicalRadiusMeters = 11.0;
        body.positionUncertaintyMeters = pu;
        body.velocityUncertaintyMps = vu;
        body.ageSeconds = 0.3;
        av::ConflictResult c;
        for (double t0 = 0.0; t0 < 12.0; t0 += 0.5)
            av::accumulateConflictInterval(c, t0, 0.5, p0 + u * t0, p0 + u * (t0 + 0.5), 0.0, q0 + v * t0, v, 17.3, body,
                                           policy);
        return c;
    };
    const auto exact = run(0.0, 0.0);
    const auto uncertain = run(10.0, 3.0);
    std::printf("uncertainty: exact track safety clearance %.1f m, uncertain %.1f m\n", exact.safetyClearanceMeters,
                uncertain.safetyClearanceMeters);
    check(!exact.physicalCollisionPredicted && !uncertain.physicalCollisionPredicted, "no physical touch either way");
    check(!exact.safetyConflictPredicted, "an exact track passing 20 m outside is safe (base gap 15 m)");
    check(uncertain.safetyConflictPredicted, "the uncertain track on the same nominal path is a safety conflict");
}
} // namespace

av::ControlShipState episodeShip()
{
    av::ControlShipState ship;
    ship.params = ShipParams {};
    ship.params.maxPitchRate = ship.params.maxYawRate = ship.params.maxRollRate = 1.0f;
    ship.params.angularAccel = 2.0f;
    ship.params.angularDamping = 2.0f;
    ship.params.maxCombatSpeed = ship.params.maxCruiseSpeed = 300.0f;
    ship.params.throttleAccel = 30.0f;
    ship.params.forwardMainEngineAccelerationMps2 = 30.0f;
    ship.params.strafeAccel = 20.0f;
    ship.params.maxStrafeSpeed = 80.0f;
    ship.law = game::navigation::LocalFlightControlLaw::Assisted;
    ship.velocityBody = glm::dvec3(0.0, 0.0, -60.0);
    ship.targetForwardSpeedMps = 60.0;
    return ship;
}

// NAV-7: a dodge never outlives its episode (a route that ended mid-dodge, a new phase / request) or a clock going back
void testEpisode()
{
    game::radar::RadarScanReport scan;
    game::radar::RadarTrackReport head;
    head.relativePositionMeters = glm::dvec3(0.0, 0.0, -220.0);   // dead ahead, closing 120 m/s: touch in ~1.4 s
    head.relativeVelocityMps = glm::dvec3(0.0, 0.0, 60.0);
    head.kind = game::radar::RadarTrackKindShip;
    head.boundingRadiusMeters = 15.0;
    scan.tracks.push_back(head);
    const av::ControlShipState ship = episodeShip();

    av::EvasionReflex reflex;
    scan.measuredAtUniverseTimeSeconds = 10.0;
    reflex.bindEpisode(100, 10.0);
    const bool dodging = reflex.updatePrograms(scan, 10.0, ship, 15.0).active;
    check(dodging, "episode: a head-on contact 1.4 s out makes a dodge");
    // the route ends: the next consumer (another phase of the same request) binds its own episode at once
    reflex.bindEpisode(101, 10.05);
    check(!reflex.command().active, "episode: a new episode starts without the old dodge (NAV-7 phantom dodge)");
    // the same episode keeps it (no flicker inside one run)
    reflex.bindEpisode(100, 10.1);
    scan.measuredAtUniverseTimeSeconds = 10.1;
    reflex.updatePrograms(scan, 10.1, ship, 15.0);
    reflex.bindEpisode(100, 10.12);
    check(reflex.command().active, "episode: the same episode keeps its dodge");
    // the clock goes back (a rebind / a reset universe time): clean
    reflex.bindEpisode(100, 5.0);
    check(!reflex.command().active, "episode: the clock going back resets the reflex");
    std::printf("episode: dodge %d, cleared on a new episode and on a clock rollback\n", dodging ? 1 : 0);
}

// NAV-5: the urgency window from the physics' own lateral authority when the caller gives none
void testUrgencyFromPhysics()
{
    av::EvasionReflex reflex;
    const double weak = reflex.urgencyFor(2.0, 17.3, 30.0);
    const double strong = reflex.urgencyFor(73.5, 17.3, 30.0);
    check(weak > strong + 2.0, "urgency: a weak RCS needs a wider window than a strong one");
    game::radar::RadarScanReport scan;
    game::radar::RadarTrackReport t;
    t.relativePositionMeters = glm::dvec3(0.0, 0.0, -900.0);   // touch in ~7.5 s
    t.relativeVelocityMps = glm::dvec3(0.0, 0.0, 60.0);
    t.kind = game::radar::RadarTrackKindShip;
    t.boundingRadiusMeters = 30.0;
    scan.tracks.push_back(t);
    scan.measuredAtUniverseTimeSeconds = 1.0;
    av::ControlShipState ship = episodeShip();
    ship.params.strafeAccel = 2.0f;   // a weak RCS: the window must reach ~8 s with no lateral figure passed
    reflex.bindEpisode(7, 1.0);
    const auto& cmd = reflex.updatePrograms(scan, 1.0, ship, 17.3);
    check(cmd.urgencySeconds > 7.0, "urgency: the window from the programs' own physics (2 m/s2 -> ~8 s, not a fixed 5 s)");
    std::printf("urgency: 2 m/s2 %.2f s, 73.5 m/s2 %.2f s; physics-derived window %.2f s\n", weak, strong, cmd.urgencySeconds);
}

// NAV-6: the reflex's flag cancels a persistent alignment mode (BrakeToStop) in the shared physics
void testAlignmentCancel()
{
    for (const bool cancel : {false, true})
    {
        ShipTransform tr {};
        tr.motion.mode = game::navigation::MotionMode::HubTactical;
        tr.motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
        tr.motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::BrakeToStop;
        ShipParams p {};
        p.strafeAccel = 20.0f;
        ShipControlState c {};
        c.strafeInput = 0.4f;
        c.cancelVelocityAlignment = cancel;
        WorldParams world {};
        SharedShipPhysics::integrate(tr, p, c, world, 0.02f);
        const bool still = tr.motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BrakeToStop;
        check(cancel ? !still : still, cancel ? "alignment: the reflex's flag cancels BrakeToStop"
                                              : "alignment: a plain strafe leaves BrakeToStop (the scenario matters)");
    }
    std::printf("alignment: BrakeToStop cancelled by the reflex flag\n");
}

// NAV-9: the dodge is predicted through the execution gateway with the consumer's RCS delta-v flag - a Newtonian route
// keeps the dodge's lateral velocity after the hold, the keypad's release cancels it
void testGatewayFlags()
{
    av::ControlShipState ship = episodeShip();
    ship.law = game::navigation::LocalFlightControlLaw::Newtonian;
    ship.params.reverseMainEngineAvailable = true;
    ship.params.forwardMainEngineAvailable = true;
    const auto& programs = av::controlPrograms();
    int strafe = -1;
    for (std::size_t i = 0; i < programs.size(); ++i)
        if (programs[i].strafe > 0.5f && programs[i].lift == 0.0f && programs[i].forward == 0.0f && programs[i].pitch == 0.0f &&
            programs[i].yaw == 0.0f && programs[i].speed == 0.0f)
            strafe = static_cast<int>(i);
    check(strafe >= 0, "gateway: a pure strafe program exists");
    if (strafe < 0)
        return;
    ship.rcsKeepsTranslationDeltaV = false;
    const auto released = av::flyControlProgram(ship, programs[strafe], 2.0, 8.0, 0.02);
    ship.rcsKeepsTranslationDeltaV = true;
    const auto kept = av::flyControlProgram(ship, programs[strafe], 2.0, 8.0, 0.02);
    const double vReleased = std::abs(released.vel.back().x), vKept = std::abs(kept.vel.back().x);
    check(vKept > vReleased + 1.0, "gateway: the kept delta-v flag reaches the predicted flight (Newtonian route)");
    std::printf("gateway: lateral speed after the hold - released %.2f m/s, kept %.2f m/s\n", vReleased, vKept);
}

// NAV-10: the earliest violating threat, not the global minimum's time, decides the urgency
void testEarliestViolation()
{
    // our hands-off flight: straight at 60 m/s along -Z
    av::ControlFlight f;
    f.sampleSeconds = 0.5;
    for (int k = 0; k <= 24; ++k)
    {
        f.pos.push_back(glm::dvec3(0.0, 0.0, -60.0 * 0.5 * k));
        f.vel.push_back(glm::dvec3(0.0, 0.0, -60.0));
        if (k > 0)
            f.chordBound.push_back(0.0);
    }
    av::EvasionScene sc;
    sc.caps.radiusMeters = 10.0;
    sc.marginMeters = 15.0;
    const glm::dvec3 ours(0.0, 0.0, -60.0);
    // A: shallow early - sits 34 m beside our point at t = 2 s (radii 10 + 10, gap 15: clearance -1 m)
    av::EvasionThreatTrack a;
    a.radiusMeters = 10.0;
    a.position = glm::dvec3(34.0, 0.0, -120.0) - ours * 0.0;
    a.velocity = -ours;   // relative: it is at rest in the world
    // B: deep late - dead ahead on our line at t = 10 s (clearance -35 m)
    av::EvasionThreatTrack b;
    b.radiusMeters = 10.0;
    b.position = glm::dvec3(0.0, 0.0, -600.0);
    b.velocity = -ours;
    sc.threats = {a, b};
    const auto c = av::judgeControlFlight(f, sc, ours);
    check(c.timeOfMinSeconds > 8.0, "the global minimum is the late deep one (the scenario)");
    check(c.earliestViolationSeconds >= 0.0 && c.earliestViolationSeconds < 3.0,
          "the earliest violation is the shallow early threat (NAV-10)");
    std::printf("earliest violation: global minimum at %.2f s (%.1f m), earliest violation at %.2f s\n", c.timeOfMinSeconds,
                c.minMarginMeters, c.earliestViolationSeconds);
}

int main()
{
    testContinuousCrossings();
    testRandomEncounters();
    testNonLinearFlight();
    testGeometry();
    testScanRotation();
    testUncertainty();
    testEpisode();
    testUrgencyFromPhysics();
    testAlignmentCancel();
    testGatewayFlags();
    testEarliestViolation();
    std::printf("evasion_trajectory_conflict: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
