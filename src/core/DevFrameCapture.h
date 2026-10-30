#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace core::dev
{

/*
    DEV only: the game's OWN back buffer written as a PNG (glReadPixels of
    this window's framebuffer - never a screen grab, so no other window can
    appear in it). ELITE_DEV_FRAME_CAPTURE="SECONDS:PATH[;SECONDS:PATH...]"
    captures once at each time since startup. Uncompressed (stored deflate)
    PNG, no external library.
*/
namespace detail
{
inline std::uint32_t crc32(const unsigned char* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu)
{
    static const auto table = [] {
        std::array<std::uint32_t, 256> t {};
        for (std::uint32_t n = 0; n < 256; ++n)
        {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    for (std::size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

inline void be32(std::vector<unsigned char>& out, std::uint32_t v)
{
    out.push_back(static_cast<unsigned char>(v >> 24));
    out.push_back(static_cast<unsigned char>(v >> 16));
    out.push_back(static_cast<unsigned char>(v >> 8));
    out.push_back(static_cast<unsigned char>(v));
}

inline void chunk(std::ofstream& f, const char* type, const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    std::vector<unsigned char> head;
    be32(head, static_cast<std::uint32_t>(data.size()));
    f.write(reinterpret_cast<const char*>(head.data()), 4);
    f.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
    std::vector<unsigned char> crc;
    be32(crc, crc32(body.data(), body.size()) ^ 0xFFFFFFFFu);
    f.write(reinterpret_cast<const char*>(crc.data()), 4);
}
} // namespace detail

// rgb: bottom-up rows (OpenGL order), 3 bytes per pixel.
inline bool writePngRgb(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    f.write(reinterpret_cast<const char*>(signature), 8);
    std::vector<unsigned char> ihdr;
    detail::be32(ihdr, static_cast<std::uint32_t>(width));
    detail::be32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    detail::chunk(f, "IHDR", ihdr);
    // Raw scanlines (filter 0), top-down.
    std::vector<unsigned char> raw;
    const std::size_t stride = static_cast<std::size_t>(width) * 3;
    raw.reserve((stride + 1) * static_cast<std::size_t>(height));
    for (int y = height - 1; y >= 0; --y)
    {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(stride * y),
                   rgb.begin() + static_cast<std::ptrdiff_t>(stride * (y + 1)));
    }
    // zlib stream of stored blocks.
    std::vector<unsigned char> z {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (const unsigned char c : raw)
    {
        a = (a + c) % 65521u;
        b = (b + a) % 65521u;
    }
    for (std::size_t pos = 0; pos < raw.size();)
    {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(n & 0xFF));
        z.push_back(static_cast<unsigned char>(n >> 8));
        z.push_back(static_cast<unsigned char>(~n & 0xFF));
        z.push_back(static_cast<unsigned char>((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    }
    detail::be32(z, (b << 16) | a);
    detail::chunk(f, "IDAT", z);
    detail::chunk(f, "IEND", {});
    return static_cast<bool>(f);
}

struct FrameCaptureRequest
{
    double atSeconds = 0.0;
    std::string path;
    bool done = false;
};

inline std::vector<FrameCaptureRequest>& frameCaptureRequests()
{
    static std::vector<FrameCaptureRequest> requests = [] {
        std::vector<FrameCaptureRequest> out;
        const char* v = std::getenv("ELITE_DEV_FRAME_CAPTURE");
        if (!v || !*v)
            return out;
        std::stringstream all(v);
        std::string item;
        while (std::getline(all, item, ';'))
        {
            const auto colon = item.find(':');
            if (colon == std::string::npos)
                continue;
            out.push_back({std::atof(item.substr(0, colon).c_str()), item.substr(colon + 1), false});
        }
        return out;
    }();
    return requests;
}

} // namespace core::dev
