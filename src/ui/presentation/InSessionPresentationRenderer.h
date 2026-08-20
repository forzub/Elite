#pragma once

#include <optional>

#include "render/types/Viewport.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/ui/services/ServiceUiDefinition.h"

namespace game::localization
{
class LocalizationService;
}

namespace ui::presentation
{
class InSessionPresentationRenderer
{
public:
    void renderServicePanel(
        const Viewport& viewport,
        const game::localization::LocalizationService& localization,
        ui::services::ServiceUiId service
    ) const;

    void renderSystemMapPanel(
        const Viewport& viewport,
        const game::localization::LocalizationService& localization,
        const game::presentation::SystemMapPanelPresentation& panel
    );

    bool systemMapPanelContains(
        const Viewport& viewport,
        double mouseX,
        double mouseY
    ) const;

    std::optional<game::presentation::SystemMapPanelAction>
    handleSystemMapPanelInput(
        const Viewport& viewport,
        const game::presentation::SystemMapPanelPresentation& panel,
        double mouseX,
        double mouseY,
        bool leftDown,
        double scrollY
    );

private:
    bool m_systemDropdownOpen = false;
    bool m_systemPanelLeftWasDown = false;
    int m_systemDropdownFirstRow = 0;
};
}
