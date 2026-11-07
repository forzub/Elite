#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/game/docking/model/DockFacilityDescriptor.h"

namespace game::docking::traffic
{

enum class DockAvailabilityState : std::uint8_t
{
    Invalid = 0,
    IncompatiblePhysical,
    ClassRestricted,
    Available,
    Queued,
    Occupied,
    Reserved,
    EntryBusy,
    Maintenance,
    EmergencyAvailable
};

enum class AllocationReason : std::uint8_t
{
    None = 0,
    NormalClass,
    EmergencyLargerClass
};

struct DockingTrafficRequest
{
    std::uint64_t shipId = 0;
    DockSizeClass shipClass = DockSizeClass::Unknown;
    DockingEnvelope envelope {};
    bool emergency = false;
    // Why the ship comes (DockPurpose*); empty = any place will do.
    std::string purpose;

    double etaToFacilitySeconds = 0.0;

    // False means remote/provisional inquiry only. Such a request MUST NOT
    // receive a queue token or reserve any infrastructure resource.
    bool insideControlledApproachHorizon = false;
};

struct DockingTrafficView
{
    DockAvailabilityState state = DockAvailabilityState::Invalid;
    bool physicallyCompatible = false;
    bool classPolicyCompatible = false;

    std::string facilityId;
    std::string portalId;
    std::string padId;
    std::string holdPointId;

    std::size_t queueLength = 0;
    std::size_t queuePosition = 0;
    double estimatedWaitSeconds = 0.0;
};

struct DockingClearance
{
    DockAvailabilityState state = DockAvailabilityState::Invalid;
    AllocationReason allocationReason = AllocationReason::None;

    bool committed = false;
    std::uint64_t queueToken = 0;
    std::uint64_t clearanceRevision = 0;

    std::string facilityId;
    std::string portalId;
    std::string padId;
    std::string holdPointId;
    std::vector<std::string> permittedTrafficResources;

    std::size_t queuePosition = 0;
    double estimatedWaitSeconds = 0.0;
};

// Dispatcher lifecycle of one ship (server authority). Traffic clearance is
// independent of who flies: autopilot, NPC traffic or a human pilot report
// the same progress events.
enum class DockTrafficPhase : std::uint8_t
{
    None = 0,
    Queued,           // logical queue, no physical hold yet
    HoldAssigned,     // fly to / wait at the assigned hold point
    AtHold,           // stabilised at the hold point, waiting for entry
    EntryCleared,     // pad + shared lanes reserved, fly hold -> axis -> portal
    StationControl,   // inside the chamber, the station moves it to the pad
    Docked,           // latched on the pad
    DepartureQueued,  // asked to leave, waiting for the shared lanes
    DepartureCleared, // station lifts it, then it leaves along the axis
    Departed          // left the controlled zone (record kept for the log)
};

[[nodiscard]] const char* dockTrafficPhaseName(DockTrafficPhase phase) noexcept;

struct DockTrafficShipStatus
{
    std::uint64_t shipId = 0;
    DockTrafficPhase phase = DockTrafficPhase::None;
    std::size_t queuePosition = 0; // 1-based in the arrival queue while Queued
    std::size_t queueLength = 0;
    std::size_t departureQueuePosition = 0;
    std::string holdPointId;
    std::string padId;
    std::uint64_t revision = 0;
    double phaseSinceSeconds = 0.0;
    // Rough wait until this ship's own movement through the entrance is
    // cleared: lane movements ahead of it x a typical movement time.
    double estimatedWaitSeconds = 0.0;
    std::size_t movementsAhead = 0;
};

// Whole-facility view for terminals / logs / tests.
struct DockTrafficSnapshot
{
    std::vector<DockTrafficShipStatus> ships;
    std::vector<std::pair<std::string, std::uint64_t>> holds;     // id -> ship (0 free)
    std::vector<std::pair<std::string, std::uint64_t>> pads;      // id -> ship (0 free)
    std::vector<std::pair<std::string, std::uint64_t>> resources; // id -> holder (0 free)
    std::size_t arrivalQueueLength = 0;
    std::size_t departureQueueLength = 0;
};

struct DockTrafficEvent
{
    double timeSeconds = 0.0;
    std::uint64_t shipId = 0;
    std::string text;
};

class DockTrafficController final
{
public:
    explicit DockTrafficController(DockFacilityDescriptor facility);
    ~DockTrafficController();

