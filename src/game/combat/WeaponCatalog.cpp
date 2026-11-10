#include "src/game/combat/WeaponCatalog.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace game::combat
{
namespace
{
using json = nlohmann::json;

WeaponCatalog load()
{
    WeaponCatalog cat;
    std::ifstream in("assets/data/combat/weapons.json");
    if (!in)
        in.open("src/assets/data/combat/weapons.json");
    json j;
    try
    {
        if (in)
            in >> j;
    }
    catch (...)
    {
        j = json();
    }
    if (j.is_object())
    {
        const json m = j.value("materials", json::object());
        cat.materials.reflectArmourScale = m.value("reflect_armour_scale", cat.materials.reflectArmourScale);
        cat.materials.coolingMW = m.value("cooling_mw", cat.materials.coolingMW);
        cat.materials.burnMJPerPen = m.value("burn_mj_per_pen", cat.materials.burnMJPerPen);
        cat.materials.healthPerMJ = m.value("health_per_mj", cat.materials.healthPerMJ);
        cat.materials.slugPenFactor = m.value("slug_pen_factor", cat.materials.slugPenFactor);
        cat.materials.spotMergeM = m.value("spot_merge_m", cat.materials.spotMergeM);
        for (const auto& l : j.value("lasers", json::array()))
        {
            LaserSpec s;
            s.id = l.value("id", std::string());
            s.name = l.value("name", s.id);
            s.powerMW = l.value("power_mw", s.powerMW);
            s.duty = l.value("duty", s.duty);
            s.pulseHz = l.value("pulse_hz", s.pulseHz);
            s.rangeM = l.value("range_km", s.rangeM / 1000.0) * 1000.0;
            s.beamRadiusM = l.value("beam_radius_m", s.beamRadiusM);
            s.efficiency = l.value("efficiency", s.efficiency);
            s.heatCapacityMJperK = l.value("heat_capacity_mj_per_k", s.heatCapacityMJperK);
            s.maxTemperatureK = l.value("max_temperature_k", s.maxTemperatureK);
            s.coolingMWperK = l.value("cooling_mw_per_k", s.coolingMWperK);
            s.capacitorMJ = l.value("capacitor_mj", s.capacitorMJ);
            s.chargeMW = l.value("charge_mw", s.chargeMW);
            s.dischargeS = l.value("discharge_s", s.dischargeS);
            if (l.contains("colour") && l["colour"].size() == 3)
                s.colour = glm::vec3(l["colour"][0].get<float>(), l["colour"][1].get<float>(), l["colour"][2].get<float>());
            cat.lasers.push_back(s);
        }
        for (const auto& mj : j.value("missiles", json::array()))
        {
            MissileSpec s;
            s.id = mj.value("id", std::string());
            s.name = mj.value("name", s.id);
            s.warhead = mj.value("warhead", std::string("shaped")) == "slug" ? WarheadKind::Slug : WarheadKind::Shaped;
            s.jetMJ = mj.value("jet_mj", 0.0);
            s.slugKg = mj.value("slug_kg", 0.0);
            s.massKg = mj.value("mass_kg", s.massKg);
            s.accelMps2 = mj.value("accel_mps2", s.accelMps2);
            s.maxSpeedMps = mj.value("max_speed_mps", s.maxSpeedMps);
            s.fuelS = mj.value("fuel_s", s.fuelS);
            s.turnDps = mj.value("turn_dps", s.turnDps);
            s.seekerDeg = mj.value("seeker_deg", s.seekerDeg);
            s.rangeM = mj.value("range_km", s.rangeM / 1000.0) * 1000.0;
            cat.missiles.push_back(s);
        }
        const json d = j.value("default_loadout", json::object());
        cat.defaultLaser = d.value("laser", std::string());
        cat.defaultMissile = d.value("missile", std::string());
        cat.defaultMissiles = d.value("missiles", cat.defaultMissiles);
    }
    if (cat.lasers.empty())
    {
        LaserSpec s;
        s.id = "laser.pulse_1";
        s.name = "laser";
        cat.lasers.push_back(s);
        std::cerr << "[Combat] weapons.json missing: built-in laser only" << std::endl;
    }
    if (cat.missiles.empty())
    {
        MissileSpec s;
        s.id = "missile.needle";
        s.name = "missile";
        s.jetMJ = 30.0;
        cat.missiles.push_back(s);
    }
    if (!cat.laser(cat.defaultLaser))
        cat.defaultLaser = cat.lasers.front().id;
    if (!cat.missile(cat.defaultMissile))
        cat.defaultMissile = cat.missiles.front().id;
    return cat;
}
} // namespace

const LaserSpec* WeaponCatalog::laser(const std::string& id) const
{
    // the former ids stay valid (ship passports): pulse -> Pulsar, beam -> Squall, military -> Discharger
    const std::string key = id == "laser.pulse_1" ? "laser.pulsar" : id == "laser.beam_4" ? "laser.squall"
                          : id == "laser.military_10" ? "laser.discharger" : id;
    for (const auto& l : lasers)
        if (l.id == key || l.id == id)
            return &l;
    return nullptr;
}

const MissileSpec* WeaponCatalog::missile(const std::string& id) const
{
    for (const auto& m : missiles)
        if (m.id == id)
            return &m;
    return nullptr;
}

const WeaponCatalog& weaponCatalog()
{
    static const WeaponCatalog catalog = load();
    return catalog;
}
} // namespace game::combat
