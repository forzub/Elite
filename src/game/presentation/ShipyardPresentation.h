#pragma once

#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/refit/ShipMarket.h"

// F6 Shipyard page model. ONE source: the ship design catalog (passports)
// and the runtime ShipDescriptor the generic builder makes from it. Every
// number on the page is read from them (or derived from them by the same
// formula the runtime uses); nothing here is a second table of ships.
namespace game::presentation
{
struct ShipyardModelRow
{
    std::string passportId;
    std::string name;
    std::string manufacturer;
    std::string shipClass;
    double priceListCr = 0.0;
    bool selected = false;
};

struct ShipyardSpec
{
    std::string labelRu;
    std::string value;
};

struct ShipyardPreviewBlock
{
    std::string category;
    glm::vec3 centerMeters {0.0f};
    glm::vec3 halfSizeMeters {0.0f};
};

struct ShipyardModelCard
{
    bool valid = false;
    std::string passportId;
    std::string name;
    std::string manufacturer;
    std::string shipClass;
    std::string purpose;
    glm::vec3 envelopeMeters {0.0f};   // length, width, height
    std::vector<ShipyardPreviewBlock> preview;   // the runtime primitive blocks
    // the numbers (all from passport / descriptor)
    double dryTonnes = 0.0, serviceTonnes = 0.0, grossTonnes = 0.0;
    int crew = 0, passengers = 0, containerSlots = 0;
    double cargoTonnes = 0.0;
    std::string propulsionScheme;      // e.g. "Newtonian Y: 3 x OFU U-43"
    double thrustMN = 0.0;
    double serviceAccelerationMps2 = 0.0, grossAccelerationMps2 = 0.0;
    std::string jump;                  // core model + rating or "нет (внутрисистемный)"
    double jumpRatingTonnes = 0.0;
    std::string radar;
    double radarRangeKm = 0.0;
    double propulsionPowerMW = 0.0, servicePowerMW = 0.0;
    double priceListCr = 0.0;
    std::vector<ShipyardSpec> mainSpecs;      // the compact visible block
    std::vector<ShipyardSpec> passportSpecs;  // the expandable full passport
    std::vector<game::ship::refit::ShipOffer> offers;
};

struct ShipyardPresentation
{
    std::vector<ShipyardModelRow> models;
    int selected = -1;
    ShipyardModelCard card;
    bool passportExpanded = false;
};

[[nodiscard]] ShipyardPresentation buildShipyardPresentation(const std::string& selectedPassportId, bool passportExpanded);
} // namespace game::presentation
