#include "jammers.h"
#include "src/game/localization/LocalizedMessage.h"

const JammerDesc CIV_JAMMER_MK1 = {
    .base = {
        "JAM-CIV-01",
        "Weyland Signal Division",
        game::localization::Message("equipment.description.jam_civ_01")
    },
    .jammingPower = 0.5f,
    .radius       = 10'000.0f   // 10 км
};

const JammerDesc MIL_JAMMER_TACTICAL = {
    .base = {
        "JAM-MIL-22",
        "Aegis Dynamics",
        game::localization::Message("equipment.description.jam_mil_22")
    },
    .jammingPower = 2.5f,
    .radius       = 35'000.0f   // 35 км
};

const JammerDesc ORBITAL_NOISE_ARRAY = {
    .base = {
        "ORB-NOISE-9",
        "Helios Defense Systems",
        game::localization::Message("equipment.description.orb_noise_9")
    },
    .jammingPower = 8.0f,
    .radius       = 250'000.0f  // 250 км
};
