#include <memory>

#include "RadarWidgetFactory.h"
#include "RadarWidgetBase.h"
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/types/RadarType.h"
#include "src/ui/components/radar/ppi/RadarPPIWidget.h"






std::unique_ptr<RadarWidgetBase> RadarWidgetFactory::create(
    game::RadarType type, 
    game::RadarVisualProfile visualProfile)
{
    switch(type) {
        case game::RadarType::PPI:
        {   
            auto widget = std::make_unique<RadarPPIWidget>();
            widget->init(visualProfile);
            return widget;
        }
        
        default:
            auto widget = std::make_unique<RadarPPIWidget>();
            widget->init(visualProfile);
            return widget;
    }
}