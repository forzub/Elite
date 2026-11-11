#include "src/world/celestial/surface/TextureBlockCompression.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace world::celestial::surface
{
namespace
{
std::uint16_t to565(float r, float g, float b)
{
    const int R = std::clamp(static_cast<int>(std::lround(r * 31.0f / 255.0f)), 0, 31);
    const int G = std::clamp(static_cast<int>(std::lround(g * 63.0f / 255.0f)), 0, 63);
    const int B = std::clamp(static_cast<int>(std::lround(b * 31.0f / 255.0f)), 0, 31);
    return static_cast<std::uint16_t>((R << 11) | (G << 5) | B);
}

std::array<int, 3> from565(std::uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return {(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
}

void encodeBlock(const std::array<std::array<float, 3>, 16>& px, std::uint8_t* out)
{
    // mean and covariance -> principal axis (power iteration)
    float mean[3] = {0, 0, 0};
    for (const auto& p : px)
        for (int k = 0; k < 3; ++k)
            mean[k] += p[k] / 16.0f;
    float cov[6] = {0, 0, 0, 0, 0, 0};   // xx xy xz yy yz zz
    for (const auto& p : px)
    {
        const float d[3] = {p[0] - mean[0], p[1] - mean[1], p[2] - mean[2]};
        cov[0] += d[0] * d[0];
        cov[1] += d[0] * d[1];
        cov[2] += d[0] * d[2];
        cov[3] += d[1] * d[1];
        cov[4] += d[1] * d[2];
        cov[5] += d[2] * d[2];
    }
    float axis[3] = {0.577f, 0.577f, 0.577f};
    for (int it = 0; it < 6; ++it)
    {
        const float v[3] = {cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2],
                            cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2],
                            cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2]};
        const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (len < 1e-6f)
            break;
        for (int k = 0; k < 3; ++k)
            axis[k] = v[k] / len;
    }
    float lo = 1e30f, hi = -1e30f;
    for (const auto& p : px)
    {
        const float t = (p[0] - mean[0]) * axis[0] + (p[1] - mean[1]) * axis[1] + (p[2] - mean[2]) * axis[2];
        lo = std::min(lo, t);
        hi = std::max(hi, t);
    }
    std::uint16_t c0 = to565(mean[0] + axis[0] * hi, mean[1] + axis[1] * hi, mean[2] + axis[2] * hi);
    std::uint16_t c1 = to565(mean[0] + axis[0] * lo, mean[1] + axis[1] * lo, mean[2] + axis[2] * lo);
    if (c0 < c1)
        std::swap(c0, c1);
    std::uint32_t indices = 0;
    if (c0 != c1)   // c0 > c1: the 4-colour (opaque) mode
    {
        const auto a = from565(c0), b = from565(c1);
        std::array<std::array<float, 3>, 4> palette;
        for (int k = 0; k < 3; ++k)
        {
            palette[0][k] = static_cast<float>(a[k]);
            palette[1][k] = static_cast<float>(b[k]);
            palette[2][k] = (2.0f * a[k] + b[k]) / 3.0f;
            palette[3][k] = (a[k] + 2.0f * b[k]) / 3.0f;
        }
        for (int i = 0; i < 16; ++i)
        {
            int best = 0;
            float bestD = 1e30f;
            for (int j = 0; j < 4; ++j)
            {
                float d = 0.0f;
                for (int k = 0; k < 3; ++k)
                    d += (px[static_cast<std::size_t>(i)][k] - palette[static_cast<std::size_t>(j)][k]) *
                         (px[static_cast<std::size_t>(i)][k] - palette[static_cast<std::size_t>(j)][k]);
                if (d < bestD)
                {
                    bestD = d;
                    best = j;
                }
            }
            indices |= static_cast<std::uint32_t>(best) << (2 * i);
        }
    }
    out[0] = static_cast<std::uint8_t>(c0 & 0xFF);
    out[1] = static_cast<std::uint8_t>(c0 >> 8);
    out[2] = static_cast<std::uint8_t>(c1 & 0xFF);
    out[3] = static_cast<std::uint8_t>(c1 >> 8);
    for (int k = 0; k < 4; ++k)
        out[4 + k] = static_cast<std::uint8_t>((indices >> (8 * k)) & 0xFF);
}
} // namespace

