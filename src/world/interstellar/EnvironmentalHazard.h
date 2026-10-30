#pragma once

#include <string>
#include <vector>

namespace world::interstellar
{

/*
    Environmental traversal hazard of an interstellar medium: a DERIVED
    presentation of physical quantities (never a stored property, never
    "dangerous = true"). It says how harsh the environment itself is -
    gas column per distance, dust, solid bodies, plasma, turbulence. The
    risk for a particular ship at a particular speed is a separate,
    future quantity (RouteRiskForShip: velocity, shielding, cross-section),
    computed by the interstellar planner from the same physics.
*/
enum class EnvironmentalHazardLevel
{
    Negligible = 0,
    Low,
    Moderate,
    High,
    Severe
};

struct EnvironmentalHazardInput
{
    double gasDensityCm3 = 0.0;          // peak hydrogen density
    double temperatureK = 0.0;
    double ionizationFraction = 0.0;
    double dustToGas = 0.0;
    double turbulenceKms = 0.0;
    // Solid bodies (icy debris fields): number densities per AU^3.
    double solidsAbove1CmPerAu3 = 0.0;
    double solidsAbove10mPerAu3 = 0.0;
};

struct EnvironmentalHazard
{
    EnvironmentalHazardLevel level = EnvironmentalHazardLevel::Negligible;
    std::vector<std::string> reasons;    // short physical causes
};

inline const char* environmentalHazardName(EnvironmentalHazardLevel level)
{
    switch (level)
    {
    case EnvironmentalHazardLevel::Negligible: return "NEGLIGIBLE";
    case EnvironmentalHazardLevel::Low: return "LOW";
    case EnvironmentalHazardLevel::Moderate: return "MODERATE";
    case EnvironmentalHazardLevel::High: return "HIGH";
    default: return "SEVERE";
    }
}

/*
    Bands (environment only): gas density decades from the diffuse warm
    medium (~0.1-1 cm^-3) to dense molecular cores (>1e3); dust follows the
    gas with its dust-to-gas ratio (dust-rich: >= 1 %); solid bodies above 1 cm
    are the impact population of an icy field; plasma from the electron
    density. The level is the worst of the contributions.
*/
inline EnvironmentalHazard environmentalHazard(const EnvironmentalHazardInput& in)
{
    EnvironmentalHazard out;
    const auto raise = [&](EnvironmentalHazardLevel level, const char* reason) {
        if (static_cast<int>(level) > static_cast<int>(out.level))
            out.level = level;
        if (static_cast<int>(level) >= static_cast<int>(EnvironmentalHazardLevel::Low))
            out.reasons.push_back(reason);
    };
    const double n = in.gasDensityCm3;
    if (n >= 1000.0)
        raise(EnvironmentalHazardLevel::Severe, "very dense molecular gas");
    else if (n >= 100.0)
        raise(EnvironmentalHazardLevel::High, "dense molecular gas");
    else if (n >= 10.0)
        raise(EnvironmentalHazardLevel::Moderate, "cold dense gas");
    else if (n >= 1.0)
        raise(EnvironmentalHazardLevel::Low, "neutral gas");
    if (n * in.dustToGas >= 1.0)
        raise(EnvironmentalHazardLevel::High, "dense dust");
    else if (n * in.dustToGas >= 0.1)
        raise(EnvironmentalHazardLevel::Moderate, "dust");
    if (in.solidsAbove10mPerAu3 >= 1.0)
        raise(EnvironmentalHazardLevel::Severe, "icy bodies above 10 m");
    else if (in.solidsAbove1CmPerAu3 >= 1.0e6)
        raise(EnvironmentalHazardLevel::High, "icy fragments");
    else if (in.solidsAbove1CmPerAu3 > 0.0)
        raise(EnvironmentalHazardLevel::Moderate, "sparse icy fragments");
    if (n * in.ionizationFraction >= 1.0)
        raise(EnvironmentalHazardLevel::Moderate, "dense plasma");
    if (in.turbulenceKms >= 10.0 && n >= 1.0)
        raise(EnvironmentalHazardLevel::Moderate, "turbulent");
    if (out.level == EnvironmentalHazardLevel::Negligible)
        out.reasons.push_back(in.temperatureK > 1.0e5 ? "hot tenuous plasma, negligible solids"
                                                      : "diffuse gas, negligible solids");
    return out;
}

// Future seam: the risk for a given ship and velocity (impact energy flux,
// shielding, sensor extinction) belongs to the interstellar planner; the map
// shows "not computed" until a ship profile and planned velocity exist.
struct RouteRiskForShip
{
    bool computed = false;
};

} // namespace world::interstellar
