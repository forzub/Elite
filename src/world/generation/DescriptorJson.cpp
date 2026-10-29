#include "src/world/generation/DescriptorJson.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "src/world/generation/StellarSystemGenerator.h"

namespace world::generation
{

namespace fs = std::filesystem;
using ojson = nlohmann::ordered_json;
using json = nlohmann::json;

namespace
{

bool readJsonFile(const fs::path& path, json& out, std::string& error)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        error = "cannot open " + path.string();
        return false;
    }
    try
    {
        f >> out;
    }
    catch (const std::exception& e)
    {
        error = path.string() + ": " + e.what();
        return false;
    }
    return true;
}

std::vector<fs::path> sortedJsonFiles(const fs::path& dir)
{
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
        return out;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == ".json")
            out.push_back(e.path());
    std::sort(out.begin(), out.end());
    return out;
}

// {"value": x, "source": "..."} or a bare number.
std::optional<double> readValue(const json& j, const char* key, std::map<std::string, std::string>* sources = nullptr)
{
    if (!j.contains(key) || j[key].is_null())
        return std::nullopt;
    const json& v = j[key];
    if (v.is_number())
        return v.get<double>();
    if (v.is_object() && v.contains("value") && v["value"].is_number())
    {
        if (sources && v.contains("source") && v["source"].is_string())
            (*sources)[key] = v["source"].get<std::string>();
        return v["value"].get<double>();
    }
    return std::nullopt;
}

std::vector<std::string> readStrings(const json& j, const char* key)
{
    std::vector<std::string> out;
    if (j.contains(key) && j[key].is_array())
        for (const auto& s : j[key])
            if (s.is_string()) out.push_back(s.get<std::string>());
    return out;
}

std::string readString(const json& j, const char* key, const std::string& fallback = "")
{
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : fallback;
}

HierarchyNodeInput readNode(const json& j)
{
    HierarchyNodeInput n;
    if (j.contains("star"))
    {
        n.starKey = j["star"].get<std::string>();
        n.id = n.starKey;
        return n;
    }
    n.id = readString(j, "id", "system_barycenter");
    if (j.contains("children") && j["children"].is_array())
        for (const auto& c : j["children"])
            n.children.push_back(readNode(c));
    if (j.contains("orbit") && j["orbit"].is_object())
    {
        const json& o = j["orbit"];
        n.semiMajorAxisAu = readValue(o, "semi_major_axis_au");
        n.eccentricity = readValue(o, "eccentricity");
        n.periodYears = readValue(o, "period_years");
        n.inclinationDeg = readValue(o, "inclination_deg");
        n.projectedSeparationAu = readValue(o, "projected_separation_au");
        n.orbitSource = readString(o, "source");
    }
    return n;
}

ojson orbitJson(const KeplerOrbit& o)
{
    ojson j;
    j["semi_major_axis_au"] = o.semiMajorAxisAu;
    j["eccentricity"] = o.eccentricity;
    j["inclination_deg"] = o.inclinationDeg;
    j["longitude_ascending_node_deg"] = o.longitudeAscendingNodeDeg;
    j["argument_of_periapsis_deg"] = o.argumentOfPeriapsisDeg;
    j["mean_anomaly_at_epoch_deg"] = o.meanAnomalyAtEpochDeg;
    j["epoch_universe_seconds"] = o.epochUniverseSeconds;
    j["period_days"] = o.periodDays;
    j["direction"] = o.direction;
    return j;
}

ojson provenanceJson(const ProvenanceMap& p)
{
    ojson j = ojson::object();
    for (const auto& [k, v] : p)
        j[k] = toString(v);
    return j;
}

ojson pairsJson(const std::vector<std::pair<std::string, double>>& v)
{
    ojson j = ojson::object();
    for (const auto& [k, x] : v)
        j[k] = x;
    return j;
}

ojson gapsJson(const std::vector<std::pair<double, double>>& v)
{
    ojson j = ojson::array();
    for (const auto& [a, b] : v)
        j.push_back(ojson::array({a, b}));
    return j;
}

ojson extraJson(const json& extra)
{
    return ojson::parse(extra.dump());
}

ojson versionsJson(const UniverseGenerationVersions& v)
{
    ojson j;
    j["stellar_system"] = v.stellarSystem;
    j["celestial_body"] = v.celestialBody;
    j["satellites"] = v.satellites;
    j["interstellar"] = v.interstellar;
    j["planet_surface"] = v.planetSurface;
    return j;
}

} // namespace

