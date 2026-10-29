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
//       generate every system twice, in forward and reverse order, compare bytes;
//       the interstellar files on disk must equal a fresh generation
//   EliteUniverseGenerator interstellar <galaxy_details_root>
//       replace objects_details/interstellar_cells/ (one file per 25-ly
//       generation cell covering the navigation domain: background medium
//       lattice + rogue bodies, remnants, comets, debris groups) and
//       objects_details/interstellar_medium/ (density-lattice structures)
//   EliteUniverseGenerator domain <galaxy_details_root>
//       report the navigation domain of navigation_grid.json

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "src/world/coordinates/GalacticGrids.h"
#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/DescriptorJson.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/InterstellarGenerator.h"
#include "src/world/interstellar/InterstellarNavigationEnvironment.h"
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

std::vector<glm::dvec3> catalogPositions(const fs::path& root)
{
    std::vector<glm::dvec3> positions;
    for (const auto& dir : systemDirectories(root))
    {
        SystemInput in;
        std::string error;
        if (readSystemInput(dir, in, error))
            positions.push_back(in.positionLy);
    }
    return positions;
}

// The navigation domain of navigation_grid.json (the same policy as the
// runtime GalaxyNavigationGrid) extended to every catalog system.
world::coordinates::GalacticNavigationDomain navigationDomain(const fs::path& root)
{
    std::ifstream f(root.parent_path() / "navigation" / "navigation_grid.json");
    const auto cfg = nlohmann::json::parse(f);
    double edgeKm = cfg.at("terminalCellEdgeKm").get<double>();
    for (int i = 0; i < cfg.at("maximumDepth").get<int>(); ++i)
        edgeKm *= cfg.at("subdivisionPerAxis").get<double>();
    const auto& rn = cfg.at("rootNavigation");
    world::coordinates::GalacticNavigationDomain domain(
        {edgeKm / 9.4607304725808e12},
        {rn.at("localEnvelopeRadiusLy").get<double>(), rn.at("remoteEnvelopeRadiusLy").get<double>()});
    for (const auto& p : catalogPositions(root))
        domain.extendTo(p);
    return domain;
}

int domainReport(const fs::path& root)
{
    using namespace world::coordinates;
    const auto domain = navigationDomain(root);
    const auto cells = domain.cells();
    const auto comps = faceConnectedComponents(cells);
    const auto gen = generationCellsCovering(domain, interstellarGenerationGrid());
    std::map<GridCell3, int> systems;
    for (const auto& p : catalogPositions(root))
        ++systems[domain.grid().cellOf(p)];
    int outsideBall = 0;
    for (const auto& c : cells)
        outsideBall += glm::length(domain.grid().centre(c)) > domain.settings().localEnvelopeRadiusLy + domain.grid().edgeLy;
    std::cout << "navigation domain (connected envelope policy)\n"
              << "  root edge ly: " << domain.grid().edgeLy << "\n"
              << "  local envelope radius ly: " << domain.settings().localEnvelopeRadiusLy
              << ", remote envelope radius ly: " << domain.settings().remoteEnvelopeRadiusLy << "\n"
              << "  root cells: " << cells.size() << " (remote extension/corridor cells: " << outsideBall << ")\n"
              << "  root cells holding catalog systems: " << systems.size() << "\n"
              << "  face-connected components: " << comps.size() << "\n"
              << "  interstellar generation cells (25 ly) covering the domain: " << gen.size() << "\n";
    for (const auto& [c, n] : systems)
        if (!domain.contains(c))
            std::cout << "  ERROR: system root cell outside domain\n";
    return comps.size() == 1 ? 0 : 1;
}

nlohmann::ordered_json objectJson(const InterstellarObject& o)
{
    nlohmann::ordered_json j;
    j["schema_version"] = 1;
    j["kind"] = "galaxy_object";
    j["id"] = o.id;
    j["name"] = o.name;
    j["object_type"] = o.objectType;
    j["position_ly"] = {{"x", o.positionLy.x}, {"y", o.positionLy.y}, {"z", o.positionLy.z}};
    j["description"] = o.description;
    j["tags"] = o.tags;
    j["properties"] = o.properties;
    return j;
}

