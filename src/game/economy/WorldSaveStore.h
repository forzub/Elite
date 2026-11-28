#pragma once

// THE WORLD SAVE CONTRACT (stabilization stage 8; docs/WORLD_SAVE_CONTRACT.md). A world's persisted state is written as
// ONE GENERATION of parts - today "economy" (the banks, markets, boards, cargo: economy_state/N) and "ships" (the
// runtime ships and hires: runtime_ships/N) - and committed by ONE atomic write of the manifest that names them:
//
//   <base>.manifest.json   {"format": "hanted_world_save/1", "generation": N, "parts": {"economy": {file, bytes, fnv1a64}, ...}}
//   <base>.g<N>.economy.json, <base>.g<N>.ships.json     (each written atomically BEFORE the manifest)
//
// A crash before the manifest's rename leaves the previous generation in force, complete; after it, the new one. The
// two parts can no longer be torn apart (before: the economy and the ships were separate files written at separate
// times - a ship bought between the two writes had its price debited and no ship on load). The generation N-2's
// files are removed after a commit (N-1 stays as the fallback a damaged N falls back to).
//
// Loading: resolve(base, part) - the committed generation's file of that part, verified (size + hash); a damaged part
// falls back to the previous generation (both parts together); no manifest = the legacy layout (<base> and
// <base>.ships.json) - read once, the next save writes the generational layout.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace game::economy
{
struct WorldSavePart
{
    std::string name;   // "economy", "ships"
    std::string text;
};

// what load() found (audit #11 4.2 - every case told apart; none of the failures falls back to the legacy layout)
enum class WorldSaveStatus
{
    None,                 // no save at all (no manifest, no legacy file)
    Loaded,               // the manifest's generation, verified
    LoadedPrevious,       // the newest generation damaged / incomplete - the previous one, verified
    Legacy,               // no manifest: the pre-manifest layout
    ManifestCorrupt,      // a manifest exists and cannot be read (never written over)
    VersionUnsupported,   // a manifest of another format version (never written over)
    NoUsableGeneration    // a readable manifest, but neither generation verifies (never written over)
};

struct WorldSaveLoaded
{
    WorldSaveStatus status = WorldSaveStatus::None;
    bool found = false;               // a manifest (or the legacy files) exists
    bool legacy = false;              // read from the pre-manifest layout
    std::uint64_t generation = 0;
    std::vector<WorldSavePart> parts;  // verified parts of ONE generation
    std::string note;                 // what happened (fallback, damage) for the log
    [[nodiscard]] const std::string* part(const std::string& name) const
    {
        for (const auto& p : parts)
            if (p.name == name)
                return &p.text;
        return nullptr;
    }
};

class WorldSaveStore
{
public:
    // the committed generation of <base> (0: none yet)
    [[nodiscard]] static std::uint64_t committedGeneration(const std::string& base);
    // writes the parts as generation N = committed + 1 and commits it (the manifest). false: nothing committed (the
    // previous generation stays in force); `error` says why
    static bool commit(const std::string& base, const std::vector<WorldSavePart>& parts, std::string* error = nullptr);
    // the parts of the newest complete, verified generation (or the legacy layout)
    [[nodiscard]] static WorldSaveLoaded load(const std::string& base);
    // a load that must NOT be followed by saving over the files (they may be repairable by hand)
    [[nodiscard]] static bool protectsFiles(WorldSaveStatus s) noexcept
    {
        return s == WorldSaveStatus::ManifestCorrupt || s == WorldSaveStatus::VersionUnsupported ||
               s == WorldSaveStatus::NoUsableGeneration;
    }
    // FNV-1a 64 over the bytes (the integrity check of a part)
    [[nodiscard]] static std::uint64_t fnv1a64(const std::string& bytes);
};
} // namespace game::economy
