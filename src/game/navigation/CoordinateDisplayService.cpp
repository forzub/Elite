#include "src/game/navigation/CoordinateDisplayService.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace game::navigation
{
namespace
{

constexpr CoordinateDisplayMode StraightThereMode {
    CoordinateDisplayFormat::Hierarchical,
    "hierarchical",
    "STRAIGHT THERE"
};

constexpr CoordinateDisplayMode ThreeAxesMode {
    CoordinateDisplayFormat::Axis,
    "axis",
    "THREE AXES"
};

constexpr CoordinateDisplayMode VerySecretCodeMode {
    CoordinateDisplayFormat::PackedBase32,
    "packed_base32",
    "VERY SECRET CODE"
};

} // namespace

const CoordinateDisplayMode& coordinateDisplayMode(
    CoordinateDisplayFormat format
)
{
    switch (format)
    {
        case CoordinateDisplayFormat::Axis:
            return ThreeAxesMode;

        case CoordinateDisplayFormat::PackedBase32:
            return VerySecretCodeMode;

        case CoordinateDisplayFormat::Hierarchical:
        default:
            return StraightThereMode;
    }
}

CoordinateDisplayFormat coordinateDisplayFormatFromString(
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

    if (value == "hierarchical" ||
        value == "straight_there" ||
        value == "straight there")
    {
        return CoordinateDisplayFormat::Hierarchical;
    }

    if (value == "axis" ||
        value == "three_axes" ||
        value == "three axes")
    {
        return CoordinateDisplayFormat::Axis;
    }

    if (value == "packed" ||
        value == "packed_base32" ||
        value == "base32" ||
        value == "very_secret_code" ||
        value == "very secret code")
    {
        return CoordinateDisplayFormat::PackedBase32;
    }

    return CoordinateDisplayFormat::Hierarchical;
}

CoordinateDisplayFormat nextCoordinateDisplayFormat(
    CoordinateDisplayFormat format
)
{
    switch (format)
    {
        case CoordinateDisplayFormat::Hierarchical:
            return CoordinateDisplayFormat::Axis;

        case CoordinateDisplayFormat::Axis:
            return CoordinateDisplayFormat::PackedBase32;

        case CoordinateDisplayFormat::PackedBase32:
            return CoordinateDisplayFormat::Hierarchical;
    }

    return CoordinateDisplayFormat::Hierarchical;
}

std::string formatCoordinateDisplayLine(
    CoordinateDisplayFormat format,
    std::string coordinateText
)
{
    std::string result;
    const char* name =
        coordinateDisplayMode(format).displayName;

    result.reserve(
        std::char_traits<char>::length(name) +
        coordinateText.size() +
        4
    );

    result += '[';
    result += name;
    result += "] ";
    result += coordinateText;

    return result;
}

CoordinateDisplayService& CoordinateDisplayService::instance()
{
    static CoordinateDisplayService service;
    return service;
}

CoordinateDisplayFormat
CoordinateDisplayService::format() const noexcept
{
    return m_format;
}

const CoordinateDisplayMode&
CoordinateDisplayService::mode() const noexcept
{
    return coordinateDisplayMode(m_format);
}

const char*
CoordinateDisplayService::formatName() const noexcept
{
    return mode().displayName;
}

void CoordinateDisplayService::setFormat(
    CoordinateDisplayFormat format
) noexcept
{
    m_format = format;
}

void CoordinateDisplayService::cycle() noexcept
{
    m_format =
        nextCoordinateDisplayFormat(
            m_format
        );
}

std::string CoordinateDisplayService::formatLine(
    std::string coordinateText
) const
{
    return formatCoordinateDisplayLine(
        m_format,
        std::move(coordinateText)
    );
}

std::string formatCurrentCoordinateDisplayLine(
    std::string coordinateText
)
{
    return CoordinateDisplayService::instance().formatLine(
        std::move(coordinateText)
    );
}

} // namespace game::navigation
