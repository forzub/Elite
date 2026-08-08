#pragma once

namespace game::client
{

/*
    System-local coordinates are meaningful only inside one authoritative
    star-system domain. A numeric position from system A may never be blended
    with the same numeric position from system B.
*/
inline constexpr bool canInterpolateSystemLocalState(
    int olderSystemId,
    int newerSystemId
) noexcept
{
    return
        olderSystemId >= 0 &&
        olderSystemId == newerSystemId;
}

inline constexpr bool belongsToRenderSystem(
    int entitySystemId,
    int renderSystemId
) noexcept
{
    return
        renderSystemId >= 0 &&
        entitySystemId == renderSystemId;
}

} // namespace game::client
