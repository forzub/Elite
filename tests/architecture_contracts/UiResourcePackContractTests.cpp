#include "src/ui/platform/UiResourcePack.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct FixtureEntry
{
    std::string resource;
    std::string mime;
    std::string payload;
};

template <typename T>
void writeLe(std::ofstream& out, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i)
    {
        const unsigned char byte =
            static_cast<unsigned char>((value >> (8U * i)) & 0xffU);
        out.put(static_cast<char>(byte));
    }
}

bool writePack(
    const std::filesystem::path& path,
    const std::vector<FixtureEntry>& entries,
    bool validMagic = true
)
{
    constexpr std::array<char, 8> goodMagic {{'E','L','I','T','E','U','I','1'}};
    constexpr std::array<char, 8> badMagic  {{'B','A','D','P','A','C','K','!'}};

    std::uint64_t indexSize = 0;
    for (const FixtureEntry& entry : entries)
    {
        indexSize += 2U + 2U + 8U + 8U +
            entry.resource.size() + entry.mime.size();
    }
    std::uint64_t cursor = 8U + 4U + 4U + indexSize;

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
        return false;

    const auto& magic = validMagic ? goodMagic : badMagic;
    out.write(magic.data(), magic.size());
    writeLe<std::uint32_t>(out, 1U);
    writeLe<std::uint32_t>(out, static_cast<std::uint32_t>(entries.size()));

    for (const FixtureEntry& entry : entries)
    {
        writeLe<std::uint16_t>(out, static_cast<std::uint16_t>(entry.resource.size()));
        writeLe<std::uint16_t>(out, static_cast<std::uint16_t>(entry.mime.size()));
        writeLe<std::uint64_t>(out, cursor);
        writeLe<std::uint64_t>(out, entry.payload.size());
        out.write(entry.resource.data(), entry.resource.size());
        out.write(entry.mime.data(), entry.mime.size());
        cursor += entry.payload.size();
    }

    for (const FixtureEntry& entry : entries)
        out.write(entry.payload.data(), entry.payload.size());

    return static_cast<bool>(out);
}
}

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "elite_ui_resource_pack_contract";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error)
    {
        std::cerr << "[FAIL] cannot create UI resource-pack fixture directory\n";
        return 1;
    }

    const std::filesystem::path validPath = root / "elite_ui.pak";
    const std::vector<FixtureEntry> entries {
        {"/main.html", "text/html; charset=utf-8", "<html>Elite</html>"},
        {"/ui/fonts/test.ttf", "font/ttf", std::string("abc\0xyz", 7)}
    };
    if (!writePack(validPath, entries))
    {
        std::cerr << "[FAIL] cannot write UI resource-pack fixture\n";
        return 1;
    }

    ui::platform::UiResourcePack pack;
    std::string loadError;
    if (!pack.load(validPath.string(), &loadError))
    {
        std::cerr << "[FAIL] valid UI resource pack rejected: " << loadError << '\n';
        return 1;
    }
    if (!pack.contains("/main.html") || pack.contains("/missing"))
    {
        std::cerr << "[FAIL] UI resource-pack lookup contract violated\n";
        return 1;
    }

    std::string body;
    std::string mime;
    if (!pack.read("/main.html", body, mime) ||
        body != "<html>Elite</html>" || mime != "text/html; charset=utf-8")
    {
        std::cerr << "[FAIL] text resource did not round-trip through UI pack\n";
        return 1;
    }
    if (!pack.read("/ui/fonts/test.ttf", body, mime) ||
        body != std::string("abc\0xyz", 7) || mime != "font/ttf")
    {
        std::cerr << "[FAIL] binary resource did not round-trip through UI pack\n";
        return 1;
    }

    const std::filesystem::path badPath = root / "bad.pak";
    if (!writePack(badPath, entries, false))
        return 1;
    if (pack.load(badPath.string(), &loadError))
    {
        std::cerr << "[FAIL] invalid UI resource-pack magic was accepted\n";
        return 1;
    }

    std::filesystem::remove_all(root, error);
    std::cout << "[PASS] binary UI resource-pack index, MIME and payload round-trip\n";
    return 0;
}
