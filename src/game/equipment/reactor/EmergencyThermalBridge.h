#pragma once

#include <algorithm>
#include <cmath>

namespace game::equipment::reactor
{
// Normally isolated, heat-only emergency link. This is a physical
// component model, NOT an installed module or an electrical connection.
struct EmergencyThermalBridgeDescriptor
{
    double maxTransferMW = 60.0;
    double conductanceMWperK = 0.25;
    double pumpTransferMWPerElectricalMW = 20.0;
    double maxPumpElectricalMW = 3.0;
    double engageSourceK = 1950.0;
    double releaseSourceK = 1700.0;
};
enum class EmergencyThermalBridgeMode : unsigned char
{
    Off, Auto, ManualOpen
};
struct EmergencyThermalBridgeState
{
    bool open = false;
    double lastTransferMW = 0.0;
    double lastPumpElectricalMW = 0.0;
};
struct EmergencyThermalBridgeInput
{
    double sourceK = 0.0;
    double sinkK = 0.0;
    double authority01 = 0.0;
    double pumpPowerAvailableMW = 0.0;
    double sinkHeadroomMW = 0.0;
    double sourceExtractableMW = 0.0;
    double sinkOpenBelowK = 0.0;
    double sinkCloseAboveK = 0.0;
};
struct EmergencyThermalBridgeTick
{
    double transferMW = 0.0;
    double pumpElectricalMW = 0.0;
    bool open = false;
    bool interlocked = false;
};
[[nodiscard]] inline EmergencyThermalBridgeTick advanceEmergencyThermalBridge(
    const EmergencyThermalBridgeDescriptor& d,
    EmergencyThermalBridgeState& state,
    EmergencyThermalBridgeMode mode,
    const EmergencyThermalBridgeInput& in) noexcept
{
    EmergencyThermalBridgeTick out;
    const bool finite =
        std::isfinite(in.sourceK) && std::isfinite(in.sinkK) &&
        std::isfinite(in.authority01) &&
        std::isfinite(in.pumpPowerAvailableMW) &&
        std::isfinite(in.sinkHeadroomMW) &&
        std::isfinite(in.sourceExtractableMW) &&
        std::isfinite(in.sinkOpenBelowK) &&
        std::isfinite(in.sinkCloseAboveK);
    const bool validHardware = std::isfinite(d.maxTransferMW) &&
        std::isfinite(d.conductanceMWperK) &&
        std::isfinite(d.pumpTransferMWPerElectricalMW) &&
        d.maxTransferMW>0.0 && d.conductanceMWperK>0.0 &&
        d.pumpTransferMWPerElectricalMW>0.0 &&
        std::isfinite(d.maxPumpElectricalMW) && d.maxPumpElectricalMW>0.0;
    const double auth = finite ? std::clamp(in.authority01,0.0,1.0) : 0.0;
    const double availablePumpMW = validHardware && finite
        ? std::clamp(in.pumpPowerAvailableMW,0.0,d.maxPumpElectricalMW)
        : 0.0;
    out.interlocked = !finite || !validHardware ||
        mode == EmergencyThermalBridgeMode::Off ||
        auth<=0.0 || availablePumpMW<=0.0 ||
        in.sinkK>=in.sinkCloseAboveK;
    if(out.interlocked) state.open=false;
    else if(!state.open)
    {
        const bool requested = mode==EmergencyThermalBridgeMode::ManualOpen ||
            (mode==EmergencyThermalBridgeMode::Auto &&
             in.sourceK>=d.engageSourceK);
        state.open=requested && in.sinkK<=in.sinkOpenBelowK;
    }
    else if(mode==EmergencyThermalBridgeMode::Auto &&
            in.sourceK<=d.releaseSourceK)
        state.open=false;

    if(state.open)
        out.transferMW=std::max(0.0,std::min({
            d.maxTransferMW*auth,
            d.conductanceMWperK*auth*std::max(0.0,in.sourceK-in.sinkK),
            availablePumpMW*d.pumpTransferMWPerElectricalMW*auth,
            std::max(0.0,in.sinkHeadroomMW),
            std::max(0.0,in.sourceExtractableMW)}));
    out.pumpElectricalMW=out.transferMW>0.0
        ? out.transferMW/(d.pumpTransferMWPerElectricalMW*auth):0.0;
    out.open=state.open;
    state.lastTransferMW=out.transferMW;
    state.lastPumpElectricalMW=out.pumpElectricalMW;
    return out;
}
} // namespace game::equipment::reactor
