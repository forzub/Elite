#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/network/WireProtocol.h"

namespace game::network::wire::binary
{

/*
    Generic binaryizer for the wire data plane.

    This layer deliberately knows nothing about ships, maps, modules or the
    number of entities in a snapshot. Game-specific field order lives only in
    WireDataSchema.h through WireSchema<T>::fields().
*/
inline constexpr std::uint32_t MaxWireContainerElements = 1000000u;

template<typename T>
struct WireSchema;

template<typename E>
struct WireEnumRange;

template<typename T, typename = void>
struct HasWireSchema : std::false_type {};

template<typename T>
struct HasWireSchema<
    T,
    std::void_t<decltype(WireSchema<T>::fields(std::declval<T&>()))>
> : std::true_type {};

template<typename E, typename = void>
struct HasWireEnumRange : std::false_type {};

template<typename E>
struct HasWireEnumRange<
    E,
    std::void_t<
        decltype(WireEnumRange<E>::minValue),
        decltype(WireEnumRange<E>::maxValue)
    >
> : std::true_type {};

template<typename T>
struct IsVector : std::false_type {};

template<typename T, typename Alloc>
struct IsVector<std::vector<T, Alloc>> : std::true_type {};

template<typename T>
struct IsVariant : std::false_type {};

template<typename... Ts>
struct IsVariant<std::variant<Ts...>> : std::true_type {};

template<typename>
struct AlwaysFalse : std::false_type {};

template<typename T>
bool encodeValue(WireWriter& writer, const T& value);

template<typename T>
bool decodeValue(WireReader& reader, T& value);

namespace detail
{

template<typename T>
bool encodeInteger(WireWriter& writer, T value)
{
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);

    if constexpr (std::is_signed_v<T>)
    {
        if constexpr (sizeof(T) <= sizeof(std::int32_t))
            writer.i32(static_cast<std::int32_t>(value));
        else if constexpr (sizeof(T) <= sizeof(std::int64_t))
            writer.i64(static_cast<std::int64_t>(value));
        else
            static_assert(sizeof(T) <= sizeof(std::int64_t), "unsupported signed wire integer");
    }
    else
    {
        if constexpr (sizeof(T) == 1u)
            writer.u8(static_cast<std::uint8_t>(value));
        else if constexpr (sizeof(T) == 2u)
            writer.u16(static_cast<std::uint16_t>(value));
        else if constexpr (sizeof(T) == 4u)
            writer.u32(static_cast<std::uint32_t>(value));
        else if constexpr (sizeof(T) == 8u)
            writer.u64(static_cast<std::uint64_t>(value));
        else
            static_assert(sizeof(T) <= sizeof(std::uint64_t), "unsupported unsigned wire integer");
    }

    return true;
}

template<typename T>
bool decodeInteger(WireReader& reader, T& value)
{
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);

    if constexpr (std::is_signed_v<T>)
    {
        if constexpr (sizeof(T) <= sizeof(std::int32_t))
        {
            std::int32_t raw = 0;
            if (!reader.i32(raw) ||
                static_cast<std::int64_t>(raw) <
                    static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
                static_cast<std::int64_t>(raw) >
                    static_cast<std::int64_t>(std::numeric_limits<T>::max()))
            {
                return false;
            }
            value = static_cast<T>(raw);
        }
        else if constexpr (sizeof(T) <= sizeof(std::int64_t))
        {
            std::int64_t raw = 0;
            if (!reader.i64(raw))
                return false;
            value = static_cast<T>(raw);
        }
        else
        {
            static_assert(sizeof(T) <= sizeof(std::int64_t), "unsupported signed wire integer");
        }
    }
    else
    {
        if constexpr (sizeof(T) == 1u)
        {
            std::uint8_t raw = 0;
            if (!reader.u8(raw))
                return false;
            value = static_cast<T>(raw);
        }
        else if constexpr (sizeof(T) == 2u)
        {
            std::uint16_t raw = 0;
            if (!reader.u16(raw))
                return false;
            value = static_cast<T>(raw);
        }
        else if constexpr (sizeof(T) == 4u)
        {
            std::uint32_t raw = 0;
            if (!reader.u32(raw))
                return false;
            value = static_cast<T>(raw);
        }
        else if constexpr (sizeof(T) == 8u)
        {
            std::uint64_t raw = 0;
            if (!reader.u64(raw))
                return false;
            value = static_cast<T>(raw);
        }
        else
        {
            static_assert(sizeof(T) <= sizeof(std::uint64_t), "unsupported unsigned wire integer");
        }
    }

