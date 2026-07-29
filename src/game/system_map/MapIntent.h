#pragma once

#include <string>

#include <glm/glm.hpp>

namespace game::system_map
{
    enum class MapIntentType
    {
        None,

        // Galaxy -> System transitions.
        EnterKnownSystem,
        EnterEmptySector,

        // Reserved common intents for the next map extraction phases.
        OpenBody,
        OpenHub,
        SelectObject,
        SetNavigationTarget,
        PlotRoute
    };

    /*
        User action produced by a map view.

        Renderers may update their own presentation state immediately, but
        world or mode changes must leave the renderer through this value.
        SpaceState remains the coordinator which decides how an intent is
        executed and which server snapshot must be requested.
    */
    struct MapIntent
    {
        MapIntentType type = MapIntentType::None;

        int systemId = -1;
        glm::dvec3 positionLy {0.0};

        std::string objectId;
        std::string secondaryObjectId;

        bool valid() const noexcept
        {
            return type != MapIntentType::None;
        }

        bool entersKnownSystem() const noexcept
        {
            return
                type == MapIntentType::EnterKnownSystem &&
                systemId >= 0;
        }

        bool entersEmptySector() const noexcept
        {
            return type == MapIntentType::EnterEmptySector;
        }

        static MapIntent enterKnownSystem(
            int destinationSystemId,
            const glm::dvec3& destinationPositionLy
        )
        {
            MapIntent result;
            result.type = MapIntentType::EnterKnownSystem;
            result.systemId = destinationSystemId;
            result.positionLy = destinationPositionLy;
            return result;
        }

        static MapIntent enterEmptySector(
            const glm::dvec3& destinationPositionLy
        )
        {
            MapIntent result;
            result.type = MapIntentType::EnterEmptySector;
            result.positionLy = destinationPositionLy;
            return result;
        }
    };
}
