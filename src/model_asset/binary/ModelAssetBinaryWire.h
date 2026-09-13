#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary
{

constexpr std::uint32_t MaxCollectionCount = 50'000'000u;
constexpr std::uint32_t MaxStringBytes = 16u * 1024u * 1024u;

inline void setError(std::string* error, const std::string& value)
{
    if (error) *error = value;
}

struct Writer
{
    std::ostream& out;
    bool ok = true;

    template <typename T>
    void pod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
        ok = ok && static_cast<bool>(out);
    }

    void string(const std::string& value)
    {
        const auto size = static_cast<std::uint32_t>(value.size());
        pod(size);
        if (size)
            out.write(value.data(), static_cast<std::streamsize>(size));
        ok = ok && static_cast<bool>(out);
    }

    void vec2(const glm::vec2& v) { pod(v.x); pod(v.y); }
    void vec3(const glm::vec3& v) { pod(v.x); pod(v.y); pod(v.z); }
    void vec4(const glm::vec4& v) { pod(v.x); pod(v.y); pod(v.z); pod(v.w); }
};

struct Reader
{
    std::istream* in = nullptr;
    const std::uint8_t* cursor = nullptr;
    const std::uint8_t* end = nullptr;
    bool ok = true;

    explicit Reader(std::istream& stream) : in(&stream) {}
    Reader(const std::uint8_t* data, std::size_t size) : cursor(data), end(data + size) {}

    void bytes(void* destination, std::size_t size)
    {
        if (!ok) return;
        if (cursor)
        {
            const auto remaining = static_cast<std::size_t>(end - cursor);
            if (size > remaining)
            {
                ok = false;
                return;
            }
            if (size) std::memcpy(destination, cursor, size);
            cursor += size;
            return;
        }
        in->read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
        ok = static_cast<bool>(*in);
    }

    template <typename T>
    void pod(T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&value, sizeof(T));
    }

    bool count(std::uint32_t& value)
    {
        pod(value);
        if (!ok || value > MaxCollectionCount)
        {
            ok = false;
            return false;
        }
        return true;
    }

    void string(std::string& value)
    {
        std::uint32_t size = 0;
        pod(size);
        if (!ok || size > MaxStringBytes)
        {
            ok = false;
            return;
        }
        value.resize(size);
        if (size) bytes(value.data(), size);
    }

    void vec2(glm::vec2& v) { pod(v.x); pod(v.y); }
    void vec3(glm::vec3& v) { pod(v.x); pod(v.y); pod(v.z); }
    void vec4(glm::vec4& v) { pod(v.x); pod(v.y); pod(v.z); pod(v.w); }

    std::size_t remaining()
    {
        if (cursor) return static_cast<std::size_t>(end - cursor);
        if (!in) return 0;
        const auto current = in->tellg();
        if (current == std::streampos(-1)) return 0;
        in->seekg(0, std::ios::end);
        const auto finish = in->tellg();
        in->seekg(current);
        if (finish == std::streampos(-1) || finish < current) return 0;
        return static_cast<std::size_t>(finish - current);
    }
};

inline void writeStrings(Writer& w, const std::vector<std::string>& values)
{
    w.pod(static_cast<std::uint32_t>(values.size()));
    for (const auto& value : values) w.string(value);
}

inline void readStrings(Reader& r, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    values.resize(count);
    for (auto& value : values) r.string(value);
}

} // namespace elite::model_asset::binary
