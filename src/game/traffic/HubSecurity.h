#pragma once

// Hub security / traffic control (docs/HUB_SECURITY.md; author 2026-11-19: «оборонительные установки - военный охранный
// периметр + уничтожение непрошенных астероидов в мирное время ... если мы летим по красному корридору - влоб против
// движения кораблей - корабли должны уходить от столкновения ... за красные корридоры включатся какие-то санкции, в
// основном - денежные»; «любой мирный корабль при попытке столкнуться с ним должен выполнять маневр уклонения в меру
// своих возможностей»).
//
// PURE math and rules, no simulation state: the server (GameSimulationHubSecurity.cpp) owns the state and calls these;
// the client presentation (src/scene/HubDefencePresentation) calls the deterministic parts (defence layout, stray bodies,
// intercepts) with the same inputs, so both see the same turrets and the same kills.
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
class HubPlan;
}
namespace game::navigation::traffic
{
class HubLaneNetwork;
}

namespace game::traffic::security
{

// ------------------------------------------------------------------------------------------------ lanes, wrong way
struct LaneProbe
{
    int lane = -1;
    glm::dvec3 direction {0.0};   // the lane's direction of travel at that point (city frame)
    glm::dvec3 axisPoint {0.0};   // the nearest point of the lane's centre line
    double lateralMeters = 0.0;   // from the centre line
    double radiusMeters = 0.0;
};
// the lane whose volume (radius + marginMeters) holds `local` (city frame), nearest centre line first
bool probeLane(const game::navigation::traffic::HubLaneNetwork& lanes, const glm::dvec3& local, double marginMeters, LaneProbe& out);

struct WrongWayParams
{
    double minSpeedMps = 15.0;      // slower = manoeuvring / drifting, not "flying against the traffic"
    double enterCos = -0.5;         // velocity within 120..180 deg of the lane direction
    double exitCos = -0.2;          // hysteresis: the incident ends only when clearly not against it
    double confirmSeconds = 1.0;    // continuously against it before it counts (a crossing is not an incident)
    double releaseSeconds = 2.0;    // continuously clear before the incident closes
    double warningToFineSeconds = 5.0;   // the controller's warning, then the fine if still against the traffic
};

// one ship's watch (the server keeps one per ship)
struct WrongWayWatch
{
    bool active = false;            // an open incident
    int lane = -1;
    double againstSeconds = 0.0;    // candidate time (before confirmation) / incident duration (after)
    double clearSeconds = 0.0;
    double metersAgainst = 0.0;     // flown against the lane in this incident
    double fineChargedCr = 0.0;     // charged in this incident
    double nextPeriodicAt = 0.0;    // incident seconds of the next periodic fine
    double lastKmCharged = 0.0;
    bool finedOnce = false;
};

enum class WrongWayEvent : int
{
    None = 0,
    Started = 1,    // confirmed: the controller's warning
    Fine = 2,       // a fine is due (amount in fineCr)
    Ended = 3,
};

struct FineSchedule
{
    double firstFineCr = 1500.0;    // the incident fine, after the warning period
    double periodicFineCr = 750.0;  // each further 10 s still against the traffic
    double periodicSeconds = 10.0;
    double perKmCr = 300.0;         // each km flown against the lane
    double incidentCapCr = 15000.0; // one incident never costs more
    double repeatFactor = 1.5;      // x per earlier incident in the window ...
    double repeatCapFactor = 4.0;   // ... at most x4
};

struct WrongWayStep
{
    WrongWayEvent event = WrongWayEvent::None;
    double fineCr = 0.0;
    double cosAgainst = 0.0;
};

// one step: inLane = probe hit, laneDir its direction, velocity relative to the city; priorIncidents = closed
// incidents of this ship in the repeat window (escalation)
WrongWayStep stepWrongWay(WrongWayWatch& watch, bool inLane, int lane, const glm::dvec3& laneDir, const glm::dvec3& velocity, double dt,
                          int priorIncidents, const WrongWayParams& params = {}, const FineSchedule& fines = {});

// ------------------------------------------------------------------------------------------------ collision avoidance
struct Cpa
{
    double timeSeconds = 0.0;     // > 0: ahead
    double distanceMeters = 0.0;  // at that time
    glm::dvec3 missVector {0.0};  // threat minus self at CPA
};
// relative position (threat - self) and relative velocity (threat - self)
Cpa closestApproach(const glm::dvec3& relPos, const glm::dvec3& relVel);

// what a ship can do (its own capabilities: the Newtonian limits; it cannot stop or jump sideways)
struct EvasionCaps
{
    double lateralAccelMps2 = 0.5;   // RCS / manoeuvre thrust, available at once in any direction
    double mainAccelMps2 = 3.0;      // the main drive, along the nose only
    double turnRateRadPerS = 0.1;    // to bring the main drive to bear
    double radiusMeters = 30.0;      // hull half length (the collision sphere)
};
// estimated caps of a hull by its length (a heavier, longer hull: weaker RCS and slower turns)
EvasionCaps evasionCapsForLength(double lengthMeters);

struct EvasionPlan
{
    bool threat = false;          // a CPA inside the safe radius within the horizon
    bool burn = false;            // accelerate now
    glm::dvec3 direction {0.0};   // unit, the burn direction
    double accelMps2 = 0.0;
    double timeToCpa = 0.0;
    double cpaMeters = 0.0;       // without evasion
    double predictedMissMeters = 0.0;   // with the best burn reachable before the CPA
    double requiredMeters = 0.0;  // the miss it wants (both hulls + margin)
    bool usesMainDrive = false;
    double rotateSeconds = 0.0;   // turning the main drive to bear (RCS only meanwhile)
    bool cannotClear = false;     // even the best burn leaves the hulls touching
};
// selfForward/selfUp: the ship's nose and up (starboard rule for a dead head-on approach: both turn right, COLREGS 14)
EvasionPlan planEvasion(const glm::dvec3& selfPos, const glm::dvec3& selfVel, const glm::dvec3& selfForward, const glm::dvec3& selfUp,
                        const glm::dvec3& threatPos, const glm::dvec3& threatVel, double threatRadius, const EvasionCaps& caps,
                        double horizonSeconds = 30.0, double marginMeters = 40.0);

// the offset layer a kinematic (scripted) ship flies on top of its nominal track: a Newtonian point mass limited by
// the ship's accelerations; with no burn it returns to the track (offset 0), critically damped within its lateral acceleration
struct EvasionOffset
{
    glm::dvec3 offset {0.0};
    glm::dvec3 velocity {0.0};
    bool evading = false;
    double quietSeconds = 0.0;
    double burnSeconds = 0.0;
};
void stepEvasionOffset(EvasionOffset& state, const EvasionPlan& plan, const EvasionCaps& caps, double dt);
// back on the nominal track (offset and its rate negligible): a promoted ship may hand back to the presentation
bool evasionSettled(const EvasionOffset& state);

// ------------------------------------------------------------------------------------------------ defence
enum class DefenceKind : int
{
    PointDefenceLaser = 0,   // hubd_pdl: 2 MW-class laser turret, close-in kills of small bodies (< 1.5 m)
    Coilgun = 1,             // hubd_coil: 3 km/s coilgun battery, bigger bodies (fragments them)
    MissileCells = 2,        // hubd_vls: 16-cell launcher (military perimeter; idle in peacetime)
    RadarMast = 3,           // hubd_radar: S-band mast, search / track
    Picket = 4,              // hubd_picket: a sentry platform at the perimeter radius (sensor + laser)
    Count
};
const char* defenceModelId(DefenceKind kind);        // the base / static package
const char* defenceHeadModelId(DefenceKind kind);    // the slewing head (nullptr: none)

struct DefenceSite
{
    std::uint32_t id = 0;
    DefenceKind kind = DefenceKind::PointDefenceLaser;
    glm::dvec3 position {0.0};   // city frame
    glm::dvec3 up {0.0, 1.0, 0.0};
    double idlePhase = 0.0;      // the idle scan sweep's phase (rad)
};

struct DefenceLayout
{
    std::string hubId;
    bool military = false;
    double perimeterMeters = 6000.0;     // the security zone: inside it stray bodies are tracked / engaged
    double engageMeters = 4500.0;        // weapons free on an inbound body inside this
    double cityRadiusMeters = 4000.0;    // the city itself (what an inbound body would hit)
    std::vector<DefenceSite> sites;
    // a small DEBRIS FIELD beside the city (old wreckage; author 2026-11-22: «сделать возле хаба небольшое поле обломков»):
    // out past the rim on a diagonal (between the lanes' portals), in the city plane; the pieces circle its centre slowly
    glm::dvec3 debrisCentre {0.0};
    double debrisRadiusMeters = 500.0;
    int debrisCount = 0;
    std::uint32_t debrisSeed = 0;
};
struct DebrisPiece
{
    std::uint32_t index = 0;
    glm::dvec3 position {0.0};       // city frame
    glm::dvec3 velocity {0.0};
    double sizeMeters = 1.0;
};
// the debris field's pieces at `timeSeconds` (deterministic: the server, the radar and every client agree)
void debrisFieldAt(const DefenceLayout& layout, double timeSeconds, std::vector<DebrisPiece>& out);
// the defence ring of a planar city: deterministic from its plan (blocks at the rim, the services / police blocks) and
// its lanes' speed-limit zone; military hubs (hub id or many services blocks) are denser and carry missile cells
DefenceLayout buildDefenceLayout(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork* lanes);

// a stray body (rock / debris) drifting into the hub: deterministic per hub and index
struct StrayBody
{
    std::uint32_t index = 0;
    double spawnSeconds = 0.0;       // appears at the radar horizon
    glm::dvec3 start {0.0};          // city frame
    glm::dvec3 velocity {0.0};
    double sizeMeters = 1.0;
    bool inbound = false;            // its path crosses the city
    // the engagement (inbound only): the shot, the kill
    double fireSeconds = 0.0;
    double killSeconds = 0.0;
    int shooter = -1;                // defence site index
    bool laser = true;               // else coilgun
    glm::dvec3 killPoint {0.0};
};
double strayPeriodSeconds(const DefenceLayout& layout);
// body `index` of this hub (index = floor(t / period))
StrayBody strayBody(const DefenceLayout& layout, std::uint32_t index);
glm::dvec3 strayPosition(const StrayBody& body, double timeSeconds);
// the bodies alive at `timeSeconds` (not yet killed, not yet gone) and those killed in the last fxSeconds
void strayBodiesAt(const DefenceLayout& layout, double timeSeconds, double fxSeconds, std::vector<StrayBody>& out);

// ------------------------------------------------------------------------------------------------ hub state
enum class HubAlert : int
{
    Calm = 0,
    Alert = 1,       // a body tracked inbound / a warning issued
    Violation = 2,   // a ship against the traffic / fined / locked
};
const char* hubAlertName(HubAlert a);

// a ship's security level (replicated): what the hub does about it
enum class ShipSecurityLevel : int
{
    None = 0,
    Warned = 1,      // the controller called it
    Fined = 2,
    Locked = 3,      // fire-control lock + escort call (never fired upon in peacetime)
};
} // namespace game::traffic::security
