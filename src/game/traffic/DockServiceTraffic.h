#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/docking/model/HexDockFacility.h"
#include "src/game/docking/traffic/DockTrafficControllerApi.h"

// Living dock traffic around the rotating hex dock: Earth <-> hub cargo
// shuttles on a schedule plus Cobrett "busy traffic" loops. Server-side,
// deterministic, independent of GameSimulation: positions are produced in the
// dock FIXED frame (see HexDockFacility.h) for any time, the game maps them to
// the world, the offline check runs hours of traffic in seconds.
//
// Flight is a light kinematic model that LOOKS Newtonian: long legs are
// burn - coast - flip - main-engine braking (velocity never turns with the
// nose), local legs are slow RCS translations, the final approach syncs roll
// with the dock spin, inside the chamber the station moves the ship
// (station-assisted landing, explicit kinematic capture). All shared-lane
// rights come from DockTrafficController; this module only reports progress.
namespace game::traffic::dock
{

enum class TrafficShipKind : std::uint8_t
{
    CargoShuttle = 0,
    Cobrett
};

struct TrafficShipClass
{
    TrafficShipKind kind = TrafficShipKind::Cobrett;
    const char* name = "";
    double length = 0.0, width = 0.0, height = 0.0;
    double radius = 0.0;          // bounding sphere
    double dryMassKg = 0.0;
    double cargoCapacityKg = 0.0;  // NET payload (ShipCargoPassports)
    double mainAccel = 0.0;       // traffic burn, m/s^2
    double cruiseSpeed = 0.0;     // long legs
    double localSpeed = 0.0;      // hold / approach manoeuvres
    double localAccel = 0.0;
    double flipSeconds = 0.0;
    double serviceSeconds = 0.0;  // on the pad (unload + load + service)
    game::docking::DockSizeClass sizeClass = game::docking::DockSizeClass::Medium;
    double containerTareKg = 0.0;  // tare of the loaded C20 places (gross = net + tare)
};

[[nodiscard]] const TrafficShipClass& trafficShipClass(TrafficShipKind kind);
[[nodiscard]] game::docking::DockingEnvelope dockingEnvelope(const TrafficShipClass& c);

// One persistent flight identity: the same flight id from scheduled departure to
// arrival; the far (aggregated) part of the leg keeps it, it never vanishes at A and
// reappears independently at B.
struct CargoManifest
{
    std::string flightId;
    std::string cargoClass;
    double tonnes = 0.0;
    std::string origin;
    std::string destination;
    double scheduledSeconds = 0.0; // dock-side schedule time of this leg
    double etaSeconds = 0.0;       // arrival at the destination (aggregated far part included)
    std::string category;          // passenger | cargo | service | crew | mail | fuel
};

struct TrafficPose
{
    bool present = false;
    glm::dvec3 position {0.0};          // FIXED frame
    glm::dmat3 basis {1.0};             // columns right, up, back (FIXED)
    glm::dvec3 velocity {0.0};          // FIXED frame
};

enum class ActorPhase : std::uint8_t
{
    Inbound,        // remote leg -> own queue gate (burn/coast/flip/brake)
    WaitGate,       // logical queue at the own gate
    ToHold,
    AtHold,
    Approach,       // hold -> axis join -> entry face (roll sync)
    Corridor,       // entry corridor, co-rotating
    StationTransfer,// station control: to above the pad, nose to the exit
    Descend,
    Docked,         // service on the pad
    DepartureWait,
    Lift,
    ToChamberGate,
    CorridorOut,
    AxialOut,       // along the axis, brake to the dispersal stop
    Disperse,       // into the own departure sector
    Outbound,       // shuttle: burn towards Earth and leave the local zone
    FarLeg,         // Cobrett: out ~20 km and stop
    FarWait,
    Gone
};

[[nodiscard]] const char* actorPhaseName(ActorPhase phase) noexcept;

// Generic hub dock traffic: one model class, one config per hub dock sector. Nothing in
// the model names a particular hub; identity, namespaces and the connected network come
// from here (the game fills it from the hub record and the world-model routes).
struct TrafficConfig
{
    std::string hubId = "earth_orbital_hub";
    std::string hubDisplayName = "Hub";
    std::string dockFacilityId = "earth_orbital_hub.hex_dock_a";
    std::string dockFrameId = "hub_hex_dock_a";
    int systemId = 0;
    // callsign / flight namespaces of this hub (no shared global namespace)
    std::string shuttleCallsignPrefix = "ORB";
    std::string cobrettCallsignPrefix = "CBR";
    std::string flightPrefix = "FLT";
    // where shuttles come from / go to: the parent surface port and the connected hubs
    std::string surfaceName = "surface port";
    std::vector<std::string> connectedDestinations;
    // mix of shuttle flights: surface <-> hub vs hub <-> hub (rest)
    double surfaceShare = 0.8;
    std::uint64_t seed = 3126;
    double shuttleIntervalSeconds = 600.0; // one inbound shuttle flight per interval
    double shuttleTransferSeconds = 3.0 * 3600.0; // aggregated far part of the leg
    int cobrettCount = 4;
    double cobrettFarDistance = 20000.0;
    double materializeDistance = 25000.0;  // local simulation boundary
    // Earth direction in the dock FIXED frame (the hub radial down).
    glm::dvec3 earthDirectionFixed {0.0, -1.0, 0.0};
    // Start populated: this many shuttles already docked (mid-service).
    int initialDockedShuttles = 2;
    int initialDockedCobretts = 1;
    // Flow control (author 2026-11-07: "обычное ожидание до 5 мин, но в случае эвакуации или
    // боевых действий нагрузка может и расти"): scheduled traffic waits at its far point /
    // before entering the zone while a new arrival's predicted wait exceeds this limit
    // minus its own trip in. Emergency (evacuation, combat): no limit. Pilots are never held.
    double normalWaitLimitSeconds = 300.0;
    bool emergency = false;
};

struct TrafficActorView
{
    std::uint64_t id = 0;
    TrafficShipKind kind = TrafficShipKind::Cobrett;
    std::string callsign;
    ActorPhase phase = ActorPhase::Gone;
    double phaseSince = 0.0;
    CargoManifest cargo;
    int gateSlot = -1;
};

struct TrafficLogLine
{
    double timeSeconds = 0.0;
    std::string text;
};

class DockServiceTraffic
{
public:
    // dockPhaseRad(t): live spin angle of the dock body (rotating frame about
    // FIXED +Z). The game passes the authoritative module phase.
    DockServiceTraffic(
        TrafficConfig config,
        std::function<double(double)> dockPhaseRad,
        std::uint64_t firstShipId = 9000001);
    ~DockServiceTraffic();

