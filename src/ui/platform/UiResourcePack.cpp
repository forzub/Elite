#include "src/ui/platform/UiResourcePack.h"

#include <array>
#include <fstream>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui::platform
{
namespace
{
constexpr std::array<char, 8> kMagic {{'E', 'L', 'I', 'T', 'E', 'U', 'I', '1'}};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaxEntries = 100000;
constexpr std::uint16_t kMaxResourceLength = 4096;
constexpr std::uint16_t kMaxMimeLength = 512;

template <typename T>
bool readLittleEndian(std::ifstream& input, T& value)
{
    static_assert(std::is_unsigned<T>::value, "unsigned integer required");

    std::array<unsigned char, sizeof(T)> bytes {};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input)
        return false;

    value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value |= static_cast<T>(bytes[i]) << (8U * i);
    return true;
}

bool fail(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
    return false;
}
}

bool UiResourcePack::load(const std::string& path, std::string* error)
{
    clear();

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
        return fail(error, "cannot open resource pack");

    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end < static_cast<std::streamoff>(kMagic.size() + sizeof(std::uint32_t) * 2U))
        return fail(error, "resource pack is too small");
    const std::uint64_t fileSize = static_cast<std::uint64_t>(end);
    input.seekg(0, std::ios::beg);

    std::array<char, 8> magic {};
    input.read(magic.data(), magic.size());
    if (!input || magic != kMagic)
        return fail(error, "invalid resource-pack magic");

    std::uint32_t version = 0;
    std::uint32_t entryCount = 0;
    if (!readLittleEndian(input, version) || !readLittleEndian(input, entryCount))
        return fail(error, "truncated resource-pack header");
    if (version != kVersion)
        return fail(error, "unsupported resource-pack version");
    if (entryCount > kMaxEntries)
        return fail(error, "resource-pack entry count exceeds safety limit");

    std::unordered_map<std::string, Entry> entries;
    entries.reserve(entryCount);

    for (std::uint32_t index = 0; index < entryCount; ++index)
    {
        std::uint16_t resourceLength = 0;
        std::uint16_t mimeLength = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        if (!readLittleEndian(input, resourceLength) ||
            !readLittleEndian(input, mimeLength) ||
            !readLittleEndian(input, offset) ||
            !readLittleEndian(input, size))
        {
            return fail(error, "truncated resource-pack index");
        }

        if (resourceLength == 0 || resourceLength > kMaxResourceLength ||
            mimeLength == 0 || mimeLength > kMaxMimeLength)
        {
            return fail(error, "invalid resource-pack index string length");
        }

        std::string resource(resourceLength, '\0');
        std::string mime(mimeLength, '\0');
        input.read(resource.data(), resource.size());
        input.read(mime.data(), mime.size());
        if (!input)
            return fail(error, "truncated resource-pack index strings");

        if (resource.front() != '/' || resource.find('\\') != std::string::npos)
            return fail(error, "invalid resource path in resource pack");
        if (offset > fileSize || size > fileSize - offset)
            return fail(error, "resource payload exceeds pack bounds");

        Entry entry;
        entry.offset = offset;
        entry.size = size;
        entry.contentType = std::move(mime);
        if (!entries.emplace(std::move(resource), std::move(entry)).second)
            return fail(error, "duplicate resource in resource pack");
    }

    const std::streamoff indexEndPosition = input.tellg();
    if (indexEndPosition < 0)
        return fail(error, "invalid resource-pack index position");
    const std::uint64_t indexEnd = static_cast<std::uint64_t>(indexEndPosition);

    for (const auto& [resource, entry] : entries)
    {
        (void)resource;
        if (entry.offset < indexEnd)
            return fail(error, "resource payload overlaps resource-pack index");
    }

    m_path = path;
    m_entries = std::move(entries);
    if (error)
        error->clear();
    return true;
}

void UiResourcePack::clear()
{
    m_path.clear();
    m_entries.clear();
}

bool UiResourcePack::contains(const std::string& resource) const
{
    return m_entries.find(resource) != m_entries.end();
}

bool UiResourcePack::read(
    const std::string& resource,
    std::string& content,
    std::string& contentType
) const
{
    const auto it = m_entries.find(resource);
    if (it == m_entries.end() || m_path.empty())
        return false;

    const Entry& entry = it->second;
    if (entry.size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return false;

    std::ifstream input(m_path, std::ios::binary);
    if (!input.is_open())
        return false;

    input.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!input)
        return false;

    content.assign(static_cast<std::size_t>(entry.size), '\0');
    if (!content.empty())
        input.read(content.data(), static_cast<std::streamsize>(content.size()));
    if (!input && !content.empty())
    {
        content.clear();
        return false;
    }

    contentType = entry.contentType;
    return true;
}
}
