// Map prebake: the map textures of every generated body (albedo + relief normal as BC1 mip chains,
// small-body shapes) into assets/baked/celestial/map, so the system / Detail / Hub maps load them
// instead of generating (src/world/celestial/surface/MapBodyBake.h). Same bake, same fingerprint as
// the game's live bake: a file goes stale by itself when its body's inputs change.
//
//   build/EliteMapPrebake.exe [--width 1024] [--threads N] [--out DIR] [--system ID ...] [--force]
//                             [--preview DIR]
//   --force    bake even when a file of the same fingerprint exists
//   --preview  also write <DIR>/<body>.png: the decoded BC1 albedo (what the map shows)
// Run from the repository root or build/. Default output: <repo>/assets/baked/celestial/map (the external
// asset tree, not in git, copied into build/assets by every build).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/world/celestial/surface/MapBodyBake.h"
#include "src/world/celestial/surface/TextureBlockCompression.h"
#include "tools/planet_lab/PngWriter.h"

namespace surf = world::celestial::surface;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    int width = 1024;
    int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    std::string outDir, previewDir;
    std::vector<int> onlySystems;
    bool force = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--width")
            width = std::clamp(std::atoi(next().c_str()) / 2 * 2, 128, 4096);
        else if (a == "--threads")
            threads = std::max(1, std::atoi(next().c_str()));
        else if (a == "--out")
            outDir = next();
        else if (a == "--preview")
            previewDir = next();
        else if (a == "--system")
            onlySystems.push_back(std::atoi(next().c_str()));
        else if (a == "--force")
            force = true;
        else
        {
            std::fprintf(stderr,
                         "usage: EliteMapPrebake [--width 1024] [--threads N] [--out DIR] [--system ID ...] "
                         "[--force] [--preview DIR]\n");
            return 2;
        }
    }

    surf::MapBodyCatalog catalog;
    if (catalog.assetRoot().empty())
    {
        std::fprintf(stderr, "no asset root (assets/source/celestial) from %s\n", fs::current_path().string().c_str());
        return 1;
    }
    if (outDir.empty())
    {
        // the project's external asset tree (<repo>/assets, staged into build/assets by every build) is the
        // library's home: it survives a clean build; build/ only as the fallback
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        for (const fs::path& base : {cwd, cwd.parent_path()})
            if (fs::is_regular_file(base / "CMakeLists.txt", ec) && fs::is_directory(base / "assets", ec))
            {
                outDir = surf::mapBodyPrebakeDir(base.lexically_normal().generic_string());
                break;
            }
        if (outDir.empty())
            outDir = surf::mapBodyPrebakeDir(catalog.assetRoot());
    }

    struct Job
    {
        int systemId;
        std::string bodyId;
    };
    std::vector<Job> jobs;
    for (const int sys : catalog.systemIds())
    {
        if (!onlySystems.empty() && std::find(onlySystems.begin(), onlySystems.end(), sys) == onlySystems.end())
            continue;
        for (const std::string& body : catalog.bodyIds(sys))
            jobs.push_back({sys, body});
    }
    std::printf("[MapPrebake] %zu bodies, %dx%d, %d threads -> %s\n", jobs.size(), width, width / 2, threads,
                outDir.c_str());
    std::fflush(stdout);

    std::atomic<std::size_t> next {0};
    std::atomic<int> baked {0}, skipped {0}, none {0}, failed {0};
    std::atomic<std::uint64_t> bytes {0};
    std::mutex print;
    const auto t0 = std::chrono::steady_clock::now();
    const auto seconds = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    const auto worker = [&] {
        for (;;)
        {
            const std::size_t i = next++;
            if (i >= jobs.size())
                return;
            const Job& job = jobs[i];
            const std::uint64_t fp = surf::mapBodyFingerprint(catalog, job.systemId, job.bodyId, width);
            if (fp == 0)
            {
                ++none;
                continue;
            }
            const std::string path = surf::mapBodyPrebakePath(outDir, job.systemId, job.bodyId);
            surf::MapBodyPrebake existing;
            if (!force && surf::readMapBodyPrebake(path, fp, existing))
            {
                // the same inputs AND the same generator (the probe of the live field)
                surf::MapBodyProbe probe;
                std::string probeError;
                if (surf::mapBodyProbe(catalog, job.systemId, job.bodyId, width, probe, probeError) &&
                    surf::mapBodyProbesMatch(probe, existing.probe))
                {
                    ++skipped;
                    continue;
                }
            }
            const auto b0 = std::chrono::steady_clock::now();
            surf::MapBodyBake bake;
            std::string error;
            if (!surf::bakeMapBody(catalog, job.systemId, job.bodyId, width, 1, nullptr, bake, error))
            {
                if (error.empty())
                    ++none;
                else
                {
                    ++failed;
                    std::lock_guard lock(print);
                    std::printf("  FAILED %s: %s\n", job.bodyId.c_str(), error.c_str());
                }
                continue;
            }
            const surf::MapBodyPrebake p = surf::compressMapBodyBake(bake, fp);
            if (!surf::writeMapBodyPrebake(path, p))
            {
                ++failed;
                std::lock_guard lock(print);
                std::printf("  FAILED %s: cannot write %s\n", job.bodyId.c_str(), path.c_str());
                continue;
            }
            std::error_code ec;
            bytes += fs::file_size(path, ec);
            if (!previewDir.empty())
            {
                std::vector<std::uint8_t> rgba(static_cast<std::size_t>(p.width) * p.height * 4);
                surf::decodeBC1(p.albedoMips[0].data(), p.width, p.height, rgba.data());
                // GL rows (south first) as they are: writePng takes bottom-up rows -> north up
                std::vector<unsigned char> rgb(static_cast<std::size_t>(p.width) * p.height * 3);
                for (std::size_t t = 0; t < static_cast<std::size_t>(p.width) * p.height; ++t)
                    for (int k = 0; k < 3; ++k)
                        rgb[t * 3 + k] = rgba[t * 4 + k];
                fs::create_directories(previewDir, ec);
                const std::string name = fs::path(path).stem().string();
                planet_lab::writePng((fs::path(previewDir) / (name + ".png")).string(), p.width, p.height, rgb);
            }
            ++baked;
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b0).count();
            std::lock_guard lock(print);
            std::printf("  [%zu/%zu] %s (%s) %.0f ms\n", i + 1, jobs.size(), job.bodyId.c_str(),
                        bake.description.c_str(), ms);
            std::fflush(stdout);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker);
    for (auto& t : pool)
        t.join();
    std::printf("[MapPrebake] baked %d, up to date %d, no surface %d, failed %d; %.1f MB written in %.1f s\n",
                baked.load(), skipped.load(), none.load(), failed.load(), bytes.load() / 1048576.0, seconds());
    return failed.load() ? 1 : 0;
}
