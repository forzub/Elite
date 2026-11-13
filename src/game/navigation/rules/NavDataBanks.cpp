#include "src/game/navigation/rules/NavDataBanks.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>

#include <nlohmann/json.hpp>

#include "src/world/celestial/StarAtlasDatabase.h"

namespace game::navigation::jump
{
namespace
{
constexpr double YearS = 365.25 * 86400.0;

const char* const BankFiles[] = {"assets/data/galaxy/nav_data_banks.json", "../assets/data/galaxy/nav_data_banks.json",
                                 "../src/assets/data/galaxy/nav_data_banks.json", "src/assets/data/galaxy/nav_data_banks.json"};

std::vector<NavDataBank> load()
{
    std::vector<NavDataBank> out;
    for (const char* path : BankFiles)
    {
        std::ifstream in(path);
        if (!in)
            continue;
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.contains("banks"))
            continue;
        for (const auto& b : j["banks"])
        {
            NavDataBank bank;
            bank.operatorId = b.value("operator", std::string());
            bank.name = b.value("name", bank.operatorId);
            if (b.contains("systems"))
                for (const auto& s : b["systems"])
                    bank.systems.push_back(s.get<int>());
            if (b.contains("records"))
                for (const auto& r : b["records"])
                {
                    BankRecord rec;
                    rec.id = r.value("id", std::string());
                    rec.shellSystemId = r.value("shellSystemId", -1);
                    rec.corridor = r.value("corridor", 0);
                    rec.surveyYear = r.value("surveyYear", 0.0);
                    rec.errorDeg = r.value("errorDeg", 0.0);
                    const std::string tier = r.value("tier", std::string("public"));
                    rec.tier = tier == "secret" ? DataTier::Secret : tier == "military" ? DataTier::Military : DataTier::Public;
                    rec.priceCr = r.value("priceCr", 0);
                    bank.records.push_back(rec);
                }
            out.push_back(std::move(bank));
        }
        std::cerr << "[NavData] " << out.size() << " data bank(s) from " << path << std::endl;
        break;
    }
    return out;
}

// a fixed pseudo-random angle per record id (where the survey error turned the direction)
double hashAngle(const std::string& id)
{
    std::uint64_t h = 1469598103934665603ull;
    for (const unsigned char c : id)
        h = (h ^ c) * 1099511628211ull;
    return static_cast<double>(h % 100000ull) / 100000.0 * 6.283185307179586;
}
} // namespace

const std::vector<NavDataBank>& navDataBanks()
{
    static const std::vector<NavDataBank> banks = load();
    return banks;
}

const NavDataBank* navDataBankInSystem(int systemId)
{
    for (const auto& b : navDataBanks())
        if (std::find(b.systems.begin(), b.systems.end(), systemId) != b.systems.end())
            return &b;
    return nullptr;
}

const BankRecord* findBankRecord(const std::string& id, const NavDataBank** bank)
{
    for (const auto& b : navDataBanks())
        for (const auto& r : b.records)
            if (r.id == id)
            {
                if (bank)
                    *bank = &b;
                return &r;
            }
    return nullptr;
}

const char* dataTierText(DataTier t)
{
    switch (t)
    {
    case DataTier::Military: return "военный допуск";
    case DataTier::Secret: return "секретно";
    default: return "открыто";
    }
}

LaneRecord laneRecordOf(const world::celestial::StarAtlasDatabase& atlas, const NavDataBank& bank, const BankRecord& r)
{
    LaneRecord out;
    out.systemId = r.shellSystemId;
    out.corridor = r.corridor;
    out.errorDeg = r.errorDeg;
    out.source = bank.name;
    const ApproachShell* shell = approachShellOf(atlas, r.shellSystemId);
    if (!shell || r.corridor < 1 || r.corridor > static_cast<int>(shell->corridors.size()))
        return out;
    out.surveyUniverseS = shell->epochUniverseS + (r.surveyYear - 3126.0) * YearS;
    const auto& c = shell->corridors[static_cast<std::size_t>(r.corridor - 1)];
    const glm::dvec3 d = corridorDirAt(*shell, c, out.surveyUniverseS);
    // the error: off the true direction by errorDeg, toward a fixed side of this record
    glm::dvec3 side = glm::cross(d, glm::dvec3(0.0, 0.0, 1.0));
    if (glm::length(side) < 1e-6)
        side = glm::cross(d, glm::dvec3(0.0, 1.0, 0.0));
    side = glm::normalize(side);
    const glm::dvec3 side2 = glm::cross(d, side);
    const double a = hashAngle(r.id);
    const glm::dvec3 off = side * std::cos(a) + side2 * std::sin(a);
    const double e = r.errorDeg * 3.14159265358979323846 / 180.0;
    out.dir = glm::normalize(d * std::cos(e) + off * std::sin(e));
    return out;
}

bool storeLaneRecord(std::vector<LaneRecord>& records, const LaneRecord& r)
{
    for (auto& x : records)
        if (x.systemId == r.systemId && x.corridor == r.corridor)
        {
            if (x.surveyUniverseS > r.surveyUniverseS || (x.surveyUniverseS == r.surveyUniverseS && x.errorDeg <= r.errorDeg))
                return false;
            x = r;
            return true;
        }
    records.push_back(r);
    return true;
}

std::vector<DataCarrier> parseDataCarriers(const std::string& text)
{
    std::vector<DataCarrier> out;
    std::size_t pos = 0;
    while (pos < text.size())
    {
        std::size_t end = text.find('#', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string item = text.substr(pos, end - pos);
        pos = end + 1;
        const std::size_t bar = item.find('|');
        DataCarrier c;
        c.label = item.substr(0, bar);
        if (bar != std::string::npos)
            c.records = parseLaneRecords(item.substr(bar + 1));
        out.push_back(std::move(c));
    }
    return out;
}

std::string formatDataCarriers(const std::vector<DataCarrier>& carriers)
{
    std::string out;
    for (const auto& c : carriers)
    {
        std::string label = c.label;
        for (char& ch : label)
            if (ch == '|' || ch == '#')
                ch = ' ';
        out += (out.empty() ? "" : "#") + label + "|" + formatLaneRecords(c.records);
    }
    return out;
}
} // namespace game::navigation::jump
