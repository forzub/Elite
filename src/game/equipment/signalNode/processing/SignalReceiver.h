#pragma once

#include <vector>
#include <glm/vec3.hpp>

#include "DetectedSignal.h"
#include "game/equipment/signalNode/ReceiverModule.h"

#include "src/world/coordinates/WorldPosition.h"


class Planet;
class InterferenceSource;

class SignalReceiver
{
public:
    void update(
        float dt,
        const world::coordinates::WorldPosition& receiverWorldPosition,
        const game::ReceiverModule& receiver,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        std::vector<SignalReceptionResult>& outResults,
        const EntityId ownerShip
    );

    const std::vector<DetectedSignal>& detected() const;

private:
    std::vector<DetectedSignal> m_detected;

    float computeSignalStrength(
        const WorldSignal& sig,
        const world::coordinates::WorldPosition& receiverWorldPosition
    ) const;

    bool isOccludedByPlanet(
        const glm::vec3& signalPos,
        const glm::vec3& receiverPos,
        const Planet& planet);
    };
