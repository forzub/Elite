#include "src/game/docking/traffic/DockTrafficControllerApi.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <sstream>
#include <utility>

namespace game::docking::traffic
{
namespace
{

int classRank(DockSizeClass value) noexcept
{
    switch (value)
    {
        case DockSizeClass::Small: return 1;
        case DockSizeClass::Medium: return 2;
        case DockSizeClass::Large: return 3;
        default: return 0;
    }
}

double effectiveWidth(const DockingEnvelope& envelope) noexcept
{
    return envelope.sweptWidthMeters > 0.0
        ? envelope.sweptWidthMeters
        : envelope.widthMeters;
}

double effectiveHeight(const DockingEnvelope& envelope) noexcept
{
    return envelope.sweptHeightMeters > 0.0
        ? envelope.sweptHeightMeters
        : envelope.heightMeters;
}

} // namespace

struct DockTrafficController::Impl
{
    struct QueueEntry
    {
        std::uint64_t token = 0;
        std::uint64_t shipId = 0;
    };

    explicit Impl(DockFacilityDescriptor value)
        : facility(std::move(value))
    {
    }

    DockFacilityDescriptor facility;
    std::deque<QueueEntry> queue;
    std::uint64_t nextQueueToken = 1;
    std::uint64_t nextClearanceRevision = 1;

    // ---- dispatcher lifecycle ----
    struct ShipRecord
    {
        std::uint64_t shipId = 0;
        DockTrafficPhase phase = DockTrafficPhase::None;
        DockingTrafficRequest request {};
        std::uint64_t queueToken = 0;
        std::string hold;
        std::string pad;
        double phaseSince = 0.0;
        double readySince = 0.0;
        double preferredHoldAzimuth = -1.0;
        bool directEntry = false;
        // Pipelined arrival: cleared with the outer lane while the previous
        // arrival is still under station control in the chamber; the chamber
        // lane is handed over when that ship latches.
        bool awaitingChamber = false;
        std::uint64_t revision = 0;
    };
    std::map<std::uint64_t, ShipRecord> ships;
    std::deque<std::uint64_t> arrivalQueue;
    std::deque<std::uint64_t> departureQueue;
    std::map<std::string, std::uint64_t> holdOwner;
    std::map<std::string, std::uint64_t> padOwner;
    std::map<std::string, std::uint64_t> resourceOwner;
    std::vector<DockTrafficEvent> events;
    std::uint64_t revisionCounter = 1;

