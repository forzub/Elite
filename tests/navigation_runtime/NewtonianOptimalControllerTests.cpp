#include "src/game/navigation/autopilot/NewtonianOptimalController.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using game::navigation::autopilot::AutopilotControlRequest;
using game::navigation::autopilot::ManoeuvrePhaseKind;
using game::navigation::autopilot::ManoeuvreRouteSample;
using game::navigation::autopilot::NewtonianManoeuvreProgram;
using game::navigation::autopilot::NewtonianOptimalController;

constexpr double Pi = 3.14159265358979323846;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams params()
{
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
    p.reverseMainEngineAvailable = false;
    p.forwardMainEngineAccelerationMps2 = 50.0f;
    p.reverseMainEngineAccelerationMps2 = 0.0f;

    p.mainGimbalPitchLimitRad = 0.122173048f; // 7 deg
    p.mainGimbalYawLimitRad = 0.122173048f;   // 7 deg
    p.mainGimbalRateRadPerSec = 0.139626340f;// 8 deg/s

    p.manoeuvreThrusterAccel = 2.0f;
    p.strafeAccel = 2.0f;
    p.maxGs = 100.0f;
    p.maxLinearGs = 100.0f;
    return p;
}

AutopilotControlRequest baseRequest()
{
    AutopilotControlRequest r;
    r.forwardMap = {1.0, 0.0, 0.0};
    r.rightMap = {0.0, 0.0, 1.0};
    r.upMap = {0.0, 1.0, 0.0};
    r.desiredForwardMap = r.forwardMap;
    r.desiredUpMap = r.upMap;
    r.deltaSeconds = 0.02;
    r.positionReferenceValid = true;
    r.corridorToleranceMeters = 64.0;
    return r;
}

double rotationInput(const ShipControlState& control)
{
    return std::abs(control.pitchInput) + std::abs(control.yawInput);
}

double translationInput(const ShipControlState& control)
{
    return std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput);
}

// Route along +X: a straight, then (optionally) an arc turning toward +Z.
// `speed(s)` and `accel(s)` give the profile.
template <typename Speed, typename Accel>
std::vector<ManoeuvreRouteSample> route(
    double straightMeters,
    double arcRadiusMeters,
    double arcSweepRad,
    Speed speed,
    Accel accel,
    double spacingMeters = 5.0
)
{
    std::vector<ManoeuvreRouteSample> out;
    const double arcLength = arcRadiusMeters * arcSweepRad;
    const double total = straightMeters + arcLength;
    for (double s = 0.0; s <= total + 1.0e-9; s += spacingMeters)
    {
        ManoeuvreRouteSample sample;
        sample.progressMeters = s;
        if (s <= straightMeters || arcRadiusMeters <= 0.0)
        {
            sample.positionMapMeters = {s, 0.0, 0.0};
            sample.tangentMap = {1.0, 0.0, 0.0};
        }
        else
        {
            const double a = (s - straightMeters) / arcRadiusMeters;
            sample.positionMapMeters = {
                straightMeters + arcRadiusMeters * std::sin(a), 0.0,
                arcRadiusMeters * (1.0 - std::cos(a))};
            sample.tangentMap = {std::cos(a), 0.0, std::sin(a)};
            sample.normalMap = {-std::sin(a), 0.0, std::cos(a)};
            sample.curvaturePerMeter = 1.0 / arcRadiusMeters;
        }
        sample.speedMps = speed(s);
        sample.longitudinalAccelerationMps2 = accel(s);
        out.push_back(sample);
    }
    return out;
}

const game::navigation::autopilot::ManoeuvrePhase* phaseAt(
    const NewtonianManoeuvreProgram& program,
    double progress
)
{
    for (const auto& phase : program.phases)
        if (progress >= phase.startProgressMeters &&
            progress < phase.endProgressMeters)
            return &phase;
    return nullptr;
}