std::vector<std::uint8_t> encodeBC1(const std::uint8_t* rgba, int width, int height)
{
    const int bw = (width + 3) / 4, bh = (height + 3) / 4;
    std::vector<std::uint8_t> out(bc1Size(width, height));
    std::array<std::array<float, 3>, 16> px;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx)
        {
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                {
                    const int sx = std::min(bx * 4 + x, width - 1), sy = std::min(by * 4 + y, height - 1);
                    const std::uint8_t* p = rgba + (static_cast<std::size_t>(sy) * width + sx) * 4;
                    px[static_cast<std::size_t>(y * 4 + x)] = {static_cast<float>(p[0]), static_cast<float>(p[1]),
                                                               static_cast<float>(p[2])};
                }
            encodeBlock(px, out.data() + (static_cast<std::size_t>(by) * bw + bx) * 8);
        }
    return out;
}

void decodeBC1(const std::uint8_t* blocks, int width, int height, std::uint8_t* rgbaOut)
{
    const int bw = (width + 3) / 4, bh = (height + 3) / 4;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx)
        {
            const std::uint8_t* b = blocks + (static_cast<std::size_t>(by) * bw + bx) * 8;
            const std::uint16_t c0 = static_cast<std::uint16_t>(b[0] | (b[1] << 8));
            const std::uint16_t c1 = static_cast<std::uint16_t>(b[2] | (b[3] << 8));
            const auto a = from565(c0), c = from565(c1);
            std::array<std::array<int, 3>, 4> pal;
            for (int k = 0; k < 3; ++k)
            {
                pal[0][k] = a[k];
                pal[1][k] = c[k];
                pal[2][k] = c0 > c1 ? (2 * a[k] + c[k]) / 3 : (a[k] + c[k]) / 2;
                pal[3][k] = c0 > c1 ? (a[k] + 2 * c[k]) / 3 : 0;
            }
            const std::uint32_t idx = static_cast<std::uint32_t>(b[4]) | (static_cast<std::uint32_t>(b[5]) << 8) |
                                      (static_cast<std::uint32_t>(b[6]) << 16) | (static_cast<std::uint32_t>(b[7]) << 24);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                {
                    const int px = bx * 4 + x, py = by * 4 + y;
                    if (px >= width || py >= height)
                        continue;
                    const auto& col = pal[(idx >> (2 * (y * 4 + x))) & 3];
                    std::uint8_t* o = rgbaOut + (static_cast<std::size_t>(py) * width + px) * 4;
                    o[0] = static_cast<std::uint8_t>(col[0]);
                    o[1] = static_cast<std::uint8_t>(col[1]);
                    o[2] = static_cast<std::uint8_t>(col[2]);
                    o[3] = 255;
                }
        }
}

std::vector<std::vector<std::uint8_t>> encodeBC1MipChain(const std::uint8_t* rgba, int width, int height)
{
    std::vector<std::vector<std::uint8_t>> levels;
    std::vector<std::uint8_t> level(rgba, rgba + static_cast<std::size_t>(width) * height * 4);
    int w = width, h = height;
    for (;;)
    {
        levels.push_back(encodeBC1(level.data(), w, h));
        if (w == 1 && h == 1)
            break;
        const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
        std::vector<std::uint8_t> next(static_cast<std::size_t>(nw) * nh * 4);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x)
                for (int k = 0; k < 4; ++k)
                {
                    int sum = 0;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx)
                        {
                            const int sx = std::min(x * 2 + dx, w - 1), sy = std::min(y * 2 + dy, h - 1);
                            sum += level[(static_cast<std::size_t>(sy) * w + sx) * 4 + k];
                        }
                    next[(static_cast<std::size_t>(y) * nw + x) * 4 + k] = static_cast<std::uint8_t>((sum + 2) / 4);
                }
        level = std::move(next);
        w = nw;
        h = nh;
    }
    return levels;
}
} // namespace world::celestial::surface
