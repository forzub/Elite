#include "src/world/celestial/surface/AppearancePassport.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace world::celestial::surface
{
namespace
{
namespace fs = std::filesystem;

std::unordered_map<std::string, AppearancePassport> loadPassports()
{
    std::unordered_map<std::string, AppearancePassport> out;
    // DEV: ELITE_APPEARANCE_PASSPORTS=0 - no passports (A/B checks against the curated / placeholder maps)
    if (const char* v = std::getenv("ELITE_APPEARANCE_PASSPORTS"); v && *v == '0')
        return out;
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    fs::path file;
    for (const char* rel : {"assets/data/celestial/appearance_passports.json",
                            "src/assets/data/celestial/appearance_passports.json"})
    {
        for (const fs::path& base : {cwd, cwd.parent_path(), cwd.parent_path().parent_path()})
            if (fs::exists(base / rel, ec))
            {
                file = base / rel;
                break;
            }
        if (!file.empty())
            break;
    }
    if (file.empty())
        return out;
    try
    {
        std::ifstream in(file);
        const nlohmann::json j = nlohmann::json::parse(in);
        for (const auto& b : j.at("bodies"))
        {
            AppearancePassport p;
            p.bodyId = b.at("id").get<std::string>();
            p.procedural = b.value("procedural", false);
            if (b.contains("mean_albedo_linear"))
            {
                const auto& c = b.at("mean_albedo_linear");
                p.meanAlbedo = glm::vec3(c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>());
                p.hasMeanAlbedo = true;
            }
            p.reference = b.value("reference", std::string());
            out[p.bodyId] = p;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[AppearancePassport] " << file.generic_string() << ": " << e.what() << "\n";
        out.clear();
    }
    return out;
}

const std::unordered_map<std::string, AppearancePassport>& passports()
{
    static std::once_flag once;
    static std::unordered_map<std::string, AppearancePassport> all;
    std::call_once(once, [] { all = loadPassports(); });
    return all;
}

class PassportSurfaceField final : public SurfaceField
{
public:
    PassportSurfaceField(std::unique_ptr<SurfaceField> inner, const glm::vec3& gain)
        : m_inner(std::move(inner)), m_gain(gain)
    {
    }
    double referenceRadiusM() const override { return m_inner->referenceRadiusM(); }
    std::uint64_t identitySeed() const override { return m_inner->identitySeed(); }
    bool solidSurface() const override { return m_inner->solidSurface(); }
    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override
    {
        return m_inner->sample(dir, minWavelengthM);
    }
    SurfaceAppearance appearance(const SurfaceSample& s, const glm::dvec3& dir, double slope) const override
    {
        SurfaceAppearance a = m_inner->appearance(s, dir, slope);
        a.albedo = glm::clamp(a.albedo * m_gain, glm::vec3(0.0f), glm::vec3(1.0f));
        return a;
    }
    double detailAmplitudeBoundM() const override { return m_inner->detailAmplitudeBoundM(); }
    double macroCutoffM() const override { return m_inner->macroCutoffM(); }
    void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const override
    {
        m_inner->landmarks(dir, radiusRad, out);
    }
    std::size_t memoryBytes() const override { return m_inner->memoryBytes(); }

private:
    std::unique_ptr<SurfaceField> m_inner;
    glm::vec3 m_gain;
};
} // namespace

const AppearancePassport* findAppearancePassport(const std::string& bodyId)
{
    const auto& all = passports();
    const auto it = all.find(bodyId);
    return it == all.end() ? nullptr : &it->second;
}

glm::vec3 meanFieldAlbedo(const SurfaceField& field)
{
    // Fibonacci sphere, a band limit of ~1/256 of the circumference: the
    // macro pattern decides the mean, not the small detail.
    constexpr int N = 2048;
    const double band = 2.0 * 3.14159265358979323846 * field.referenceRadiusM() / 256.0;
    const double golden = 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
    glm::dvec3 sum(0.0);
    for (int i = 0; i < N; ++i)
    {
        const double y = 1.0 - 2.0 * (i + 0.5) / N;
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double phi = golden * i;
        const glm::dvec3 dir(r * std::cos(phi), y, r * std::sin(phi));
        const SurfaceSample s = field.sample(dir, band);
        sum += glm::dvec3(field.appearance(s, dir, 0.0).albedo);
    }
    return glm::vec3(sum / static_cast<double>(N));
}

std::unique_ptr<SurfaceField> applyAppearancePassport(std::unique_ptr<SurfaceField> field, const std::string& bodyId)
{
    const AppearancePassport* p = field ? findAppearancePassport(bodyId) : nullptr;
    if (!p || !p->hasMeanAlbedo)
        return field;
    const glm::vec3 mean = glm::max(meanFieldAlbedo(*field), glm::vec3(1.0e-4f));
    const glm::vec3 gain = glm::clamp(p->meanAlbedo / mean, glm::vec3(0.02f), glm::vec3(20.0f));
    return std::make_unique<PassportSurfaceField>(std::move(field), gain);
}
} // namespace world::celestial::surface
