// DEV authoring tool for the physical universe catalog. Runs the production
// generator (src/world/generation) offline:
//
//   EliteUniverseGenerator materialize <galaxy_details_root>
//       regenerate <root>/{systems,distant_systems}/*/generated from the inputs
//   EliteUniverseGenerator dry-run <galaxy_details_root> <out.tsv>
//       generate in memory, one summary row per system (regeneration report)
//   EliteUniverseGenerator bulk <count> <out_prefix> [universe_seed_hex]
//       synthetic systems for population statistics (<prefix>_systems.tsv,
//       <prefix>_planets.tsv); not catalog data, never committed
//   EliteUniverseGenerator determinism <galaxy_details_root>
//       generate every system twice, in forward and reverse order, compare bytes

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/DescriptorJson.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/StellarSystemGenerator.h"

namespace fs = std::filesystem;
using namespace world::generation;

namespace
{

std::vector<fs::path> systemDirectories(const fs::path& root)
{
    std::vector<fs::path> out;
    for (const char* sub : {"systems", "distant_systems"})
    {
        std::error_code ec;
        if (!fs::is_directory(root / sub, ec))
            continue;
        for (const auto& e : fs::directory_iterator(root / sub))
            if (e.is_directory() && fs::exists(e.path() / "system.json"))
                out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string fullDump(const SystemDescriptor& sd)
{
    std::string s = dumpJson(generatedSystemJson(sd));
    for (const auto& b : sd.bodies) s += dumpJson(bodyJson(b, sd));
    for (const auto& r : sd.reservoirs) s += dumpJson(reservoirJson(r, sd));
    return s;
}

int materialize(const fs::path& root)
{
    int failures = 0;
    for (const auto& dir : systemDirectories(root))
    {
        SystemInput in;
        std::string error;
        if (!readSystemInput(dir, in, error))
        {
            std::cerr << "[universe] input error: " << error << "\n";
            ++failures;
            continue;
        }
        const SystemDescriptor sd = generateStellarSystem(in, DefaultUniverseSeed);
        const auto files = writeGeneratedSystem(sd, dir);
        bool passed = true;
        for (const auto& c : sd.stability) passed = passed && c.passed;
        std::cout << "[universe] " << in.id << " " << in.name << ": " << sd.bodies.size() << " bodies, "
                  << sd.reservoirs.size() << " reservoirs, " << files.size() << " files, stability "
                  << (passed ? "PASS" : "WARN") << "\n";
    }
    return failures == 0 ? 0 : 1;
}

int dryRun(const fs::path& root, const fs::path& out)
{
    std::ofstream f(out, std::ios::binary);
    f << "id\tname\tposition_x\tposition_y\tposition_z\tstars\tstellar_parameters\tplanets\tanchors\tmoons"
         "\tringed_planets\treservoirs\tdwarf_planets\tgiants\tarchitecture\tstability\tfailed_checks"
         "\tsystem_seed\tversions\twarnings\tplanet_list\n";
    for (const auto& dir : systemDirectories(root))
    {
        SystemInput in;
        std::string error;
        if (!readSystemInput(dir, in, error))
        {
            std::cerr << "[universe] input error: " << error << "\n";
            return 1;
        }
        const SystemDescriptor sd = generateStellarSystem(in, DefaultUniverseSeed);
        int planets = 0, anchors = 0, moons = 0, ringed = 0, dwarfs = 0, giants = 0;
        std::string list;
        for (const auto& b : sd.bodies)
        {
            if (b.kind == "planet")
            {
                ++planets;
                anchors += b.origin != "generated";
                ringed += !b.rings.empty() && b.rings.front().visibilityClass != "faint";
                giants += b.massKg / astro::EarthMassKg > 50.0;
                char buf[256];
                std::snprintf(buf, sizeof buf, "%s%s[%s a=%.3g M=%.3g %s]", list.empty() ? "" : " ",
                              b.designation.c_str(), b.bodyClass.c_str(), b.orbit.semiMajorAxisAu,
                              b.massKg / astro::EarthMassKg, b.origin == "generated" ? "gen" : "obs");
                list += buf;
            }
            moons += b.kind == "moon";
            dwarfs += b.kind == "dwarf_planet";
        }
        std::string stars, failed;
        bool passed = true;
        for (const auto& s : sd.stars)
        {
            char buf[256];
            std::snprintf(buf, sizeof buf, "%s%s %s M=%.3g L=%.3g T=%.0f age=%.2g FeH=%+.2f", stars.empty() ? "" : "; ",
                          s.name.c_str(), s.spectralType.c_str(), s.massSolar, s.luminositySolar, s.teffK, s.ageGyr,
                          s.metallicityFeH);
            stars += buf;
        }
        for (const auto& c : sd.stability)
            if (!c.passed)
            {
                passed = false;
                failed += (failed.empty() ? "" : "; ") + c.name + ": " + c.detail;
            }
        std::string warnings;
        for (const auto& w : sd.warnings) warnings += (warnings.empty() ? "" : " | ") + w;
        f << in.id << '\t' << in.name << '\t' << in.positionLy.x << '\t' << in.positionLy.y << '\t' << in.positionLy.z
          << '\t' << sd.stars.size() << '\t' << stars << '\t' << planets << '\t' << anchors << '\t' << moons << '\t'
          << ringed << '\t' << sd.reservoirs.size() << '\t' << dwarfs << '\t' << giants << '\t' << sd.architectureMode
          << '\t' << (passed ? "PASS" : "FAIL") << '\t' << failed << '\t' << hexSeed(sd.systemSeed) << '\t'
          << sd.versions.stellarSystem << "." << sd.versions.celestialBody << "." << sd.versions.satellites << '\t'
          << warnings << '\t' << list << '\n';
    }
    return 0;
}

// Synthetic solar-neighbourhood systems: Kroupa primaries, mass-dependent
// multiplicity (Duchene & Kraus 2013), no observations.
SystemInput syntheticSystem(std::uint64_t universeSeed, int index)
{
    RandomStream rs(universeSeed, "bulk_system", static_cast<std::uint64_t>(index));
    SystemInput in;
    in.id = 200000 + index;
    in.name = "Synthetic " + std::to_string(index);
    const double r = 200.0 * std::cbrt(rs.uniform());
    const double th = std::acos(rs.uniform(-1.0, 1.0));
    const double ph = rs.angleRad();
    in.positionLy = {r * std::sin(th) * std::cos(ph), r * std::sin(th) * std::sin(ph), r * std::cos(th)};
    auto mass = [&]() {
        const double u = rs.uniform();
        return u < 0.75 ? rs.powerLaw(-1.3, 0.08, 0.5) : rs.powerLaw(-2.3, 0.5, 3.0);
    };
    StarInput a;
    a.key = "a";
    a.name = in.name + " A";
    a.massSolar = mass();
    in.stars.push_back(a);
    const double m = *a.massSolar;
    const double fMult = m < 0.1 ? 0.22 : m < 0.5 ? 0.26 : m < 1.3 ? 0.44 : 0.6;
    if (rs.chance(fMult))
    {
        StarInput b;
        b.key = "b";
        b.name = in.name + " B";
        b.massSolar = std::max(0.08, m * rs.uniform(0.1, 1.0));
        in.stars.push_back(b);
        in.hierarchy.id = "system_barycenter";
        HierarchyNodeInput na, nb;
        na.starKey = na.id = "a";
        nb.starKey = nb.id = "b";
        in.hierarchy.children = {na, nb};
    }
    else
    {
        in.hierarchy.starKey = in.hierarchy.id = "a";
    }
    return in;
}

int bulk(int count, const std::string& prefix, std::uint64_t universeSeed)
{
    std::ofstream fs_(prefix + "_systems.tsv", std::ios::binary);
    std::ofstream fp(prefix + "_planets.tsv", std::ios::binary);
    fs_ << "index\tstars\tprimary_mass\tfeh\tage\tpopulation\tplanets\tgiants\tmoons\tringed\treservoirs\tasteroid_belt_mass"
           "\touter_belt_mass\tcomet_cloud\tarchitecture\tstability\tfailed\n";
    fp << "index\thost_mass\tfeh\ta_au\tperiod_days\tmass_earth\tradius_earth\te\ti_deg\tclass\tmoons\tring\tteq_k"
          "\ttsurf_k\tpressure_bar\thydrosphere\tlocked\tformation\n";
    const auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < count; ++k)
    {
        const SystemInput in = syntheticSystem(universeSeed, k);
        const SystemDescriptor sd = generateStellarSystem(in, universeSeed);
        std::map<std::string, int> moonsOf;
        for (const auto& b : sd.bodies)
            if (b.kind == "moon") ++moonsOf[b.parentId];
        int planets = 0, giants = 0, moons = 0, ringed = 0;
        for (const auto& b : sd.bodies)
        {
            moons += b.kind == "moon";
            if (b.kind != "planet") continue;
            ++planets;
            const double me = b.massKg / astro::EarthMassKg;
            giants += me > 50.0;
            const bool ring = !b.rings.empty() && b.rings.front().visibilityClass != "faint";
            ringed += ring;
            fp << k << '\t' << sd.stars.front().massSolar << '\t' << sd.stars.front().metallicityFeH << '\t'
               << b.orbit.semiMajorAxisAu << '\t' << b.orbit.periodDays << '\t' << me << '\t'
               << b.radiusKm * 1000.0 / astro::EarthRadiusM << '\t' << b.orbit.eccentricity << '\t'
               << b.orbit.inclinationDeg << '\t' << b.bodyClass << '\t' << moonsOf[b.id] << '\t' << ring << '\t'
               << b.equilibriumTemperatureK << '\t' << b.meanSurfaceTemperatureK << '\t'
               << b.atmosphere.surfacePressurePa / 1e5 << '\t' << b.hydrosphere << '\t' << b.tidalLock << '\t'
               << b.formationOrigin << '\n';
        }
        double ab = 0.0, ob = 0.0;
        bool comet = false;
        for (const auto& r : sd.reservoirs)
        {
            if (r.kind == "asteroid_belt") ab += r.totalMassEarth;
            if (r.kind == "kuiper_belt" || r.kind == "debris_disk") ob += r.totalMassEarth;
            comet = comet || r.kind == "comet_cloud";
        }
        bool passed = true;
        std::string failed;
        for (const auto& c : sd.stability)
            if (!c.passed) { passed = false; failed += c.name + ":" + c.detail + ";"; }
        fs_ << k << '\t' << sd.stars.size() << '\t' << sd.stars.front().massSolar << '\t'
            << sd.stars.front().metallicityFeH << '\t' << sd.stars.front().ageGyr << '\t'
            << toString(sd.context.population) << '\t' << planets << '\t' << giants << '\t' << moons << '\t'
            << ringed << '\t' << sd.reservoirs.size() << '\t' << ab << '\t' << ob << '\t' << comet << '\t'
            << sd.architectureMode << '\t' << (passed ? "PASS" : "FAIL") << '\t' << failed << '\n';
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "[universe] bulk " << count << " systems in " << s << " s (" << count / std::max(s, 1e-9)
              << " systems/s)\n";
    return 0;
}

int determinism(const fs::path& root)
{
    const auto dirs = systemDirectories(root);
    std::vector<SystemInput> inputs;
    for (const auto& d : dirs)
    {
        SystemInput in;
        std::string error;
        if (!readSystemInput(d, in, error)) { std::cerr << error << "\n"; return 1; }
        inputs.push_back(in);
    }
    std::vector<std::string> forward, reverse(inputs.size());
    for (const auto& in : inputs) forward.push_back(fullDump(generateStellarSystem(in, DefaultUniverseSeed)));
    for (std::size_t k = inputs.size(); k-- > 0;)
        reverse[k] = fullDump(generateStellarSystem(inputs[k], DefaultUniverseSeed));
    int diff = 0;
    for (std::size_t k = 0; k < inputs.size(); ++k)
        if (forward[k] != reverse[k])
        {
            std::cerr << "[universe] NON-DETERMINISTIC: " << inputs[k].id << " " << inputs[k].name << "\n";
            ++diff;
        }
    // Materialised files must equal a fresh generation.
    int stale = 0;
    for (std::size_t k = 0; k < inputs.size(); ++k)
    {
        std::ifstream f(dirs[k] / "generated" / "system.json", std::ios::binary);
        if (!f) continue;
        const std::string onDisk((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (onDisk != dumpJson(generatedSystemJson(generateStellarSystem(inputs[k], DefaultUniverseSeed))))
        {
            std::cerr << "[universe] STALE materialisation: " << dirs[k].string() << "\n";
            ++stale;
        }
    }
    std::cout << "[universe] determinism: " << inputs.size() << " systems, " << diff << " differ, " << stale
              << " stale\n";
    return diff == 0 && stale == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: EliteUniverseGenerator materialize|dry-run|bulk|determinism ...\n";
        return 2;
    }
    const std::string cmd = argv[1];
    if (cmd == "materialize") return materialize(argv[2]);
    if (cmd == "dry-run" && argc >= 4) return dryRun(argv[2], argv[3]);
    if (cmd == "determinism") return determinism(argv[2]);
    if (cmd == "bulk" && argc >= 4)
    {
        const std::uint64_t seed = argc >= 5 ? std::stoull(argv[4], nullptr, 16) : DefaultUniverseSeed;
        return bulk(std::stoi(argv[2]), argv[3], seed);
    }
    std::cerr << "unknown command\n";
    return 2;
}
