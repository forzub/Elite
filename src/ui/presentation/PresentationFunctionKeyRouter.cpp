#include "src/ui/presentation/PresentationFunctionKeyRouter.h"

#include "src/ui/services/ServiceUiDefinition.h"

namespace ui::presentation
{
std::optional<GameUiTarget> directTargetForFunctionKey(int functionKey)
{
    switch (functionKey)
    {
        case 1: return GameUiTarget::forFlight(FlightPresentationView::Front);
        case 2: return GameUiTarget::forFlight(FlightPresentationView::Rear);
        case 3: return GameUiTarget::forFlight(FlightPresentationView::FrontDrone);
        case 4: return GameUiTarget::forFlight(FlightPresentationView::Drone);
        case 9: return GameUiTarget::forNavigation(NavigationPresentationView::Galaxy);
        case 10: return GameUiTarget::forNavigation(NavigationPresentationView::System);
        case 11: return GameUiTarget::forNavigation(NavigationPresentationView::Detail);
        case 12: return GameUiTarget::forNavigation(NavigationPresentationView::Local);
        default:
            break;
    }

    if (const auto* definition =
            ui::services::findServiceUiDefinitionByFunctionKey(functionKey))
    {
        return GameUiTarget::forService(definition->id);
    }

    return std::nullopt;
}
}