    DockTrafficController(DockTrafficController&&) noexcept;
    DockTrafficController& operator=(DockTrafficController&&) noexcept;

    DockTrafficController(const DockTrafficController&) = delete;
    DockTrafficController& operator=(const DockTrafficController&) = delete;

    [[nodiscard]] DockingTrafficView inquire(
        const DockingTrafficRequest& request
    ) const;

    [[nodiscard]] DockingClearance admit(
        const DockingTrafficRequest& request
    );

    bool releaseQueueToken(std::uint64_t queueToken);

    // ---- dispatcher lifecycle (arrival / departure through shared lanes) ----
    // Admission into the arrival queue (only inside the controlled approach
    // horizon, see admit()). preferredHoldAzimuthRad picks the closest free
    // hold when one is assigned (negative = no preference).
    // directEntry: the ship cannot use a hold point (today: the player's
    // automatic docking route ends at the portal). It gets entry straight
    // from the queue (hold "DIRECT"); while it waits no other movement is
    // cleared, so the shared lanes drain before it arrives.
    DockingClearance requestDocking(
        const DockingTrafficRequest& request,
        double nowSeconds,
        double preferredHoldAzimuthRad = -1.0,
        bool directEntry = false);
    // True when an arrival asked now would be cleared at once: nobody waits
    // (arrival queue, holds, departures), the shared lanes and a pad are free.
    // The pilot's request then is a direct entry, otherwise it takes a hold.
    [[nodiscard]] bool entryFreeNow(const DockingTrafficRequest& request) const;
    // The wait a NEW arrival asking now would get (lane movements ahead of it
    // x a typical movement): the traffic flow control holds scheduled traffic
    // back while it exceeds the normal limit.
    [[nodiscard]] double predictedNewArrivalWaitSeconds() const;
    static constexpr double TypicalMovementSeconds = 120.0;
    // Arrivals the dispatcher already knows (queued, holding, cleared) and
    // the pads not reserved by anyone.
    [[nodiscard]] std::size_t knownArrivals() const;
    [[nodiscard]] std::size_t freePadCount() const;
    bool reportAtHold(std::uint64_t shipId, double nowSeconds);
    // Past the chamber gate: the station takes over (outer lane and the hold
    // are released, the chamber lane stays until landing).
    bool reportInsideChamber(std::uint64_t shipId, double nowSeconds);
    bool reportLanded(std::uint64_t shipId, double nowSeconds);
    bool requestDeparture(std::uint64_t shipId, double nowSeconds);
    // Departure left the chamber into the corridor: chamber lane released.
    bool reportLeftChamber(std::uint64_t shipId, double nowSeconds);
    // Departure beyond the clear point: outer lane and pad released.
    bool reportClear(std::uint64_t shipId, double nowSeconds);
    bool cancel(std::uint64_t shipId, double nowSeconds);
    // A ship that is already latched on a pad (world start / load): enters
    // the lifecycle as Docked with the pad reserved.
    bool registerDocked(
        const DockingTrafficRequest& request,
        const std::string& padId,
        double nowSeconds);
    // Grants holds, entry and departure clearances. Call every tick.
    void update(double nowSeconds);

    [[nodiscard]] DockTrafficShipStatus status(std::uint64_t shipId) const;
    [[nodiscard]] DockTrafficSnapshot snapshot() const;
    // Empty when every invariant holds (one ship per hold / pad / resource,
    // holders in a consistent phase), else the first violation.
    [[nodiscard]] std::string validate() const;
    // Events since the last call (assignments, clearances, releases).
    std::vector<DockTrafficEvent> drainEvents();

    [[nodiscard]] static bool physicalFit(
        const DockingEnvelope& ship,
        const AccessPortalDescriptor& portal,
        const ParkingPadDescriptor& pad
    ) noexcept;

    // Allocation order (author 2026-11-07): does the ship fit, is the place
    // free, does the place serve why the ship comes.
    [[nodiscard]] static bool purposeAllows(const std::string& purpose, const ParkingPadDescriptor& pad) noexcept;

    [[nodiscard]] static bool classPolicyAllows(
        DockSizeClass shipClass,
        DockSizeClass dockClass,
        bool emergency
    ) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace game::docking::traffic
