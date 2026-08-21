#pragma once

#include <string>
#include <utility>

#include <glm/glm.hpp>

#include "src/game/system_map/MapMode.h"

namespace game::system_map
{
    enum class MapIntentType
    {
        None,

        // Galaxy -> System transitions.
        EnterKnownSystem,
        EnterEmptySector,

        // Common cross-layer presentation/navigation intents.
        OpenBody,
        OpenHub,
        RecallRouteMap,
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

        // Renderer requests only the presentation destination; SpaceState owns
        // the actual mode transition and snapshot lifecycle.
        MapMode requestedMapMode = MapMode::System;

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


        static MapIntent recallRouteMap(MapMode destinationMode)
        {
            MapIntent result;
            result.type = MapIntentType::RecallRouteMap;
            result.requestedMapMode = destinationMode;
            return result;
        }

        static MapIntent openBody(std::string bodyId)
        {
            MapIntent result;
            result.type = MapIntentType::OpenBody;
            result.objectId = std::move(bodyId);
            return result;
        }

        static MapIntent openHub(std::string hubId, std::string parentBodyId)
        {
            MapIntent result;
            result.type = MapIntentType::OpenHub;
            result.objectId = std::move(hubId);
            result.secondaryObjectId = std::move(parentBodyId);
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
