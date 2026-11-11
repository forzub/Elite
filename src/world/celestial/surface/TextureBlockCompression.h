#pragma once

#include <cstdint>
#include <vector>

namespace world::celestial::surface
{
/*
    BC1 (DXT1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT) for prebaked map textures: 8 bytes per 4x4 block
    (0.5 byte a texel, 1/8 of RGBA8 on disk AND in video memory), uploaded as is - no CPU decode.
    Opaque 4-colour mode only (alpha ignored). The encoder is a principal-axis range fit: good enough
    for map-scale albedo and normal maps, deterministic, no dependencies.
*/

// rgba: width x height RGBA8 rows; any size (edge blocks repeat the last texel)
std::vector<std::uint8_t> encodeBC1(const std::uint8_t* rgba, int width, int height);

// back to RGBA8 (alpha 255): verification and the fallback when the GPU has no S3TC
void decodeBC1(const std::uint8_t* blocks, int width, int height, std::uint8_t* rgbaOut);

inline std::size_t bc1Size(int width, int height)
{
    return static_cast<std::size_t>((width + 3) / 4) * static_cast<std::size_t>((height + 3) / 4) * 8;
}

// the full mip chain down to 1x1 (2x2 box filter of the RGBA8 level above), each level BC1
std::vector<std::vector<std::uint8_t>> encodeBC1MipChain(const std::uint8_t* rgba, int width, int height);
} // namespace world::celestial::surface
