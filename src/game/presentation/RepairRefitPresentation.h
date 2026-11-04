#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/refit/RepairRefit.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/ShipRefitSnapshot.h"
#include "src/scene/EntityID.h"

// F7 Repair & Refit page model. Built ONLY from a real ship instance: its
// descriptor (installed modules, hierarchy, passport placement), the
// replicated module runtime states (health, damage causes, attach state) and
// the server's RepairRefitPreview publication. No hand-made fault list, no
// second table of equipment numbers.
namespace game::presentation
{
struct RefitShipInput
{
    EntityId id {};
    const ShipDescriptor* descriptor = nullptr;
    const std::vector<game::simulation::ObjectModuleSnapshot>* modules = nullptr;
    const game::simulation::ShipRefitSnapshot* refit = nullptr;
};

struct RefitShipTab
{
    EntityId id {};
    std::string name;
    std::string registration;
    std::string model;
};

struct RefitModuleView
{
    std::string moduleId;
    std::string parentModuleId;
    std::string group;          // RefitGroupView::key
    std::string label;          // product name or module id
    std::string category;
    std::string state;          // attached / disabled / destroyed / detached / hanging / unknown
    double health01 = 0.0;
    double damage01 = 0.0;      // functional damage, all causes
    double wear01 = 0.0;
    std::vector<std::pair<std::string, double>> causes;   // non-zero damage causes
    bool hasPlacement = false;
    glm::vec3 centerMeters {0.0f};
    glm::vec3 halfSizeMeters {0.0f};
    bool critical = false;
    bool selected = false;
    // a destroyed unit (DisableOnly policy keeps it "disabled" at zero health) or a functional failure
    [[nodiscard]] bool failed() const
    {
        return state == "destroyed" || state == "detached" || (state != "unknown" && health01 <= 0.001) || damage01 >= 0.95;
    }
    [[nodiscard]] bool damaged() const { return failed() || health01 < 0.999 || damage01 > 0.001; }
};

struct RefitGroupView
{
    std::string key;
    std::string labelRu;
    int modules = 0;
    int damaged = 0;
    int failed = 0;
    bool selected = false;
};

struct RepairRefitPresentation
{
    bool valid = false;
    std::vector<RefitShipTab> ships;
    int currentShip = 0;
    std::string modelName;
    std::string passportId;
    glm::vec3 envelopeMeters {0.0f};   // width, height, length
    std::vector<RefitGroupView> groups;
    std::vector<RefitModuleView> modules;       // every installed module of the ship
    int selectedModule = -1;                    // index into modules
    game::ship::refit::RefitModuleFacts facts;  // of the selected module (passport)
    std::vector<game::ship::refit::RefitReplacement> replacements;
    // the server preview, when it belongs to the selected module
    bool previewForSelection = false;
    game::ship::refit::RefitPreview preview;
    std::string applyResult;
    bool applyResultFresh = false;
};

// Group key of an installed module (catalog category / legacy subsystem).
[[nodiscard]] std::string refitGroupOf(const std::string& category, const std::string& subsystemId);
[[nodiscard]] const char* refitGroupLabelRu(const std::string& key);

[[nodiscard]] RepairRefitPresentation buildRepairRefitPresentation(
    const std::vector<RefitShipInput>& ownedShips,   // the player's ships, current first is not required
    EntityId currentShip,
    const std::string& selectedGroup,
    const std::string& selectedModule,
    std::uint32_t seenApplySerial
);
} // namespace game::presentation
