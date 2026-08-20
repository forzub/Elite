#include "src/ui/services/ServiceUiDefinition.h"

namespace ui::services
{
const ServiceUiDefinition& repairRefitUiDefinition()
{
    static constexpr ServiceUiDefinition Definition{
        ServiceUiId::RepairRefit,
        7,
        "repair_refit",
        "service.repair_refit.title",
        "REPAIR & REFIT"
    };
    return Definition;
}
}
