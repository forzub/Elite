#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "src/game/navigation/CubicNavigationGrid.h"

namespace game::navigation
{

enum class NavigationCoordinateFormat
{
    Hierarchical,
    Axis,
    PackedBase32
};

inline NavigationCoordinateFormat navigationCoordinateFormatFromString(
    std::string value
)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    if (value == "axis")
        return NavigationCoordinateFormat::Axis;

    if (value == "packed" ||
        value == "packed_base32" ||
        value == "base32")
    {
        return NavigationCoordinateFormat::PackedBase32;
    }

    return NavigationCoordinateFormat::Hierarchical;
}

inline const char* navigationCoordinateFormatName(
    NavigationCoordinateFormat format
)
{
    switch (format)
    {
        case NavigationCoordinateFormat::Hierarchical:
            return "PATH";
        case NavigationCoordinateFormat::Axis:
            return "AXIS";
        case NavigationCoordinateFormat::PackedBase32:
            return "PACKED";
    }

    return "PATH";
}

inline NavigationCoordinateFormat nextNavigationCoordinateFormat(
    NavigationCoordinateFormat format
)
{
    switch (format)
    {
        case NavigationCoordinateFormat::Hierarchical:
            return NavigationCoordinateFormat::Axis;
        case NavigationCoordinateFormat::Axis:
            return NavigationCoordinateFormat::PackedBase32;
        case NavigationCoordinateFormat::PackedBase32:
            return NavigationCoordinateFormat::Hierarchical;
    }

    return NavigationCoordinateFormat::Hierarchical;
}

template <typename Index>
struct HierarchicalAddressParts
{
    struct Token
    {
        int x = 0;
        int y = 0;
        int z = 0;
    };

    Index root {};
    std::vector<Token> tokens;
};

template <typename Index>
inline HierarchicalAddressParts<Index> splitHierarchicalAddress(
    int level,
    const Index& index,
    int subdivision
)
{
    HierarchicalAddressParts<Index> result;
    std::vector<typename HierarchicalAddressParts<Index>::Token> reversed;

    Index child = index;
    const int centerDigit = subdivision / 2;

    for (int currentLevel = level; currentLevel > 0; --currentLevel)
    {
        Index parent;
        parent.x = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.x) / subdivision
        ));
        parent.y = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.y) / subdivision
        ));
        parent.z = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.z) / subdivision
        ));

        reversed.push_back({
            static_cast<int>(child.x - parent.x * subdivision) + centerDigit,
            static_cast<int>(child.y - parent.y * subdivision) + centerDigit,
            static_cast<int>(child.z - parent.z * subdivision) + centerDigit
        });

        child = parent;
    }

    result.root = child;
    result.tokens.assign(reversed.rbegin(), reversed.rend());
    return result;
}

inline std::string formatCellCoordinate(
    const std::string& mapPrefix,
    const CubicNavigationCell& cell
)
{
    std::ostringstream stream;

    stream << mapPrefix << cell.level
           << " · X" << std::showpos << cell.index.x
           << " Y" << std::showpos << cell.index.y
           << " Z" << std::showpos << cell.index.z;

    return stream.str();
}

template <typename Index>
inline std::string formatCellCoordinate(
    const std::string& mapPrefix,
    int level,
    const Index& index
)
{
    std::ostringstream stream;

    stream << mapPrefix << level
           << " · X" << std::showpos << index.x
           << " Y" << std::showpos << index.y
           << " Z" << std::showpos << index.z;

    return stream.str();
}

template <typename Index>
inline std::string formatHierarchicalAddress(
    const std::string& mapPrefix,
    int level,
    const Index& index,
    int subdivision
)
{
    const auto parts = splitHierarchicalAddress(
        level, index, subdivision
    );

    int digitWidth = 1;
    for (int value = subdivision - 1; value >= 10; value /= 10)
        ++digitWidth;

    std::ostringstream stream;
    stream << mapPrefix << '['
           << parts.root.x << ',' << parts.root.y << ',' << parts.root.z << ']';

    for (const auto& token : parts.tokens)
    {
        stream << '/'
               << std::setfill('0') << std::setw(digitWidth) << token.x << '.'
               << std::setfill('0') << std::setw(digitWidth) << token.y << '.'
               << std::setfill('0') << std::setw(digitWidth) << token.z;
    }

    return stream.str();
}

