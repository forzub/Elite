#include "src/game/economy/WorldSaveStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace game::economy
{
// Atomic replace (audit 2026-11-19): the accounts were rewritten in place (trunc) - a crash mid-write left a file the
// next start read as "unreadable, fresh state", silently zeroing every account. Write <file>.tmp, flush, then replace.
thread_local unsigned long g_lastWriteError = 0;   // the last Win32 error of writeFileAtomically (diagnostics)

bool writeFileAtomically(const std::string& path, const std::string& text)
{
    const std::string tmp = path + ".tmp";
#ifdef _WIN32
    // the data reaches the disk (FlushFileBuffers) before the replace: a power loss cannot leave an empty file
    const std::wstring from = std::filesystem::path(tmp).wstring(), to = std::filesystem::path(path).wstring();
    HANDLE h = CreateFileW(from.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE && (g_lastWriteError = GetLastError(), true))
        return false;
    DWORD written = 0;
    const bool ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size() &&
                    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok)
    {
        DeleteFileW(from.c_str());
        return false;
    }
    const bool moved = MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!moved)
        g_lastWriteError = GetLastError();
    return moved;
#else
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;
        f << text;
        f.flush();
        if (!f)
            return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
}

namespace
{
constexpr const char* Format = "hanted_world_save/1";

std::string manifestPath(const std::string& base) { return base + ".manifest.json"; }
std::string partPath(const std::string& base, std::uint64_t g, const std::string& name)
{
    return base + ".g" + std::to_string(g) + "." + name + ".json";
}
bool readAll(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
struct Manifest
{
    bool ok = false;
    std::uint64_t generation = 0;
    struct Entry
    {
        std::string name, file;
        std::uint64_t bytes = 0, hash = 0;
    };
    std::vector<Entry> parts;
    std::uint64_t previous = 0;   // the generation before (the fallback)
    std::vector<Entry> previousParts;
};
Manifest readManifest(const std::string& base)
{
    Manifest m;
    std::string text;
    if (!readAll(manifestPath(base), text))
        return m;
    try
    {
        const auto j = nlohmann::json::parse(text);
        if (j.value("format", std::string()) != Format)
            return m;
        const auto entries = [](const nlohmann::json& parts) {
            std::vector<Manifest::Entry> out;
            for (auto it = parts.begin(); it != parts.end(); ++it)
                out.push_back({it.key(), it->value("file", std::string()), it->value("bytes", std::uint64_t {0}),
                               std::stoull(it->value("fnv1a64", std::string("0")), nullptr, 16)});
            return out;
        };
        m.generation = j.value("generation", std::uint64_t {0});
        m.parts = entries(j.value("parts", nlohmann::json::object()));
        m.previous = j.value("previous_generation", std::uint64_t {0});
        m.previousParts = entries(j.value("previous_parts", nlohmann::json::object()));
        m.ok = m.generation > 0;
    }
    catch (...)
    {
    }
    return m;
}
std::string dirOf(const std::string& base)
{
    const auto slash = base.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : base.substr(0, slash + 1);
}
// one generation's parts, each verified; false when any is missing / damaged
bool loadGeneration(const std::string& base, const std::vector<Manifest::Entry>& entries, std::vector<WorldSavePart>& out,
                    std::string& why)
{
    out.clear();
    for (const auto& e : entries)
    {
        std::string text;
        if (!readAll(dirOf(base) + e.file, text))
        {
            why = "part " + e.name + " missing (" + e.file + ")";
            return false;
        }
        if (text.size() != e.bytes || WorldSaveStore::fnv1a64(text) != e.hash)
        {
            why = "part " + e.name + " damaged (" + e.file + ": " + std::to_string(text.size()) + " B, expected " +
                  std::to_string(e.bytes) + ")";
            return false;
        }
        out.push_back({e.name, std::move(text)});
    }
    return !entries.empty();
}
std::string fileName(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace

std::uint64_t WorldSaveStore::fnv1a64(const std::string& bytes)
{
    std::uint64_t h = 14695981039346656037ull;
    for (const unsigned char c : bytes)
    {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t WorldSaveStore::committedGeneration(const std::string& base)
{
    return readManifest(base).generation;
}

bool WorldSaveStore::commit(const std::string& base, const std::vector<WorldSavePart>& parts, std::string* error)
{
    const Manifest current = readManifest(base);
    const std::uint64_t g = current.generation + 1;
    nlohmann::json j;
    j["format"] = Format;
    j["generation"] = g;
    j["parts"] = nlohmann::json::object();
    for (const auto& p : parts)
    {
        const std::string path = partPath(base, g, p.name);
        if (!writeFileAtomically(path, p.text))
        {
            if (error)
                *error = "part " + p.name + " not written (" + path + ")";
            return false;   // the manifest still names generation g-1: nothing changed
        }
        std::ostringstream hex;
        hex << std::hex << fnv1a64(p.text);
        j["parts"][p.name] = {{"file", fileName(path)}, {"bytes", p.text.size()}, {"fnv1a64", hex.str()}};
    }
    if (current.ok)
    {
        j["previous_generation"] = current.generation;
        j["previous_parts"] = nlohmann::json::object();
        for (const auto& e : current.parts)
        {
            std::ostringstream hex;
            hex << std::hex << e.hash;
            j["previous_parts"][e.name] = {{"file", e.file}, {"bytes", e.bytes}, {"fnv1a64", hex.str()}};
        }
    }
    // THE COMMIT POINT: one atomic rename
    if (!writeFileAtomically(manifestPath(base), j.dump(1)))
    {
        if (error)
            *error = "manifest not written (" + manifestPath(base) + ")";
        return false;
    }
    // generation g-2 is no longer named by the manifest: removed (g-1 stays as the fallback)
    if (current.ok && current.previous > 0)
        for (const auto& e : current.previousParts)
        {
            std::error_code ec;
            std::filesystem::remove(dirOf(base) + e.file, ec);
        }
    return true;
}

WorldSaveLoaded WorldSaveStore::load(const std::string& base)
{
    WorldSaveLoaded out;
    const Manifest m = readManifest(base);
    if (m.ok)
    {
        out.found = true;
        std::string why;
        if (loadGeneration(base, m.parts, out.parts, why))
        {
            out.generation = m.generation;
            return out;
        }
        std::string whyPrev;
        if (m.previous > 0 && loadGeneration(base, m.previousParts, out.parts, whyPrev))
        {
            out.generation = m.previous;
            out.note = "generation " + std::to_string(m.generation) + " unusable (" + why + ") - generation " +
                       std::to_string(m.previous) + " loaded";
            return out;
        }
        out.parts.clear();
        out.note = "no usable generation (" + why + (whyPrev.empty() ? std::string() : "; previous: " + whyPrev) + ")";
        return out;
    }
    // the legacy layout (before the manifest): <base> = the economy, <base>.ships.json = the ships
    std::string economy, ships;
    if (readAll(base, economy))
    {
        out.found = true;
        out.legacy = true;
        out.parts.push_back({"economy", std::move(economy)});
        if (readAll(base + ".ships.json", ships))
            out.parts.push_back({"ships", std::move(ships)});
        out.note = "legacy layout (no manifest) - the next save writes generation 1";
    }
    return out;
}
} // namespace game::economy