    // Advances actors and the dispatcher to `nowSeconds` (monotonic).
    void advance(double nowSeconds);

    [[nodiscard]] std::vector<TrafficActorView> actors() const;
    [[nodiscard]] TrafficPose pose(std::uint64_t actorId, double nowSeconds) const;
    [[nodiscard]] const TrafficShipClass& actorClass(std::uint64_t actorId) const;
    // The remaining planned path of an actor (FIXED frame), sampled from the
    // same segments pose() evaluates: what the ship will actually fly. Empty
    // while it waits on an open-ended hold.
    [[nodiscard]] std::vector<glm::dvec3> plannedPath(std::uint64_t actorId, double nowSeconds,
                                                      std::size_t maxPoints = 32) const;
    // Human-readable destination of the current leg ("pad P3", "Earth", ...).
    [[nodiscard]] std::string destination(std::uint64_t actorId) const;
    // Radius (from the dock centre) of the dock's controlled zone: the
    // outermost queue gate / departure turn. Ships inside it belong to the hub.
    [[nodiscard]] double controlledZoneRadius() const;

    // ---- external ships (player / any materialised ship) -----------------
    // Station-assisted landing of a ship that flew to the portal itself
    // (needs an EntryCleared dispatcher status). From here the station moves
    // it kinematically: into the slot axis, through the corridor, to its pad.
    // While captured, pose(shipId) is authoritative for that ship.
    bool beginStationCapture(std::uint64_t shipId, TrafficShipKind classKind, const TrafficPose& fixedPose, double nowSeconds);
    // Docked external ship asks to leave (dispatcher departure queue). After
    // clearance the station lifts it out and releases it at the portal: pose()
    // then reports present = false and the pilot / autopilot has the ship.
    bool requestExternalDeparture(std::uint64_t shipId, double nowSeconds);
    // The pilot takes the controls during the station's axial departure (outside
    // the portal only - never inside the chamber): the station lets go at once,
    // the ship keeps its velocity; the lane is released at the clear point as usual.
    bool releaseExternalToPilot(std::uint64_t shipId, double nowSeconds);
    [[nodiscard]] bool isExternal(std::uint64_t shipId) const;

    game::docking::traffic::DockTrafficController& dispatcher();
    const game::docking::traffic::DockTrafficController& dispatcher() const;

    // Human-readable traffic + dispatcher events since the last call.
    std::vector<TrafficLogLine> drainLog();

    // Statistics for the long run check.
    struct Stats
    {
        std::size_t arrivals = 0, departures = 0, shuttleFlights = 0, cobrettLoops = 0;
        std::size_t flowHolds = 0; // flow control: scheduled traffic held back (normal wait limit)
        std::size_t maxArrivalQueue = 0, maxHoldsOccupied = 0, maxPadsOccupied = 0;
        double maxQueueWaitSeconds = 0.0;
    };
    [[nodiscard]] const Stats& stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace game::traffic::dock
