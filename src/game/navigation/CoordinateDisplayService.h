#pragma once

#include <string>

namespace game::navigation
{

enum class CoordinateDisplayFormat
{
    Hierarchical,
    Axis,
    PackedBase32
};

struct CoordinateDisplayMode
{
    CoordinateDisplayFormat format =
        CoordinateDisplayFormat::Hierarchical;

    const char* id = "hierarchical";
    const char* displayName = "STRAIGHT THERE";
};

const CoordinateDisplayMode& coordinateDisplayMode(
    CoordinateDisplayFormat format
);

CoordinateDisplayFormat coordinateDisplayFormatFromString(
    std::string value
);

CoordinateDisplayFormat nextCoordinateDisplayFormat(
    CoordinateDisplayFormat format
);

std::string formatCoordinateDisplayLine(
    CoordinateDisplayFormat format,
    std::string coordinateText
);

class CoordinateDisplayService
{
public:
    static CoordinateDisplayService& instance();

    CoordinateDisplayFormat format() const noexcept;
    const CoordinateDisplayMode& mode() const noexcept;
    const char* formatName() const noexcept;

    void setFormat(CoordinateDisplayFormat format) noexcept;
    void cycle() noexcept;

    std::string formatLine(std::string coordinateText) const;

private:
    CoordinateDisplayService() = default;

private:
    CoordinateDisplayFormat m_format =
        CoordinateDisplayFormat::Hierarchical;
};

std::string formatCurrentCoordinateDisplayLine(
    std::string coordinateText
);

} // namespace game::navigation