std::string hexSeed(std::uint64_t seed)
{
    std::ostringstream s;
    s << "0x" << std::hex;
    s.width(16);
    s.fill('0');
    s << seed;
    return s.str();
}

std::string dumpJson(const ojson& j)
{
    return j.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::strict) + "\n";
}

bool readSystemInput(const fs::path& dir, SystemInput& out, std::string& error)
{
    json sys;
    if (!readJsonFile(dir / "system.json", sys, error))
        return false;
    if (readString(sys, "schema") != SystemInputSchema)
    {
        error = (dir / "system.json").string() + ": schema is not " + SystemInputSchema;
        return false;
    }
    out = SystemInput {};
    out.id = sys.value("id", -1);
    out.name = readString(sys, "name");
    out.slug = readString(sys, "slug");
    out.alternativeNames = readStrings(sys, "alternative_names");
    out.catalogScope = readString(sys, "catalog_scope", "local");
    out.atlasVisible = sys.value("atlas_visible", true);
    out.routeTarget = sys.value("route_target", true);
    out.questRole = readString(sys, "quest_role");
    if (sys.contains("position_ly"))
    {
        const json& p = sys["position_ly"];
        out.positionLy = {p.value("x", 0.0), p.value("y", 0.0), p.value("z", 0.0)};
    }
    out.positionSource = readString(sys, "position_source");
    out.observedSystemComplete = sys.value("observed_system_complete", false);
    out.reservedDesignations = readStrings(sys, "reserved_designations");
    if (sys.contains("survey_completeness") && sys["survey_completeness"].is_object())
    {
        out.rvDetectionLimitMs = readValue(sys["survey_completeness"], "rv_detection_limit_ms");
        out.rvBaselineDays = readValue(sys["survey_completeness"], "rv_baseline_days");
        out.transitRadiusLimitEarth = readValue(sys["survey_completeness"], "transit_radius_limit_earth");
        out.transitBaselineDays = readValue(sys["survey_completeness"], "transit_baseline_days");
    }
    if (sys.contains("stellar_hierarchy"))
        out.hierarchy = readNode(sys["stellar_hierarchy"]);
    for (const char* key : {"lore", "observational_reference", "legacy", "observed_reservoirs"})
        if (sys.contains(key)) out.passthrough[key] = sys[key];

    for (const auto& path : sortedJsonFiles(dir / "stars"))
    {
        json s;
        if (!readJsonFile(path, s, error))
            return false;
        StarInput st;
        st.key = readString(s, "key", path.stem().string());
        st.name = readString(s, "name");
        st.alternativeNames = readStrings(s, "alternative_names");
        st.spectralType = readString(s, "spectral_type");
        st.stageHint = readString(s, "stage_hint");
        st.massSolar = readValue(s, "mass_solar", &st.sources);
        st.radiusSolar = readValue(s, "radius_solar", &st.sources);
        st.teffK = readValue(s, "teff_k", &st.sources);
        st.luminositySolar = readValue(s, "luminosity_solar", &st.sources);
        st.metallicityFeH = readValue(s, "metallicity_feh", &st.sources);
        st.ageGyr = readValue(s, "age_gyr", &st.sources);
        st.rotationPeriodDays = readValue(s, "rotation_period_days", &st.sources);
        if (s.contains("extra")) st.extra = s["extra"];
        out.stars.push_back(st);
    }
    // Stable component order: the order in which the hierarchy names them.
    std::vector<std::string> order;
    std::vector<const HierarchyNodeInput*> stack {&out.hierarchy};
    while (!stack.empty())
    {
        const auto* n = stack.back();
        stack.pop_back();
        if (!n->starKey.empty()) order.push_back(n->starKey);
        for (auto it = n->children.rbegin(); it != n->children.rend(); ++it) stack.push_back(&*it);
    }
    std::stable_sort(out.stars.begin(), out.stars.end(), [&](const StarInput& a, const StarInput& b) {
        const auto ia = std::find(order.begin(), order.end(), a.key) - order.begin();
        const auto ib = std::find(order.begin(), order.end(), b.key) - order.begin();
        return ia < ib;
    });

    for (const auto& path : sortedJsonFiles(dir / "observed"))
    {
        json b;
        if (!readJsonFile(path, b, error))
            return false;
        ObservedBodyInput o;
        o.designation = readString(b, "designation");
        o.alternativeNames = readStrings(b, "alternative_names");
        o.kind = readString(b, "kind", "planet");
        o.host = readString(b, "host");
        o.source = readString(b, "source");
        const json orbit = b.value("orbit", json::object());
        o.semiMajorAxisAu = readValue(orbit, "semi_major_axis_au");
        o.periodDays = readValue(orbit, "period_days");
        o.eccentricity = readValue(orbit, "eccentricity");
        o.inclinationDeg = readValue(orbit, "inclination_deg");
        o.longitudeAscendingNodeDeg = readValue(orbit, "longitude_ascending_node_deg");
        o.argumentOfPeriapsisDeg = readValue(orbit, "argument_of_periapsis_deg");
        o.meanAnomalyAtEpochDeg = readValue(orbit, "mean_anomaly_at_epoch_deg");
        o.epochUniverseSeconds = readValue(orbit, "epoch_universe_seconds");
        o.retrograde = orbit.value("retrograde", false);
        const json phys = b.value("physical", json::object());
        o.massEarth = readValue(phys, "mass_earth");
        o.minimumMassEarth = readValue(phys, "minimum_mass_earth");
        o.radiusEarth = readValue(phys, "radius_earth");
        o.rotationPeriodHours = readValue(phys, "rotation_period_hours");
        o.obliquityDeg = readValue(phys, "obliquity_deg");
        o.bondAlbedo = readValue(phys, "bond_albedo");
        o.surfacePressurePa = readValue(phys, "surface_pressure_pa");
        o.meanSurfaceTemperatureK = readValue(phys, "mean_surface_temperature_k");
        o.waterMassFraction = readValue(phys, "water_mass_fraction");
        o.ironCoreFraction = readValue(phys, "iron_core_fraction");
        o.envelopeMassFraction = readValue(phys, "envelope_mass_fraction");
        if (phys.contains("tidally_locked") && phys["tidally_locked"].is_boolean())
            o.tidallyLocked = phys["tidally_locked"].get<bool>();
        if (b.contains("atmosphere_composition") && b["atmosphere_composition"].is_object())
            for (const auto& [gas, x] : b["atmosphere_composition"].items())
                if (x.is_number()) o.atmosphereComposition.push_back({gas, x.get<double>()});
        if (b.contains("rings") && b["rings"].is_array())
            for (const auto& r : b["rings"])
            {
                ObservedRingInput ri;
                ri.name = readString(r, "name");
                ri.innerRadiusKm = r.value("inner_radius_km", 0.0);
                ri.outerRadiusKm = r.value("outer_radius_km", 0.0);
                ri.opticalDepth = r.value("optical_depth", 0.0);
                ri.iceFraction = r.value("ice_fraction", 0.0);
                ri.composition = readString(r, "composition");
                o.rings.push_back(ri);
            }
        if (b.contains("extra")) o.extra = b["extra"];
        out.observedBodies.push_back(o);
    }
    if (out.id < 0 || out.stars.empty())
    {
        error = dir.string() + ": system id or stars missing";
        return false;
    }
    return true;
}

