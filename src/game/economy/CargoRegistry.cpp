#include "src/game/economy/CargoRegistry.h"

#include "src/game/localization/LocalizedMessage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <nlohmann/json.hpp>

namespace game::economy
{
double c20NetTonnes(double densityTM3)
{
    if (!(densityTM3 > 0.0))
        return C20PayloadT;
    return std::min(C20PayloadT, densityTM3 * C20VolumeM3);
}

int c20For(double tonnes, double densityTM3)
{
    if (!(tonnes > 1.0e-9))
        return 0;
    return std::max(1, static_cast<int>(std::ceil(tonnes / c20NetTonnes(densityTM3) - 1.0e-6)));
}

double CargoManifest::netT() const
{
    double t = 0.0;
    for (const auto& l : lots)
        t += l.tonnes;
    return t;
}

int CargoManifest::c20() const
{
    int n = 0;
    for (const auto& l : lots)
        n += l.c20;
    return n;
}

double CargoManifest::ownTonnesOf(const std::string& goodId) const
{
    double t = 0.0;
    for (const auto& l : lots)
        if (l.goodId == goodId && l.contractId.empty())
            t += l.tonnes;
    return t;
}

const CargoManifest* CargoRegistry::find(std::uint64_t shipKey) const
{
    const auto it = m_manifests.find(shipKey);
    return it == m_manifests.end() ? nullptr : &it->second;
}

CargoManifest& CargoRegistry::manifest(std::uint64_t shipKey, std::uint64_t playerId)
{
    auto& m = m_manifests[shipKey];
    m.shipKey = shipKey;
    if (playerId != 0)
        m.playerId = playerId;
    return m;
}

namespace
{
bool mergeable(const CargoLot& a, const CargoLot& b)
{
    return a.goodId == b.goodId && a.contractId.empty() && b.contractId.empty() && a.originHub == b.originHub &&
           a.loadedDay == b.loadedDay && a.owner == b.owner && a.declared == b.declared;
}
} // namespace

int CargoRegistry::extraC20(const CargoManifest& m, const CargoLot& lot, double densityTM3)
{
    for (const auto& l : m.lots)
        if (mergeable(l, lot))
            return c20For(l.tonnes + lot.tonnes, densityTM3) - l.c20;
    return c20For(lot.tonnes, densityTM3);
}

std::string CargoRegistry::fits(const CargoManifest& m, const CargoCapacity& cap, double tonnes, int c20)
{
    using game::localization::Message;
    if (m.c20() + c20 > cap.c20Slots)
        return Message("economy.cargo.no_c20_space")
            .arg("need", c20)
            .arg("free", std::max(0, cap.c20Slots - m.c20()))
            .arg("total", cap.c20Slots);
    if (m.netT() + tonnes > cap.netT + 1.0e-6)
        return Message("economy.cargo.over_capacity")
            .arg("aboard", m.netT(), 1)
            .arg("adding", tonnes, 1)
            .arg("capacity", cap.netT, 0);
    return {};
}

std::string CargoRegistry::load(std::uint64_t shipKey, std::uint64_t playerId, CargoLot lot, double densityTM3)
{
    auto& m = manifest(shipKey, playerId);
    ++m_serial;
    for (auto& l : m.lots)
        if (mergeable(l, lot))
        {
            l.tonnes += lot.tonnes;
            l.costBasisCr += lot.costBasisCr;
            l.c20 = c20For(l.tonnes, densityTM3);
            return l.lotId;
        }
    lot.lotId = "L" + std::to_string(m.nextLot++);
    lot.c20 = c20For(lot.tonnes, densityTM3);
    m.lots.push_back(lot);
    return m.lots.back().lotId;
}

double CargoRegistry::fifoDutyShare(const CargoManifest& m, const std::string& goodId, double tonnes,
                                    const std::function<double(const CargoLot&)>& rateOfLot)
{
    if (!(tonnes > 0.0))
        return 0.0;
    double left = tonnes, weighted = 0.0;
    for (const auto& l : m.lots)   // the same order as unload(): the oldest own lot first
    {
        if (left <= 1.0e-9)
            break;
        if (l.goodId != goodId || !l.contractId.empty())
            continue;
        const double take = std::min(left, l.tonnes);
        weighted += take * std::max(0.0, rateOfLot(l));
        left -= take;
    }
    return weighted / tonnes;
}

bool CargoRegistry::unload(std::uint64_t shipKey, const std::string& goodId, double tonnes, double densityTM3, double& costBasisOutCr)
{
    costBasisOutCr = 0.0;
    const auto it = m_manifests.find(shipKey);
    if (it == m_manifests.end() || it->second.ownTonnesOf(goodId) + 1.0e-6 < tonnes)
        return false;
    auto& lots = it->second.lots;
    double left = tonnes;
    for (auto& l : lots)   // FIFO: the oldest lot first
    {
        if (left <= 1.0e-9)
            break;
        if (l.goodId != goodId || !l.contractId.empty())
            continue;
        const double take = std::min(left, l.tonnes);
        const double share = l.tonnes > 0.0 ? take / l.tonnes : 1.0;
        costBasisOutCr += l.costBasisCr * share;
        l.costBasisCr -= l.costBasisCr * share;
        l.tonnes -= take;
        left -= take;
        l.c20 = c20For(l.tonnes, densityTM3);
    }
    lots.erase(std::remove_if(lots.begin(), lots.end(), [](const CargoLot& l) { return l.tonnes <= 1.0e-6; }), lots.end());
    ++m_serial;
    return true;
}

void CargoRegistry::clear(std::uint64_t shipKey)
{
    if (const auto it = m_manifests.find(shipKey); it != m_manifests.end())
    {
        it->second.lots.clear();
        ++m_serial;
    }
}

std::vector<CargoLot> CargoRegistry::takeContract(std::uint64_t shipKey, const std::string& contractId)
{
    std::vector<CargoLot> out;
    const auto it = m_manifests.find(shipKey);
    if (it == m_manifests.end() || contractId.empty())
        return out;
    auto& lots = it->second.lots;
    for (auto l = lots.begin(); l != lots.end();)
    {
        if (l->contractId == contractId)
        {
            out.push_back(std::move(*l));
            l = lots.erase(l);
        }
        else
            ++l;
    }
    if (!out.empty())
        ++m_serial;
    return out;
}

double CargoRegistry::contractTonnesOf(std::uint64_t shipKey, const std::string& contractId) const
{
    double t = 0.0;
    if (const CargoManifest* m = find(shipKey))
        for (const auto& l : m->lots)
            if (l.contractId == contractId)
                t += l.tonnes;
    return t;
}

nlohmann::json CargoRegistry::toJson() const
{
    nlohmann::json out = nlohmann::json::array();
    for (const auto& [key, m] : m_manifests)
    {
        nlohmann::json lots = nlohmann::json::array();
        for (const auto& l : m.lots)
            lots.push_back(lotToJson(l));
        out.push_back({{"ship", key}, {"player", m.playerId}, {"next", m.nextLot}, {"lots", lots}});
    }
    return out;
}

void CargoRegistry::fromJson(const nlohmann::json& j)
{
    m_manifests.clear();
    if (!j.is_array())
        return;
    for (const auto& x : j)
    {
        CargoManifest m;
        m.shipKey = x.value("ship", std::uint64_t{0});
        m.playerId = x.value("player", std::uint64_t{0});
        m.nextLot = x.value("next", 1);
        const nlohmann::json lots = x.value("lots", nlohmann::json::array());
        for (const auto& l : lots)
        {
            CargoLot c = lotFromJson(l);
            m.lots.push_back(std::move(c));
        }
        m_manifests[m.shipKey] = std::move(m);
    }
    ++m_serial;
}
nlohmann::json CargoRegistry::lotToJson(const CargoLot& l)
{
    return {{"id", l.lotId}, {"good", l.goodId}, {"t", l.tonnes}, {"c20", l.c20}, {"owner", l.owner}, {"contract", l.contractId},
            {"cost", l.costBasisCr}, {"origin", l.originHub}, {"day", l.loadedDay}, {"declared", l.declared},
            {"concealment", l.concealment}, {"people", l.people}};
}

CargoLot CargoRegistry::lotFromJson(const nlohmann::json& l)
{
    CargoLot c;
    c.lotId = l.value("id", std::string());
    c.goodId = l.value("good", std::string());
    c.tonnes = l.value("t", 0.0);
    c.c20 = l.value("c20", 0);
    c.owner = l.value("owner", std::string());
    c.contractId = l.value("contract", std::string());
    c.costBasisCr = l.value("cost", 0.0);
    c.originHub = l.value("origin", std::string());
    c.loadedDay = l.value("day", 0);
    c.declared = l.value("declared", true);
    c.concealment = l.value("concealment", 0);
    c.people = l.value("people", 0);
    return c;
}

std::vector<CargoLot> CargoRegistry::takeAll(std::uint64_t shipKey, std::uint64_t& playerIdOut)
{
    std::vector<CargoLot> out;
    playerIdOut = 0;
    if (const auto it = m_manifests.find(shipKey); it != m_manifests.end())
    {
        playerIdOut = it->second.playerId;
        out = std::move(it->second.lots);
        m_manifests.erase(it);
        ++m_serial;
    }
    return out;
}

} // namespace game::economy
