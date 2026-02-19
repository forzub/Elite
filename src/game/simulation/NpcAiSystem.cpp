#include "NpcAiSystem.h"
#include <cmath>

ShipControlState NpcAiSystem::computeControl(const Ship& ship, float dt)
{
    ShipControlState control{};

    const auto& transform = ship.core().transform();

    // Простейшее поведение: плавный разворот
    control.yawInput = std::sin(transform.position.x * 0.01f);
    control.pitchInput = 0.0f;
    control.rollInput = 0.0f;

    control.forwardInput = 0.001f;
    control.targetSpeedRate = 0.001f;

    // control.yawInput = std::sin(transform.position.x * 0.0f);
    // control.pitchInput = 0.0f;
    // control.rollInput = 0.0f;

    // control.forwardInput = 0.0f;
    // control.targetSpeedRate = 0.0f;

    return control;
}