    void event(double now, std::uint64_t ship, std::string text)
    {
        events.push_back({now, ship, std::move(text)});
    }
    void setPhase(ShipRecord& record, DockTrafficPhase phase, double now)
    {
        record.phase = phase;
        record.phaseSince = now;
        record.revision = revisionCounter++;
    }
    void releaseResource(const char* id, std::uint64_t ship, double now)
    {
        auto it = resourceOwner.find(id);
        if (it != resourceOwner.end() && it->second == ship)
        {
            it->second = 0;
            event(now, ship, std::string("release ") + id);
            if (std::string(id) == "chamber_lane")
                for (auto& [otherId, other] : ships)
                    if (other.awaitingChamber && other.phase == DockTrafficPhase::EntryCleared)
                    {
                        other.awaitingChamber = false;
                        it->second = otherId;
                        event(now, otherId, "chamber_lane handed over (pipelined entry)");
                        break;
                    }
        }
    }
    bool resourcesFree(const std::vector<std::string>& ids) const
    {
        for (const auto& id : ids)
        {
            const auto it = resourceOwner.find(id);
            if (it != resourceOwner.end() && it->second != 0)
                return false;
        }
        return true;
    }
    const std::vector<std::string>& portalResources() const
    {
        static const std::vector<std::string> none;
        return facility.portals.empty() ? none : facility.portals.front().conflictResourceIds;
    }
};

DockTrafficController::DockTrafficController(DockFacilityDescriptor facility)
    : m_impl(std::make_unique<Impl>(std::move(facility)))
{
}

DockTrafficController::~DockTrafficController() = default;
DockTrafficController::DockTrafficController(
    DockTrafficController&&
) noexcept = default;
DockTrafficController& DockTrafficController::operator=(
    DockTrafficController&&
) noexcept = default;

bool DockTrafficController::physicalFit(
    const DockingEnvelope& ship,
    const AccessPortalDescriptor& portal,
    const ParkingPadDescriptor& pad
) noexcept
{
    const double width = effectiveWidth(ship);
    const double height = effectiveHeight(ship);

    if (!(std::isfinite(width) && width > 0.0) ||
        !(std::isfinite(height) && height > 0.0) ||
        !(std::isfinite(ship.landingFootprintWidthMeters) &&
          ship.landingFootprintWidthMeters > 0.0) ||
        !(std::isfinite(ship.landingFootprintLengthMeters) &&
          ship.landingFootprintLengthMeters > 0.0) ||
        width > portal.usableWidthMeters ||
        height > portal.usableHeightMeters ||
        ship.landingFootprintWidthMeters > pad.usableWidthMeters ||
        ship.landingFootprintLengthMeters > pad.usableLengthMeters)
    {
        return false;
    }

    if (pad.maxMassKg > 0.0 &&
        (!(std::isfinite(ship.massKg) && ship.massKg > 0.0) ||
         ship.massKg > pad.maxMassKg))
    {
        return false;
    }

    return true;
}

bool DockTrafficController::classPolicyAllows(
    DockSizeClass shipClass,
    DockSizeClass dockClass,
    bool emergency
) noexcept
{
    const int shipRank = classRank(shipClass);
    const int dockRank = classRank(dockClass);
    if (shipRank == 0 || dockRank == 0)
        return false;

    if (dockRank == shipRank)
        return true;

    return emergency && dockRank > shipRank;
}

DockingTrafficView DockTrafficController::inquire(
    const DockingTrafficRequest& request
) const
{
    DockingTrafficView out;
    out.facilityId = m_impl->facility.facilityId;
    out.queueLength = m_impl->queue.size();

    bool anyPhysical = false;
    bool anyNormalClass = false;
    bool anyEmergencyClass = false;

    for (const auto& portal : m_impl->facility.portals)
    {
        for (const auto& pad : m_impl->facility.pads)
        {
            if (!physicalFit(request.envelope, portal, pad))
                continue;

            anyPhysical = true;
            const bool normalClass =
                classPolicyAllows(
                    request.shipClass,
                    pad.sizeClass,
                    false
                ) &&
                classPolicyAllows(
                    request.shipClass,
                    portal.sizeClass,
                    false
                );
            const bool requestedClass =
                classPolicyAllows(
                    request.shipClass,
                    pad.sizeClass,
                    request.emergency
                ) &&
                classPolicyAllows(
                    request.shipClass,
                    portal.sizeClass,
                    request.emergency
                );

            anyNormalClass = anyNormalClass || normalClass;
            anyEmergencyClass =
                anyEmergencyClass || (requestedClass && !normalClass);

            if (!requestedClass)
                continue;

            out.physicallyCompatible = true;
            out.classPolicyCompatible = true;
            out.portalId = portal.id;
            out.padId = pad.id;
            out.holdPointId = portal.holdPointId;
            out.state =
                normalClass
                    ? DockAvailabilityState::Available
                    : DockAvailabilityState::EmergencyAvailable;
            return out;
        }
    }

    out.physicallyCompatible = anyPhysical;
    out.classPolicyCompatible = anyNormalClass || anyEmergencyClass;
    out.state = anyPhysical
        ? DockAvailabilityState::ClassRestricted
        : DockAvailabilityState::IncompatiblePhysical;
    return out;
}

DockingClearance DockTrafficController::admit(
    const DockingTrafficRequest& request
)
{
    DockingClearance out;
    const auto view = inquire(request);
    out.state = view.state;
    out.facilityId = view.facilityId;
    out.portalId = view.portalId;
    out.padId = view.padId;
    out.holdPointId = view.holdPointId;
    out.estimatedWaitSeconds = view.estimatedWaitSeconds;

    if (view.state != DockAvailabilityState::Available &&
        view.state != DockAvailabilityState::EmergencyAvailable)
    {
        return out;
    }

    if (!request.insideControlledApproachHorizon)
    {
        // Explicitly provisional: no token, no queue mutation, no reservation.
        return out;
    }

    const std::uint64_t token = m_impl->nextQueueToken++;
    m_impl->queue.push_back({token, request.shipId});

    out.committed = true;
    out.queueToken = token;
    out.clearanceRevision = m_impl->nextClearanceRevision++;
    out.queuePosition = m_impl->queue.size();
    out.state = DockAvailabilityState::Queued;
    out.allocationReason =
        view.state == DockAvailabilityState::EmergencyAvailable
            ? AllocationReason::EmergencyLargerClass
            : AllocationReason::NormalClass;

    for (const auto& portal : m_impl->facility.portals)
    {
        if (portal.id == out.portalId)
        {
            out.permittedTrafficResources.insert(
                out.permittedTrafficResources.end(),
                portal.conflictResourceIds.begin(),
                portal.conflictResourceIds.end()
            );
            break;
        }
    }
    for (const auto& pad : m_impl->facility.pads)
    {
        if (pad.id == out.padId)
        {
            out.permittedTrafficResources.insert(
                out.permittedTrafficResources.end(),
                pad.conflictResourceIds.begin(),
                pad.conflictResourceIds.end()
            );
            break;
        }
    }
    return out;
}

bool DockTrafficController::releaseQueueToken(std::uint64_t queueToken)
{
    const auto it = std::find_if(
        m_impl->queue.begin(),
        m_impl->queue.end(),
        [queueToken](const Impl::QueueEntry& entry)
        {
            return entry.token == queueToken;
        }
    );
    if (it == m_impl->queue.end())
        return false;

    m_impl->queue.erase(it);
    return true;
}

// ---------------------------------------------------------------------------
// Dispatcher lifecycle
// ---------------------------------------------------------------------------

const char* dockTrafficPhaseName(DockTrafficPhase phase) noexcept
{
    switch (phase)
    {
        case DockTrafficPhase::None: return "none";
        case DockTrafficPhase::Queued: return "queued";
        case DockTrafficPhase::HoldAssigned: return "hold-assigned";
        case DockTrafficPhase::AtHold: return "at-hold";
        case DockTrafficPhase::EntryCleared: return "entry-cleared";
        case DockTrafficPhase::StationControl: return "station-control";
        case DockTrafficPhase::Docked: return "docked";
        case DockTrafficPhase::DepartureQueued: return "departure-queued";
        case DockTrafficPhase::DepartureCleared: return "departure-cleared";
        case DockTrafficPhase::Departed: return "departed";
    }
    return "unknown";
}

DockingClearance DockTrafficController::requestDocking(
    const DockingTrafficRequest& request,
    double nowSeconds,
    double preferredHoldAzimuthRad,
    bool directEntry)
{
    if (m_impl->ships.count(request.shipId) != 0)
    {
        // Already known: report the current state, never queue twice.
        DockingClearance out;
        const auto& record = m_impl->ships.at(request.shipId);
        out.state = DockAvailabilityState::Queued;
        out.committed = true;
        out.queueToken = record.queueToken;
        out.facilityId = m_impl->facility.facilityId;
        out.holdPointId = record.hold;
        out.padId = record.pad;
        return out;
    }
    DockingClearance out = admit(request);
    if (!out.committed)
        return out;
    // The physical pad / hold come from the lifecycle, not from admit()'s
    // first compatible guess.
    out.padId.clear();
    out.holdPointId.clear();
    Impl::ShipRecord record;
    record.shipId = request.shipId;
    record.request = request;
    record.queueToken = out.queueToken;
    record.preferredHoldAzimuth = preferredHoldAzimuthRad;
    record.directEntry = directEntry;
    record.readySince = nowSeconds;
    m_impl->setPhase(record, DockTrafficPhase::Queued, nowSeconds);
    m_impl->ships[request.shipId] = record;
    if (directEntry)
        m_impl->arrivalQueue.push_front(request.shipId); // ahead of NPC holds
    else
        m_impl->arrivalQueue.push_back(request.shipId);
    std::ostringstream text;
    text << "docking request accepted, queue " << m_impl->arrivalQueue.size();
    m_impl->event(nowSeconds, request.shipId, text.str());
    return out;
}

bool DockTrafficController::reportAtHold(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::HoldAssigned)
        return false;
    m_impl->setPhase(it->second, DockTrafficPhase::AtHold, nowSeconds);
    it->second.readySince = nowSeconds;
    m_impl->event(nowSeconds, shipId, "at hold " + it->second.hold);
    return true;
}

