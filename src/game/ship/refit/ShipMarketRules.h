#pragma once

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Data-driven market / service price rules (assets/data/ships/ship_market_rules.json):
// depreciation of ships, seller categories, repair labour and material. The
// prices themselves come from the design passports; this file only turns
// them into market and service prices. Loaded once, never destroyed.
namespace game::ship::refit
{
struct ShipSellerRule
{
    std::string id;
    std::string nameRu;
    std::string nameEn;
    double priceX = 1.0;
    double ageMinYears = 0.0, ageMaxYears = 0.0;
    double wearMin01 = 0.0, wearMax01 = 0.0;
    double damageMin01 = 0.0, damageMax01 = 0.0;
};

struct ShipMarketRules
{
    double firstSaleLoss01 = 0.20;
    double ageHalflifeYears = 14.0;
    double hoursHalflife = 60000.0;
    double wearValueLoss01 = 0.6;
    double scrapCrPerTonne = 180.0;
    double salvageComponentShare01 = 0.25;
    std::vector<ShipSellerRule> sellers;
    std::map<std::string, double> labourCrPerHour {{"crew", 60.0}, {"station", 140.0}, {"yard", 320.0}, {"factory", 900.0}};
    std::map<std::string, double> defaultHours {{"crew", 2.0}, {"station", 6.0}, {"yard", 24.0}, {"factory", 120.0}};
    double repairMaterialShare01 = 0.55;
    double replaceLabourX = 1.0;

    [[nodiscard]] static const ShipMarketRules& instance()
    {
        static const ShipMarketRules* rules = [] {
            auto* r = new ShipMarketRules();
            r->load("assets/data/ships/ship_market_rules.json") || r->load("src/assets/data/ships/ship_market_rules.json");
            return r;
        }();
        return *rules;
    }

    bool load(const std::string& path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        nlohmann::json j;
        try
        {
            in >> j;
        }
        catch (...)
        {
            return false;
        }
        const nlohmann::json dep = j.value("depreciation", nlohmann::json::object());
        firstSaleLoss01 = dep.value("first_sale_loss01", firstSaleLoss01);
        ageHalflifeYears = dep.value("age_halflife_years", ageHalflifeYears);
        hoursHalflife = dep.value("hours_halflife", hoursHalflife);
        wearValueLoss01 = dep.value("wear_value_loss01", wearValueLoss01);
        scrapCrPerTonne = dep.value("scrap_cr_per_tonne", scrapCrPerTonne);
        salvageComponentShare01 = dep.value("salvage_component_share01", salvageComponentShare01);
        sellers.clear();
        for (const auto& s : j.value("sellers", nlohmann::json::array()))
        {
            ShipSellerRule r;
            r.id = s.value("id", std::string());
            r.nameRu = s.value("name_ru", r.id);
            r.nameEn = s.value("name_en", r.id);
            r.priceX = s.value("price_x", 1.0);
            const auto range = [&](const char* key, double& lo, double& hi) {
                const nlohmann::json a = s.value(key, nlohmann::json::array());
                if (a.size() == 2)
                    lo = a[0].get<double>(), hi = a[1].get<double>();
            };
            range("age_years", r.ageMinYears, r.ageMaxYears);
            range("wear01", r.wearMin01, r.wearMax01);
            range("damage01", r.damageMin01, r.damageMax01);
            sellers.push_back(std::move(r));
        }
        const nlohmann::json rep = j.value("repair", nlohmann::json::object());
        const nlohmann::json labour = rep.value("labour_cr_per_hour", nlohmann::json::object());
        for (const auto& [k, v] : labour.items())
            labourCrPerHour[k] = v.get<double>();
        const nlohmann::json hours = rep.value("default_hours", nlohmann::json::object());
        for (const auto& [k, v] : hours.items())
            defaultHours[k] = v.get<double>();
        repairMaterialShare01 = rep.value("repair_material_share01", repairMaterialShare01);
        replaceLabourX = rep.value("replace_labour_x", replaceLabourX);
        return true;
    }
};
} // namespace game::ship::refit
