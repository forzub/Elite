#include "src/ui/services/ServiceUiDefinition.h"

#include <array>
#include <cstring>

namespace ui::services
{
namespace
{
std::array<const ServiceUiDefinition*, 4> definitions()
{
    return {
        &governmentServicesUiDefinition(),
        &shipyardUiDefinition(),
        &repairRefitUiDefinition(),
        &tradeUiDefinition()
    };
}
}

const ServiceUiDefinition* findServiceUiDefinition(ServiceUiId id)
{
    for (const ServiceUiDefinition* definition : definitions())
    {
        if (definition->id == id)
            return definition;
    }
    return nullptr;
}

const ServiceUiDefinition* findServiceUiDefinitionByStableId(const char* stableId)
{
    if (!stableId || !*stableId)
        return nullptr;

    for (const ServiceUiDefinition* definition : definitions())
    {
        if (std::strcmp(definition->stableId, stableId) == 0)
            return definition;
    }
    return nullptr;
}

const ServiceUiDefinition* findServiceUiDefinitionByFunctionKey(int functionKey)
{
    for (const ServiceUiDefinition* definition : definitions())
    {
        if (definition->functionKey == functionKey)
            return definition;
    }
    return nullptr;
}
}
