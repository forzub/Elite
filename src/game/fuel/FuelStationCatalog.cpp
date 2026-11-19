#include "src/game/fuel/FuelStationCatalog.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace game::fuel
{
namespace
{
double number(const nlohmann::json& j, const char* key, double fallback)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number() ? it->get<double>() : fallback;
}
} // namespace

bool FuelStationCatalog::loadFromText(const std::string& text)
{
    m_stations.clear();
    m_index.clear();
    m_loaded = false;
    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(text);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fuel] fuel_stations.json: " << e.what() << '\n';
        return false;
    }
    if (!root.contains("stations") || !root["stations"].is_array())
        return false;
    for (const auto& s : root["stations"])
    {
        FuelStation f;
        f.id = s.value("id", std::string());
        if (f.id.empty() || m_index.count(f.id))
            continue;
        f.name = s.value("name", f.id);
        f.systemId = s.contains("system_id") && s["system_id"].is_number_integer() ? s["system_id"].get<int>() : -1;
        f.kind = s.value("kind", std::string());
        f.legality = s.value("legality", std::string("legal"));
        f.illegal = s.value("illegal", false);
        f.proposed = s.value("proposed", false);
        if (const auto st = s.find("stock_t"); st != s.end() && st->is_object())
            f.stockT = number(*st, "jump_charge", 0.0);
        if (const auto pr = s.find("price_hint_cr_per_t"); pr != s.end() && pr->is_object())
            f.priceHintCrPerT = number(*pr, "jump_charge", 0.0);
        f.pump.rateTPerMin = number(s, "rate_t_per_min", f.pump.rateTPerMin);
        f.pump.hookupS = number(s, "hookup_s", f.pump.hookupS);
        f.pump.disconnectS = number(s, "disconnect_s", f.pump.disconnectS);
        f.priceFactor = number(s, "price_factor", 1.0);
        f.risk = number(s, "risk", 0.0);
        m_index.emplace(f.id, m_stations.size());
        m_stations.push_back(std::move(f));
    }
    m_loaded = !m_stations.empty();
    return m_loaded;
}

const FuelStationCatalog& FuelStationCatalog::instance()
{
    static const FuelStationCatalog catalog = [] {
        FuelStationCatalog c;
        for (const char* path : {"assets/data/fuel/fuel_stations.json", "src/assets/data/fuel/fuel_stations.json"})
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                continue;
            std::stringstream text;
            text << file.rdbuf();
            if (c.loadFromText(text.str()))
            {
                std::cerr << "[Fuel] " << c.stations().size() << " fuel stations from " << path << '\n';
                return c;
            }
        }
        std::cerr << "[Fuel] fuel_stations.json missing: no station sells fuel\n";
        return c;
    }();
    return catalog;
}

const FuelStation* FuelStationCatalog::find(const std::string& hubId) const
{
    const auto it = m_index.find(hubId);
    return it == m_index.end() ? nullptr : &m_stations[it->second];
}

bool FuelStationCatalog::sellsFuel(const std::string& hubId) const
{
    const FuelStation* s = find(hubId);
    return s && !s->illegal && s->stockT > 0.0;
}
} // namespace game::fuel
