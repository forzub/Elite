#pragma once

#include <vector>
#include <glm/vec3.hpp>

#include "DetectedSignal.h"
#include "game/ship/equipment/ReceiverModule.h"



class Planet;
class InterferenceSource;

class SignalReceiver
{
public:
    void update(
        float dt,
        const glm::vec3& receiverPos,
        const ReceiverModule& receiver,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        std::vector<SignalReceptionResult>& outResults,
        const Ship* ownerShip
    );

    const std::vector<DetectedSignal>& detected() const;

private:
    std::vector<DetectedSignal> m_detected;

    float computeSignalStrength(
        const WorldSignal& sig,
        const glm::vec3& receiverPos
    ) const;

    bool isOccludedByPlanet(
        const glm::vec3& signalPos,
        const glm::vec3& receiverPos,
        const Planet& planet);
    };
