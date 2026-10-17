#include "src/game/bite/BiteApi.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace game::bite
{
namespace
{

double clamp01(double v) noexcept
{
    return std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.0;
}

Severity maxSeverity(Severity a, Severity b) noexcept
{
    return static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b)
        ? a : b;
}

Severity severityForLoss(
    double loss01,
    const Thresholds& thresholds
) noexcept
{
    if (loss01 >= 1.0 - 1.0e-6)
        return Severity::Failed;
    if (loss01 >= thresholds.warningLoss01)
        return Severity::Warning;
    if (loss01 >= thresholds.cautionLoss01)
        return Severity::Caution;
    return Severity::Advisory;
}

std::string percent(double fraction)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(0)
        << clamp01(fraction) * 100.0 << "%";
    return out.str();
}

std::string increasePercent(double multiplier)
{
    if (!std::isfinite(multiplier) || multiplier <= 1.0)
        return "0%";
    std::ostringstream out;
    out << std::fixed << std::setprecision(0)
        << (multiplier - 1.0) * 100.0 << "%";
    return out.str();
}

const char* severityText(Severity severity) noexcept
{
    switch (severity)
    {
        case Severity::Advisory: return "ADVISORY";
        case Severity::Caution: return "CAUTION";
        case Severity::Warning: return "WARNING";
        case Severity::Failed: return "FAILED";
    }
    return "ADVISORY";
}

} // namespace

BiteApi::BiteApi(Thresholds thresholds) noexcept
    : m_thresholds(thresholds)
{
    m_thresholds.cautionLoss01 =
        clamp01(m_thresholds.cautionLoss01);
    m_thresholds.warningLoss01 =
        std::max(
            m_thresholds.cautionLoss01,
            clamp01(m_thresholds.warningLoss01)
        );
}

State BiteApi::evaluate(
    std::uint64_t sourceRevision,
    const std::vector<EquipmentSample>& equipment
) const
{
    State out;
    out.sourceRevision = sourceRevision;

    for (const auto& sample : equipment)
    {
        const double functionalLoss =
            1.0 - clamp01(sample.functionalAuthority01);
        const double secondaryLoss =
            1.0 - clamp01(sample.secondaryAuthority01);

        const bool maintenance =
            sample.maintenanceWearThreshold01 > 0.0 &&
            clamp01(sample.wear01) >=
                clamp01(sample.maintenanceWearThreshold01);

        const bool functionalFault =
            !sample.operational ||
            functionalLoss >= m_thresholds.cautionLoss01;
        const bool secondaryFault =
            sample.operational &&
            secondaryLoss >= m_thresholds.cautionLoss01;

        if (!functionalFault && !secondaryFault && !maintenance)
            continue;

        Fault fault;
        fault.equipmentId = sample.equipmentId;
        fault.displayName = sample.displayName.empty()
            ? sample.equipmentId
            : sample.displayName;
        fault.functionalAuthority01 =
            clamp01(sample.functionalAuthority01);
        fault.secondaryAuthority01 =
            clamp01(sample.secondaryAuthority01);
        fault.powerIncrease01 =
            std::max(0.0, sample.powerCostMultiplier - 1.0);
        fault.heatIncrease01 =
            std::max(0.0, sample.heatMultiplier);
        fault.wear01 = clamp01(sample.wear01);
        fault.maintenanceRequired = maintenance;

        if (!sample.operational)
        {
            fault.kind = FaultKind::EquipmentFailed;
            fault.severity = Severity::Failed;
        }
        else if (functionalFault)
        {
            fault.kind = FaultKind::FunctionalDegradation;
            fault.severity =
                severityForLoss(functionalLoss, m_thresholds);
        }
        else if (secondaryFault)
        {
            fault.kind = FaultKind::NozzleDegradation;
            fault.severity =
                severityForLoss(secondaryLoss, m_thresholds);
        }
        else
        {
            fault.kind = FaultKind::MaintenanceRequired;
            fault.severity = Severity::Advisory;
        }

        fault.hudAttention =
            fault.severity == Severity::Caution ||
            fault.severity == Severity::Warning ||
            fault.severity == Severity::Failed;

        out.highestSeverity =
            maxSeverity(out.highestSeverity, fault.severity);
        out.faults.push_back(std::move(fault));
    }

    return out;
}

std::vector<std::string> BiteApi::terminalLines(
    const State& state
)
{
    std::vector<std::string> out;
    out.reserve(state.faults.size());

    for (const auto& fault : state.faults)
    {
        std::ostringstream line;
        line << fault.displayName << " - "
             << severityText(fault.severity);

        switch (fault.kind)
        {
            case FaultKind::EquipmentFailed:
                line << ": equipment inoperative";
                break;

            case FaultKind::FunctionalDegradation:
                line << ": available output "
                     << percent(fault.functionalAuthority01);
                break;

            case FaultKind::NozzleDegradation:
                line << ": nozzle authority "
                     << percent(fault.secondaryAuthority01);
                break;

            case FaultKind::MaintenanceRequired:
                line << ": maintenance required, wear "
                     << percent(fault.wear01);
                break;
        }

        if (fault.powerIncrease01 > 0.0)
            line << ", power +" << increasePercent(
                1.0 + fault.powerIncrease01);
        if (fault.heatIncrease01 > 0.0)
            line << ", heat +" << percent(fault.heatIncrease01);

        out.push_back(line.str());
    }

    return out;
}

} // namespace game::bite
