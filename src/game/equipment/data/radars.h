#pragma once
#include "src/game/equipment/radar/RadarDesc.h"

namespace game {

// Минимальный навигационный
extern const RadarDesc PPI_CRT_RADAR;
// Минимальный навигационный
extern const RadarDesc PPI_LCD_RADAR;

// Development sensor backend installed in the player radar slot until a
// production radar implementation replaces it. Consumers must not special-case
// this descriptor; its public output is the production RadarScanReport contract.
extern const RadarDesc TEST_IDEAL_RADAR;

// Минимальный навигационный
// extern const RadarDesc YACHT_RADAR;

// // Гражданский транспортный
// extern const RadarDesc CIVIL_RADAR;

// // Истребительный
// extern const RadarDesc FIGHTER_RADAR;

// // Торгово-боевой
// extern const RadarDesc COMBAT_RADAR;

// // Носитель / крупный корабль
// extern const RadarDesc CARRIER_RADAR;

}