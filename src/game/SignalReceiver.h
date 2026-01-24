#pragma once

#include <vector>
#include <glm/vec3.hpp>

#include "DetectedSignal.h"
#include "SignalEnvironment.h"

class SignalReceiver
{
public:
    void update(
        float dt,
        const glm::vec3& receiverPos,
        const std::vector<WorldSignal>& worldSignals
    );

    const std::vector<DetectedSignal>& detected() const;

private:
    std::vector<DetectedSignal> m_detected;

    float computeSignalStrength(
        const WorldSignal& sig,
        const glm::vec3& receiverPos
    ) const;

    SignalEnvironment env{0.0f, 0.0f};
};
