#include "src/ui/services/ServiceUiDefinition.h"

namespace ui::services
{
const ServiceUiDefinition& governmentServicesUiDefinition()
{
    static constexpr ServiceUiDefinition Definition{
        ServiceUiId::GovernmentServices,
        5,
        "government",
        "service.government.title",
        "CIVIC & GOVERNMENT SERVICES"
    };
    return Definition;
}
}
