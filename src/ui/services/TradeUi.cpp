#include "src/ui/services/ServiceUiDefinition.h"

namespace ui::services
{
const ServiceUiDefinition& tradeUiDefinition()
{
    static constexpr ServiceUiDefinition Definition{
        ServiceUiId::Trade,
        8,
        "trade",
        "service.trade.title",
        "TRADE"
    };
    return Definition;
}
}