    return true;
}

template<typename Tuple, std::size_t... I>
bool encodeTuple(
    WireWriter& writer,
    const Tuple& tuple,
    std::index_sequence<I...>)
{
    return (encodeValue(writer, std::get<I>(tuple)) && ...);
}

template<typename Tuple, std::size_t... I>
bool decodeTuple(
    WireReader& reader,
    Tuple&& tuple,
    std::index_sequence<I...>)
{
    return (decodeValue(reader, std::get<I>(tuple)) && ...);
}

template<std::size_t I = 0, typename... Ts>
bool decodeVariantAlternative(
    WireReader& reader,
    std::uint8_t tag,
    std::variant<Ts...>& outValue)
{
    if constexpr (I >= sizeof...(Ts))
    {
        (void)reader;
        (void)tag;
        (void)outValue;
        return false;
    }
    else
    {
        if (tag == I)
        {
            using Alternative = std::variant_alternative_t<I, std::variant<Ts...>>;
            Alternative decoded {};
            if (!decodeValue(reader, decoded))
                return false;
            outValue = std::move(decoded);
            return true;
        }

        return decodeVariantAlternative<I + 1u>(reader, tag, outValue);
    }
}

} // namespace detail

template<typename T>
bool encodeValue(WireWriter& writer, const T& value)
{
    using U = std::decay_t<T>;

    if constexpr (std::is_same_v<U, bool>)
    {
        writer.boolean(value);
        return true;
    }
    else if constexpr (std::is_integral_v<U>)
    {
        return detail::encodeInteger(writer, value);
    }
    else if constexpr (std::is_same_v<U, float>)
    {
        writer.f32(value);
        return true;
    }
    else if constexpr (std::is_same_v<U, double>)
    {
        writer.f64(value);
        return true;
    }
    else if constexpr (std::is_same_v<U, std::string>)
    {
        return writer.string(value);
    }
    else if constexpr (std::is_same_v<U, glm::vec2>)
    {
        return encodeValue(writer, value.x) &&
            encodeValue(writer, value.y);
    }
    else if constexpr (std::is_same_v<U, glm::vec3>)
    {
        return encodeValue(writer, value.x) &&
            encodeValue(writer, value.y) &&
            encodeValue(writer, value.z);
    }
    else if constexpr (std::is_same_v<U, glm::vec4>)
    {
        return encodeValue(writer, value.x) &&
            encodeValue(writer, value.y) &&
            encodeValue(writer, value.z) &&
            encodeValue(writer, value.w);
    }
    else if constexpr (std::is_same_v<U, glm::dvec3>)
    {
        return encodeValue(writer, value.x) &&
            encodeValue(writer, value.y) &&
            encodeValue(writer, value.z);
    }
    else if constexpr (std::is_same_v<U, glm::mat3>)
    {
        for (int column = 0; column < 3; ++column)
            for (int row = 0; row < 3; ++row)
                if (!encodeValue(writer, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_same_v<U, glm::mat4>)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!encodeValue(writer, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_same_v<U, glm::dmat3>)
    {
        for (int column = 0; column < 3; ++column)
            for (int row = 0; row < 3; ++row)
                if (!encodeValue(writer, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_enum_v<U>)
    {
        static_assert(
            HasWireEnumRange<U>::value,
            "Register every wire enum in WireDataSchema.h so invalid network values are rejected"
        );

        using Underlying = std::underlying_type_t<U>;
        const auto raw = static_cast<Underlying>(value);
        const auto minRaw = static_cast<Underlying>(WireEnumRange<U>::minValue);
        const auto maxRaw = static_cast<Underlying>(WireEnumRange<U>::maxValue);
        if (raw < minRaw || raw > maxRaw)
            return false;
        return detail::encodeInteger(writer, raw);
    }
    else if constexpr (IsVector<U>::value)
    {
        if (value.size() > MaxWireContainerElements ||
            value.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        writer.u32(static_cast<std::uint32_t>(value.size()));
        for (const auto& element : value)
        {
            if (!encodeValue(writer, element))
                return false;
        }
        return true;
    }
    else if constexpr (IsVariant<U>::value)
    {
        if (value.index() > std::numeric_limits<std::uint8_t>::max())
            return false;

        writer.u8(static_cast<std::uint8_t>(value.index()));
        return std::visit(
            [&](const auto& alternative)
            {
                return encodeValue(writer, alternative);
            },
            value
        );
    }
    else if constexpr (HasWireSchema<U>::value)
    {
        const auto fields = WireSchema<U>::fields(value);
        constexpr std::size_t fieldCount =
            std::tuple_size_v<std::remove_reference_t<decltype(fields)>>;
        return detail::encodeTuple(
            writer,
            fields,
            std::make_index_sequence<fieldCount> {}
        );
    }
    else
    {
        static_assert(AlwaysFalse<U>::value, "No binary wire codec/schema for this type");
    }
}

template<typename T>
bool decodeValue(WireReader& reader, T& value)
{
    using U = std::decay_t<T>;

    if constexpr (std::is_same_v<U, bool>)
    {
        return reader.boolean(value);
    }
    else if constexpr (std::is_integral_v<U>)
    {
        return detail::decodeInteger(reader, value);
    }
    else if constexpr (std::is_same_v<U, float>)
    {
        return reader.f32(value);
    }
    else if constexpr (std::is_same_v<U, double>)
    {
        return reader.f64(value);
    }
    else if constexpr (std::is_same_v<U, std::string>)
    {
        return reader.string(value);
    }
    else if constexpr (std::is_same_v<U, glm::vec2>)
    {
        return decodeValue(reader, value.x) &&
            decodeValue(reader, value.y);
    }
    else if constexpr (std::is_same_v<U, glm::vec3>)
    {
        return decodeValue(reader, value.x) &&
            decodeValue(reader, value.y) &&
            decodeValue(reader, value.z);
    }
    else if constexpr (std::is_same_v<U, glm::vec4>)
    {
        return decodeValue(reader, value.x) &&
            decodeValue(reader, value.y) &&
            decodeValue(reader, value.z) &&
            decodeValue(reader, value.w);
    }
    else if constexpr (std::is_same_v<U, glm::dvec3>)
    {
        return decodeValue(reader, value.x) &&
            decodeValue(reader, value.y) &&
            decodeValue(reader, value.z);
    }
    else if constexpr (std::is_same_v<U, glm::mat3>)
    {
        for (int column = 0; column < 3; ++column)
            for (int row = 0; row < 3; ++row)
                if (!decodeValue(reader, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_same_v<U, glm::mat4>)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!decodeValue(reader, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_same_v<U, glm::dmat3>)
    {
        for (int column = 0; column < 3; ++column)
            for (int row = 0; row < 3; ++row)
                if (!decodeValue(reader, value[column][row]))
                    return false;
        return true;
    }
    else if constexpr (std::is_enum_v<U>)
    {
        static_assert(
            HasWireEnumRange<U>::value,
            "Register every wire enum in WireDataSchema.h so invalid network values are rejected"
        );

        using Underlying = std::underlying_type_t<U>;
        Underlying raw {};
        if (!detail::decodeInteger(reader, raw))
            return false;

        const auto minRaw = static_cast<Underlying>(WireEnumRange<U>::minValue);
        const auto maxRaw = static_cast<Underlying>(WireEnumRange<U>::maxValue);
        if (raw < minRaw || raw > maxRaw)
            return false;

        value = static_cast<U>(raw);
        return true;
    }
    else if constexpr (IsVector<U>::value)
    {
        std::uint32_t count = 0;
        if (!reader.u32(count) || count > MaxWireContainerElements)
            return false;

        value.clear();
        // Do not reserve an untrusted network count up front. The payload limit
        // and per-element decoding bound actual allocation as bytes are read.
        for (std::uint32_t i = 0; i < count; ++i)
        {
            typename U::value_type element {};
            if (!decodeValue(reader, element))
                return false;
            value.push_back(std::move(element));
        }
        return true;
    }
    else if constexpr (IsVariant<U>::value)
    {
        static_assert(std::variant_size_v<U> <= 256u, "wire variant tag is one byte");
        std::uint8_t tag = 0;
        if (!reader.u8(tag) || tag >= std::variant_size_v<U>)
            return false;
        return detail::decodeVariantAlternative(reader, tag, value);
    }
    else if constexpr (HasWireSchema<U>::value)
    {
        auto fields = WireSchema<U>::fields(value);
        constexpr std::size_t fieldCount =
            std::tuple_size_v<std::remove_reference_t<decltype(fields)>>;
        return detail::decodeTuple(
            reader,
            fields,
            std::make_index_sequence<fieldCount> {}
        );
    }
    else
    {
        static_assert(AlwaysFalse<U>::value, "No binary wire codec/schema for this type");
    }
}

} // namespace game::network::wire::binary
