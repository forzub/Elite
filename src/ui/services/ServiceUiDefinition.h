#pragma once

#include <cstdint>

namespace ui::services
{
enum class ServiceUiId : std::uint8_t
{
    None = 0,
    GovernmentServices,
    Shipyard,
    RepairRefit,
    Trade
};

struct ServiceUiDefinition
{
    ServiceUiId id = ServiceUiId::None;
    int functionKey = 0;
    const char* stableId = "";
    const char* titleKey = "";
    const char* englishTitle = "";
};

const ServiceUiDefinition& governmentServicesUiDefinition();
const ServiceUiDefinition& shipyardUiDefinition();
const ServiceUiDefinition& repairRefitUiDefinition();
const ServiceUiDefinition& tradeUiDefinition();

const ServiceUiDefinition* findServiceUiDefinition(ServiceUiId id);
const ServiceUiDefinition* findServiceUiDefinitionByStableId(const char* stableId);
const ServiceUiDefinition* findServiceUiDefinitionByFunctionKey(int functionKey);
}
