#include "src/ui/services/ServiceUiDefinition.h"

namespace ui::services
{
const ServiceUiDefinition& shipyardUiDefinition()
{
    static constexpr ServiceUiDefinition Definition{
        ServiceUiId::Shipyard,
        6,
        "shipyard",
        "service.shipyard.title",
        "SHIPYARD"
    };
    return Definition;
}
}
