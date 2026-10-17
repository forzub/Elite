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

    bool operational = true;
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
};

struct State
{
    std::uint64_t sourceRevision = 0;
    Severity highestSeverity = Severity::Advisory;
    std::vector<Fault> faults;
};

} // namespace game::bite
