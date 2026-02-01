#include "receivers.h"

const ReceiverDesc STANDARD_RECEIVER = {
    .base = {
        "RX-CIV-01",
        "Weyland Signal Division",
        "Гражданский приёмник ближнего и среднего радиуса"
    },
    .sensitivity = 1.0f
};

const ReceiverDesc MILITARY_RECEIVER = {
    .base = {
        "RX-MIL-07",
        "Aegis Dynamics",
        "Военный приёмник с повышенной чувствительностью и фильтрацией"
    },
    .sensitivity = 2.5f
};
