#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace world::interstellar
{

/*
    Coarse 3D density lattice of the interstellar medium - the one data
    contract between the generator (src/world/generation/InterstellarGenerator)
    and its readers (Galaxy Map, future ship sampling).

    Voxel (i, j, k) has its centre at originLy + (i + 0.5, j + 0.5, k + 0.5)
    * voxelLy (ICRS light-years), index i + nx (j + ny k). values are 0..255:
      encoding "relative": density = value / 255 x peakDensityCm3
      encoding "log10":    log10(density) = log10Min + value / 255 x (log10Max - log10Min)
    phases: optional phase code per voxel (see mediumPhaseName).
*/

// Phase codes: h hot_ionized, p warm_partially_ionized, w warm_neutral,
// c cold_neutral, m molecular (dust-rich), s supernova_enriched_shell.
inline const char* mediumPhaseName(char code)
{
    switch (code)
    {
    case 'h': return "hot_ionized";
    case 'p': return "warm_partially_ionized";
    case 'w': return "warm_neutral";
    case 'c': return "cold_neutral";
    case 'm': return "molecular";
    case 's': return "supernova_enriched_shell";
    default: return "unknown";
    }
}

inline char mediumPhaseCode(const std::string& name)
{
    for (const char c : std::string("hpwcms"))
        if (name == mediumPhaseName(c))
            return c;
    return 0;
}

struct DensityLattice
{
    glm::dvec3 originLy {0.0};
    double voxelLy = 1.0;
    glm::ivec3 dims {0};
    std::string encoding = "relative";
    double peakDensityCm3 = 0.0;
    double log10Min = -3.5;
    double log10Max = 2.5;
    std::vector<std::uint8_t> values;
    std::string phases;

    std::size_t size() const
    {
        return static_cast<std::size_t>(std::max(dims.x, 0)) * std::max(dims.y, 0) * std::max(dims.z, 0);
    }

    glm::dvec3 voxelCentreLy(int i, int j, int k) const
    {
        return originLy + (glm::dvec3(i, j, k) + 0.5) * voxelLy;
    }

    double densityCm3(std::size_t index) const
    {
        const double u = values[index] / 255.0;
        return encoding == "log10" ? std::pow(10.0, log10Min + u * (log10Max - log10Min)) : u * peakDensityCm3;
    }
};

namespace detail
{

inline std::string toHex(const std::vector<std::uint8_t>& bytes)
{
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes)
    {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}

inline bool fromHex(const std::string& s, std::vector<std::uint8_t>& out)
{
    if (s.size() % 2 != 0)
        return false;
    out.resize(s.size() / 2);
    const auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const int hi = nib(s[2 * i]), lo = nib(s[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    return true;
}

} // namespace detail

inline nlohmann::ordered_json latticeJson(const DensityLattice& L)
{
    nlohmann::ordered_json j;
    j["origin_ly"] = {L.originLy.x, L.originLy.y, L.originLy.z};
    j["voxel_ly"] = L.voxelLy;
    j["dims"] = {L.dims.x, L.dims.y, L.dims.z};
    j["encoding"] = L.encoding;
    if (L.encoding == "log10")
    {
        j["log10_min"] = L.log10Min;
        j["log10_max"] = L.log10Max;
    }
    else
    {
        j["peak_density_cm3"] = L.peakDensityCm3;
    }
    j["values_hex"] = detail::toHex(L.values);
    if (!L.phases.empty())
        j["phases"] = L.phases;
    return j;
}

inline bool latticeFromJson(const nlohmann::json& j, DensityLattice& out)
{
    try
    {
        const auto& o = j.at("origin_ly");
        const auto& d = j.at("dims");
        out.originLy = {o.at(0).get<double>(), o.at(1).get<double>(), o.at(2).get<double>()};
        out.voxelLy = j.at("voxel_ly").get<double>();
        out.dims = {d.at(0).get<int>(), d.at(1).get<int>(), d.at(2).get<int>()};
        out.encoding = j.value("encoding", std::string("relative"));
        out.peakDensityCm3 = j.value("peak_density_cm3", 0.0);
        out.log10Min = j.value("log10_min", -3.5);
        out.log10Max = j.value("log10_max", 2.5);
        out.phases = j.value("phases", std::string());
        const std::size_t n = out.size();
        return out.dims.x > 0 && out.dims.y > 0 && out.dims.z > 0 && out.voxelLy > 0.0 &&
               detail::fromHex(j.at("values_hex").get<std::string>(), out.values) && out.values.size() == n &&
               (out.phases.empty() || out.phases.size() == n);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

} // namespace world::interstellar
