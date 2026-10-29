#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    On-disk layout of one physical star system (one system = one directory):

        <NNN>_<slug>/
            system.json            INPUT  identity, position, stellar hierarchy
            stars/<key>.json       INPUT  one stellar component each
            observed/<slug>.json   INPUT  observational / authored anchors
            generated/             OUTPUT owned by the generator, replaced whole
                system.json        context, present-day stars, barycenters,
                                   disks, architecture, stability, provenance
                bodies/<kind>_<slug>.json
                reservoirs/<slug>.json

    Values in input files are {"value": x, "source": "..."} so every number
    keeps its origin. Seeds are hex strings (exact 64-bit).
*/

inline constexpr const char* SystemInputSchema = "elite.star_system.v2";
inline constexpr const char* StarInputSchema = "elite.star.v2";
inline constexpr const char* ObservedBodySchema = "elite.observed_body.v2";
inline constexpr const char* GeneratedSystemSchema = "elite.generated_system.v1";
inline constexpr const char* GeneratedBodySchema = "elite.generated_body.v1";
inline constexpr const char* GeneratedReservoirSchema = "elite.generated_reservoir.v1";

bool readSystemInput(const std::filesystem::path& systemDir, SystemInput& out, std::string& error);

nlohmann::ordered_json generatedSystemJson(const SystemDescriptor& system);
nlohmann::ordered_json bodyJson(const BodyDescriptor& body, const SystemDescriptor& system);
nlohmann::ordered_json reservoirJson(const ReservoirDescriptor& reservoir, const SystemDescriptor& system);

// Replaces <systemDir>/generated with the descriptor. Returns written files.
std::vector<std::filesystem::path> writeGeneratedSystem(
    const SystemDescriptor& system, const std::filesystem::path& systemDir);

// Text serialisation used for files and determinism checks (2-space indent, LF).
std::string dumpJson(const nlohmann::ordered_json& j);

std::string hexSeed(std::uint64_t seed);

} // namespace world::generation
