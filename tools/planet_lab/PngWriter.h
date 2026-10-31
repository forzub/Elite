#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace planet_lab
{
    // ---- PNG (stored deflate) ---------------------------------------------
    inline std::uint32_t crc32(const unsigned char* p, std::size_t n, std::uint32_t c = 0xFFFFFFFFu)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            c ^= p[i];
            for (int k = 0; k < 8; ++k)
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
        return c;
    }

    // rgb: GL rows (bottom-up).
    inline void writePng(const std::string& path, int w, int h, const std::vector<unsigned char>& rgb)
    {
        std::vector<unsigned char> raw;
        raw.reserve(static_cast<std::size_t>((w * 3 + 1) * h));
        for (int y = h - 1; y >= 0; --y) // GL rows are bottom-up
        {
            raw.push_back(0);
            raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y * w * 3),
                       rgb.begin() + static_cast<std::ptrdiff_t>((y + 1) * w * 3));
        }
        std::vector<unsigned char> z {0x78, 0x01};
        std::uint32_t a = 1, b = 0;
        for (std::size_t off = 0; off < raw.size(); off += 65535)
        {
            const std::size_t len = std::min<std::size_t>(65535, raw.size() - off);
            z.push_back(off + len == raw.size() ? 1 : 0);
            z.push_back(static_cast<unsigned char>(len & 0xFF));
            z.push_back(static_cast<unsigned char>(len >> 8));
            z.push_back(static_cast<unsigned char>(~len & 0xFF));
            z.push_back(static_cast<unsigned char>((~len >> 8) & 0xFF));
            z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(off),
                     raw.begin() + static_cast<std::ptrdiff_t>(off + len));
        }
        for (unsigned char c : raw)
        {
            a = (a + c) % 65521u;
            b = (b + a) % 65521u;
        }
        const std::uint32_t adler = (b << 16) | a;
        for (int s = 24; s >= 0; s -= 8)
            z.push_back(static_cast<unsigned char>(adler >> s));

        std::ofstream out(path, std::ios::binary);
        const unsigned char sig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        out.write(reinterpret_cast<const char*>(sig), 8);
        const auto chunk = [&](const char* type, const std::vector<unsigned char>& data) {
            const std::uint32_t len = static_cast<std::uint32_t>(data.size());
            const unsigned char l[] = {static_cast<unsigned char>(len >> 24), static_cast<unsigned char>(len >> 16),
                                       static_cast<unsigned char>(len >> 8), static_cast<unsigned char>(len)};
            out.write(reinterpret_cast<const char*>(l), 4);
            std::vector<unsigned char> body(type, type + 4);
            body.insert(body.end(), data.begin(), data.end());
            out.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
            const std::uint32_t c = crc32(body.data(), body.size()) ^ 0xFFFFFFFFu;
            const unsigned char cc[] = {static_cast<unsigned char>(c >> 24), static_cast<unsigned char>(c >> 16),
                                        static_cast<unsigned char>(c >> 8), static_cast<unsigned char>(c)};
            out.write(reinterpret_cast<const char*>(cc), 4);
        };
        std::vector<unsigned char> ihdr = {
            static_cast<unsigned char>(w >> 24), static_cast<unsigned char>(w >> 16), static_cast<unsigned char>(w >> 8),
            static_cast<unsigned char>(w), static_cast<unsigned char>(h >> 24), static_cast<unsigned char>(h >> 16),
            static_cast<unsigned char>(h >> 8), static_cast<unsigned char>(h), 8, 2, 0, 0, 0};
        chunk("IHDR", ihdr);
        chunk("IDAT", z);
        chunk("IEND", {});
    }

} // namespace planet_lab
