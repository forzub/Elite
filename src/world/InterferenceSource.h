#pragma once

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"

enum class InterferenceType
{
    Passive, // бури, астероиды, солнечный ветер
    Active   // РЭБ, глушилки
};

struct InterferenceSource
{
    InterferenceType type = InterferenceType::Passive;

    world::coordinates::WorldPosition worldPosition;

    // Legacy mirror. Не источник истины.
    glm::vec3 position {0.0f};

    float power = 0.0f;  // мощность помех
    float radius = 0.0f; // эффективный радиус действия
    bool enabled = false;

    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }

    void setWorldPositionMeters(
        const glm::dvec3& meters
    )
    {
        setWorldPosition(
            world::coordinates::makeWorldPositionFromMeters(meters)
        );
    }
};