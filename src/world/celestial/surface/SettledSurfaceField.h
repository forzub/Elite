#pragma once

// A body's surface field with its settlements built in: flat discs under the landing pads and the
// structures (the 30 m relief height at each centre), blended into the relief around them. Applied
// once in loadBodySurface, so the renderer draws and the server collides with the SAME ground.

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"
#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::surface
{

class SettledSurfaceField final : public SurfaceField
{
public:
    SettledSurfaceField(std::shared_ptr<SurfaceField> base, const std::vector<settlements::SettlementLayout>& layouts)
        : m_base(std::move(base))
    {
        const double radius = std::max(1.0, m_base->referenceRadiusM());
        for (const auto& layout : layouts)
            for (const auto& s : layout.structures)
            {
                if (s.flattenRadiusM <= 0.0)
                    continue;
                Disc d;
                d.dir = glm::normalize(s.bodyDirection);
                d.radiusM = s.flattenRadiusM;
                d.blendM = std::max(25.0, 0.8 * s.flattenRadiusM);
                d.cosOuter = std::cos((d.radiusM + d.blendM) / radius);
                d.heightM = m_base->sample(d.dir, 30.0).heightM;   // flat at the local relief
                m_discs.push_back(d);
            }
    }

    std::size_t discCount() const { return m_discs.size(); }

    double referenceRadiusM() const override { return m_base->referenceRadiusM(); }
    std::uint64_t identitySeed() const override { return m_base->identitySeed(); }
    bool solidSurface() const override { return m_base->solidSurface(); }

    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override
    {
        SurfaceSample s = m_base->sample(dir, minWavelengthM);
        if (m_discs.empty())
            return s;
        const glm::dvec3 n = glm::normalize(dir);
        const double radius = m_base->referenceRadiusM();
        for (const auto& d : m_discs)
        {
            const double c = glm::dot(n, d.dir);
            if (c < d.cosOuter)
                continue;
            const double dist = std::acos(std::min(1.0, c)) * radius;
            double w = 1.0;
            if (dist > d.radiusM)
            {
                const double t = std::clamp(1.0 - (dist - d.radiusM) / d.blendM, 0.0, 1.0);
                w = t * t * (3.0 - 2.0 * t);
            }
            s.heightM += (d.heightM - s.heightM) * w;
        }
        return s;
    }

    SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir, double slope) const override
    {
        return m_base->appearance(sample, dir, slope);
    }
    double detailAmplitudeBoundM() const override { return m_base->detailAmplitudeBoundM(); }
    double macroCutoffM() const override { return m_base->macroCutoffM(); }
    void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const override
    {
        m_base->landmarks(dir, radiusRad, out);
    }

private:
    struct Disc
    {
        glm::dvec3 dir {0.0, 1.0, 0.0};
        double radiusM = 0.0;
        double blendM = 0.0;
        double cosOuter = 1.0;
        double heightM = 0.0;
    };
    std::shared_ptr<SurfaceField> m_base;
    std::vector<Disc> m_discs;
};

} // namespace world::celestial::surface
