#include "src/game/equipment/data/decryptors.h"

const DecryptorDesc DECRYPTOR_STANDARD =
{   
    
    .base = {
        "CRYPTONEXUS/2",
        "GalCom",
        "Стандартная корабельная Система дешифрования связи общего назначения"
    },

    .slotCount = 2
};

const DecryptorDesc DECRYPTOR_ADVANCED =
{
    .base = {
        "AEGIS-CRYPT 4C-DeLuxe",
        "GalCom",
        "Корабельный передатчик"
    },

    .slotCount = 4
};

const DecryptorDesc DECRYPTOR_MILITARY =
{
    .base = {
        "MIL-F.L.A.P.P.E.R. 8",
        "Aegis Dynamics",
        "Fast Linguistic Analysis & Protocol Penetration Electronic Receiver 8-channel / Быстрый электронный приёмник для лингвистического анализа и вскрытия протоколов, 8-канальный"
    },

    .slotCount = 8
};

const DecryptorDesc DECRYPTOR_QUEST =
{
    .base = {
        "COBBLER/16",
        "Unknown",
        "Custom Omnichannel Bullshit Breaker & Language Eradicator, Realtime / Заказной всеканальный брехню-ломатель и истребитель языка, реального времени"
    },

    .slotCount =16
};