// Braking straight into a curve that needs the main engine: the program
// brakes, then coasts long enough to turn the hull onto the curve axis
// before the curve begins (the profile is reshaped, not the geometry).
void testBrakeIntoCurveGetsTurnWindow()
{
    NewtonianOptimalController controller;
    // 40 m/s on R 100 m needs 16 m/s2 of path-normal acceleration.
    const double brakeStart = 1000.0;
    const double straight = 1500.0;
    const double decel = 30.0;
    const double vCurve = 40.0;
    const double vStart = std::sqrt(vCurve * vCurve + 2.0 * decel * 500.0);
    auto speed = [&](double s)
    {
        if (s < brakeStart)
            return vStart;
        if (s < straight)
            return std::sqrt(vCurve * vCurve +
                             2.0 * decel * (straight - s));
        return vCurve;
    };
    auto accel = [&](double s)
    {
        return s >= brakeStart && s < straight ? -decel : 0.0;
    };
    controller.setManoeuvreRoute(
        route(straight, 100.0, 0.5 * Pi, speed, accel), params(),
        {-1.0, 0.0, 0.0});
    const auto& program = controller.program();
    require(program.valid, "program did not compile");

    const auto* brake = phaseAt(program, 1100.0);
    require(brake && brake->kind == ManoeuvrePhaseKind::Brake,
        "the braking straight is not a BRAKE phase");
    const auto* curve = phaseAt(program, straight + 50.0);
    require(curve && curve->kind == ManoeuvrePhaseKind::Curve,
        "the arc is not a CURVE phase");
    require(brake->endTimeSeconds + 1.0 < curve->startTimeSeconds,
        "no turn window between the brake and the curve");
    const auto* between = phaseAt(program, straight - 10.0);
    require(between &&
            !game::navigation::autopilot::manoeuvrePhaseUsesMain(
                between->kind),
        "the stretch before the curve still burns");
    require(program.worstThrustDeficitRad < 0.2,
        "the planned hull does not reach the curve axis in time");
}

// A curve flown into an acceleration: the speed-up starts one turn
// window after the curve, and the curve keeps its own axis meanwhile.
void testCurveIntoAccelerationGetsTurnWindow()
{
    NewtonianOptimalController controller;
    const double straight = 200.0;
    const double radius = 100.0;
    const double arcEnd = straight + radius * 0.5 * Pi;
    auto speed = [&](double s)
    {
        return s <= arcEnd ? 40.0
                           : std::sqrt(1600.0 + 2.0 * 30.0 * (s - arcEnd));
    };
    auto accel = [&](double s) { return s <= arcEnd ? 0.0 : 30.0; };
    // Route continues straight along +Z after the arc.
    auto samples = route(straight, radius, 0.5 * Pi, speed, accel);
    const glm::dvec3 exit = samples.back().positionMapMeters;
    for (double d = 5.0; d <= 1000.0; d += 5.0)
    {
        ManoeuvreRouteSample sample;
        sample.progressMeters = arcEnd + d;
        sample.positionMapMeters = exit + glm::dvec3(0.0, 0.0, d);
        sample.tangentMap = {0.0, 0.0, 1.0};
        sample.speedMps = speed(arcEnd + d);
        sample.longitudinalAccelerationMps2 = accel(arcEnd + d);
        samples.push_back(sample);
    }
    controller.setManoeuvreRoute(samples, params(), {0.0, 0.0, 1.0});
    const auto& program = controller.program();
    require(program.valid, "program did not compile");
    const auto* curve = phaseAt(program, straight + 20.0);
    require(curve && curve->kind == ManoeuvrePhaseKind::Curve,
        "the arc is not a CURVE phase");
    const auto* burn = phaseAt(program, arcEnd + 900.0);
    require(burn && burn->kind == ManoeuvrePhaseKind::Burn,
        "the acceleration after the arc is not a BURN phase");
    require(curve->endTimeSeconds + 1.0 < burn->startTimeSeconds,
        "no turn window between the curve and the acceleration");
}