bool DockTrafficController::reportInsideChamber(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::EntryCleared)
        return false;
    auto& record = it->second;
    m_impl->setPhase(record, DockTrafficPhase::StationControl, nowSeconds);
    if (!record.hold.empty())
    {
        m_impl->holdOwner[record.hold] = 0;
        m_impl->event(nowSeconds, shipId, "release hold " + record.hold);
        record.hold.clear();
    }
    for (const auto& id : m_impl->portalResources())
        if (id != "chamber_lane")
            m_impl->releaseResource(id.c_str(), shipId, nowSeconds);
    m_impl->event(nowSeconds, shipId, "station control (handoff) -> pad " + record.pad);
    return true;
}

bool DockTrafficController::reportLanded(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::StationControl)
        return false;
    m_impl->setPhase(it->second, DockTrafficPhase::Docked, nowSeconds);
    for (const auto& id : m_impl->portalResources())
        m_impl->releaseResource(id.c_str(), shipId, nowSeconds);
    m_impl->event(nowSeconds, shipId, "docked on pad " + it->second.pad);
    return true;
}

bool DockTrafficController::requestDeparture(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::Docked)
        return false;
    m_impl->setPhase(it->second, DockTrafficPhase::DepartureQueued, nowSeconds);
    it->second.readySince = nowSeconds;
    m_impl->departureQueue.push_back(shipId);
    m_impl->event(nowSeconds, shipId, "departure queued, position " + std::to_string(m_impl->departureQueue.size()));
    return true;
}