ojson generatedSystemJson(const SystemDescriptor& sd)
{
    ojson j;
    j["schema"] = GeneratedSystemSchema;
    j["system_id"] = sd.systemId;
    j["name"] = sd.name;
    ojson gen;
    gen["generator"] = "world::generation::generateStellarSystem";
    gen["universe_seed"] = hexSeed(sd.universeSeed);
    gen["system_seed"] = hexSeed(sd.systemSeed);
    gen["versions"] = versionsJson(sd.versions);
    j["generation"] = gen;

    const auto& c = sd.context;
    ojson ctx;
    ctx["population"] = toString(c.population);
    ctx["heliocentric_galactic_kpc"] = ojson::array({c.galactocentricKpc.x, c.galactocentricKpc.y, c.galactocentricKpc.z});
    ctx["galactocentric_radius_kpc"] = c.galactocentricRadiusKpc;
    ctx["height_above_plane_kpc"] = c.heightAbovePlaneKpc;
    ctx["population_metallicity_mean_feh"] = c.populationMetallicityMeanFeH;
    ctx["population_metallicity_sigma_dex"] = c.populationMetallicitySigmaDex;
    ctx["alpha_enhancement"] = c.alphaEnhancement;
    ctx["local_stellar_density_per_pc3"] = c.localStellarDensityPerPc3;
    ctx["formation_environment_density"] = c.formationEnvironmentDensity;
    ctx["interstellar_density_per_cm3"] = c.interstellarDensityPerCm3;
    j["galactic_context"] = ctx;

    ojson stars = ojson::array();
    for (const auto& s : sd.stars)
    {
        ojson st;
        st["key"] = s.key;
        st["id"] = s.id;
        st["name"] = s.name;
        st["alternative_names"] = s.alternativeNames;
        st["spectral_type"] = s.spectralType;
        st["stage"] = toString(s.stage);
        st["seed"] = hexSeed(s.seed);
        st["mass_solar"] = s.massSolar;
        st["initial_mass_solar"] = s.initialMassSolar;
        st["radius_solar"] = s.radiusSolar;
        st["teff_k"] = s.teffK;
        st["luminosity_solar"] = s.luminositySolar;
        st["metallicity_feh"] = s.metallicityFeH;
        st["age_gyr"] = s.ageGyr;
        st["main_sequence_lifetime_gyr"] = s.mainSequenceLifetimeGyr;
        st["rotation_period_days"] = s.rotationPeriodDays;
        st["mu_m3s2"] = s.muM3s2;
        st["parent_node"] = s.parentNode;
        if (s.orbit) st["orbit"] = orbitJson(*s.orbit);
        st["stable_zone_au"] = ojson::array({s.stableZoneInnerAu, s.stableZoneOuterAu});
        st["provenance"] = provenanceJson(s.provenance);
        ojson src = ojson::object();
        for (const auto& [k, v] : s.sources) src[k] = v;
        st["sources"] = src;
        if (!s.extra.empty()) st["extra"] = extraJson(s.extra);
        stars.push_back(st);
    }
    j["stars"] = stars;

    ojson nodes = ojson::array();
    for (const auto& n : sd.barycenters)
    {
        ojson b;
        b["id"] = n.id;
        b["body_id"] = n.bodyId;
        b["parent_node"] = n.parentNode;
        b["children"] = n.children;
        b["mass_solar"] = n.massSolar;
        if (n.orbit) b["orbit"] = orbitJson(*n.orbit);
        if (n.relativeOrbit) b["relative_orbit"] = orbitJson(*n.relativeOrbit);
        b["circumbinary_inner_au"] = n.circumbinaryInnerAu;
        b["provenance"] = provenanceJson(n.provenance);
        nodes.push_back(b);
    }
    j["barycenters"] = nodes;

    ojson disks = ojson::array();
    for (const auto& d : sd.disks)
    {
        ojson x;
        x["host_node"] = d.hostNode;
        x["gas_mass_solar"] = d.gasMassSolar;
        x["solid_mass_earth"] = d.solidMassEarth;
        x["dust_to_gas"] = d.dustToGas;
        x["inner_edge_au"] = d.innerEdgeAu;
        x["characteristic_radius_au"] = d.characteristicRadiusAu;
        x["outer_truncation_au"] = d.outerTruncationAu;
        x["lifetime_myr"] = d.lifetimeMyr;
        x["snow_line_au"] = d.snowLineAu;
        x["turbulence_alpha"] = d.turbulenceAlpha;
        x["migration_regime"] = d.migrationRegime;
        disks.push_back(x);
    }
    j["protoplanetary_disks"] = disks;
    j["architecture_mode"] = sd.architectureMode;

    ojson stab = ojson::array();
    bool allPassed = true;
    for (const auto& c : sd.stability)
    {
        stab.push_back(ojson {{"check", c.name}, {"passed", c.passed}, {"detail", c.detail}});
        allPassed = allPassed && c.passed;
    }
    j["stability_passed"] = allPassed;
    j["stability"] = stab;
    j["warnings"] = sd.warnings;
    return j;
}

