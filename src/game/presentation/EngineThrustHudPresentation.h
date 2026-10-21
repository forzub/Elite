#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/presentation/ShipBitePresentation.h"
#include "src/game/presentation/EngineLiveTelemetryPresentation.h"
#include "src/game/ship/computer/MotherComputer.h"

namespace game::presentation
{

enum class EngineThrustFillDirection : std::uint8_t
{
    BottomUp = 0,
    TopDown
};

struct EngineThrustBarPresentation
{
    std::string slotId;
    EngineThrustFillDirection fillDirection =
        EngineThrustFillDirection::BottomUp;

    // Yellow availability and red ACTUALLY EXECUTED thrust, both relative
    // to this installed engine's undamaged nominal Newton rating.
    // The read-only HUD may not reconstruct actual firing from acceleration.
    float capability01 = 0.0f;
    float thrust01 = 0.0f;

    // Presentation fault signal only. Animation remains client-renderer state.
    bool fault = false;
};

struct EngineThrustGroupPresentation
{
    glm::dvec2 positionBodyXY {0.0};
    std::vector<EngineThrustBarPresentation> bars;
};

struct EngineThrustHudPresentation
{
    bool visible = false;
    std::string modelLabel;
    std::string styleId = "human_amber";
    bool warning = false;
    std::vector<EngineThrustGroupPresentation> groups;
};

namespace detail
{

inline const char* physicalModelLabel(
    game::ship::plant::PhysicalControlModelId model
) noexcept
{
    using Model = game::ship::plant::PhysicalControlModelId;
    switch (model)
    {
        case Model::AssistedI: return "ASSISTED-I";
        case Model::AssistedY: return "ASSISTED-Y";
        case Model::AssistedX: return "ASSISTED-X";
        case Model::NewtonianI: return "NEWTONIAN-I";
        case Model::NewtonianY: return "NEWTONIAN-Y";
        case Model::NewtonianX: return "NEWTONIAN-X";
    }
    return "ENGINES";
}

inline bool sameGroupPosition(
    const glm::dvec2& a,
    const glm::dvec2& b
) noexcept
{
    return glm::length(a - b) <= 1.0e-8;
}

} // namespace detail

inline EngineThrustHudPresentation buildEngineThrustHudPresentation(
    const ClientShipState& ship
)
{
    using namespace game::ship;

    EngineThrustHudPresentation out;
    if (!ship.descriptor)
        return out;

    const ShipDescriptor& descriptor = *ship.descriptor;

    // The descriptor describes the ship type, but the selected I/Y/X +
    // Assisted/Newtonian construction is per-instance runtime state. Ctrl+F9
    // and Ctrl+F10 change ship.physicalControlModel authoritatively; the HUD
    // must consume that same replicated state instead of silently falling back
    // to the descriptor's spawn-time model.
    const auto model = ship.physicalControlModel;
    const auto& definitions =
        plant::physicalControlSlots(model);

    // Diagnostic construction switching deliberately creates a healthy fixed
    // actuator topology on the server. When it differs from the descriptor's
    // native construction there is no descriptor module binding for those
    // synthetic slots, so mirror the same healthy diagnostic state on the
    // client. For the native construction, keep real replicated module damage.
    const auto slotState =
        model != descriptor.physicalControlModel
            ? plant::healthySlotState(definitions)
            : capability::buildPhysicalControlSlotState(
                  descriptor,
                  model,
                  ship.modules
              );

    if (definitions.empty() ||
        slotState.size() != definitions.size())
    {
        return out;
    }

    // BITE supplies faults only; live per-engine output is authoritative.
    const auto biteState = buildShipBiteState(
        descriptor, ship.modules
    );
    out.modelLabel = detail::physicalModelLabel(model);
    out.styleId = descriptor.hud.engineStatusStyleId.empty()
        ? "human_amber"
        : descriptor.hud.engineStatusStyleId;
    const bool assisted = plant::familyOf(model) ==
        plant::PhysicalControlFamily::Assisted;

    for (std::size_t i = 0; i < definitions.size(); ++i)
    {
        const auto role = definitions[i].role;
        if (role != plant::EngineSlotRole::AftMain &&
            role != plant::EngineSlotRole::ForeMain)
        {
            continue;
        }

        // Newtonian ships have no forward main-engine bank in the cockpit
        // thrust instrument. A fore slot, if ever present in transitional data,
        // must not reappear as a dead/zero bar: it is outside the Newtonian
        // propulsion contract and therefore absent from the HUD.
        if (!assisted &&
            role == plant::EngineSlotRole::ForeMain)
        {
            continue;
        }

        const glm::dvec2 groupPosition(
            definitions[i].normalizedPositionBody.x,
            definitions[i].normalizedPositionBody.y
        );

        auto groupIt = std::find_if(
            out.groups.begin(),
            out.groups.end(),
            [&](const auto& group)
            {
                return detail::sameGroupPosition(
                    group.positionBodyXY,
                    groupPosition
                );
            }
        );

        if (groupIt == out.groups.end())
        {
            EngineThrustGroupPresentation group;
            group.positionBodyXY = groupPosition;
            out.groups.push_back(std::move(group));
            groupIt = std::prev(out.groups.end());
        }

        const double available01 =
            plant::effectiveSlotThrust01(slotState[i]);

        const auto live = readEngineMotorLiveView(
            ship.transform.motion, i, available01
        );

        EngineThrustBarPresentation bar;
        bar.slotId = definitions[i].id;
        bar.fillDirection =
            role == plant::EngineSlotRole::AftMain
                ? EngineThrustFillDirection::BottomUp
                : EngineThrustFillDirection::TopDown;
        bar.capability01 = static_cast<float>(live.available01);
        // RED = actual executed thrust / THIS engine's healthy nominal.
        // Never scale or reconstruct it from net ship acceleration.
        bar.thrust01 = static_cast<float>(live.thrust01);
        const auto& binding =
            descriptor.physicalControl.slotModuleIds;
        const std::string equipmentId =
            i < binding.size() ? binding[i] : std::string {};

        // HUD alarm thresholds belong to BITE. Capacity still comes from the
        // physical propulsion projection; presentation does not invent a
        // second damage policy.
        // Synthetic Ctrl+F9 engines have no native module damage identity.
        // Do not attach an unrelated Cobra module's BITE alarm to them.
        bar.fault =
            model == descriptor.physicalControlModel &&
            !equipmentId.empty() &&
            biteHudAttentionForEquipment(
                biteState,
                equipmentId
            );
        out.warning = out.warning || bar.fault;

        groupIt->bars.push_back(std::move(bar));
    }

    std::sort(
        out.groups.begin(),
        out.groups.end(),
        [](const auto& a, const auto& b)
        {
            if (std::abs(a.positionBodyXY.x - b.positionBodyXY.x) >
                1.0e-8)
            {
                return a.positionBodyXY.x < b.positionBodyXY.x;
            }

            return a.positionBodyXY.y > b.positionBodyXY.y;
        }
    );

    // Keep the Assisted-I silhouette immediately readable:
    // aft bars sit on the outside and fore bars on the inside.
    // The same pairing rule remains stable for Y/X groups.
    for (auto& group : out.groups)
    {
        const bool rightSide = group.positionBodyXY.x > 1.0e-8;
        std::stable_sort(
            group.bars.begin(),
            group.bars.end(),
            [rightSide](const auto& a, const auto& b)
            {
                const auto rank = [rightSide](const auto& bar)
                {
                    const bool fore =
                        bar.fillDirection ==
                        EngineThrustFillDirection::TopDown;
                    if (rightSide)
                        return fore ? 0 : 1;
                    return fore ? 1 : 0;
                };

                return rank(a) < rank(b);
            }
        );
    }

    out.visible = !out.groups.empty();
    return out;
}


/**
 * MotherComputer is the ONLY live data source for cockpit thrust bars.
 * This renderer adapter receives a compact, verified ship/tick-matched
 * snapshot; it never evaluates engine damage, BITE, or server state itself.
 */
inline EngineThrustHudPresentation buildEngineThrustHudPresentation(
    const game::ship::computer::MotherComputerCockpitView& cockpit
)
{
    EngineThrustHudPresentation out;
    if (!cockpit.valid)
        return out;
    out.modelLabel = cockpit.physicalModelLabel;
    out.styleId = cockpit.engineHudStyleId;
    for (const auto& input : cockpit.engineSlots)
    {
        const glm::dvec2 position(input.groupX,input.groupY);
        auto groupIt = std::find_if(
            out.groups.begin(),out.groups.end(),
            [&](const auto& g)
            {
                return detail::sameGroupPosition(g.positionBodyXY,position);
            }
        );
        if(groupIt == out.groups.end())
        {
            EngineThrustGroupPresentation group;
            group.positionBodyXY = position;
            out.groups.push_back(std::move(group));
            groupIt = std::prev(out.groups.end());
        }

        EngineThrustBarPresentation bar;
        bar.slotId = input.id;
        bar.fillDirection = input.fore
            ? EngineThrustFillDirection::TopDown
            : EngineThrustFillDirection::BottomUp;
        bar.capability01 = input.available01;
        bar.thrust01 = input.delivered01;
        bar.fault = input.fault;
        out.warning = out.warning || input.fault;
        groupIt->bars.push_back(std::move(bar));
    }
    std::sort(out.groups.begin(),out.groups.end(),
        [](const auto& a,const auto& b)
        {
            if(std::abs(a.positionBodyXY.x-b.positionBodyXY.x)>1.0e-8)
                return a.positionBodyXY.x < b.positionBodyXY.x;
            return a.positionBodyXY.y > b.positionBodyXY.y;
        });
    for(auto& group : out.groups)
    {
        const bool rightSide = group.positionBodyXY.x>1.0e-8;
        std::stable_sort(group.bars.begin(),group.bars.end(),
            [rightSide](const auto& a,const auto& b)
            {
                const auto rank=[rightSide](const auto& bar)
                {
                    const bool fore =
                        bar.fillDirection == EngineThrustFillDirection::TopDown;
                    return rightSide ? (fore?0:1) : (fore?1:0);
                };
                return rank(a)<rank(b);
            });
    }
    out.visible = !out.groups.empty();
    return out;
}

} // namespace game::presentation
