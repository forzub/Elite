#pragma once

namespace game::promo::tuning
{

// Скорость звена.
// Берём долю от maxCombatSpeed, но зажимаем в читаемый диапазон.
constexpr float WingSpeedFactor = 0.9f;
constexpr float WingSpeedMin = 95.0f;
constexpr float WingSpeedMax = 350.0f;

// Дальность пролёта.
// SpawnDistance — откуда звено появляется перед игроком.
// Если надо укоротить дистанцию ЗА игроком, уменьшай SplitDistance / split offsets.
constexpr float SpawnDistance = 2250.0f;

// Высота пролёта над игроком.
constexpr float PassHeight = 260.0f;

// Когда начинается распад звена после пролёта игрока.
constexpr float SplitAfterPlayerDistance = 80.0f;

// Длина фазы распада.
constexpr float SplitDurationDistance = 520.0f;

// Насколько расходятся боковые группы.
constexpr float SideSplitDistance = 900.0f;

// Насколько центральная группа уходит вверх.
constexpr float CenterClimb = 520.0f;

} // namespace game::promo::tuning