bool DockTrafficController::reportLeftChamber(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::DepartureCleared)
        return false;
    m_impl->releaseResource("chamber_lane", shipId, nowSeconds);
    return true;
}

bool DockTrafficController::reportClear(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end() || it->second.phase != DockTrafficPhase::DepartureCleared)
        return false;
    for (const auto& id : m_impl->portalResources())
        m_impl->releaseResource(id.c_str(), shipId, nowSeconds);
    if (!it->second.pad.empty())
    {
        m_impl->padOwner[it->second.pad] = 0;
        m_impl->event(nowSeconds, shipId, "release pad " + it->second.pad);
    }
    m_impl->event(nowSeconds, shipId, "departed, clear of the controlled zone");
    releaseQueueToken(it->second.queueToken);
    m_impl->ships.erase(it);
    return true;
}

bool DockTrafficController::cancel(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end())
        return false;
    auto& record = it->second;
    for (const auto& id : m_impl->portalResources())
        m_impl->releaseResource(id.c_str(), shipId, nowSeconds);
    if (!record.hold.empty())
        m_impl->holdOwner[record.hold] = 0;
    if (!record.pad.empty())
        m_impl->padOwner[record.pad] = 0;
    auto erase = [shipId](std::deque<std::uint64_t>& q) {
        q.erase(std::remove(q.begin(), q.end(), shipId), q.end());
    };
    erase(m_impl->arrivalQueue);
    erase(m_impl->departureQueue);
    releaseQueueToken(record.queueToken);
    m_impl->event(nowSeconds, shipId, "cancelled");
    m_impl->ships.erase(it);
    return true;
}

bool DockTrafficController::registerDocked(
    const DockingTrafficRequest& request,
    const std::string& padId,
    double nowSeconds)
{
    if (m_impl->ships.count(request.shipId) != 0)
        return false;
    const auto owner = m_impl->padOwner.find(padId);
    if (owner != m_impl->padOwner.end() && owner->second != 0)
        return false;
    const auto pad = std::find_if(m_impl->facility.pads.begin(), m_impl->facility.pads.end(),
        [&](const ParkingPadDescriptor& p) { return p.id == padId; });
    if (pad == m_impl->facility.pads.end() || m_impl->facility.portals.empty() ||
        !physicalFit(request.envelope, m_impl->facility.portals.front(), *pad))
        return false;
    Impl::ShipRecord record;
    record.shipId = request.shipId;
    record.request = request;
    record.pad = padId;
    record.readySince = nowSeconds;
    m_impl->setPhase(record, DockTrafficPhase::Docked, nowSeconds);
    m_impl->ships[request.shipId] = record;
    m_impl->padOwner[padId] = request.shipId;
    m_impl->event(nowSeconds, request.shipId, "registered docked on pad " + padId);
    return true;
}

