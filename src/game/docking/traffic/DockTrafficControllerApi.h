#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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

    [[nodiscard]] static bool physicalFit(
        const DockingEnvelope& ship,
        const AccessPortalDescriptor& portal,
        const ParkingPadDescriptor& pad
    ) noexcept;

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