// A braking burn the hull cannot reach in time stays in the plan (the
// route ahead needs it); only an optional speed-up may be dropped.
void testBrakingIsNeverDroppedAsUnreachable()
{
    NewtonianOptimalController controller;
    auto speed = [](double s)
    {
        return s < 100.0 ? 60.0 : std::sqrt(std::max(0.0,
            3600.0 - 2.0 * 30.0 * (s - 100.0)));
    };
    auto accel = [](double s) { return s >= 100.0 ? -30.0 : 0.0; };
    // Hull points along the motion: the retro burn starts 1.7 s ahead,
    // less than a calm flip needs.
    controller.setManoeuvreRoute(
        route(160.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    const auto* brake = phaseAt(controller.program(), 140.0);
    require(brake && brake->kind == ManoeuvrePhaseKind::Brake,
        "an unreachable braking burn was dropped from the plan");
}

// A sub-m/s speed trim right after an acceleration (a profile re-anchored
// on a slightly faster actual speed) is RCS work: no BRAKE phase the hull
// could never be prepared for.
void testMicroTrimIsNotAMainBurn()
{
    NewtonianOptimalController controller;
    auto speed = [](double s)
    {
        if (s < 800.0)
            return std::sqrt(1.0 + 2.0 * 30.0 * s);
        if (s < 805.0)
            return std::sqrt(1.0 + 2.0 * 30.0 * 800.0) + 0.9;
        return std::sqrt(1.0 + 2.0 * 30.0 * 800.0);
    };
    auto accel = [](double s)
    {
        return s < 800.0 ? 30.0 : (s >= 805.0 && s < 810.0 ? -30.0 : 0.0);
    };
    controller.setManoeuvreRoute(
        route(3000.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    for (const auto& phase : controller.program().phases)
        require(phase.kind != ManoeuvrePhaseKind::Brake,
            "a sub-m/s trim became a main BRAKE phase");
    require(controller.program().worstThrustDeficitRad < 0.2,
        "the program contains a burn the hull cannot be prepared for");
}

// In a coast phase a small offset on a safe velocity stays ballistic: no
// main, no hull chase, no RCS dribble.
void testCoastStaysBallistic()
{
    NewtonianOptimalController controller;
    auto speed = [](double) { return 100.0; };
    auto accel = [](double) { return 0.0; };
    controller.setManoeuvreRoute(
        route(5000.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    auto request = baseRequest();
    request.programProgressValid = true;
    request.programProgressMeters = 1000.0;
    request.desiredPositionMapMeters = {1000.0, 0.0, 0.0};
    request.actualPositionMapMeters = {1000.0, 0.0, 6.0};
    request.actualVelocityMapMps = {100.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {100.0, 0.0, 0.0};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f, "coast fired the main engine");
    require(rotationInput(out) < 0.05, "coast turned the hull");
    require(translationInput(out) < 0.05, "coast dribbled RCS");
}

// On a curve the main stays on the curve axis: an under-speed catch-up
// may not swing the burn off it.
void testCurveKeepsItsAxisAgainstSpeedCatchUp()
{
    NewtonianOptimalController controller;
    const double radius = 100.0;
    auto speed = [](double) { return 40.0; };
    auto accel = [](double) { return 0.0; };
    controller.setManoeuvreRoute(
        route(100.0, radius, 0.5 * Pi, speed, accel), params(),
        {0.0, 0.0, 1.0});
    // Station on the arc 0.3 rad in; hull on the planned (normal) axis.
    const double a = 0.3;
    auto request = baseRequest();
    request.programProgressValid = true;
    request.programProgressMeters = 100.0 + radius * a;
    const glm::dvec3 position {
        100.0 + radius * std::sin(a), 0.0, radius * (1.0 - std::cos(a))};
    const glm::dvec3 tangent {std::cos(a), 0.0, std::sin(a)};
    const glm::dvec3 normal {-std::sin(a), 0.0, std::cos(a)};
    request.desiredPositionMapMeters = position;
    request.actualPositionMapMeters = position;
    request.forwardMap = normal;
    request.rightMap = glm::normalize(glm::cross(normal, glm::dvec3(0, 1, 0)));
    request.upMap = {0.0, 1.0, 0.0};
    // 15 m/s slower than planned.
    request.actualVelocityMapMps = tangent * 25.0;
    request.desiredVelocityMapMps = tangent * 40.0;
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate > 0.05f,
        "the curve burn did not fire with the hull on the curve axis");
    require(rotationInput(out) < 0.5,
        "the speed catch-up turned the hull off the curve axis");
}

// The burn gate: no main until the achievable thrust axis is aligned.
void testBurnGateHoldsMainUntilAligned()
{
    NewtonianOptimalController controller;
    auto speed = [](double s) { return std::sqrt(2.0 * 30.0 * s + 1.0); };
    auto accel = [](double) { return 30.0; };
    controller.setManoeuvreRoute(
        route(2000.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    auto request = baseRequest();
    request.programProgressValid = true;
    request.programProgressMeters = 10.0;
    request.desiredPositionMapMeters = {10.0, 0.0, 0.0};
    request.actualPositionMapMeters = {10.0, 0.0, 0.0};
    request.actualVelocityMapMps = {24.0, 0.0, 0.0};
    // Hull 40 degrees off the burn axis.
    const double t = 40.0 * Pi / 180.0;
    request.forwardMap = {std::cos(t), 0.0, std::sin(t)};
    request.rightMap = {-std::sin(t), 0.0, std::cos(t)};
    auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f,
        "main fired 40 degrees off the burn axis");
    require(rotationInput(out) > 0.05, "the hull is not turned to the burn");

    NewtonianOptimalController aligned;
    aligned.setManoeuvreRoute(
        route(2000.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    out = aligned.update(request, params());
    require(out.targetSpeedRate > 0.3f, "aligned burn did not fire");
}

// A finished phase is never revived by a small step back in progress.
void testFinishedPhaseIsNotRevived()
{
    NewtonianOptimalController controller;
    auto speed = [](double s)
    {
        return s < 1000.0 ? std::sqrt(2.0 * 30.0 * s + 1.0)
                          : std::sqrt(2.0 * 30.0 * 1000.0 + 1.0);
    };
    auto accel = [](double s) { return s < 1000.0 ? 30.0 : 0.0; };
    controller.setManoeuvreRoute(
        route(3000.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    auto request = baseRequest();
    request.programProgressValid = true;
    request.actualVelocityMapMps = {speed(1000.0), 0.0, 0.0};
    request.desiredPositionMapMeters = {1010.0, 0.0, 0.0};
    request.actualPositionMapMeters = {1010.0, 0.0, 0.0};
    request.programProgressMeters = 1010.0;
    (void)controller.update(request, params());
    request.programProgressMeters = 995.0;
    request.desiredPositionMapMeters = {995.0, 0.0, 0.0};
    request.actualPositionMapMeters = {995.0, 0.0, 0.0};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f,
        "a step back in progress revived the finished BURN");
}

// The route progress can stall a few metres short of the last station
// (the projection clamps at the route end) while the plan still reads a
// braking station. The brake must end at near-zero speed: no retro burn
// that would reverse the motion, and the terminal settle takes over.
void testBrakeNeverReversesTheMotion()
{
    NewtonianOptimalController controller;
    auto speed = [](double s)
    {
        return std::sqrt(std::max(0.0, 2.0 * 30.0 * (500.0 - s)));
    };
    auto accel = [](double) { return -30.0; };
    controller.setManoeuvreRoute(
        route(500.0, 0.0, 0.0, speed, accel), params(), {-1.0, 0.0, 0.0});
    auto request = baseRequest();
    // Hull retro, aligned with the braking axis.
    request.forwardMap = {-1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, -1.0};
    request.programProgressValid = true;
    request.programProgressMeters = 498.0;
    request.desiredPositionMapMeters = {498.0, 0.0, 0.0};
    request.actualPositionMapMeters = {498.0, 0.0, 0.0};
    request.actualVelocityMapMps = {-1.0, 0.0, 0.0};
    auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f,
        "a finished brake kept burning against a craft already reversing");

    request.stopRequested = true;
    request.actualVelocityMapMps = {1.5, 0.0, 0.0};
    out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f,
        "terminal settle fired the main engine");
    require(out.forwardInput > 0.0f,
        "terminal settle did not trim the residual motion with RCS");
}

AutopilotControlRequest terminalRequest()
{
    auto request = baseRequest();
    request.stopRequested = true;
    request.terminalTargetValid = true;
    request.terminalPositionMapMeters = {0.0, 0.0, 0.0};
    request.terminalForwardMap = {1.0, 0.0, 0.0};
    request.terminalUpMap = {0.0, 1.0, 0.0};
    request.terminalPositionToleranceMeters = 3.0;
    request.terminalSpeedToleranceMps = 0.75;
    request.terminalAttitudeToleranceRad = 5.0 * Pi / 180.0;
    request.terminalRollToleranceRad = 5.0 * Pi / 180.0;
    request.terminalAngularRateToleranceRadPerSec = 0.05;
    request.desiredPositionMapMeters = {0.0, 0.0, 0.0};
    request.actualPositionMapMeters = {0.0, 0.0, 0.0};
    return request;
}

// The last metres are RCS work: main inhibited, the hull keeps the
// attitude it arrived with while it is being placed.
void testTerminalLastMetresAreRcsTrim()
{
    NewtonianOptimalController controller;
    auto request = terminalRequest();
    request.actualPositionMapMeters = {0.0, 0.0, 5.0};
    request.actualVelocityMapMps = {0.0, 0.0, 0.0};
    // Hull retro after braking.
    request.forwardMap = {-1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, -1.0};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f, "terminal placement fired main");
    require(rotationInput(out) < 0.05,
        "terminal placement turned the hull before the craft was placed");
    require(translationInput(out) > 0.01,
        "terminal placement did not use translational RCS");
}

// The terminal turn is calm: at most the preferred share of the angular
// authority, even for a 180 degree turn.
void testTerminalAlignIsCalm()
{
    NewtonianOptimalController controller;
    auto request = terminalRequest();
    request.forwardMap = {-1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, -1.0};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f, "terminal align fired main");
    require(rotationInput(out) > 0.05, "terminal align did not turn");
    require(std::abs(out.pitchInput) <= 0.51f &&
            std::abs(out.yawInput) <= 0.51f &&
            std::abs(out.rollInput) <= 0.51f,
        "terminal align used more than the calm share of the authority");
}

// DONE is final: tiny residual errors inside the hysteresis band change
// nothing, and a profile refresh does not recompile the program.
void testTerminalDoneCorrectsNothing()
{
    NewtonianOptimalController controller;
    auto speed = [](double) { return 50.0; };
    auto accel = [](double) { return 0.0; };
    controller.setManoeuvreRoute(
        route(500.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    const std::size_t stations = controller.program().stations.size();
    auto request = terminalRequest();
    for (int i = 0; i < 80; ++i)
        (void)controller.update(request, params());
    // Inside 2x the tolerances: 4 m off, 0.08 m/s drift, 8 deg yaw.
    request.actualPositionMapMeters = {0.0, 0.0, 4.0};
    request.actualVelocityMapMps = {0.0, 0.0, 0.08};
    const double a = 8.0 * Pi / 180.0;
    request.forwardMap = {std::cos(a), 0.0, std::sin(a)};
    request.rightMap = {-std::sin(a), 0.0, std::cos(a)};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f, "DONE fired main");
    require(rotationInput(out) == 0.0 && std::abs(out.rollInput) == 0.0f,
        "DONE corrected an attitude inside the hysteresis band");
    require(translationInput(out) == 0.0,
        "DONE trimmed a position inside the hysteresis band");
    controller.setManoeuvreRoute(
        route(900.0, 0.0, 0.0, speed, accel), params(), {1.0, 0.0, 0.0});
    require(controller.program().stations.size() == stations,
        "a profile refresh recompiled the program after the terminal stop");
}

void testPrecisionStopRcsRemainsIsolated()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    request.stopRequested = true;
    request.actualVelocityMapMps = {0.10, 0.0, 0.0};
    request.desiredVelocityMapMps = {0.0, 0.0, 0.0};
    const auto out = controller.update(request, params());
    require(out.targetSpeedRate == 0.0f, "precision stop fired the main");
    require(out.forwardInput < 0.0f,
        "precision stop did not trim the residual velocity with RCS");
}

} // namespace

int main()
{
    try
    {
        testBrakeIntoCurveGetsTurnWindow();
        testCurveIntoAccelerationGetsTurnWindow();
        testBrakingIsNeverDroppedAsUnreachable();
        testMicroTrimIsNotAMainBurn();
        testCoastStaysBallistic();
        testCurveKeepsItsAxisAgainstSpeedCatchUp();
        testBurnGateHoldsMainUntilAligned();
        testFinishedPhaseIsNotRevived();
        testBrakeNeverReversesTheMotion();
        testTerminalLastMetresAreRcsTrim();
        testTerminalAlignIsCalm();
        testTerminalDoneCorrectsNothing();
        testPrecisionStopRcsRemainsIsolated();

        std::cout
            << "NEWTONIAN MANOEUVRE PROGRAM PILOT: PASS\n"
            << " - braking into a curve leaves a turn window before it\n"
            << " - a curve into an acceleration leaves a turn window after it\n"
            << " - a needed braking burn is never dropped as unreachable\n"
            << " - a sub-m/s trim is RCS work, not a main burn\n"
            << " - a coast phase stays ballistic\n"
            << " - a curve keeps its thrust axis against a speed catch-up\n"
            << " - the burn gate holds main until aligned\n"
            << " - a finished phase is not revived\n"
            << " - a finished brake never reverses the motion\n"
            << " - the last terminal metres are RCS trim, main inhibited\n"
            << " - the terminal turn is calm\n"
            << " - DONE corrects nothing and recompiles nothing\n"
            << " - precision-stop RCS remains isolated\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NEWTONIAN MANOEUVRE PROGRAM PILOT: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
