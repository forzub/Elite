#include "decryptors.h"

const DecryptorDesc Decryptor_Standard =
{   
    .base = {
        "CRYPTONEXUS/2",
        "GalCom",
        "Стандартная корабельная Система дешифрования связи общего назначения"
    },

    .slotCount = 2
};

const DecryptorDesc Decryptor_Advanced =
{
    .base = {
        "AEGIS-CRYPT 4C-DeLuxe",
        "GalCom",
        "Корабельный передатчик"
    },

    .slotCount = 4
};

const DecryptorDesc Decryptor_Military =
{
    .base = {
        "MIL-F.L.A.P.P.E.R. 8",
        "Aegis Dynamics",
        "Fast Linguistic Analysis & Protocol Penetration Electronic Receiver 8-channel / Быстрый электронный приёмник для лингвистического анализа и вскрытия протоколов, 8-канальный"
    },

    .slotCount = 8
};

const DecryptorDesc Decryptor_Quest =
{
    .base = {
        "COBBLER/16",
        "Unknown",
        "Custom Omnichannel Bullshit Breaker & Language Eradicator, Realtime / Заказной всеканальный брехню-ломатель и истребитель языка, реального времени"
    },

    .slotCount =16
};
