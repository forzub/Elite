#include "src/world/generation/SystemValidation.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "src/world/generation/AstroUnits.h"

namespace world::generation
{

namespace
{

bool finite(double v)
{
    return std::isfinite(v);
}

struct Gate
{
    std::string name;
    std::vector<std::string> failures;

    void fail(const std::string& what)
    {
        if (failures.size() < 8)
            failures.push_back(what);
        else if (failures.size() == 8)
            failures.push_back("...");
    }
};

} // namespace

bool validateSystem(SystemDescriptor& sd)
{
    using namespace astro;
    Gate hierarchy {"hierarchy_acyclic_parents_valid", {}};
    Gate finiteness {"finite_values", {}};
    Gate density {"sane_densities", {}};
    Gate composition {"normalised_compositions", {}};
    Gate moons {"moon_hill_roche", {}};
    Gate rings {"ring_bounds", {}};
    Gate crossing {"non_crossing_orbits", {}};

    std::set<std::string> known;
    std::map<std::string, const BodyDescriptor*> bodies;
    std::map<std::string, double> massKgById;
    for (const auto& s : sd.stars)
    {
        known.insert(s.id);
        massKgById[s.id] = s.massSolar * SunMassKg;
        if (!finite(s.massSolar) || !finite(s.luminositySolar) || !finite(s.radiusSolar) || s.massSolar <= 0.0)
            finiteness.fail(s.id);
    }
    for (const auto& n : sd.barycenters)
    {
        known.insert(n.bodyId);
        massKgById[n.bodyId] = n.massSolar * SunMassKg;
    }
    // Barycenter tree: parents must exist and the chain must terminate.
    for (const auto& n : sd.barycenters)
    {
        std::string p = n.parentNode;
        int depth = 0;
        while (!p.empty() && depth < 16)
        {
            const auto it = std::find_if(sd.barycenters.begin(), sd.barycenters.end(),
                                         [&](const BarycenterDescriptor& b) { return b.id == p; });
            if (it == sd.barycenters.end()) { hierarchy.fail(n.id + " -> missing " + p); break; }
            p = it->parentNode;
            ++depth;
        }
        if (depth >= 16) hierarchy.fail(n.id + " cyclic");
    }

    for (const auto& b : sd.bodies)
    {
        if (!known.count(b.parentId))
            hierarchy.fail(b.id + " parent " + b.parentId + " not defined before it");
        if (known.count(b.id))
            hierarchy.fail(b.id + " duplicate id");
        known.insert(b.id);
        bodies[b.id] = &b;
        massKgById[b.id] = b.massKg;

        const double vals[] = {b.massKg, b.radiusKm, b.densityKgM3, b.muM3s2, b.surfaceGravityMs2,
                               b.orbit.semiMajorAxisAu, b.orbit.eccentricity, b.orbit.periodDays,
                               b.meanSurfaceTemperatureK, b.equilibriumTemperatureK, b.atmosphere.surfacePressurePa};
        for (const double v : vals)
            if (!finite(v)) { finiteness.fail(b.id); break; }
        if (b.massKg <= 0.0 || b.radiusKm <= 0.0 || b.orbit.semiMajorAxisAu <= 0.0 ||
            b.orbit.eccentricity < 0.0 || b.orbit.eccentricity >= 1.0)
            finiteness.fail(b.id + " non-positive mass/radius/orbit");

        const bool fluid = b.bodyClass == "gas_giant" || b.bodyClass == "super_jupiter" ||
            b.bodyClass == "ice_giant" || b.bodyClass == "sub_neptune";
        const double lo = fluid ? 100.0 : 400.0;
        if (b.densityKgM3 < lo || b.densityKgM3 > 30000.0)
        {
            std::ostringstream s;
            s << b.id << " rho=" << b.densityKgM3;
            density.fail(s.str());
        }
        const auto& c = b.composition;
        const double sum = c.ironCore + c.silicateMantle + c.waterIce + c.otherVolatiles + c.hydrogenHeliumEnvelope;
        if (std::abs(sum - 1.0) > 0.02 && b.composition.hydrogenHeliumEnvelope < 0.99)
        {
            std::ostringstream s;
            s << b.id << " bulk sum=" << sum;
            composition.fail(s.str());
        }
        double gas = 0.0;
        for (const auto& g : b.atmosphere.composition) gas += g.second;
        if (!b.atmosphere.composition.empty() && std::abs(gas - 1.0) > 0.01)
            composition.fail(b.id + " atmosphere");

        if (b.kind == "moon")
        {
            const auto pit = bodies.find(b.parentId);
            if (pit != bodies.end())
            {
                const BodyDescriptor& p = *pit->second;
                const double aKm = b.orbit.semiMajorAxisAu * AuMeters / 1000.0;
                const double roche = 2.44 * p.radiusKm * std::cbrt(p.densityKgM3 / std::max(b.densityKgM3, 100.0));
                const double hillMax = (b.orbit.direction < 0 ? 0.6 : 0.4) * p.hillRadiusKm;
                const bool authored = b.origin == "authored" || b.origin == "observed_anchor";
                if (!authored && (aKm < 0.95 * roche || aKm * (1.0 + b.orbit.eccentricity) > hillMax * 1.05))
                {
                    std::ostringstream s;
                    s << b.id << " a=" << aKm << "km roche=" << roche << " hill=" << hillMax;
                    moons.fail(s.str());
                }
            }
        }
        for (const auto& r : b.rings)
        {
            if (!(r.innerRadiusKm > b.radiusKm && r.outerRadiusKm > r.innerRadiusKm))
                rings.fail(b.id + ":" + r.id);
        }
    }

    // Planets about the same parent must not cross (observed pairs reported, not removed).
    std::map<std::string, std::vector<const BodyDescriptor*>> byParent;
    for (const auto& b : sd.bodies)
        if (b.kind != "moon") byParent[b.parentId].push_back(&b);
    for (auto& [parent, list] : byParent)
    {
        std::sort(list.begin(), list.end(), [](auto* x, auto* y) {
            return x->orbit.semiMajorAxisAu < y->orbit.semiMajorAxisAu;
        });
        for (std::size_t k = 0; k + 1 < list.size(); ++k)
        {
            const auto* p = list[k];
            const auto* q = list[k + 1];
            if (p->kind == "dwarf_planet" || q->kind == "dwarf_planet")
                continue; // reservoir members share their belt by construction
            const double apo = p->orbit.semiMajorAxisAu * (1.0 + p->orbit.eccentricity);
            const double peri = q->orbit.semiMajorAxisAu * (1.0 - q->orbit.eccentricity);
            if (apo >= peri)
                crossing.fail(p->id + " x " + q->id);
        }
    }

    bool all = true;
    for (Gate* g : {&hierarchy, &finiteness, &density, &composition, &moons, &rings, &crossing})
    {
        StabilityCheck c;
        c.name = "validate_" + g->name;
        c.passed = g->failures.empty();
        std::ostringstream d;
        for (std::size_t k = 0; k < g->failures.size(); ++k)
            d << (k ? "; " : "") << g->failures[k];
        c.detail = c.passed ? "ok" : d.str();
        all = all && c.passed;
        sd.stability.push_back(c);
    }
    return all;
}

} // namespace world::generation
