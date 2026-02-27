#include "receivers.h"

namespace game {

const ReceiverDesc STANDARD_RECEIVER = {
    5.0,   // powerConsumption
    0.2,   // heatGeneration
    1.0    // sensitivity
};

const ReceiverDesc MILITARY_RECEIVER = {
    8.0,
    0.4,
    1.5
};

}
