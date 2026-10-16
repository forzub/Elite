#pragma once

#include <algorithm>
#include <array>
#include <string>

#include <glm/glm.hpp>

#include "render/HUD/TextRenderer.h"
#include "render/types/Viewport.h"
#include "src/game/localization/LocalizationService.h"

namespace game::presentation
{

// TEMPORARY DEVELOPER SHORTCUT STRIP.
//
// Presentation-only, no input/control dependencies and no retained state.
// Safe removal procedure:
//   1. delete this file;
//   2. remove its include from SpaceState.cpp;
//   3. remove the two TemporaryShortcutStrip::render(...) calls;
//   4. remove cockpit.shortcut_strip.* localization keys.
//
// Do not move keyboard semantics into this component.
class TemporaryShortcutStrip final
{
public:
    static void render(
        const Viewport& viewport,
        const game::localization::LocalizationService& localization
    )
    {
        if (viewport.width <= 0 || viewport.height <= 0)
            return;

        // Modified function-key chords are the primary purpose of
        // this temporary strip. Ordinary utility shortcuts follow them.
        const std::array<std::string, 10> items {
            key("Ctrl+F10") + " " +
                localization.text(
                    "cockpit.shortcut_strip.flight_law",
                    "FLIGHT LAW"
                ),
            key("Ctrl+F11") + " " +
                localization.text(
                    "cockpit.shortcut_strip.coordinates",
                    "COORDS"
                ),
            key("Ctrl+F12") + " " +
                localization.text(
                    "cockpit.shortcut_strip.constellations",
                    "CONSTELL."
                ),
            key("Alt+F12") + " " +
                localization.text(
                    "cockpit.shortcut_strip.sky_culture",
                    "SKY"
                ),
            key("Ctrl+Alt+F12") + " " +
                localization.text(
                    "cockpit.shortcut_strip.language",
                    "LANG"
                ),
            key("Esc") + " " +
                localization.text(
                    "cockpit.shortcut_strip.menu",
                    "MENU"
                ),
            key("P") + " " +
                localization.text(
                    "cockpit.shortcut_strip.detail",
                    "DETAIL"
                ),
            key("Backspace") + " " +
                localization.text(
                    "cockpit.shortcut_strip.back",
                    "BACK"
                ),
            key("Ctrl+R") + " " +
                localization.text(
                    "cockpit.shortcut_strip.repair",
                    "REPAIR"
                ),
            key("L-Ctrl+Q") + " " +
                localization.text(
                    "cockpit.shortcut_strip.eject",
                    "EJECT"
                )
        };

        std::string label;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (i != 0)
                label += "   ·   ";
            label += items[i];
        }

        auto& text = TextRenderer::instance();
        text.beginFrameForViewport(viewport.width, viewport.height);

        int fontPx = 11;
        float labelWidth = text.measureTextPx(label, fontPx);
        const float availableWidth =
            std::max(1.0f, static_cast<float>(viewport.width) - 24.0f);

        // Keep this temporary aid on exactly one line. It may shrink on narrow
        // windows, but never grows into a second HUD row.
        while (labelWidth > availableWidth && fontPx > 7)
        {
            --fontPx;
            labelWidth = text.measureTextPx(label, fontPx);
        }

        const float screenW = static_cast<float>(viewport.width);
        const float screenH = static_cast<float>(viewport.height);
        const float paddingX = 12.0f;
        const float stripHeight = 22.0f;
        const float stripWidth =
            std::min(screenW, labelWidth + paddingX * 2.0f);
        const float x = std::max(0.0f, (screenW - stripWidth) * 0.5f);
        const float y = std::max(0.0f, screenH - stripHeight);
        const float textX =
            std::max(4.0f, x + (stripWidth - labelWidth) * 0.5f);
        const float baselineY = screenH - 6.0f;

        // Deliberately subtle: this is a temporary developer aid, not cockpit
        // instrumentation. One line only.
        text.solidRectPx(
            x,
            y,
            stripWidth,
            stripHeight,
            glm::vec4(0.01f, 0.02f, 0.03f, 0.70f)
        );

        text.textDrawPx(
            label,
            textX,
            baselineY,
            fontPx,
            glm::vec4(0.68f, 0.80f, 0.88f, 0.86f)
        );

        text.endFrame();
    }

private:
    static std::string key(const char* value)
    {
        return std::string("[") + value + "]";
    }
};

} // namespace game::presentation
