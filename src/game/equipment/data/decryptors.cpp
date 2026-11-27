#include "src/game/equipment/data/decryptors.h"
#include "src/game/localization/LocalizedMessage.h"

const DecryptorDesc DECRYPTOR_STANDARD =
{   
    
    .base = {
        "CRYPTONEXUS/2",
        "GalCom",
        game::localization::Message("equipment.description.cryptonexus_2")
    },

    .slotCount = 2
};

const DecryptorDesc DECRYPTOR_ADVANCED =
{
    .base = {
        "AEGIS-CRYPT 4C-DeLuxe",
        "GalCom",
        game::localization::Message("equipment.description.aegis_crypt_4c_deluxe")
    },

    .slotCount = 4
};

const DecryptorDesc DECRYPTOR_MILITARY =
{
    .base = {
        "MIL-F.L.A.P.P.E.R. 8",
        "Aegis Dynamics",
        game::localization::Message("equipment.description.mil_flapper_8")
    },

    .slotCount = 8
};

const DecryptorDesc DECRYPTOR_QUEST =
{
    .base = {
        "COBBLER/16",
        "Unknown",
        game::localization::Message("equipment.description.cobbler_16")
    },

    .slotCount =16
};
