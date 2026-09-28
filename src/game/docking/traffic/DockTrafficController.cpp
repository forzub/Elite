#include "src/game/docking/traffic/DockTrafficControllerApi.h"

#include <algorithm>
#include <cmath>
#include <deque>
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

} // namespace game::docking::traffic
