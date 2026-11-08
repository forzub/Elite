#pragma once

// The body autopilot (NAVIGATION_V2 §1, author decision 2026-11-06: planners and autopilots on the
// CLIENT, the server = plant + authority on ordinary commands). Flies a ship to a body and lands it:
// the J transfer onto a tangential parking orbit at the J floor, the coast, the de-orbit in normal
// flight on the main engines, the hand-over to the ground stabilization and the terminal landing.
//
// Neutral: no simulation, no network. It reads a sample of the ship (world state, hull, flight law,
// the plant's ground sample) and the world through callbacks (bodies, body orientation, terrain,
// keep-out), and emits the SAME command surface the client sends to the server
// (ShipControlState: the global-flight part + the ordinary controls of the landing pilot). The
// server's plant executes it within the rules (GameSimulation::applyGlobalFlightCommand).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/landing/LandingAutopilot.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::globalflight
{

// the autopilot's internal stage; the values are the ones the "[Autopilot] ... phase N ->" log prints
enum class AutopilotStage : int
{
    None = 0,
    Departure = 1,         // leaving the departure body on the main engines / gentle J
    Cruise = 2,            // the J transfer
    Approach = 3,          // braking into the parking orbit
    OrbitHold = 9,         // on the parking orbit, holding before the de-orbit
    GroundStabilized = 10, // the terminal in the ground frame (landing autopilot)
    Deorbit = 11           // the de-orbit in normal flight (main engines)
};

// the frame the autopilot needs the plant in (ShipControlState::globalFrameRequest)
enum class FrameRequest : std::uint8_t
{
    None = 0,
    NormalFlight = 1,          // leave a stabilization (hub / orbit / ground) for normal flight
    GroundStabilization = 2,   // normal flight -> stabilization against the ground (the hand-over)
    Stabilization = 3          // normal flight -> the stabilization the plant finds (a hub capture / the ground / the orbit)
};

struct KeepOutHit
{
    std::string objectId;
    glm::dvec3 positionM {0.0};
    glm::dvec3 velocityMps {0.0};
    double distanceM = 0.0;
    bool ship = false;
};

struct BodyAutopilotEnvironment
{
    const std::vector<GravityBody>* bodies = nullptr;
    std::function<bool(const std::string& bodyId, glm::dmat3& worldFromBody)> bodyOrientation;
    // ground height above the radius in a WORLD direction from the body centre (false = unknown yet)
    std::function<bool(const std::string& bodyId, const glm::dvec3& worldDirection, landing::TerrainPoint& out)> terrain;
    std::function<bool(const glm::dvec3& worldM, KeepOutHit& hit)> keepOut;
};

// a sample of the ship, consistent with the bodies' positions
struct BodyAutopilotShip
{
    std::uint32_t shipId = 0;          // logs only
    glm::dvec3 positionM {0.0};        // world
    glm::dvec3 velocityMps {0.0};      // world
    glm::dvec3 forward {0.0, 0.0, -1.0}, right {1.0, 0.0, 0.0}, up {0.0, 1.0, 0.0};
    double pitchRate = 0.0, yawRate = 0.0, rollRate = 0.0;
    ShipParams physics {};             // effective
    ShipParams controlParams {};       // with the control-authority telemetry applied
    LocalFlightControlLaw nativeLaw = LocalFlightControlLaw::Assisted;
    LocalFlightControlLaw currentLaw = LocalFlightControlLaw::Assisted;
    bool assistedFreeRotation = false;
    bool shuttle = false;              // a surface lifter: may land in an atmosphere
    // the plant's view of the bodies
    std::string primaryBodyId;         // the dominant body (sphere of influence)
    double primaryAltitudeM = 0.0;
    std::string groundBodyId;          // the body the plant sampled the ground of
    double groundHeightM = 0.0;
    int groundMaterial = -1;
    glm::dvec3 referenceVelocityMps {0.0};   // the dominant body's velocity
    bool groundFrame = false;          // the plant flies the ground stabilization frame
    int surfaceState = 0;              // 0 flying, 1 landed, 2 crashed
    glm::dvec3 mainEngineAccelerationMps2 {0.0};
    glm::dvec3 engineAccelerationMps2 {0.0};
    std::uint8_t refusal = 0;          // the plant's last FlightRefusal code
    bool jAccelKnown = false;          // the plant's J field acceleration now (replicated)
    double jAccelMps2 = 0.0;
};

struct BodyAutopilotOutput
{
    // the command for the plant (the global part + the landing pilot's ordinary controls)
    ShipControlState control {};
    bool pilotActive = false;          // control carries the landing pilot's ordinary controls
    bool setAssistedFreeRotation = false;
    std::uint8_t refusal = 0;          // the FlightRefusal code the autopilot reports
    // the panel
    std::uint8_t displayPhase = 0;     // AutopilotDisplayPhase
    std::string displayTargetId;
    double displayRangeM = 0.0;
    double displayRelSpeedMps = 0.0;
    float displayAccelMps2 = 0.0f;
    bool finished = false;             // the target vanished: the autopilot is off
};

class BodyAutopilot
{
public:
    // a landing check + the engage; false = refused (out.refusal / the log say why)
    bool engage(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const std::string& targetBodyId,
                const std::string& departureBodyId, const std::string& landingSpec, std::uint8_t& refusal);
    void cancel();
    // engaged at full J speed by a route (past a guide point): the drive keeps its ramp
    void carryDriveRamp(double seconds) { m_rampSeconds = seconds; }
    void clear() { m_targetId.clear(); m_stage = AutopilotStage::None; m_pilotActive = false; }   // silent
    bool active() const { return !m_targetId.empty(); }
    const std::string& targetId() const { return m_targetId; }
    AutopilotStage stage() const { return m_stage; }
    double seconds() const { return m_seconds; }
    const landing::LandingAutopilot& landing() const { return m_landing; }
    bool pilotActive() const { return m_pilotActive; }
    LocalFlightControlLaw lawBefore() const { return m_lawBefore; }
    bool freeRotation() const { return m_freeRotation; }
    void clearFreeRotation() { m_freeRotation = false; }
    // the plant reports a touchdown / crash: the autopilot ends
    void finishOnSurface() { m_targetId.clear(); m_stage = AutopilotStage::None; m_pilotActive = false; }

    // one control step (dt = the time since the previous sample)
    BodyAutopilotOutput step(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, double dt);

private:
    const GravityBody* find(const BodyAutopilotEnvironment& env, const std::string& id) const;
    void stepTransfer(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& target,
                      const glm::dvec3& targetVelocity, double dt, BodyAutopilotOutput& out);
    struct GlideSample
    {
        double arcM = 0.0, heightM = 0.0, horizontalMps = 0.0, verticalMps = 0.0, radiusM = 0.0;
        glm::dvec3 toSite {0.0}, up {0.0}, velocityMps {0.0};
        bool terrainKnown = false;
    };
    void stepGlide(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& body,
                   const glm::dvec3& bodyVelocityMps, const GlideSample& gs, double dt, BodyAutopilotOutput& out);
    void stepDescent(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& body,
                     const glm::dvec3& bodyVelocityMps, double dt, BodyAutopilotOutput& out);
    void stepSurface(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& target, double dt,
                     BodyAutopilotOutput& out);

    std::string m_targetId;
    std::string m_departureId;
    AutopilotStage m_stage = AutopilotStage::None;
    double m_seconds = 0.0;
    double m_rampSeconds = 0.0;        // the drive ramp: counts only outside the J keep-out
    double m_orbitCoastSeconds = 0.0;
    glm::dvec3 m_orbitNormal {0.0};    // the parking orbit plane, fixed once (zero = not yet)
    bool m_orbitAcquire = false;
    bool m_bellyLanding = false;
    double m_lastLogSeconds = -1.0e30;
    bool m_haveLastCenter = false;
    glm::dvec3 m_lastTargetCenter {0.0};
    std::string m_lastPrimaryId;       // the floor body one sample ago (its velocity by difference)
    glm::dvec3 m_lastPrimaryCenter {0.0};
    landing::LandingRequest m_landingRequest;
    landing::LandingAutopilot m_landing;
    bool m_landingStarted = false;
    double m_landingPlanSeconds = 0.0;
    glm::dvec3 m_descentNose {0.0};
    // the aircraft-like de-orbit (a ship with a fore braking bank, stepGlide): its phase (1 the
    // nose-up sink, 2 the nose-down braking), the fore-bank thrust of the last re-plan
    bool m_glideInit = false;
    int m_glidePhase = 1;
    double m_glideThrustMps2 = 0.0;
    double m_glideReplanSeconds = -1.0e30;
    bool m_haveLastBasis = false;      // the target's basis one sample ago (the ground's turn)
    glm::dmat3 m_lastBasis {1.0};
    // the landing pilot
    bool m_pilotActive = false;
    LocalFlightControlLaw m_lawBefore = LocalFlightControlLaw::Assisted;
    double m_pilotLastLogSeconds = -1.0e30;
    bool m_freeRotation = false;
    std::uint8_t m_refusal = 0;      // the autopilot's own last refusal (FlightRefusal)
};

// the ship's own main-engine acceleration (forward drive); a legacy descriptor without it falls back
// to its linear g envelope
double mainEngineAccelMps2(const ShipParams& p);
// a ship with a fore (braking) main bank lands like an aircraft: the de-orbit glides nose along the
// path and brakes nose-first (author 2026-11-07); others fly the thrust-axis descent
inline bool alongPathLander(const ShipParams& p)
{
    return p.reverseMainEngineAvailable && p.reverseMainEngineAccelerationMps2 > 0.0f;
}

// the hull's half height along a vertical: the box projected on it (belly down = half.y, tail down = half.z)
double halfHeightAlong(const ShipParams& p, const glm::dvec3& right, const glm::dvec3& up, const glm::dvec3& forward,
                       const glm::dvec3& vertical);

} // namespace game::navigation::globalflight