template <typename Index>
inline std::string formatAxisAddress(
    const std::string& mapPrefix,
    int level,
    const Index& index,
    int subdivision
)
{
    const auto parts = splitHierarchicalAddress(
        level, index, subdivision
    );

    int digitWidth = 1;
    for (int value = subdivision - 1; value >= 10; value /= 10)
        ++digitWidth;

    std::ostringstream stream;
    stream << mapPrefix << '['
           << parts.root.x << ',' << parts.root.y << ',' << parts.root.z << ']';

    const auto appendAxis = [&](char axis, auto member)
    {
        stream << "  " << axis;

        for (std::size_t i = 0; i < parts.tokens.size(); ++i)
        {
            if (i != 0)
                stream << "\xC2\xB7";

            stream << std::setfill('0') << std::setw(digitWidth)
                   << parts.tokens[i].*member;
        }
    };

    using Token = typename HierarchicalAddressParts<Index>::Token;
    appendAxis('X', &Token::x);
    appendAxis('Y', &Token::y);
    appendAxis('Z', &Token::z);

    return stream.str();
}

inline std::string encodeFixedCrockfordBase32(
    std::uint32_t value,
    int width
)
{
    static constexpr char Alphabet[] =
        "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

    std::string result(static_cast<std::size_t>(width), '0');

    for (int i = width - 1; i >= 0; --i)
    {
        result[static_cast<std::size_t>(i)] = Alphabet[value & 31u];
        value >>= 5u;
    }

    return result;
}

template <typename Index>
inline std::string formatPackedAddress(
    const std::string& mapPrefix,
    int level,
    const Index& index,
    int subdivision
)
{
    const auto parts = splitHierarchicalAddress(
        level, index, subdivision
    );

    const std::uint64_t cellCount =
        static_cast<std::uint64_t>(subdivision) *
        static_cast<std::uint64_t>(subdivision) *
        static_cast<std::uint64_t>(subdivision);

    int tokenWidth = 1;
    for (std::uint64_t capacity = 32;
         capacity < cellCount;
         capacity *= 32)
    {
        ++tokenWidth;
    }

    std::ostringstream stream;
    stream << mapPrefix << '['
           << parts.root.x << ',' << parts.root.y << ',' << parts.root.z << "]:";

    for (const auto& token : parts.tokens)
    {
        const std::uint32_t packed = static_cast<std::uint32_t>(
            (token.x * subdivision + token.y) * subdivision + token.z
        );

        stream << encodeFixedCrockfordBase32(packed, tokenWidth);
    }

    return stream.str();
}

template <typename Index>
inline std::string formatNavigationAddress(
    NavigationCoordinateFormat format,
    const std::string& mapPrefix,
    int level,
    const Index& index,
    int subdivision
)
{
    switch (format)
    {
        case NavigationCoordinateFormat::Axis:
            return formatAxisAddress(
                mapPrefix, level, index, subdivision
            );

        case NavigationCoordinateFormat::PackedBase32:
            return formatPackedAddress(
                mapPrefix, level, index, subdivision
            );

        case NavigationCoordinateFormat::Hierarchical:
        default:
            return formatHierarchicalAddress(
                mapPrefix, level, index, subdivision
            );
    }
}

inline glm::dvec3 terminalOffsetKmFromAu(
    const glm::dvec3& pointAu,
    const CubicNavigationCell& terminalCell,
    double auKm = 149597870.7
)
{
    return (pointAu - terminalCell.center) * auKm;
}

inline std::string formatTerminalOffsetKm(
    const glm::dvec3& offsetKm,
    int precision = 3
)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision)
           << "dX=" << offsetKm.x << " km "
           << "dY=" << offsetKm.y << " km "
           << "dZ=" << offsetKm.z << " km";
    return stream.str();
}

} // namespace game::navigation
