#pragma once

namespace game {
enum class RadarType {
    PPI,              // Круглый радар с лучом
    VerticalScreen,    // Вертикальный экран
    HolographicSphere, // 3D голограмма
    FighterHUD         // Нашлемный дисплей
};
}