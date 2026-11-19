#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// The server's cargo manifest of every ship (docs/TRADE_DESIGN.md §7.2). Owned by EconomyServer - the ONLY writer;
// the simulation gets the derived mass (GameServer::applyCargoLoad: net tonnes + the C20 boxes, whose tare the ship's
// mass model adds), the manifest itself never lives in the simulation.
//
// Keyed by the persistent ship instance id (ShipInstanceId), not by the runtime EntityId: the manifest survives a
// restart (economy_state/2) and is not an EntityId-keyed table of the simulation.
namespace game::economy
{
// standard container C20 (trade_goods.json container; catalog::ships::C20*): payload 20 t, 23.3 m3, tare 1.7 t
inline constexpr double C20PayloadT = 20.0;
inline constexpr double C20VolumeM3 = 23.3;
inline constexpr double C20TareT = 1.7;

// net tonnes of a good one C20 takes: mass-limited (20 t) or volume-limited (density x 23.3 m3)
[[nodiscard]] double c20NetTonnes(double densityTM3);
// C20 boxes `tonnes` of that good need
[[nodiscard]] int c20For(double tonnes, double densityTM3);

struct CargoLot
{
    std::string lotId;
    std::string goodId;
    double tonnes = 0.0;
    int c20 = 0;
    std::string owner;          // "player:<id>" | "issuer:<actor>" (contract cargo, step 5)
    std::string contractId;     // "" = own goods
    double costBasisCr = 0.0;   // what the lot cost in total (goods + handling + VAT not reclaimable)
    std::string originHub;
    int loadedDay = 0;
    bool declared = true;
    int concealment = 0;
};

struct CargoCapacity
{
    double netT = 0.0;   // the hull's structural payload limit (descriptor storage.cargoMass)
    int c20Slots = 0;    // C20 places
};

struct CargoManifest
{
    std::uint64_t shipKey = 0;
    std::uint64_t playerId = 0;
    int nextLot = 1;
    std::vector<CargoLot> lots;

    [[nodiscard]] double netT() const;
    [[nodiscard]] int c20() const;
    [[nodiscard]] double grossT() const { return netT() + C20TareT * c20(); }   // what the hull carries
    [[nodiscard]] double ownTonnesOf(const std::string& goodId) const;
};

class CargoRegistry
{
public:
    [[nodiscard]] const CargoManifest* find(std::uint64_t shipKey) const;
    CargoManifest& manifest(std::uint64_t shipKey, std::uint64_t playerId);

    // "" = fits; else the reason (the C20 places / the payload limit)
    [[nodiscard]] static std::string fits(const CargoManifest& m, const CargoCapacity& cap, double tonnes, int c20);
    // the C20 boxes loading `lot` adds (it may merge into a lot whose last box still has room)
    [[nodiscard]] static int extraC20(const CargoManifest& m, const CargoLot& lot, double densityTM3);
    // loads a lot (merging into a same-good own lot of the same origin and day); returns the lot id
    std::string load(std::uint64_t shipKey, std::uint64_t playerId, CargoLot lot, double densityTM3);
    // the import duty share of a sale of `tonnes` (TRADE_DESIGN §5.2, step 4): the lots unload() would take (own goods,
    // oldest first), each weighted by the tonnes taken and its rate (rateOfLot: 0 for goods of this polity) / tonnes.
    // The server (EconomyServer::trade) and the deal window (from the replicated per-lot rate) call the same function.
    [[nodiscard]] static double fifoDutyShare(const CargoManifest& m, const std::string& goodId, double tonnes,
                                              const std::function<double(const CargoLot&)>& rateOfLot);
    // takes `tonnes` of own goods FIFO; returns the cost basis taken out (Cr); false = not that much aboard
    bool unload(std::uint64_t shipKey, const std::string& goodId, double tonnes, double densityTM3, double& costBasisOutCr);
    void clear(std::uint64_t shipKey);

    [[nodiscard]] std::uint64_t serial() const { return m_serial; }
    [[nodiscard]] const std::map<std::uint64_t, CargoManifest>& all() const { return m_manifests; }

    [[nodiscard]] nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    [[nodiscard]] static nlohmann::json lotToJson(const CargoLot& l);
    [[nodiscard]] static CargoLot lotFromJson(const nlohmann::json& l);
    // the ship is lost: every lot leaves its manifest (returned) and the manifest is erased - the lots are no longer
    // "aboard" anything that can come back (docs/CARGO_SALVAGE_DESIGN.md: the loss event decides their fate)
    std::vector<CargoLot> takeAll(std::uint64_t shipKey, std::uint64_t& playerIdOut);

private:
    std::map<std::uint64_t, CargoManifest> m_manifests;
    std::uint64_t m_serial = 1;
};
} // namespace game::economy
