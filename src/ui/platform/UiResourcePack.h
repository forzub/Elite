#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ui::platform
{
class UiResourcePack
{
public:
    bool load(const std::string& path, std::string* error = nullptr);
    void clear();

    bool loaded() const { return !m_path.empty(); }
    bool contains(const std::string& resource) const;

    bool read(
        const std::string& resource,
        std::string& content,
        std::string& contentType
    ) const;

private:
    struct Entry
    {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::string contentType;
    };

    std::string m_path;
    std::unordered_map<std::string, Entry> m_entries;
};
}