void DockTrafficController::update(double nowSeconds)
{
    auto& impl = *m_impl;

    // 0. Direct-entry ships (no hold point): entry as soon as the lanes and a
    // pad are free; until then nothing else is cleared.
    Impl::ShipRecord* direct = nullptr;
    for (const std::uint64_t id : impl.arrivalQueue)
        if (impl.ships.at(id).directEntry)
        {
            direct = &impl.ships.at(id);
            break;
        }

    // 1. Physical holds in queue order (no overtaking).
    while (!impl.arrivalQueue.empty())
    {
        const std::uint64_t shipId = impl.arrivalQueue.front();
        auto& record = impl.ships.at(shipId);
        if (record.directEntry)
        {
            // Not a hold user: skip it for hold assignment.
            if (impl.arrivalQueue.size() < 2)
                break;
            const std::uint64_t next = impl.arrivalQueue[1];
            auto& nextRecord = impl.ships.at(next);
            const EntryHoldPointDescriptor* free = nullptr;
            for (const auto& hold : impl.facility.holdPoints)
            {
                const auto owner = impl.holdOwner.find(hold.id);
                if (owner == impl.holdOwner.end() || owner->second == 0)
                {
                    free = &hold;
                    break;
                }
            }
            if (!free || nextRecord.directEntry)
                break;
            impl.holdOwner[free->id] = next;
            nextRecord.hold = free->id;
            impl.setPhase(nextRecord, DockTrafficPhase::HoldAssigned, nowSeconds);
            impl.arrivalQueue.erase(impl.arrivalQueue.begin() + 1);
            impl.event(nowSeconds, next, "hold assigned " + free->id);
            continue;
        }
        const EntryHoldPointDescriptor* best = nullptr;
        double bestCost = 1.0e300;
        for (const auto& hold : impl.facility.holdPoints)
        {
            const auto owner = impl.holdOwner.find(hold.id);
            if (owner != impl.holdOwner.end() && owner->second != 0)
                continue;
            double cost = 0.0;
            if (record.preferredHoldAzimuth >= 0.0)
            {
                const double a = std::atan2(hold.positionLocalMeters.y, hold.positionLocalMeters.x);
                cost = std::abs(std::remainder(a - record.preferredHoldAzimuth, 2.0 * 3.14159265358979323846));
            }
            if (!best || cost < bestCost - 1.0e-9)
            {
                best = &hold;
                bestCost = cost;
            }
        }
        if (!best)
            break;
        impl.holdOwner[best->id] = shipId;
        record.hold = best->id;
        impl.setPhase(record, DockTrafficPhase::HoldAssigned, nowSeconds);
        impl.arrivalQueue.pop_front();
        impl.event(nowSeconds, shipId, "hold assigned " + best->id);
    }

    // 2. One movement through the shared lanes at a time, with one exception:
    // arrival pipelining. While the previous arrival is under station control
    // inside the chamber (outer lane already released), the next arrival may
    // leave its hold: its hold -> chamber-gate approach (>= ~100 s) outlasts
    // the station transfer (< 60 s), and the chamber lane is handed to it when
    // the previous ship latches. Departures still need both lanes free.
    if (impl.facility.portals.empty())
        return;
    bool pipelineArrival = false;
    if (!impl.resourcesFree(impl.portalResources()))
    {
        if (direct)
            return;
        bool outerFree = true;
        std::uint64_t chamberOwner = 0;
        for (const auto& id : impl.portalResources())
        {
            const auto it = impl.resourceOwner.find(id);
            const std::uint64_t owner = it == impl.resourceOwner.end() ? 0 : it->second;
            if (id == "chamber_lane")
                chamberOwner = owner;
            else if (owner != 0)
                outerFree = false;
        }
        const auto owner = impl.ships.find(chamberOwner);
        bool someoneAwaiting = false;
        for (const auto& [id, record] : impl.ships)
            someoneAwaiting = someoneAwaiting || record.awaitingChamber;
        if (!outerFree || owner == impl.ships.end() || owner->second.phase != DockTrafficPhase::StationControl ||
            someoneAwaiting)
            return;
        pipelineArrival = true;
    }
    const auto& portal = impl.facility.portals.front();

    auto freePadFor = [&](const Impl::ShipRecord& record) -> const ParkingPadDescriptor* {
        for (const auto& pad : impl.facility.pads)
        {
            const auto owner = impl.padOwner.find(pad.id);
            if (owner != impl.padOwner.end() && owner->second != 0)
                continue;
            if (!physicalFit(record.request.envelope, portal, pad) ||
                !classPolicyAllows(record.request.shipClass, pad.sizeClass, record.request.emergency))
                continue;
            return &pad;
        }
        return nullptr;
    };

    if (direct)
    {
        const auto* pad = freePadFor(*direct);
        if (!pad)
        {
            // No pad: let one departure out first, never another arrival.
            if (!impl.departureQueue.empty())
            {
                auto& dep = impl.ships.at(impl.departureQueue.front());
                for (const auto& id : impl.portalResources())
                    impl.resourceOwner[id] = dep.shipId;
                impl.setPhase(dep, DockTrafficPhase::DepartureCleared, nowSeconds);
                impl.departureQueue.pop_front();
                impl.event(nowSeconds, dep.shipId, "departure cleared from pad " + dep.pad + " (frees a pad for a direct entry)");
            }
            return;
        }
        for (const auto& id : impl.portalResources())
            impl.resourceOwner[id] = direct->shipId;
        impl.padOwner[pad->id] = direct->shipId;
        direct->pad = pad->id;
        impl.arrivalQueue.erase(std::remove(impl.arrivalQueue.begin(), impl.arrivalQueue.end(), direct->shipId),
                                impl.arrivalQueue.end());
        impl.setPhase(*direct, DockTrafficPhase::EntryCleared, nowSeconds);
        impl.event(nowSeconds, direct->shipId, "direct entry cleared to pad " + direct->pad);
        return;
    }

    Impl::ShipRecord* arrival = nullptr;
    const ParkingPadDescriptor* arrivalPad = nullptr;
    bool arrivalsWaitingWithoutPad = false;
    for (auto& [id, record] : impl.ships)
    {
        if (record.phase != DockTrafficPhase::AtHold)
            continue;
        const auto* pad = freePadFor(record);
        if (!pad)
        {
            arrivalsWaitingWithoutPad = true;
            continue;
        }
        if (!arrival || record.readySince < arrival->readySince)
        {
            arrival = &record;
            arrivalPad = pad;
        }
    }
    Impl::ShipRecord* departure = nullptr;
    if (!impl.departureQueue.empty())
        departure = &impl.ships.at(impl.departureQueue.front());

    const bool chooseDeparture = departure &&
        (!arrival || arrivalsWaitingWithoutPad || departure->readySince <= arrival->readySince);
    // A pipelined slot is only for an arrival that is next in line anyway: an
    // older departure waits for both lanes instead of being overtaken.
    if (pipelineArrival && (chooseDeparture || !arrival))
        return;
    if (chooseDeparture)
    {
        for (const auto& id : impl.portalResources())
            impl.resourceOwner[id] = departure->shipId;
        impl.setPhase(*departure, DockTrafficPhase::DepartureCleared, nowSeconds);
        impl.departureQueue.pop_front();
        impl.event(nowSeconds, departure->shipId, "departure cleared from pad " + departure->pad);
        return;
    }
    if (arrival)
    {
        for (const auto& id : impl.portalResources())
            if (!pipelineArrival || id != "chamber_lane")
                impl.resourceOwner[id] = arrival->shipId;
        arrival->awaitingChamber = pipelineArrival;
        impl.padOwner[arrivalPad->id] = arrival->shipId;
        arrival->pad = arrivalPad->id;
        impl.setPhase(*arrival, DockTrafficPhase::EntryCleared, nowSeconds);
        impl.event(nowSeconds, arrival->shipId,
            "entry cleared from hold " + arrival->hold + " to pad " + arrival->pad);
    }
}

