#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace game::bite
{

enum class Severity : std::uint8_t
{
    Advisory = 0,
    Caution,
    Warning,
    Failed
};

enum class FaultKind : std::uint8_t
{
    FunctionalDegradation = 0,
    NozzleDegradation,
    MaintenanceRequired,
    EquipmentFailed
};

struct EquipmentSample
{
    std::string equipmentId;
    std::string displayName;

    double functionalAuthority01 = 1.0;
    double secondaryAuthority01 = 1.0;

    // Relative to healthy operation for the same useful output.
    double powerCostMultiplier = 1.0;
    double heatMultiplier = 0.0;

    double wear01 = 0.0;
    double maintenanceWearThreshold01 = 0.20;

    // Physical equipment availability is separate from function failure.
    // An installed engine may keep axial thrust while its TVC function fails,
    // or lose thrust while the module itself remains present.
    bool operational = true;
    bool functionalFailed = false;
    bool secondaryFailed = false;
};

struct Fault
{
    std::string equipmentId;
    std::string displayName;
    FaultKind kind = FaultKind::FunctionalDegradation;
    Severity severity = Severity::Advisory;

    double functionalAuthority01 = 1.0;
    double secondaryAuthority01 = 1.0;
    double powerIncrease01 = 0.0;
    double heatIncrease01 = 0.0;
    double wear01 = 0.0;

    bool maintenanceRequired = false;
    bool hudAttention = false;
};

struct Summary
{
    std::uint32_t equipmentCount = 0;
    std::uint32_t degradedCount = 0;
    std::uint32_t failedCount = 0;
    std::uint32_t maintenanceRequiredCount = 0;
    Severity highestSeverity = Severity::Advisory;

    [[nodiscard]] bool nominal() const noexcept
    {
        return degradedCount == 0 &&
               failedCount == 0 &&
               maintenanceRequiredCount == 0;
    }
};

struct State
{
    std::uint64_t sourceRevision = 0;
    Summary summary;
    std::vector<Fault> faults;
};

} // namespace game::bite
