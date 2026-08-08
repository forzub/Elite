#pragma once

namespace game::simulation
{

/*
    Star-system membership is part of spatial identity. The current dynamic
    runtime intentionally owns one active celestial context at a time; these
    helpers make the boundary testable without depending on GameSimulation.
*/
constexpr bool validRuntimeSystemId(int systemId) noexcept
{
    return systemId >= 0;
}

constexpr bool sameRuntimeSystem(int a, int b) noexcept
{
    return a >= 0 && b >= 0 && a == b;
}

constexpr bool canCreateInActiveRuntimeSystem(
    int entitySystemId,
    int activeSystemId
) noexcept
{
    return
        validRuntimeSystemId(entitySystemId) &&
        (activeSystemId < 0 || entitySystemId == activeSystemId);
}

} // namespace game::simulation