// Relative path -> file text of every materialised interstellar file.
std::map<std::string, std::string> interstellarFiles(const fs::path& root)
{
    const auto domain = navigationDomain(root);
    const auto cells = world::coordinates::generationCellsCovering(
        domain, world::coordinates::interstellarGenerationGrid());
    const auto systems = catalogPositions(root);
    std::map<std::string, std::string> files;
    // Cells are stored in blocks of 4 x 4 x 4 cells (100 ly) so the runtime
    // opens ~100 files instead of thousands. The content of a cell does not
    // depend on its block or on its neighbours.
    constexpr long long BlockCells = 4;
    const auto floorDiv = [](long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    std::map<world::coordinates::GridCell3, nlohmann::ordered_json> blocks;
    for (const auto& cell : cells)
    {
        const InterstellarCell c = generateInterstellarCell(DefaultUniverseSeed, cell, systems);
        const world::coordinates::GridCell3 block {
            floorDiv(cell[0], BlockCells), floorDiv(cell[1], BlockCells), floorDiv(cell[2], BlockCells)};
        auto& b = blocks[block];
        if (b.is_null())
        {
            b["schema"] = "elite.interstellar_cell_block.v1";
            b["kind"] = "interstellar_cell_block";
            b["block"] = {block[0], block[1], block[2]};
            b["block_size_cells"] = BlockCells;
            b["cell_size_ly"] = world::coordinates::InterstellarGenerationCellLy;
            b["interstellar_version"] = CurrentGenerationVersions.interstellar;
            b["cells"] = nlohmann::ordered_json::array();
        }
        nlohmann::ordered_json j;
        j["cell"] = {cell[0], cell[1], cell[2]};
        j["seed"] = hexSeed(c.seed);
        j["objects"] = nlohmann::ordered_json::array();
        for (const auto& o : c.objects)
            j["objects"].push_back(objectJson(o));
        b["cells"].push_back(std::move(j));
    }
    for (const auto& [block, j] : blocks)
        files["interstellar_cells/block_" + interstellarCellTag(block) + ".json"] = dumpJson(j);
    // Distinct structures: blocks of the same layout (by the generation cell
    // holding the structure centre); cavities: one document each.
    std::map<world::coordinates::GridCell3, std::map<world::coordinates::GridCell3, nlohmann::ordered_json>> mediumBlocks;
    for (const auto& o : generateInterstellarMedium(DefaultUniverseSeed, mediumStructureCells(cells)))
    {
        if (o.objectType != "interstellar_medium_region")
        {
            files["interstellar_medium/" + o.id + ".json"] = dumpJson(objectJson(o));
            continue;
        }
        const double edge = world::coordinates::InterstellarGenerationCellLy;
        const world::coordinates::GridCell3 cell {
            static_cast<long long>(std::floor(o.positionLy.x / edge)),
            static_cast<long long>(std::floor(o.positionLy.y / edge)),
            static_cast<long long>(std::floor(o.positionLy.z / edge))};
        const world::coordinates::GridCell3 block {
            floorDiv(cell[0], BlockCells), floorDiv(cell[1], BlockCells), floorDiv(cell[2], BlockCells)};
        auto& c = mediumBlocks[block][cell];
        if (c.is_null())
        {
            c["cell"] = {cell[0], cell[1], cell[2]};
            c["objects"] = nlohmann::ordered_json::array();
        }
        c["objects"].push_back(objectJson(o));
    }
    for (const auto& [block, blockCells] : mediumBlocks)
    {
        nlohmann::ordered_json b;
        b["schema"] = "elite.interstellar_cell_block.v1";
        b["kind"] = "interstellar_cell_block";
        b["content"] = "medium_structures";
        b["block"] = {block[0], block[1], block[2]};
        b["block_size_cells"] = BlockCells;
        b["cell_size_ly"] = world::coordinates::InterstellarGenerationCellLy;
        b["interstellar_version"] = CurrentGenerationVersions.interstellar;
        b["cells"] = nlohmann::ordered_json::array();
        for (const auto& [cell, j] : blockCells)
            b["cells"].push_back(j);
        files["interstellar_medium/block_" + interstellarCellTag(block) + ".json"] = dumpJson(b);
    }
    return files;
}

int interstellar(const fs::path& root)
{
    const fs::path dir = root / "objects_details";
    for (const char* sub : {"interstellar_cells", "interstellar_medium"})
    {
        fs::remove_all(dir / sub);
        fs::create_directories(dir / sub);
    }
    for (const auto& e : fs::directory_iterator(dir))
        if (e.path().filename().string().rfind("gen_", 0) == 0)
            fs::remove(e.path()); // the pre-domain flat layout
    const auto files = interstellarFiles(root);
    std::map<std::string, int> counts;
    for (const auto& [relative, text] : files)
    {
        std::ofstream f(dir / relative, std::ios::binary);
        f << text;
        const auto j = nlohmann::json::parse(text);
        if (j.value("kind", std::string()) == "interstellar_cell_block")
            for (const auto& c : j["cells"])
                for (const auto& o : c["objects"])
                    ++counts[o["object_type"].get<std::string>()];
        else
            ++counts[j["object_type"].get<std::string>()];
    }
    std::cout << "[universe] interstellar files: " << files.size() << ";";
    for (const auto& [type, n] : counts) std::cout << " " << type << "=" << n;
    std::cout << "\n";
    return 0;
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
    const auto files = interstellarFiles(root);
    for (const auto& [relative, text] : files)
    {
        std::ifstream f(root / "objects_details" / relative, std::ios::binary);
        const std::string onDisk((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (onDisk != text)
        {
            std::cerr << "[universe] STALE interstellar file: " << relative << "\n";
            ++stale;
        }
    }
    for (const char* sub : {"interstellar_cells", "interstellar_medium"})
    {
        std::error_code ec;
        if (fs::is_directory(root / "objects_details" / sub, ec))
            for (const auto& e : fs::directory_iterator(root / "objects_details" / sub))
                if (!files.count(std::string(sub) + "/" + e.path().filename().string()))
                {
                    std::cerr << "[universe] STALE extra interstellar file: " << e.path().string() << "\n";
                    ++stale;
                }
    }
    // A cell is independent of the others: generating it alone gives the same bytes.
    {
        const auto domain = navigationDomain(root);
        const auto cells = world::coordinates::generationCellsCovering(
            domain, world::coordinates::interstellarGenerationGrid());
        const auto systems = catalogPositions(root);
        const auto& probe = cells[cells.size() / 2];
        const auto a = generateInterstellarCell(DefaultUniverseSeed, probe, systems);
        const auto b = generateInterstellarCell(DefaultUniverseSeed, probe, systems);
        if (a.objects.size() != b.objects.size() ||
            dumpJson(objectJson(a.objects.front())) != dumpJson(objectJson(b.objects.front())))
        {
            std::cerr << "[universe] interstellar cell not reproducible\n";
            ++diff;
        }
    }
    std::cout << "[universe] determinism: " << inputs.size() << " systems, " << diff << " differ, " << stale
              << " stale\n";
    return diff == 0 && stale == 0 ? 0 : 1;
}

} // namespace

/*
    ism-check: the interstellar medium model against its own claims.
    1. Calibration on the observed sample: expected warm cloudlets within the
       15-pc census sphere (observed ~15, Redfield & Linsky 2008).
    2. No observational privilege: expected structure density per environment
       (immersed wall / compressed wall / wall intersection / cavity interior /
       warm neutral), split inside and outside the census sphere; the Sun's
       surroundings must look like any other place of the same environment.
    3. Materialised population: counts per kind x provenance x environment,
       icy debris fields per origin, cavities.
*/
int ismCheck(const fs::path& root)
{
    using namespace world::generation;
    const auto domain = navigationDomain(root);
    const auto cells = world::coordinates::generationCellsCovering(
        domain, world::coordinates::interstellarGenerationGrid());
    const std::uint64_t seed = DefaultUniverseSeed;
    const double voxel = world::coordinates::InterstellarGenerationCellLy / 5.0;
    const auto structureCells = mediumStructureCells(cells);
    std::cout << "[ism-check] domain cells " << cells.size() << ", structure cells " << structureCells.size() << "\n";

    struct Acc
    {
        double volume = 0.0, warm = 0.0, cold = 0.0, mol = 0.0;
    };
    std::map<std::string, Acc> inside, outside;
    double expectedWarmCensus = 0.0;
    const auto environmentOf = [](const MediumSample& m) -> std::string {
        return m.wallCount >= 2 ? "wall_intersection" : m.wallCount == 1 ? "compressed_wall"
             : m.immersedWallCount >= 2 ? "immersed_wall_interaction"
             : m.immersedWall ? "wall_immersed_in_cavity" : m.cavityCount > 0 ? "cavity_interior"
             : "warm_neutral_medium";
    };
    for (const auto& cell : structureCells)
    {
        const glm::dvec3 origin = glm::dvec3(cell[0], cell[1], cell[2]) * world::coordinates::InterstellarGenerationCellLy;
        for (int k = 0; k < 5; ++k)
            for (int j = 0; j < 5; ++j)
                for (int i = 0; i < 5; ++i)
                {
                    const glm::dvec3 x = origin + (glm::dvec3(i, j, k) + 0.5) * voxel;
                    const MediumSample m = backgroundMediumAt(seed, x);
                    const CloudFormationPotential p = cloudFormationPotential(m);
                    const double v = voxel * voxel * voxel;
                    auto& acc = (glm::length(x) < ObservedCloudCompletenessLy ? inside : outside)[environmentOf(m)];
                    acc.volume += v;
                    acc.warm += p.warm * v * WarmCloudletsPerLy3;
                    acc.cold += p.cold * v * ColdCloudsPerLy3;
                    acc.mol += p.molecular * v * MolecularClumpsPerLy3;
                    if (glm::length(x) < ObservedCloudCompletenessLy)
                        expectedWarmCensus += p.warm * v * WarmCloudletsPerLy3;
                }
    }
    const MediumSample sol = backgroundMediumAt(seed, glm::dvec3(0.0));
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "[ism-check] Sun's environment: " << environmentOf(sol) << " phase=" << sol.phase
              << " n=" << sol.densityCm3 << " T=" << sol.temperatureK << "\n";
    std::cout << "[ism-check] calibration: expected warm cloudlets within 15 pc (incl. the 4 observed) = "
              << expectedWarmCensus << "  (observed census ~15)\n";
    std::cout << "[ism-check] expected structures per 1e5 ly^3 by environment (inside 15 pc | outside):\n";
    for (const auto* table : {&inside, &outside})
        for (const auto& [env, a] : *table)
            if (a.volume > 0.0)
                std::cout << "  " << (table == &inside ? "in  " : "out ") << std::left << std::setw(26) << env
                          << std::right << " volume=" << std::setw(12) << a.volume
                          << " warm=" << std::setw(8) << 1.0e5 * a.warm / a.volume
                          << " cold=" << std::setw(8) << 1.0e5 * a.cold / a.volume
                          << " molecular=" << std::setw(8) << 1.0e5 * a.mol / a.volume << "\n";

    std::map<std::string, int> counts;
    int inCensus = 0;
    for (const auto& o : generateInterstellarMedium(seed, structureCells))
    {
        const auto& p = o.properties;
        const std::string cls = p.contains("provenance") ? p["provenance"].value("class", std::string("?")) : "?";
        const std::string env = p.contains("formation") ? p["formation"].value("environment", std::string("-")) : "-";
        ++counts[o.objectType + " " + p.value("phase", std::string("")) + " " + cls + " " + env];
        if (o.objectType == "interstellar_medium_region" && p.value("phase", std::string()) == "warm_partially_ionized" &&
            glm::length(o.positionLy) < ObservedCloudCompletenessLy)
            ++inCensus;
    }
    std::cout << "[ism-check] materialised warm cloudlets within 15 pc (incl. observed): " << inCensus << "\n";
    for (const auto& [key, n] : counts)
        std::cout << "  " << std::setw(5) << n << "  " << key << "\n";
    std::map<std::string, int> icy;
    const auto systems = catalogPositions(root);
    for (const auto& cell : cells)
        for (const auto& o : generateInterstellarCell(seed, cell, systems).objects)
            if (o.objectType == "icy_debris_field")
                ++icy[o.properties.value("origin", std::string())];
    std::cout << "[ism-check] icy debris fields:";
    for (const auto& [origin, n] : icy)
        std::cout << " " << origin << "=" << n;
    std::cout << "\n";

    // 4. Navigation environment (planner-ready physical hazards).
    std::vector<InterstellarObject> objects = generateInterstellarMedium(seed, structureCells);
    for (const auto& cell : cells)
        for (auto& o : generateInterstellarCell(seed, cell, systems).objects)
            if (o.objectType == "icy_debris_field")
                objects.push_back(std::move(o));
    std::vector<nlohmann::json> plainProperties;
    plainProperties.reserve(objects.size());
    for (const auto& o : objects)
        plainProperties.push_back(nlohmann::json::parse(o.properties.dump()));
    std::vector<world::interstellar::InterstellarNavigationEnvironment::ObjectView> views;
    for (std::size_t i = 0; i < objects.size(); ++i)
        views.push_back({objects[i].id, objects[i].objectType, objects[i].positionLy, &plainProperties[i]});
    const world::interstellar::InterstellarNavigationEnvironment env(seed, views);
    std::cout << "[ism-check] environment: structures=" << env.structureCount()
              << " debris_fields=" << env.debrisFieldCount() << "\n";
    const auto report = [&](const char* label, const glm::dvec3& a, const glm::dvec3& b) {
        const auto s = env.segment(a, b, 0.1);
        std::cout << std::scientific << std::setprecision(2) << "  " << label << ": L=" << s.lengthLy
                  << " ly N_H=" << s.gasColumnCm2 << " cm^-2 dust=" << s.dustColumnKgM2
                  << " kg/m^2 Av=" << s.extinctionAv << " hits/m^2 [100um,1cm,10m,1km]=" << s.solidColumnM2[0]
                  << "," << s.solidColumnM2[1] << "," << s.solidColumnM2[2] << "," << s.solidColumnM2[3]
                  << " n_e,max=" << s.maxElectronDensityCm3 << " rad,max=" << s.maxIonizingRadiationIndex
                  << " H-fuel,max=" << s.maxHydrogenFuelIndex << " volatile,max=" << s.maxVolatileIndex
                  << " structures=" << s.structuresCrossed.size() << " debris=" << s.debrisFieldsCrossed.size()
                  << std::fixed << std::setprecision(3) << "\n";
    };
    report("Sol -> alpha Cen", glm::dvec3(0.0), galacticToIcrsLy(315.7, -0.7, 4.37));
    for (const auto& o : objects)
        if (o.objectType == "interstellar_medium_region" && o.properties.value("phase", std::string()) == "molecular")
        {
            report("through a molecular clump", o.positionLy - glm::dvec3(15.0, 0.0, 0.0),
                   o.positionLy + glm::dvec3(15.0, 0.0, 0.0));
            break;
        }
    for (const auto& o : objects)
        if (o.objectType == "icy_debris_field")
        {
            report("through an icy debris field", o.positionLy - glm::dvec3(3.0, 0.0, 0.0),
                   o.positionLy + glm::dvec3(3.0, 0.0, 0.0));
            const std::uint64_t fieldSeed = std::stoull(o.properties["provenance"]["seed"].get<std::string>(), nullptr, 16);
            const nlohmann::json plain = nlohmann::json::parse(o.properties.dump());
            const auto a = world::interstellar::materializeIcyBodies(plain, o.positionLy, fieldSeed,
                                                                     o.positionLy, 0.02, 10.0, 2000);
            const auto b = world::interstellar::materializeIcyBodies(plain, o.positionLy, fieldSeed,
                                                                     o.positionLy, 0.02, 10.0, 2000);
            bool same = a.size() == b.size();
            for (std::size_t i = 0; same && i < a.size(); ++i)
                same = a[i].seed == b[i].seed && a[i].diameterM == b[i].diameterM;
            std::cout << "  materialised bodies > 10 m (largest 2000) within 0.02 ly of the field centre: " << a.size()
                      << " (largest " << (a.empty() ? 0.0 : a.front().diameterM) << " m), deterministic="
                      << (same ? "yes" : "NO") << "\n";
            break;
        }
    return 0;
}

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
    if (cmd == "interstellar") return interstellar(argv[2]);
    if (cmd == "domain") return domainReport(argv[2]);
    if (cmd == "ism-check") return ismCheck(argv[2]);
    if (cmd == "bulk" && argc >= 4)
    {
        const std::uint64_t seed = argc >= 5 ? std::stoull(argv[4], nullptr, 16) : DefaultUniverseSeed;
        return bulk(std::stoi(argv[2]), argv[3], seed);
    }
    std::cerr << "unknown command\n";
    return 2;
}