ojson bodyJson(const BodyDescriptor& b, const SystemDescriptor& sd)
{
    ojson j;
    j["schema"] = GeneratedBodySchema;
    j["id"] = b.id;
    j["designation"] = b.designation;
    j["alternative_names"] = b.alternativeNames;
    j["kind"] = b.kind;
    j["parent_id"] = b.parentId;
    j["host_star"] = b.hostStarKey;
    j["origin"] = b.origin;
    j["formation_origin"] = b.formationOrigin;
    j["seed"] = hexSeed(b.seed);
    j["generator_versions"] = versionsJson(sd.versions);
    if (!b.source.empty()) j["source"] = b.source;
    j["orbit"] = orbitJson(b.orbit);

    ojson phys;
    phys["mass_kg"] = b.massKg;
    phys["mass_earth"] = b.massKg / 5.9722e24;
    phys["radius_km"] = b.radiusKm;
    phys["density_kg_m3"] = b.densityKgM3;
    phys["mu_m3s2"] = b.muM3s2;
    phys["surface_gravity_ms2"] = b.surfaceGravityMs2;
    phys["escape_velocity_kms"] = b.escapeVelocityKms;
    phys["hill_radius_km"] = b.hillRadiusKm;
    phys["sphere_of_influence_km"] = b.sphereOfInfluenceKm;
    phys["body_class"] = b.bodyClass;
    j["physical"] = phys;

    ojson comp;
    comp["iron_core"] = b.composition.ironCore;
    comp["silicate_mantle"] = b.composition.silicateMantle;
    comp["water_ice"] = b.composition.waterIce;
    comp["other_volatiles"] = b.composition.otherVolatiles;
    comp["hydrogen_helium_envelope"] = b.composition.hydrogenHeliumEnvelope;
    comp["water_mass_fraction"] = b.waterMassFraction;
    comp["formation_distance_au"] = b.formationDistanceAu;
    j["bulk_composition"] = comp;

    ojson th;
    th["instellation_earth"] = b.instellationEarth;
    th["bond_albedo"] = b.bondAlbedo;
    th["equilibrium_temperature_k"] = b.equilibriumTemperatureK;
    th["mean_surface_temperature_k"] = b.meanSurfaceTemperatureK;
    th["min_surface_temperature_k"] = b.minSurfaceTemperatureK;
    th["max_surface_temperature_k"] = b.maxSurfaceTemperatureK;
    th["temperature_regime"] = b.temperatureRegime;
    th["habitable_zone_position"] = b.habitableZonePosition;
    th["radiogenic_heat_flux_wm2"] = b.radiogenicHeatFluxWm2;
    th["tidal_heat_flux_wm2"] = b.tidalHeatFluxWm2;
    j["thermal"] = th;

    ojson at;
    at["surface_pressure_pa"] = b.atmosphere.surfacePressurePa;
    at["composition"] = pairsJson(b.atmosphere.composition);
    at["mean_molecular_weight"] = b.atmosphere.meanMolecularWeight;
    at["scale_height_km"] = b.atmosphere.scaleHeightKm;
    at["greenhouse_warming_k"] = b.atmosphere.greenhouseWarmingK;
    at["cloud_potential"] = b.atmosphere.cloudPotential;
    at["origin"] = b.atmosphere.origin;
    at["retention_index"] = b.atmosphere.retentionIndex;
    at["outer_radius_km"] = b.atmosphere.outerRadiusKm;
    j["atmosphere"] = at;

    ojson geo;
    geo["tectonic_regime"] = b.tectonicRegime;
    geo["volcanism"] = b.volcanism;
    geo["magnetic_field_earth"] = b.magneticFieldEarth;
    geo["hydrosphere"] = b.hydrosphere;
    j["geophysics"] = geo;

    ojson sp;
    sp["rotation_period_hours"] = b.rotationPeriodHours;
    sp["obliquity_deg"] = b.obliquityDeg;
    sp["rotation_direction"] = b.rotationDirection;
    sp["axis_node_deg"] = b.axisNodeDeg;
    sp["tidal_lock"] = b.tidalLock;
    j["spin"] = sp;

    const auto& s = b.surface;
    ojson sf;
    sf["terrain_profile"] = s.terrainProfile;
    sf["relief_amplitude_km"] = s.reliefAmplitudeKm;
    sf["roughness"] = s.roughness;
    sf["crater_density"] = s.craterDensity;
    sf["resurfacing_rate"] = s.resurfacingRate;
    sf["erosion_potential"] = s.erosionPotential;
    sf["glaciation_potential"] = s.glaciationPotential;
    sf["cave_tectonic_propensity"] = s.caveTectonicPropensity;
    sf["lava_tube_propensity"] = s.lavaTubePropensity;
    sf["karst_propensity"] = s.karstPropensity;
    sf["weather_potential"] = s.weatherPotential;
    sf["ocean_coverage"] = s.oceanCoverage;
    sf["ice_coverage"] = s.iceCoverage;
    sf["surface_materials"] = pairsJson(s.surfaceMaterials);
    j["surface_generation"] = sf;

    ojson rings = ojson::array();
    for (const auto& r : b.rings)
    {
        ojson x;
        x["id"] = r.id;
        x["name"] = r.name;
        x["inner_radius_km"] = r.innerRadiusKm;
        x["outer_radius_km"] = r.outerRadiusKm;
        x["mass_kg"] = r.massKg;
        x["optical_depth"] = r.opticalDepth;
        x["ice_fraction"] = r.iceFraction;
        x["rock_fraction"] = r.rockFraction;
        x["dust_fraction"] = r.dustFraction;
        x["age_myr"] = r.ageMyr;
        x["plane_inclination_deg"] = r.planeInclinationDeg;
        x["origin"] = r.origin;
        x["visibility_class"] = r.visibilityClass;
        x["gaps_km"] = gapsJson(r.gapsKm);
        rings.push_back(x);
    }
    j["rings"] = rings;
    j["environment_preset_id"] = b.environmentPresetId;
    j["provenance"] = provenanceJson(b.provenance);
    if (!b.extra.empty()) j["extra"] = extraJson(b.extra);
    return j;
}

