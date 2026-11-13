#pragma once

// The navigation data banks of the colonies (JUMP_MECHANICS 10k/10l; author 2026-11-13: "a bank at every colony,
// partly open about the currents and the sumps, the rest military or secret"). One bank per beacon operator, open at
// the hubs of its beacon systems; data galaxy/nav_data_banks.json. Data is not free: every record has a price - shown
// and logged, not charged until the market exists (author 2026-11-13).
// Lane records live in the ship's onboard computer (its memory); data carriers ("flash drives") carry records between
// a bank, a wreck, another pilot and the computer.
#include <string>
#include <vector>

#include "src/game/navigation/rules/InterstellarJumpRoute.h"

namespace world::celestial
{
class StarAtlasDatabase;
}

namespace game::navigation::jump
{
enum class DataTier : int
{
    Public = 0,
    Military = 1,
    Secret = 2
};

struct BankRecord
{
    std::string id;
    int shellSystemId = -1;
    int corridor = 0;
    double surveyYear = 0.0;
    double errorDeg = 0.0;
    DataTier tier = DataTier::Public;
    int priceCr = 0;
};

struct NavDataBank
{
    std::string operatorId;
    std::string name;
    std::vector<int> systems;
    std::vector<BankRecord> records;
};

const std::vector<NavDataBank>& navDataBanks();
// the bank open in a system (its operator has a beacon there), nullptr if none
const NavDataBank* navDataBankInSystem(int systemId);
const BankRecord* findBankRecord(const std::string& id, const NavDataBank** bank = nullptr);
const char* dataTierText(DataTier t);
// the record as a lane record (the corridor's direction at the survey, off by the survey error), the source = the bank
LaneRecord laneRecordOf(const world::celestial::StarAtlasDatabase& atlas, const NavDataBank& bank, const BankRecord& r);

// ---- the onboard computer's memory and the data carriers (text on the wire)
// put a record into a set: a fresher record of the same corridor replaces an older one; false if the set has a fresher one
bool storeLaneRecord(std::vector<LaneRecord>& records, const LaneRecord& r);

struct DataCarrier
{
    std::string label;
    std::vector<LaneRecord> records;
};
// "label|records#label|records..." (the records text of formatLaneRecords; labels without '|' and '#')
std::vector<DataCarrier> parseDataCarriers(const std::string& text);
std::string formatDataCarriers(const std::vector<DataCarrier>& carriers);
} // namespace game::navigation::jump