DockTrafficShipStatus DockTrafficController::status(std::uint64_t shipId) const
{
    DockTrafficShipStatus out;
    out.shipId = shipId;
    out.queueLength = m_impl->arrivalQueue.size();
    const auto it = m_impl->ships.find(shipId);
    if (it == m_impl->ships.end())
        return out;
    const auto& record = it->second;
    out.phase = record.phase;
    out.holdPointId = record.hold.empty() && record.directEntry &&
                              (record.phase == DockTrafficPhase::Queued || record.phase == DockTrafficPhase::EntryCleared)
                          ? std::string("DIRECT")
                          : record.hold;
    out.padId = record.pad;
    out.revision = record.revision;
    out.phaseSinceSeconds = record.phaseSince;
    const auto& q = m_impl->arrivalQueue;
    const auto qi = std::find(q.begin(), q.end(), shipId);
    if (qi != q.end())
        out.queuePosition = static_cast<std::size_t>(qi - q.begin()) + 1;
    const auto& d = m_impl->departureQueue;
    const auto di = std::find(d.begin(), d.end(), shipId);
    if (di != d.end())
        out.departureQueuePosition = static_cast<std::size_t>(di - d.begin()) + 1;
    return out;
}

DockTrafficSnapshot DockTrafficController::snapshot() const
{
    DockTrafficSnapshot out;
    for (const auto& [id, record] : m_impl->ships)
        out.ships.push_back(status(id));
    for (const auto& hold : m_impl->facility.holdPoints)
    {
        const auto it = m_impl->holdOwner.find(hold.id);
        out.holds.emplace_back(hold.id, it == m_impl->holdOwner.end() ? 0 : it->second);
    }
    for (const auto& pad : m_impl->facility.pads)
    {
        const auto it = m_impl->padOwner.find(pad.id);
        out.pads.emplace_back(pad.id, it == m_impl->padOwner.end() ? 0 : it->second);
    }
    for (const auto& id : m_impl->portalResources())
    {
        const auto it = m_impl->resourceOwner.find(id);
        out.resources.emplace_back(id, it == m_impl->resourceOwner.end() ? 0 : it->second);
    }
    out.arrivalQueueLength = m_impl->arrivalQueue.size();
    out.departureQueueLength = m_impl->departureQueue.size();
    return out;
}