ojson reservoirJson(const ReservoirDescriptor& r, const SystemDescriptor& sd)
{
    ojson j;
    j["schema"] = GeneratedReservoirSchema;
    j["id"] = r.id;
    j["name"] = r.name;
    j["kind"] = r.kind;
    j["parent_id"] = r.parentId;
    j["seed"] = hexSeed(r.seed);
    j["generator_versions"] = versionsJson(sd.versions);
    j["inner_au"] = r.innerAu;
    j["outer_au"] = r.outerAu;
    j["peak_au"] = r.peakAu;
    j["total_mass_earth"] = r.totalMassEarth;
    j["eccentricity_mean"] = r.eccentricityMean;
    j["inclination_mean_deg"] = r.inclinationMeanDeg;
    j["size_distribution"] = ojson {{"slope", r.sizeSlope}, {"min_body_km", r.minBodyKm}, {"max_body_km", r.maxBodyKm},
                                    {"estimated_bodies_above_1km", r.estimatedBodiesAbove1Km}};
    j["clumpiness"] = r.clumpiness;
    j["composition"] = pairsJson(r.composition);
    j["gaps_au"] = gapsJson(r.gapsAu);
    j["resonance"] = r.resonance;
    j["formation_origin"] = r.formationOrigin;
    j["provenance"] = provenanceJson(r.provenance);
    return j;
}

