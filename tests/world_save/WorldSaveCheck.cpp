// Stabilization stage 8: the world save contract (WorldSaveStore) - one generation of all parts, one commit, verified
// load, the previous generation as the fallback, the legacy layout read once.
//   EliteWorldSaveCheck <scratch dir>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "src/game/economy/WorldSaveStore.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}
void writeRaw(const std::string& path, const std::string& text)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}
} // namespace

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    using game::economy::WorldSaveStore;
    const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "elite_world_save_check";
    std::error_code ec;
    fs::create_directories(dir, ec);
    for (const auto& e : fs::directory_iterator(dir, ec))   // only this check's own files
        if (e.path().filename().string().rfind("world", 0) == 0)
            fs::remove(e.path(), ec);
    const std::string base = (dir / "world.json").string();

    // 1. the legacy layout is read (economy + ships), marked legacy
    writeRaw(base, "{\"schema\":\"economy_state/5\",\"day\":7}");
    writeRaw(base + ".ships.json", "{\"schema\":\"runtime_ships/2\",\"ships\":[]}");
    auto l = WorldSaveStore::load(base);
    check(l.found && l.legacy && l.part("economy") && l.part("ships"), "legacy layout read with both parts");

    // 2. commits: generation 1, 2, 3; each load gives ONE generation's parts together
    for (int g = 1; g <= 3; ++g)
        check(WorldSaveStore::commit(base, {{"economy", "E" + std::to_string(g)}, {"ships", "S" + std::to_string(g)}}),
              "commit");
    l = WorldSaveStore::load(base);
    check(l.found && !l.legacy && l.generation == 3, "the newest generation is in force");
    check(l.part("economy") && *l.part("economy") == "E3" && l.part("ships") && *l.part("ships") == "S3",
          "both parts of the SAME generation");
    check(!fs::exists(base + ".g1.economy.json") && !fs::exists(base + ".g1.ships.json"), "generation N-2 removed");
    check(fs::exists(base + ".g2.economy.json"), "generation N-1 kept as the fallback");

    // 3. a damaged part of the newest generation: the previous generation, BOTH parts of it
    writeRaw(base + ".g3.ships.json", "S3-torn");
    l = WorldSaveStore::load(base);
    check(l.generation == 2 && l.part("economy") && *l.part("economy") == "E2" && l.part("ships") && *l.part("ships") == "S2",
          "damaged newest -> the previous generation, parts together");
    check(!l.note.empty(), "the fallback is reported");

    // 4. a crash between the parts and the manifest (parts of g4 written, manifest not): g3's manifest still rules
    writeRaw(base + ".g3.ships.json", "S3");
    writeRaw(base + ".g4.economy.json", "E4");   // as if the writer died before the manifest rename
    l = WorldSaveStore::load(base);
    check(l.generation == 3 && *l.part("economy") == "E3", "parts without a manifest commit are not in force");

    // 5. both generations unusable: nothing loaded, found (the caller must not write over them)
    writeRaw(base + ".g3.economy.json", "x");
    writeRaw(base + ".g2.economy.json", "y");
    l = WorldSaveStore::load(base);
    check(l.found && !l.part("economy"), "no usable generation: found, nothing loaded");
    check(l.status == game::economy::WorldSaveStatus::NoUsableGeneration && WorldSaveStore::protectsFiles(l.status),
          "no usable generation: told, the files protected");

    // ---- audit #11 4.5: the failure tests
    const auto fresh = [&](const std::string& name) {
        const std::string b = (dir / (name + ".json")).string();
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (e.path().filename().string().rfind(name, 0) == 0)
                fs::remove(e.path(), ec);
        return b;
    };
    using game::economy::WorldSaveStatus;
    {   // a corrupt manifest: told, NOT the legacy layout, and never written over
        const std::string b = fresh("corrupt");
        writeRaw(b, "{\"legacy\":true}");   // a legacy file next to it must not be read
        writeRaw(b + ".manifest.json", "{\"format\": \"hanted_world_save/1\", \"generation\": 3, \"parts\": {");
        const auto r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::ManifestCorrupt && !r.part("economy"), "corrupt manifest: told, nothing loaded");
        check(!r.legacy, "corrupt manifest: no fallback to the legacy layout");
        std::string why;
        check(!WorldSaveStore::commit(b, {{"economy", "E"}}, &why), "corrupt manifest: commit refuses to write over it");
    }
    {   // an unsupported version
        const std::string b = fresh("version");
        writeRaw(b + ".manifest.json", "{\"format\": \"hanted_world_save/9\", \"generation\": 1, \"parts\": {}}");
        const auto r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::VersionUnsupported && WorldSaveStore::protectsFiles(r.status), "unsupported version: told, protected");
        check(!WorldSaveStore::commit(b, {{"economy", "E"}}), "unsupported version: commit refuses");
    }
    {   // a missing part of the newest generation -> the previous one
        const std::string b = fresh("missing");
        WorldSaveStore::commit(b, {{"economy", "E1"}, {"ships", "S1"}});
        WorldSaveStore::commit(b, {{"economy", "E2"}, {"ships", "S2"}});
        fs::remove(b + ".g2.ships.json", ec);
        const auto r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::LoadedPrevious && r.part("ships") && *r.part("ships") == "S1",
              "missing part: the previous generation, both parts");
    }
    {   // a partial write (truncated part) -> the previous one; THEN a save keeps the known-good generation (4.3)
        const std::string b = fresh("partial");
        WorldSaveStore::commit(b, {{"economy", "E1-good"}, {"ships", "S1-good"}});
        WorldSaveStore::commit(b, {{"economy", "E2-written"}, {"ships", "S2-written"}});
        writeRaw(b + ".g2.economy.json", "E2-wr");   // torn
        auto r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::LoadedPrevious && *r.part("economy") == "E1-good", "partial write: the previous generation");
        // the restored game saves again: the new generation's fallback must be generation 1 (the good one), not 2 (torn)
        check(WorldSaveStore::commit(b, {{"economy", "E3"}, {"ships", "S3"}}), "save after a restore: committed");
        check(fs::exists(b + ".g1.economy.json") && fs::exists(b + ".g1.ships.json"), "save after a restore: the known-good generation kept");
        // ... and the new generation damaged in turn: the known-good one is what loads
        writeRaw(b + ".g3.ships.json", "S");
        r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::LoadedPrevious && r.generation == 1 && *r.part("ships") == "S1-good",
              "the new generation damaged again: the known-good generation loads");
    }
#ifdef _WIN32
    {   // a write failure (the manifest locked by another process): nothing committed, the previous generation intact
        const std::string b = fresh("locked");
        WorldSaveStore::commit(b, {{"economy", "E1"}});
        const std::wstring m = fs::path(b + ".manifest.json").wstring();
        HANDLE h = CreateFileW(m.c_str(), GENERIC_READ, 0 /* no sharing */, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        std::string why;
        const bool committed = WorldSaveStore::commit(b, {{"economy", "E2"}}, &why);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        check(!committed, "a locked manifest: the commit fails, told");
        const auto r = WorldSaveStore::load(b);
        check(r.status == WorldSaveStatus::Loaded && *r.part("economy") == "E1", "a locked manifest: generation 1 intact");
        check(WorldSaveStore::commit(b, {{"economy", "E2"}}) && *WorldSaveStore::load(b).part("economy") == "E2",
              "unlocked: the next commit goes through");
    }
#endif

    if (g_failures)
    {
        std::printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("world save contract OK: one generation, one commit, verified load, fallback, legacy read\n");
    return 0;
}