std::string DockTrafficController::validate() const
{
    const auto& impl = *m_impl;
    std::map<std::string, int> holdClaims, padClaims;
    for (const auto& [id, record] : impl.ships)
    {
        if (!record.hold.empty())
        {
            if (++holdClaims[record.hold] > 1)
                return "hold " + record.hold + " claimed by two ships";
            const auto owner = impl.holdOwner.find(record.hold);
            if (owner == impl.holdOwner.end() || owner->second != id)
                return "hold " + record.hold + " owner differs from ship " + std::to_string(id);
            if (record.phase != DockTrafficPhase::HoldAssigned && record.phase != DockTrafficPhase::AtHold &&
                record.phase != DockTrafficPhase::EntryCleared)
                return "ship " + std::to_string(id) + " keeps hold in phase " + dockTrafficPhaseName(record.phase);
        }
        if (!record.pad.empty())
        {
            if (++padClaims[record.pad] > 1)
                return "pad " + record.pad + " claimed by two ships";
            const auto owner = impl.padOwner.find(record.pad);
            if (owner == impl.padOwner.end() || owner->second != id)
                return "pad " + record.pad + " owner differs from ship " + std::to_string(id);
        }
    }
    for (const auto& [hold, owner] : impl.holdOwner)
        if (owner != 0 && (impl.ships.count(owner) == 0 || impl.ships.at(owner).hold != hold))
            return "hold " + hold + " owned by a ship that does not hold it";
    for (const auto& [pad, owner] : impl.padOwner)
        if (owner != 0 && (impl.ships.count(owner) == 0 || impl.ships.at(owner).pad != pad))
            return "pad " + pad + " owned by a ship that does not hold it";
    for (const auto& [resource, owner] : impl.resourceOwner)
    {
        if (owner == 0)
            continue;
        const auto it = impl.ships.find(owner);
        if (it == impl.ships.end())
            return "resource " + resource + " owned by an unknown ship";
        const auto phase = it->second.phase;
        if (phase != DockTrafficPhase::EntryCleared && phase != DockTrafficPhase::StationControl &&
            phase != DockTrafficPhase::DepartureCleared)
            return "resource " + resource + " held in phase " + dockTrafficPhaseName(phase);
    }
    return {};
}

std::vector<DockTrafficEvent> DockTrafficController::drainEvents()
{
    std::vector<DockTrafficEvent> out;
    out.swap(m_impl->events);
    return out;
}

} // namespace game::docking::traffic
