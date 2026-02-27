#include <memory>

#include "RadarWidgetFactory.h"
#include "RadarWidgetBase.h"
#include "RadarCRTWidget.h"
#include "src/game/equipment/types/RadarVisualProfile.h"

std::unique_ptr<RadarWidgetBase>
RadarWidgetFactory::create(game::RadarVisualProfile profile)
{
    switch (profile)
    {
        case game::RadarVisualProfile::CRT:
            return std::make_unique<RadarCRTWidget>();

        default:
            return nullptr;
    }
}