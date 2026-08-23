#pragma once

#include <memory>
#include <string>

#include "src/game/presentation/GalacticCompassPresentation.h"
#include "src/render/Font.h"
#include "src/render/HUD/HudPrimitiveBatch.h"
#include "src/render/types/Viewport.h"

namespace render::cockpit
{

class GalacticCompassRenderer
{
public:
    void init();

    void render(
        const game::presentation::GalacticCompassPresentation& presentation,
        const Viewport& viewport
    );

private:
    void ensureFont(int pixelSize);
    static double wrapLongitude(double degrees);
    static double signedLongitudeDelta(double a, double b);
    std::string longitudeLabel(
        int roundedDegrees,
        const game::presentation::GalacticCompassVocabulary& vocabulary
    ) const;

    render::hud::HudPrimitiveBatch m_batch;
    std::unique_ptr<Font> m_font;
    int m_fontPixelSize = 0;
};

} // namespace render::cockpit
