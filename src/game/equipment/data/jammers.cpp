#include "jammers.h"

const JammerDesc CIV_JAMMER_MK1 = {
    .base = {
        "JAM-CIV-01",
        "Weyland Signal Division",
        "Гражданский глушитель ближнего радиуса действия"
    },
    .jammingPower = 0.5f,
    .radius       = 10'000.0f   // 10 км
};

const JammerDesc MIL_JAMMER_TACTICAL = {
    .base = {
        "JAM-MIL-22",
        "Aegis Dynamics",
        "Тактический военный глушитель для подавления связи"
    },
    .jammingPower = 2.5f,
    .radius       = 35'000.0f   // 35 км
};

const JammerDesc ORBITAL_NOISE_ARRAY = {
    .base = {
        "ORB-NOISE-9",
        "Helios Defense Systems",
        "Стационарный массив широкозонного шумового подавления"
    },
    .jammingPower = 8.0f,
    .radius       = 250'000.0f  // 250 км
};
