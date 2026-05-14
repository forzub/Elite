#pragma once

enum class EquipmentState {
    Absent,     // слота нет
    Installed,  // есть, но выключено
    Active,     // работает
    Damaged,    // повреждено
    Offline     // есть, но недоступно (нет питания и т.п.)
};
