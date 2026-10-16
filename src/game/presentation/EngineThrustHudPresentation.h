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
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

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

    // Yellow capacity envelope and red current generated thrust, both relative
    // to the slot's undamaged nominal thrust.
    float capability01 = 0.0f;
    float thrust01 = 0.0f;
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

inline double normalizedDemand(
    double actual,
    double authority
) noexcept
{
    if (!std::isfinite(actual) ||
        !std::isfinite(authority) ||
        authority <= 1.0e-9)
    {
        return 0.0;
    }

    return std::clamp(
        std::abs(actual) / authority,
        0.0,
        1.0
    );
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

    const auto shipCapability =
        capability::buildShipCapability(
            descriptor,
            model,
            slotState
        );
    if (!shipCapability.valid)
        return out;

    out.modelLabel =
        detail::physicalModelLabel(model);

    const auto family =
        plant::familyOf(model);
    const bool assisted =
        family == plant::PhysicalControlFamily::Assisted;

    glm::dvec3 forward(ship.transform.forward());
    const double forwardLength = glm::length(forward);
    if (forwardLength <= 1.0e-9)
        forward = glm::dvec3(0.0, 0.0, -1.0);
    else
        forward /= forwardLength;

    double aftDemand01 = 0.0;
    double foreDemand01 = 0.0;
    double lateralDemand01 = 0.0;

    const ShipParams& effective = shipCapability.effectivePhysics;

    if (assisted)
    {
        // engineAccelerationMps2 is the actually executed propulsion
        // acceleration after the controlled-speed envelope. In Assisted the
        // physical model owns both axial thrust and lateral vectoring, so its
        // axial/lateral decomposition is the closest current actuator truth.
        const glm::dvec3 actual =
            ship.transform.motion.engineAccelerationMps2;
        const double axial = glm::dot(actual, forward);
        const glm::dvec3 lateral =
            actual - forward * axial;

        if (axial >= 0.0)
        {
            aftDemand01 = detail::normalizedDemand(
                axial,
                forwardMainAccelerationLimitMps2(effective)
            );
        }
        else
        {
            foreDemand01 = detail::normalizedDemand(
                axial,
                reverseMainAccelerationLimitMps2(effective)
            );
        }

        lateralDemand01 = detail::normalizedDemand(
            glm::length(lateral),
            assistedLateralStabilizationAccelerationLimitMps2(
                effective
            )
        );
    }
    else
    {
        // Newtonian currently keeps its main request separate from gas-limited
        // manoeuvre clusters. Until the actuator allocator moves fully behind
        // ShipControlExecutionGateway, use that isolated main vector rather
        // than contaminating the red main-engine bars with RCS acceleration.
        const double axial = glm::dot(
            ship.transform.motion.mainEngineAccelerationMps2,
            forward
        );

        if (axial >= 0.0)
        {
            aftDemand01 = detail::normalizedDemand(
                axial,
                forwardMainAccelerationLimitMps2(effective)
            );
        }
        else
        {
            foreDemand01 = detail::normalizedDemand(
                axial,
                reverseMainAccelerationLimitMps2(effective)
            );
        }
    }

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

        double demand01 =
            role == plant::EngineSlotRole::AftMain
                ? aftDemand01
                : foreDemand01;

        if (assisted)
        {
            // Vectoring main engines can simultaneously spend thrust on axial
            // and lateral components. The Euclidean demand is bounded by the
            // slot's available nominal thrust.
            demand01 = std::hypot(
                demand01,
                lateralDemand01
            );
        }

        EngineThrustBarPresentation bar;
        bar.slotId = definitions[i].id;
        bar.fillDirection =
            role == plant::EngineSlotRole::AftMain
                ? EngineThrustFillDirection::BottomUp
                : EngineThrustFillDirection::TopDown;
        bar.capability01 = static_cast<float>(
            std::clamp(available01, 0.0, 1.0)
        );
        bar.thrust01 = static_cast<float>(
            std::clamp(
                available01 * std::clamp(demand01, 0.0, 1.0),
                0.0,
                available01
            )
        );

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

} // namespace game::presentation
