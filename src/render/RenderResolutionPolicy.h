#pragma once

namespace elite::render
{

// Project maximum supported render resolution. Desktop window sizing and
// content/LOD authoring use the same ceiling so assets never require detail
// that only becomes useful above the supported 2560x1440 target.
inline constexpr int MaximumSupportedRenderWidth = 2560;
inline constexpr int MaximumSupportedRenderHeight = 1440;

} // namespace elite::render