std::vector<fs::path> writeGeneratedSystem(const SystemDescriptor& sd, const fs::path& dir)
{
    std::vector<fs::path> written;
    const fs::path gen = dir / "generated";
    std::error_code ec;
    fs::remove_all(gen, ec);
    fs::create_directories(gen / "bodies");
    fs::create_directories(gen / "reservoirs");
    auto write = [&](const fs::path& path, const ojson& j) {
        std::ofstream f(path, std::ios::binary);
        f << dumpJson(j);
        written.push_back(path);
    };
    write(gen / "system.json", generatedSystemJson(sd));
    std::set<std::string> used;
    for (const auto& b : sd.bodies)
    {
        std::string base = b.kind + "_" + fileSlug(b.designation, b.alternativeNames);
        std::string name = base;
        for (int k = 2; used.count(name); ++k) name = base + "_" + std::to_string(k);
        used.insert(name);
        write(gen / "bodies" / (name + ".json"), bodyJson(b, sd));
    }
    used.clear();
    for (const auto& r : sd.reservoirs)
    {
        std::string base = fileSlug(r.name, {});
        std::string name = base;
        for (int k = 2; used.count(name); ++k) name = base + "_" + std::to_string(k);
        used.insert(name);
        write(gen / "reservoirs" / (name + ".json"), reservoirJson(r, sd));
    }
    return written;
}

} // namespace world::generation
