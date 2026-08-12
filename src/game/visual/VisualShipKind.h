#pragma once

namespace game::visual
{

// Client-presentation classification only. It keeps debug visibility filters
// independent without turning traffic/promo visuals into authoritative entities.
enum class VisualShipKind
{
    Generic,
    Traffic,
    Promo
};

} // namespace game::visual
