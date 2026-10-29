#include "src/world/generation/StellarSystemGenerator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/BodyPhysics.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/PlanetFormation.h"
#include "src/world/generation/SatelliteSystems.h"
#include "src/world/generation/StellarPhysics.h"
#include "src/world/generation/SystemValidation.h"

namespace world::generation
{

namespace
{

using namespace astro;

struct HostRun
{
    PlanetHost host;
    std::string prefix;             // planet name prefix
    const StarDescriptor* lightStar = nullptr;
    std::vector<std::pair<double, double>> companions; // (L_solar, distance AU) outside the host
    std::vector<const ObservedBodyInput*> anchors;
    FormationResult formation;
};

double periodDays(double aAu, double massKg)
{
    const double a = aAu * AuMeters;
    return TwoPi * std::sqrt(a * a * a / (G * massKg)) / DaySeconds;
}

std::string stripComponentLetter(std::string name)
{
    if (name.size() > 2 && name[name.size() - 2] == ' ' && std::isupper(static_cast<unsigned char>(name.back())))
        name.resize(name.size() - 2);
    return name;
}

std::string upper(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::uint64_t catalogSystemSeed(std::uint64_t universeSeed, int systemId)
{
    return deriveSeed(universeSeed, "catalog_system", static_cast<std::uint64_t>(systemId));
}

std::uint64_t galacticCellSeed(std::uint64_t universeSeed, long long cx, long long cy, long long cz)
{
    std::uint64_t h = deriveSeed(universeSeed, "galactic_cell_x", static_cast<std::uint64_t>(cx));
    h = deriveSeed(h, "galactic_cell_y", static_cast<std::uint64_t>(cy));
    return deriveSeed(h, "galactic_cell_z", static_cast<std::uint64_t>(cz));
}

std::string safeIdPart(const std::string& name)
{
    std::string s = name;
    for (char& c : s)
        if (c == ' ' || c == '(' || c == ')' || c == '/' || c == '\\')
            c = '_';
    return s;
}

std::string fileSlug(const std::string& name, const std::vector<std::string>& alternatives)
{
    auto ascii = [](const std::string& s) {
        for (unsigned char c : s)
            if (c >= 0x80) return false;
        return !s.empty();
    };
    std::string src = name;
    if (!ascii(src))
        for (const auto& a : alternatives)
            if (ascii(a)) { src = a; break; }
    std::string out;
    for (unsigned char c : src)
    {
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
        else if (!out.empty() && out.back() != '_') out += '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty())
    {
        std::ostringstream h;
        h << std::hex << hashTag(name);
        out = "body_" + h.str();
    }
    return out;
}

SystemDescriptor generateStellarSystem(
    const SystemInput& in, std::uint64_t universeSeed, const UniverseGenerationVersions& versions)
{
    SystemDescriptor sd;
    sd.systemId = in.id;
    sd.name = in.name;
    sd.universeSeed = universeSeed;
    sd.systemSeed = catalogSystemSeed(universeSeed, in.id);
    sd.versions = versions;
    const std::uint64_t seed = sd.systemSeed;

    sd.context = buildGalacticContext(in.positionLy, in.stars, seed);
    double age = 4.6, feh = 0.0;
    ProvenanceMap systemProvenance;
    sampleSystemAgeMetallicity(in.stars, sd.context, seed, age, feh, systemProvenance);

    for (const auto& s : in.stars)
    {
        StarDescriptor st = buildStar(s, sd.context, age, feh, deriveSeed(seed, "star_" + s.key), sd.warnings);
        st.id = "system_" + std::to_string(in.id) + "." + safeIdPart(st.name);
        if (!s.ageGyr && systemProvenance["age_gyr"] == Provenance::Generated)
            st.provenance["age_gyr"] = Provenance::Generated;
        if (!s.metallicityFeH && systemProvenance["metallicity_feh"] == Provenance::Generated)
            st.provenance["metallicity_feh"] = Provenance::Generated;
        sd.stars.push_back(st);
    }
    // Stable zone each star needs for its observed planets (apoapsis + margin).
    std::map<std::string, double> requiredZone;
    for (const auto& o : in.observedBodies)
    {
        if (o.kind == "moon")
            continue;
        const StarDescriptor* host = nullptr;
        for (const auto& s : sd.stars) if (s.key == o.host) host = &s;
        if (!host)
            continue;
        const double a = o.semiMajorAxisAu ? *o.semiMajorAxisAu
            : o.periodDays ? semiMajorAxisAuFromPeriod(*o.periodDays / 365.25, host->massSolar) : 0.0;
        const double need = a * (1.0 + o.eccentricity.value_or(0.0)) * 1.15;
        requiredZone[o.host] = std::max(requiredZone[o.host], need);
    }
    sd.barycenters = buildStellarHierarchy(in.hierarchy, sd.stars, in.id, seed, requiredZone, sd.stability, sd.warnings);

    auto findStar = [&](const std::string& key) -> const StarDescriptor* {
        for (const auto& s : sd.stars) if (s.key == key) return &s;
        return nullptr;
    };
    auto findNode = [&](const std::string& id) -> const BarycenterDescriptor* {
        for (const auto& n : sd.barycenters) if (n.id == id) return &n;
        return nullptr;
    };
    auto subtreeStars = [&](const std::string& id) {
        std::vector<const StarDescriptor*> out;
        std::vector<std::string> stack {id};
        while (!stack.empty())
        {
            const std::string cur = stack.back();
            stack.pop_back();
            if (const auto* s = findStar(cur)) { out.push_back(s); continue; }
            if (const auto* n = findNode(cur))
                for (const auto& c : n->children) stack.push_back(c);
        }
        return out;
    };
    // Separation between a subtree and each star outside it (relative orbit of the joining node).
    auto companionsOf = [&](const std::string& id, std::string parent) {
        std::vector<std::pair<double, double>> out;
        std::string child = id;
        while (!parent.empty())
        {
            const auto* p = findNode(parent);
            if (!p || !p->relativeOrbit) break;
            for (const auto& c : p->children)
                if (c != child)
                    for (const auto* s : subtreeStars(c))
                        out.push_back({s->luminositySolar, p->relativeOrbit->semiMajorAxisAu});
            child = parent;
            parent = p->parentNode;
        }
        return out;
    };
    auto nodeOuterLimit = [&](const std::string& id, std::string parent) {
        double limit = 1.0e5;
        std::string child = id;
        while (!parent.empty())
        {
            const auto* p = findNode(parent);
            if (!p || !p->relativeOrbit || p->children.size() != 2) break;
            const std::string other = p->children[0] == child ? p->children[1] : p->children[0];
            double mc = 0.0, mo = 0.0;
            for (const auto* s : subtreeStars(child)) mc += s->massSolar;
            for (const auto* s : subtreeStars(other)) mo += s->massSolar;
            limit = std::min(limit, holmanWiegertSTypeCritical(mo / (mc + mo), p->relativeOrbit->eccentricity) *
                                        p->relativeOrbit->semiMajorAxisAu);
            child = parent;
            parent = p->parentNode;
        }
        return limit;
    };

    // ---- planet hosts -------------------------------------------------
    std::vector<HostRun> hosts;
    for (const auto& s : sd.stars)
    {
        const double rAu = s.radiusSolar * SunRadiusM / AuMeters;
        if (s.stableZoneOuterAu < std::max(0.02, 6.0 * rAu))
        {
            sd.stability.push_back({"host_" + s.key, true, "no stable S-type zone around this star"});
            continue;
        }
        HostRun h;
        PlanetHost& ph = h.host;
        ph.nodeId = s.key;
        ph.bodyId = s.id;
        ph.lightStarKey = s.key;
        ph.massSolar = s.massSolar;
        ph.initialMassSolar = s.initialMassSolar;
        ph.zamsLuminositySolar = s.stage == StellarStage::BrownDwarf ? s.luminositySolar * 10.0
                                                                     : zeroAgeLuminositySolar(s.initialMassSolar);
        ph.luminositySolar = s.luminositySolar;
        ph.radiusSolar = s.radiusSolar;
        ph.metallicityFeH = s.metallicityFeH;
        ph.ageGyr = s.ageGyr;
        ph.innerLimitAu = 0.0;
        ph.outerLimitAu = s.stableZoneOuterAu;
        ph.stage = s.stage;
        ph.rvDetectionLimitMs = in.rvDetectionLimitMs.value_or(0.0);
        ph.rvBaselineDays = in.rvBaselineDays.value_or(0.0);
        ph.transitRadiusLimitEarth = in.transitRadiusLimitEarth.value_or(0.0);
        ph.transitBaselineDays = in.transitBaselineDays.value_or(0.0);
        ph.seed = deriveSeed(seed, "host_" + s.key);
        h.prefix = s.name;
        h.lightStar = &s;
        h.companions = companionsOf(s.key, s.parentNode);
        hosts.push_back(h);
    }
    for (const auto& n : sd.barycenters)
    {
        if (!n.relativeOrbit || n.children.size() != 2)
            continue;
        const double outer = nodeOuterLimit(n.id, n.parentNode);
        if (n.circumbinaryInnerAu >= 0.8 * outer)
        {
            sd.stability.push_back({"host_" + n.id, true, "no stable circumbinary zone"});
            continue;
        }
        const auto members = subtreeStars(n.id);
        HostRun h;
        PlanetHost& ph = h.host;
        ph.nodeId = n.id;
        ph.bodyId = n.bodyId;
        ph.circumbinary = true;
        double l = 0.0, lz = 0.0, mi = 0.0;
        const StarDescriptor* brightest = members.empty() ? nullptr : members.front();
        for (const auto* m : members)
        {
            l += m->luminositySolar;
            lz += m->stage == StellarStage::BrownDwarf ? m->luminositySolar : zeroAgeLuminositySolar(m->initialMassSolar);
            mi += m->initialMassSolar;
            if (m->luminositySolar > brightest->luminositySolar) brightest = m;
        }
        if (!brightest) continue;
        ph.lightStarKey = brightest->key;
        ph.massSolar = n.massSolar;
        ph.initialMassSolar = mi;
        ph.zamsLuminositySolar = lz;
        ph.luminositySolar = l;
        ph.radiusSolar = brightest->radiusSolar;
        ph.metallicityFeH = brightest->metallicityFeH;
        ph.ageGyr = brightest->ageGyr;
        ph.innerLimitAu = n.circumbinaryInnerAu;
        ph.outerLimitAu = outer;
        ph.stage = brightest->stage;
        ph.rvDetectionLimitMs = in.rvDetectionLimitMs.value_or(0.0);
        ph.rvBaselineDays = in.rvBaselineDays.value_or(0.0);
        ph.transitRadiusLimitEarth = in.transitRadiusLimitEarth.value_or(0.0);
        ph.transitBaselineDays = in.transitBaselineDays.value_or(0.0);
        // Snow line from the summed pre-main-sequence light: 2.7 AU sqrt(sum M^4).
        double l4 = 0.0;
        for (const auto* m : members) l4 += std::pow(m->initialMassSolar, 4.0);
        ph.snowLineOverrideAu = 2.7 * std::sqrt(l4);
        ph.seed = deriveSeed(seed, "host_" + n.id);
        std::string letters;
        for (const auto* m : members)
        {
            const std::string& nm = m->name;
            if (nm.size() > 2 && nm[nm.size() - 2] == ' ' && std::isupper(static_cast<unsigned char>(nm.back())))
                letters += nm.back();
        }
        h.prefix = stripComponentLetter(members.front()->name) + " (" + (letters.empty() ? upper(n.id) : letters) + ")";
        h.lightStar = brightest;
        h.companions = companionsOf(n.id, n.parentNode);
        hosts.push_back(h);
    }

    // Observational anchors per host.
    for (const auto& o : in.observedBodies)
    {
        if (o.kind == "moon")
            continue;
        bool placed = false;
        for (auto& h : hosts)
            if (o.host == h.host.nodeId) { h.anchors.push_back(&o); placed = true; }
        if (!placed)
            sd.warnings.push_back("observed body " + o.designation + " has no planet host '" + o.host + "'");
    }

    // ---- formation and bodies ---------------------------------------
    std::vector<std::string> architectureModes;
    for (auto& h : hosts)
    {
        h.formation = formPlanets(h.host, sd.context, h.anchors, in.observedSystemComplete, sd.stability, sd.warnings);
        sd.disks.push_back(h.formation.disk);
        architectureModes.push_back(h.host.nodeId + ":" + h.formation.architectureMode);

        // Designations: anchors keep theirs; procedural planets take the next free letters.
        std::set<char> used;
        auto reserve = [&](const std::string& d) {
            if (d.size() >= 2 && d[d.size() - 2] == ' ' && std::islower(static_cast<unsigned char>(d.back())))
                used.insert(d.back());
        };
        for (const auto* a : h.anchors)
            reserve(a->designation);
        for (const auto& d : in.reservedDesignations)
            if (d.rfind(h.prefix + " ", 0) == 0 || h.anchors.empty() || d.rfind(h.anchors.front()->designation.substr(0, d.size() - 2), 0) == 0)
                reserve(d);
        char next = 'b';
        const double hostMassKg = h.host.massSolar * SunMassKg;
        const double snow = h.formation.disk.snowLineAu;
        std::vector<std::size_t> planetIndices;
        int ordinal = 0;
        for (const auto& p : h.formation.planets)
        {
            const ObservedBodyInput* obs = p.anchor ? h.anchors[static_cast<std::size_t>(p.anchorIndex)] : nullptr;
            BodyDescriptor b;
            if (obs)
            {
                b.designation = obs->designation;
                b.alternativeNames = obs->alternativeNames;
                b.kind = obs->kind.empty() ? "planet" : obs->kind;
                b.source = obs->source;
                b.extra = obs->extra;
                b.origin = in.observedSystemComplete ? "authored" : "observed_anchor";
                b.seed = deriveSeed(h.host.seed, "anchor_" + obs->designation);
            }
            else
            {
                while (used.count(next) && next <= 'z') ++next;
                // Past 'z' (never in practice) the letter gets an ordinal suffix.
                b.designation = next <= 'z' ? h.prefix + " " + std::string(1, next)
                                            : h.prefix + " z" + std::to_string(ordinal);
                used.insert(next);
                b.kind = "planet";
                b.origin = "generated";
                b.seed = deriveSeed(h.host.seed, "planet", static_cast<std::uint64_t>(ordinal));
            }
            ++ordinal;
            b.id = h.host.bodyId + "." + safeIdPart(b.designation);
            b.parentId = h.host.bodyId;
            b.hostStarKey = h.host.lightStarKey;
            b.formationOrigin = p.formationOrigin;
            b.provenance = p.provenance;
            KeplerOrbit& o = b.orbit;
            o.semiMajorAxisAu = p.aAu;
            o.eccentricity = p.e;
            o.inclinationDeg = p.iDeg;
            o.longitudeAscendingNodeDeg = p.nodeDeg;
            o.argumentOfPeriapsisDeg = p.periDeg;
            o.meanAnomalyAtEpochDeg = p.meanAnomalyDeg;
            o.direction = (obs && obs->retrograde) || p.iDeg > 90.0 ? -1 : 1;
            o.periodDays = periodDays(p.aAu, hostMassKg + p.massEarth * EarthMassKg);

            BodySeedProperties sp;
            sp.massEarth = p.massEarth;
            sp.coreMassEarth = p.coreMassEarth;
            sp.envelopeFraction = p.envelopeFraction;
            sp.waterFraction = p.waterFraction;
            sp.formationAu = p.formationAu;
            if (obs && !p.giant && p.envelopeFraction == 0.0 && !obs->envelopeMassFraction &&
                !obs->radiusEarth && p.massEarth > 2.0 && p.massEarth < 20.0)
            {
                // Observed small planet of unknown radius: formation-consistent guess.
                RandomStream es(b.seed, "anchor_envelope");
                sp.envelopeFraction = p.aAu > 0.15 ? es.uniform(0.0, 0.03) : 0.0;
                sp.waterFraction = p.aAu > snow ? 0.3 : es.uniform(0.0, 0.01);
            }
            if (obs && obs->envelopeMassFraction) sp.envelopeFraction = *obs->envelopeMassFraction;
            if (obs && p.giant && sp.envelopeFraction == 0.0)
                sp.envelopeFraction = 1.0 - std::min(0.9, 57.9 * std::pow(p.massEarth / 318.0, 0.61) / p.massEarth);
            if (obs && obs->waterMassFraction) sp.waterFraction = *obs->waterMassFraction;

            BodyEnvironment env;
            env.ageGyr = h.host.ageGyr;
            env.metallicityFeH = h.host.metallicityFeH;
            env.gravitationalHostMassKg = hostMassKg;
            env.orbitAu = p.aAu;
            env.eccentricity = p.e;
            double flux = h.host.luminositySolar / (p.aAu * p.aAu);
            for (const auto& [lc, dc] : h.companions)
                flux += lc / std::max(dc * dc, 1e-6);
            env.lightLuminositySolar = flux * p.aAu * p.aAu;
            env.lightDistanceAu = p.aAu;
            env.lightTeffK = h.lightStar->teffK;
            env.lightMassSolar = h.lightStar->massSolar;
            env.snowLineAu = snow;
            deriveBodyPhysics(b, sp, env, obs, b.seed);
            if (obs && obs->extra.contains("environment_preset_id"))
                b.environmentPresetId = obs->extra["environment_preset_id"].get<std::string>();

            sd.bodies.push_back(b);
            const std::size_t planetIndex = sd.bodies.size() - 1;
            planetIndices.push_back(planetIndex);

            // Moons: observed ones for this planet, then generated ones.
            std::vector<MoonCandidate> moons;
            std::vector<const ObservedBodyInput*> moonObs;
            for (const auto& mo : in.observedBodies)
            {
                if (mo.kind != "moon" || mo.host != b.designation)
                    continue;
                MoonCandidate mc;
                mc.massEarth = mo.massEarth.value_or(1e-6);
                mc.aKm = mo.semiMajorAxisAu.value_or(0.0) * AuMeters / 1000.0;
                mc.e = mo.eccentricity.value_or(0.0);
                mc.iDeg = mo.inclinationDeg.value_or(0.0);
                mc.retrograde = mo.retrograde;
                mc.waterFraction = mo.waterMassFraction.value_or(0.0);
                mc.origin = "observed";
                moons.push_back(mc);
                moonObs.push_back(&mo);
            }
            if (!in.observedSystemComplete)
                for (const auto& mc : generateMoons(sd.bodies[planetIndex], hostMassKg, snow, h.host.ageGyr,
                                                    sd.bodies[planetIndex].seed, sd.stability))
                {
                    moons.push_back(mc);
                    moonObs.push_back(nullptr);
                }

            std::vector<std::size_t> order(moons.size());
            for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
            std::sort(order.begin(), order.end(), [&](auto x, auto y) { return moons[x].aKm < moons[y].aKm; });
            int moonNumber = 0;
            std::vector<const BodyDescriptor*> moonPtrs;
            std::vector<BodyDescriptor> moonBodies;
            for (const std::size_t k : order)
            {
                const auto& mc = moons[k];
                const ObservedBodyInput* mobs = moonObs[k];
                const BodyDescriptor& planet = sd.bodies[planetIndex];
                BodyDescriptor m;
                if (mobs)
                {
                    m.designation = mobs->designation;
                    m.alternativeNames = mobs->alternativeNames;
                    m.source = mobs->source;
                    m.extra = mobs->extra;
                    m.origin = in.observedSystemComplete ? "authored" : "observed_anchor";
                    m.seed = deriveSeed(planet.seed, "moon_" + mobs->designation);
                }
                else
                {
                    m.designation = planet.designation + " " + romanNumeral(++moonNumber);
                    m.origin = "generated";
                    m.seed = deriveSeed(planet.seed, "moon", static_cast<std::uint64_t>(k));
                }
                m.kind = "moon";
                m.id = planet.id + "." + safeIdPart(m.designation);
                m.parentId = planet.id;
                m.hostStarKey = planet.hostStarKey;
                m.formationOrigin = mc.origin;
                KeplerOrbit& mo = m.orbit;
                mo.semiMajorAxisAu = mc.aKm * 1000.0 / AuMeters;
                mo.eccentricity = mc.e;
                mo.inclinationDeg = mc.iDeg;
                RandomStream os(m.seed, "orbit");
                mo.longitudeAscendingNodeDeg = mobs && mobs->longitudeAscendingNodeDeg ? *mobs->longitudeAscendingNodeDeg : os.uniform(0.0, 360.0);
                mo.argumentOfPeriapsisDeg = mobs && mobs->argumentOfPeriapsisDeg ? *mobs->argumentOfPeriapsisDeg : os.uniform(0.0, 360.0);
                mo.meanAnomalyAtEpochDeg = mobs && mobs->meanAnomalyAtEpochDeg ? *mobs->meanAnomalyAtEpochDeg : os.uniform(0.0, 360.0);
                mo.direction = mc.retrograde || mc.iDeg > 90.0 ? -1 : 1;
                mo.periodDays = periodDays(mo.semiMajorAxisAu, planet.massKg + mc.massEarth * EarthMassKg);
                m.provenance["orbit"] = mobs ? (in.observedSystemComplete ? Provenance::Authored : Provenance::Observed)
                                             : Provenance::Generated;

                BodySeedProperties msp;
                msp.massEarth = mc.massEarth;
                msp.coreMassEarth = mc.massEarth;
                msp.waterFraction = mc.waterFraction;
                msp.formationAu = planet.formationDistanceAu;
                if (mc.origin == "giant_impact") msp.ironFraction = 0.08;
                BodyEnvironment menv;
                menv.ageGyr = h.host.ageGyr;
                menv.metallicityFeH = h.host.metallicityFeH;
                menv.gravitationalHostMassKg = planet.massKg;
                menv.orbitAu = mo.semiMajorAxisAu;
                menv.eccentricity = mc.e;
                menv.lightLuminositySolar = planet.instellationEarth * planet.orbit.semiMajorAxisAu *
                    planet.orbit.semiMajorAxisAu * std::sqrt(1.0 - planet.orbit.eccentricity * planet.orbit.eccentricity);
                menv.lightDistanceAu = planet.orbit.semiMajorAxisAu;
                menv.lightTeffK = h.lightStar->teffK;
                menv.lightMassSolar = h.lightStar->massSolar;
                menv.snowLineAu = snow;
                menv.isMoon = true;
                menv.parentPlanetMassKg = planet.massKg;
                menv.parentPlanetRadiusKm = planet.radiusKm;
                deriveBodyPhysics(m, msp, menv, mobs, m.seed);
                if (mobs && mobs->extra.contains("environment_preset_id"))
                    m.environmentPresetId = mobs->extra["environment_preset_id"].get<std::string>();
                moonBodies.push_back(m);
            }
            for (const auto& mb : moonBodies) moonPtrs.push_back(&mb);

            // Rings: observed or generated.
            BodyDescriptor& pl = sd.bodies[planetIndex];
            if (obs && !obs->rings.empty())
            {
                for (const auto& ri : obs->rings)
                {
                    RingDescriptor r;
                    r.id = fileSlug(ri.name, {});
                    r.name = ri.name;
                    r.innerRadiusKm = ri.innerRadiusKm;
                    r.outerRadiusKm = ri.outerRadiusKm;
                    r.opticalDepth = ri.opticalDepth;
                    r.iceFraction = ri.iceFraction;
                    r.rockFraction = 1.0 - ri.iceFraction;
                    r.origin = "observed";
                    r.visibilityClass = ri.opticalDepth > 0.3 ? "main" : ri.opticalDepth > 0.01 ? "secondary" : "faint";
                    const double area = Pi * (r.outerRadiusKm * r.outerRadiusKm - r.innerRadiusKm * r.innerRadiusKm) * 1.0e6;
                    r.massKg = 500.0 * r.opticalDepth * area;
                    pl.rings.push_back(r);
                }
            }
            else if (!in.observedSystemComplete)
            {
                pl.rings = generateRings(pl, moonPtrs, snow, pl.seed);
            }
            for (auto& mb : moonBodies)
                sd.bodies.push_back(std::move(mb));
        }

        // Small-body reservoirs and their largest members.
        ReservoirHostInput rh;
        rh.hostBodyId = h.host.bodyId;
        rh.hostName = h.prefix;
        rh.hostMassSolar = h.host.massSolar;
        rh.luminositySolar = h.host.luminositySolar;
        rh.ageGyr = h.host.ageGyr;
        rh.snowLineAu = snow;
        rh.outerLimitAu = h.host.outerLimitAu;
        rh.outerPlanetesimalEdgeAu = std::max(h.formation.outerPlanetesimalEdgeAu,
                                              h.formation.disk.characteristicRadiusAu * 2.0);
        rh.leftoverInnerSolidsEarth = h.formation.leftoverInnerSolidsEarth;
        rh.leftoverOuterSolidsEarth = h.formation.leftoverOuterSolidsEarth;
        rh.seed = deriveSeed(h.host.seed, "reservoirs");
        std::vector<const BodyDescriptor*> planetPtrs;
        for (const auto idx : planetIndices) planetPtrs.push_back(&sd.bodies[idx]);
        std::vector<ReservoirDescriptor> reservoirs;
        std::vector<DwarfPlanetCandidate> dwarfs;
        generateReservoirs(rh, planetPtrs, reservoirs, dwarfs);
        // Authored reservoirs of an observed system override the population model.
        if (in.passthrough.contains("observed_reservoirs"))
        {
            for (auto& r : reservoirs)
            {
                const auto& obsRes = in.passthrough["observed_reservoirs"];
                if (!obsRes.contains(r.kind))
                    continue;
                const auto& o = obsRes[r.kind];
                r.innerAu = o.value("inner_au", r.innerAu);
                r.outerAu = o.value("outer_au", r.outerAu);
                r.peakAu = 0.5 * (r.innerAu + r.outerAu);
                r.totalMassEarth = o.value("total_mass_earth", r.totalMassEarth);
                r.provenance["population"] = Provenance::Authored;
            }
        }
        for (auto& r : reservoirs)
        {
            const std::string localId = r.id;
            r.id = (r.kind == "trojans" ? r.parentId : h.host.bodyId) + "." +
                (r.kind == "trojans" ? std::string("trojans") : localId);
            sd.reservoirs.push_back(r);
        }
        if (!in.observedSystemComplete)
        {
            int dn = 0;
            for (const auto& d : dwarfs)
            {
                BodyDescriptor b;
                const bool asteroid = d.reservoirId == "asteroid_belt";
                b.designation = h.prefix + " " + (asteroid ? "A" : "K") + std::to_string(++dn);
                b.kind = "dwarf_planet";
                b.origin = "generated";
                b.formationOrigin = "reservoir_largest_member";
                b.seed = deriveSeed(h.host.seed, "dwarf", static_cast<std::uint64_t>(dn));
                b.id = h.host.bodyId + "." + safeIdPart(b.designation);
                b.parentId = h.host.bodyId;
                b.hostStarKey = h.host.lightStarKey;
                RandomStream os(b.seed, "orbit");
                b.orbit.semiMajorAxisAu = d.aAu;
                b.orbit.eccentricity = std::min(d.e, 0.5);
                b.orbit.inclinationDeg = d.iDeg;
                b.orbit.longitudeAscendingNodeDeg = os.uniform(0.0, 360.0);
                b.orbit.argumentOfPeriapsisDeg = os.uniform(0.0, 360.0);
                b.orbit.meanAnomalyAtEpochDeg = os.uniform(0.0, 360.0);
                b.orbit.periodDays = periodDays(d.aAu, hostMassKg);
                b.provenance["orbit"] = Provenance::Generated;
                BodySeedProperties sp;
                sp.massEarth = d.massEarth;
                sp.coreMassEarth = d.massEarth;
                sp.waterFraction = d.waterFraction;
                sp.formationAu = d.aAu;
                BodyEnvironment env;
                env.ageGyr = h.host.ageGyr;
                env.gravitationalHostMassKg = hostMassKg;
                env.orbitAu = d.aAu;
                env.eccentricity = b.orbit.eccentricity;
                double flux = h.host.luminositySolar / (d.aAu * d.aAu);
                for (const auto& [lc, dc] : h.companions) flux += lc / std::max(dc * dc, 1e-6);
                env.lightLuminositySolar = flux * d.aAu * d.aAu;
                env.lightDistanceAu = d.aAu;
                env.lightTeffK = h.lightStar->teffK;
                env.lightMassSolar = h.lightStar->massSolar;
                env.snowLineAu = snow;
                deriveBodyPhysics(b, sp, env, nullptr, b.seed);
                sd.bodies.push_back(b);
            }
        }
        for (const auto& e : h.formation.ejected)
        {
            std::ostringstream w;
            w << "ejected planet from " << h.host.nodeId << ": " << e.massEarth << " M_earth (rogue population)";
            sd.stability.push_back({"ejection_" + h.host.nodeId, true, w.str()});
        }
    }

    std::ostringstream modes;
    for (std::size_t k = 0; k < architectureModes.size(); ++k)
        modes << (k ? "; " : "") << architectureModes[k];
    sd.architectureMode = modes.str();
    validateSystem(sd);
    return sd;
}

} // namespace world::generation
