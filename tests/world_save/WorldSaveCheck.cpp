// Stabilization stage 8: the world save contract (WorldSaveStore) - one generation of all parts, one commit, verified
// load, the previous generation as the fallback, the legacy layout read once.
//   EliteWorldSaveCheck <scratch dir>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "src/game/economy/WorldSaveStore.h"

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

    if (g_failures)
    {
        std::printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("world save contract OK: one generation, one commit, verified load, fallback, legacy read\n");
    return 0;
}
