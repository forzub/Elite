#include "src/render/cockpit/EngineThrustIndicatorRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "src/render/HUD/TextRenderer.h"
#include "src/render/cockpit/EngineStatusWidgetStyle.h"

namespace render::cockpit
{
namespace
{

constexpr float Pi = 3.14159265358979323846f;

glm::vec4 withAlpha(
    glm::vec4 color,
    float alphaScale
) noexcept
{
    color.a *= std::clamp(alphaScale, 0.0f, 1.0f);
    return color;
}

float warningPulse(
    const EngineStatusWidgetStyle& style
) noexcept
{
    using Clock = std::chrono::steady_clock;
    const double seconds =
        std::chrono::duration<double>(
            Clock::now().time_since_epoch()
        ).count();

    const double period =
        std::max(0.25, static_cast<double>(style.warningPeriodSeconds));

    const float wave =
        0.5f +
        0.5f * std::sin(
            static_cast<float>(
                seconds * (2.0 * static_cast<double>(Pi)) / period
            )
        );

    // Smoothstep removes the sharp-looking speed change at mid-cycle and makes
    // the damaged engine bar feel like a slow breathing warning lamp.
    const float smooth =
        wave * wave * (3.0f - 2.0f * wave);

    return
        style.warningMinimumAlpha +
        (1.0f - style.warningMinimumAlpha) * smooth;
}

// TextRenderer currently exposes axis-aligned solid quads only. A small
// stepped corner is sufficient at HUD scale and keeps this widget independent
// from cockpit mesh/UI dependencies. When a generic rounded primitive arrives,
// this helper is the only place that needs to change.
void roundedRectPx(
    TextRenderer& text,
    float x,
    float y,
    float width,
    float height,
    float radius,
    const glm::vec4& color
)
{
    const float r = std::clamp(
        radius,
        0.0f,
        std::min(width, height) * 0.25f
    );

    if (r <= 0.5f)
    {
        text.solidRectPx(x, y, width, height, color);
        return;
    }

    text.solidRectPx(x + r, y, width - 2.0f * r, height, color);
    text.solidRectPx(x, y + r, width, height - 2.0f * r, color);

    const float step = std::max(1.0f, r * 0.42f);
    text.solidRectPx(
        x + step,
        y + step,
        width - 2.0f * step,
        height - 2.0f * step,
        color
    );
}

void roundedOutlinePx(
    TextRenderer& text,
    float x,
    float y,
    float width,
    float height,
    float radius,
    float borderWidth,
    const glm::vec4& border,
    const glm::vec4& inner
)
{
    roundedRectPx(
        text,
        x,
        y,
        width,
        height,
        radius,
        border
    );

    const float inset = std::max(1.0f, borderWidth);
    roundedRectPx(
        text,
        x + inset,
        y + inset,
        std::max(0.0f, width - 2.0f * inset),
        std::max(0.0f, height - 2.0f * inset),
        std::max(0.0f, radius - inset),
        inner
    );
}

void drawCheckEngineIcon(
    TextRenderer& text,
    float x,
    float y,
    float size,
    const glm::vec4& color
)
{
    const float u = std::max(1.0f, size / 12.0f);

    // Compact automotive-style engine silhouette built from HUD quads.
    text.solidRectPx(x + 2.0f*u, y + 3.0f*u, 7.0f*u, 6.0f*u, color);
    text.solidRectPx(x + 3.0f*u, y + 1.0f*u, 3.5f*u, 2.0f*u, color);
    text.solidRectPx(x + 0.5f*u, y + 4.0f*u, 1.5f*u, 4.0f*u, color);
    text.solidRectPx(x + 9.0f*u, y + 4.0f*u, 1.5f*u, 4.0f*u, color);

    // Hollow the body enough that it reads as an icon rather than a block.
    text.solidRectPx(
        x + 3.0f*u,
        y + 4.2f*u,
        5.0f*u,
        3.5f*u,
        glm::vec4(0.0f, 0.0f, 0.0f, color.a)
    );
}

} // namespace

void EngineThrustIndicatorRenderer::render(
    const game::presentation::EngineThrustHudPresentation& presentation,
    const Viewport& viewport
)
{
    using FillDirection =
        game::presentation::EngineThrustFillDirection;

    if (!presentation.visible ||
        presentation.groups.empty() ||
        viewport.width <= 0 ||
        viewport.height <= 0)
    {
        return;
    }

    auto& text = TextRenderer::instance();
    const auto& style =
        resolveEngineStatusWidgetStyle(presentation.styleId);

    const float scale = std::clamp(
        static_cast<float>(viewport.height) / 1440.0f,
        0.72f,
        1.40f
    );

    const float barWidth =
        std::max(6.0f, style.barWidthPx * scale);
    const float barHeight =
        std::max(54.0f, style.barHeightPx * scale);
    const float barGap =
        std::max(8.0f, style.barGapPx * scale);
    const float groupGap =
        std::max(8.0f, style.groupGapPx * scale);
    const float padding =
        std::max(7.0f, style.paddingPx * scale);
    const float iconSize =
        std::max(18.0f, style.iconSizePx * scale);
    const float iconGap =
        std::max(3.0f, style.iconToLabelGapPx * scale);
    const float labelGap =
        std::max(2.0f, style.labelToBarsGapPx * scale);
    const float radius =
        std::max(4.0f, style.cornerRadiusPx * scale);
    const float borderWidth =
        std::max(1.0f, style.borderWidthPx * scale);

    const int labelPx = std::max(
        11,
        static_cast<int>(std::lround(13.5f * scale))
    );

    float barsWidth = 0.0f;
    for (std::size_t g = 0; g < presentation.groups.size(); ++g)
    {
        const auto& group = presentation.groups[g];
        if (!group.bars.empty())
        {
            barsWidth +=
                static_cast<float>(group.bars.size()) * barWidth +
                static_cast<float>(group.bars.size() - 1) * barGap;
        }

        if (g + 1 < presentation.groups.size())
            barsWidth += groupGap;
    }

    const float labelWidth =
        text.measureTextPx(presentation.modelLabel, labelPx);

    // Content-derived dimensions: the panel is only as large as the icon,
    // label and actual I/Y/X bar topology require.
    const float contentWidth =
        std::max({
            barsWidth,
            labelWidth,
            iconSize
        });

    const float panelWidth =
        contentWidth + 2.0f * padding;
    const float panelHeight =
        padding +
        iconSize +
        iconGap +
        static_cast<float>(labelPx) +
        labelGap +
        barHeight +
        padding;

    const float panelX =
        static_cast<float>(viewport.width) * 0.025f;
    const float panelBottom =
        static_cast<float>(viewport.height) * 0.965f;
    const float panelY =
        panelBottom - panelHeight;

    // A faint larger underlay approximates amber border glow without adding a
    // second rendering subsystem.
    roundedRectPx(
        text,
        panelX - 2.0f * scale,
        panelY - 2.0f * scale,
        panelWidth + 4.0f * scale,
        panelHeight + 4.0f * scale,
        radius + 2.0f * scale,
        style.panelBorderGlow
    );

    roundedOutlinePx(
        text,
        panelX,
        panelY,
        panelWidth,
        panelHeight,
        radius,
        borderWidth,
        style.panelBorder,
        style.panelBackground
    );

    const float pulse =
        presentation.warning
            ? warningPulse(style)
            : 1.0f;

    const float contentX = panelX + padding;
    float contentY = panelY + padding;

    drawCheckEngineIcon(
        text,
        contentX + (contentWidth - iconSize) * 0.5f,
        contentY,
        iconSize,
        presentation.warning
            ? withAlpha(style.warning, pulse)
            : style.idleWarningIcon
    );

    // Keep the warning icon in the upper diagnostic area, but anchor the
    // model label to the thrust bars instead of to the icon. This makes the
    // requested vertical move visually meaningful rather than adding a few
    // pixels to both label and bars.
    const float labelY =
        panelY + panelHeight - padding - barHeight -
        labelGap - static_cast<float>(labelPx);

    text.textDrawPx(
        presentation.modelLabel,
        contentX + (contentWidth - labelWidth) * 0.5f,
        labelY,
        labelPx,
        style.label
    );

    const float barsY =
        labelY + static_cast<float>(labelPx) + labelGap;

    // Keep fore/aft bars inside one physical engine group close together,
    // then distribute physical groups uniformly across the instrument width.
    // This makes I/Y/X silhouettes readable instead of left-packed.
    float totalGroupWidth = 0.0f;
    std::size_t nonEmptyGroups = 0;
    for (const auto& group : presentation.groups)
    {
        if (group.bars.empty())
            continue;
        ++nonEmptyGroups;
        totalGroupWidth +=
            static_cast<float>(group.bars.size()) * barWidth +
            static_cast<float>(group.bars.size() - 1) * barGap;
    }

    const float distributedGroupGap =
        nonEmptyGroups > 1
            ? std::max(
                  groupGap,
                  (contentWidth - totalGroupWidth) /
                      static_cast<float>(nonEmptyGroups - 1)
              )
            : 0.0f;

    float x =
        nonEmptyGroups == 1
            ? contentX + (contentWidth - totalGroupWidth) * 0.5f
            : contentX;

    std::size_t renderedGroups = 0;
    for (std::size_t g = 0; g < presentation.groups.size(); ++g)
    {
        const auto& group = presentation.groups[g];
        if (group.bars.empty())
            continue;

        for (std::size_t b = 0; b < group.bars.size(); ++b)
        {
            const auto& bar = group.bars[b];
            const float barPulse =
                bar.fault ? pulse : 1.0f;

            text.solidRectPx(
                x - 1.0f * scale,
                barsY - 1.0f * scale,
                barWidth + 2.0f * scale,
                barHeight + 2.0f * scale,
                withAlpha(style.barOutline, barPulse)
            );
            text.solidRectPx(
                x,
                barsY,
                barWidth,
                barHeight,
                withAlpha(style.track, barPulse)
            );

            const float capability =
                std::clamp(bar.capability01, 0.0f, 1.0f);
            const float thrust =
                std::clamp(bar.thrust01, 0.0f, capability);

            const float capabilityHeight =
                barHeight * capability;
            const float thrustHeight =
                barHeight * thrust;

            const bool topDown =
                bar.fillDirection == FillDirection::TopDown;

            const float capabilityY = topDown
                ? barsY
                : barsY + barHeight - capabilityHeight;
            const float thrustY = topDown
                ? barsY
                : barsY + barHeight - thrustHeight;

            if (capabilityHeight > 0.0f)
            {
                text.solidRectPx(
                    x,
                    capabilityY,
                    barWidth,
                    capabilityHeight,
                    withAlpha(style.capacity, barPulse)
                );
            }

            if (thrustHeight > 0.0f)
            {
                const float redWidth =
                    std::max(2.0f, barWidth * 0.56f);
                const float redX =
                    x + (barWidth - redWidth) * 0.5f;

                text.solidRectPx(
                    redX,
                    thrustY,
                    redWidth,
                    thrustHeight,
                    withAlpha(style.thrust, barPulse)
                );
            }

            // Independent observed temperature strips. A dark track means
            // telemetry is unavailable; never imply a healthy cold emitter.
            const auto strip = [&](float sx, bool valid, float kelvin,
                                   float safeK) {
                const float w = std::max(2.0f, 2.7f * scale);
                text.solidRectPx(
                    sx, barsY, w, barHeight,
                    glm::vec4(0.12f, 0.14f, 0.18f, 0.8f));
                if (!valid || !std::isfinite(kelvin) || safeK <= 0.0f)
                    return;
                const float fraction = std::clamp(
                    kelvin / (safeK * 1.25f), 0.0f, 1.0f);
                const glm::vec4 cold(0.1f,0.55f,0.95f,0.94f);
                const glm::vec4 green(0.2f,0.85f,0.42f,0.94f);
                const glm::vec4 hot(1.0f,0.24f,0.09f,0.98f);
                const auto color = fraction < 0.5f
                    ? glm::mix(cold,green,fraction*2.0f)
                    : glm::mix(green,hot,(fraction-0.5f)*2.0f);
                text.solidRectPx(sx,barsY+barHeight*(1.0f-fraction),
                    w,barHeight*fraction,color);
            };
            strip(x-4.0f*scale, bar.hasEmitterTemperature,
                  bar.emitterTemperatureK, 2500.0f);
            strip(x+barWidth+1.0f*scale, bar.hasEngineTemperature,
                  bar.engineTemperatureK, bar.engineSafeTemperatureK);

            x += barWidth;
            if (b + 1 < group.bars.size())
                x += barGap;
        }

        ++renderedGroups;
        if (renderedGroups < nonEmptyGroups)
            x += distributedGroupGap;
    }
}

void EngineThrustIndicatorRenderer::renderThermalSystems(
    const game::ShipCoreStatus& status,
    const game::presentation::EngineThrustHudPresentation& engines,
    const Viewport& viewport
)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    auto& text = TextRenderer::instance();
    const auto& style=resolveEngineStatusWidgetStyle("human_amber");
    const float scale=std::clamp(
        static_cast<float>(viewport.height)/1440.0f,0.72f,1.40f);
    const float x=static_cast<float>(viewport.width)*0.025f;
    const float bottom=static_cast<float>(viewport.height)*0.965f;
    // Reserve space to the right of the existing compact engine gauge.
    // Only text/geometry; no full-sized instrument windows.
    // Use exactly the same sizing equations as render() above. The model
    // label can be wider than the actual I/Y/X engine columns.
    const float barWidth = std::max(6.0f,style.barWidthPx*scale);
    const float barHeight = std::max(54.0f,style.barHeightPx*scale);
    const float barGap = std::max(8.0f,style.barGapPx*scale);
    const float groupGap = std::max(8.0f,style.groupGapPx*scale);
    const float padding = std::max(7.0f,style.paddingPx*scale);
    const float iconSize = std::max(18.0f,style.iconSizePx*scale);
    const float iconGap = std::max(3.0f,style.iconToLabelGapPx*scale);
    const float labelGap = std::max(2.0f,style.labelToBarsGapPx*scale);
    const int labelPx = std::max(11,
        static_cast<int>(std::lround(13.5f*scale)));
    float barsWidth=0.0f;
    for(std::size_t i=0;i<engines.groups.size();++i)
    {
        const auto& group=engines.groups[i];
        if(!group.bars.empty())
            barsWidth+=static_cast<float>(group.bars.size())*barWidth+
                static_cast<float>(group.bars.size()-1)*barGap;
        if(i+1<engines.groups.size())
            barsWidth+=groupGap;
    }
    const float engineWidth=std::max({
        barsWidth,text.measureTextPx(engines.modelLabel,labelPx),iconSize
    })+2.0f*padding;
    const float h=padding+iconSize+iconGap+static_cast<float>(labelPx)+
        labelGap+barHeight+padding;
    const float w=142.0f*scale;
    const float gap=9.0f*scale;
    const float reactorX=x+engineWidth+gap;
    const float coolantX=reactorX+w+gap;
    const float y=bottom-h;
    auto panel=[&](float px) {
        roundedOutlinePx(text,px,y,w,h,7.0f*scale,1.0f*scale,
            style.panelBorder,style.panelBackground);
    };
    // When this resolution is too narrow, do not obscure the boresight.
    if(coolantX+w > viewport.width*0.44f)
        return;
    const glm::vec4 amber=style.label;
    const glm::vec4 cyan(0.2f,0.72f,0.94f,1.0f);
    const glm::vec4 warn(1.0f,0.35f,0.12f,1.0f);
    const int small=std::max(12,static_cast<int>(15.0f*scale));
    const int caption=std::max(11,static_cast<int>(13.0f*scale));
    char buffer[48];
    panel(reactorX);
    panel(coolantX);
    const int tiny=std::max(9,static_cast<int>(11.0f*scale));
    text.textDrawPx("REACTOR",reactorX+8*scale,y+7*scale,small,amber);
    // Core | reactor pump | common coolant. Pump lights amber only if it
    // receives power and hysteresis requests it; otherwise empty square.
    const float r=std::clamp(static_cast<float>(
        status.reactor.criticalTemp>0
            ? status.reactor.temperature/status.reactor.criticalTemp : 0),
        0.0f,1.0f);
    const float chainY=y+19*scale;
    roundedOutlinePx(text,reactorX+10*scale,chainY,
        15*scale,37*scale,3*scale,1*scale,amber,style.track);
    text.solidRectPx(reactorX+12*scale,chainY+(35-31*r)*scale,
        11*scale,31*r*scale,r>0.85f?warn:cyan);
    text.solidRectPx(reactorX+25*scale,chainY+17*scale,
        17*scale,2*scale,amber);
    roundedOutlinePx(text,reactorX+42*scale,chainY+9*scale,
        22*scale,20*scale,2*scale,1*scale,amber,style.track);
    const bool pumping=status.cooling.reactorPumpActive &&
        status.cooling.allocatedPowerMW>0.0;
    if(pumping)
        text.solidRectPx(reactorX+46*scale,chainY+13*scale,
            14*scale,12*scale,style.capacity);
    text.solidRectPx(reactorX+64*scale,chainY+17*scale,
        14*scale,2*scale,amber);
    roundedOutlinePx(text,reactorX+78*scale,chainY+9*scale,
        23*scale,20*scale,3*scale,1*scale,cyan,style.track);
    text.solidRectPx(reactorX+81*scale,chainY+19*scale,
        17*scale,7*scale,cyan);
    const bool radiatorAlarm=status.cooling.panelThermalCritical ||
        status.cooling.criticalPanelCount>0;
    const bool reactorAlarm=status.reactor.status>=1 ||
        status.reactor.integrity<0.95 ||
        status.reactor.temperature<status.reactor.minOperatingTemperatureK;
    const float blink=warningPulse(style);
    if(radiatorAlarm)
        text.solidRectPx(reactorX+108*scale,chainY+8*scale,
            7*scale,7*scale,withAlpha(warn,blink));
    if(reactorAlarm)
        text.solidRectPx(reactorX+120*scale,chainY+8*scale,
            7*scale,7*scale,withAlpha(warn,blink));
    std::snprintf(buffer,sizeof(buffer),"%.0f K",status.reactor.temperature);
    text.textDrawPx(buffer,reactorX+8*scale,y+h-52*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"%.0f MW  %.0f%%",
        status.reactor.outputMW,status.reactor.efficiency01*100.0);
    text.textDrawPx(buffer,reactorX+8*scale,y+h-34*scale,caption,amber);
    text.textDrawPx("PWR / EFF",reactorX+8*scale,y+h-16*scale,tiny,amber);

    text.textDrawPx("COOLANT",coolantX+8*scale,y+7*scale,small,amber);
    const float sx=coolantX+10*scale;
    const float sy=y+19*scale;
    const float sw=w-20*scale;
    const float sh=20*scale;
    roundedOutlinePx(text,sx,sy,sw,sh,4*scale,1*scale,
        amber,style.track);
    for(int i=1;i<9;++i)
        text.solidRectPx(sx+sw*(static_cast<float>(i)/9.0f),
            sy+4*scale,2*scale,sh-8*scale,
            status.cooling.panelThermalCritical?warn:amber);
    std::snprintf(buffer,sizeof(buffer),"P %.0f / C %.0f K",
        status.cooling.effectiveRadiatorTemperatureK,
        status.cooling.coolantTemp);
    text.textDrawPx(buffer,coolantX+8*scale,y+h-63*scale,tiny,amber);
    std::snprintf(buffer,sizeof(buffer),"OK %.0f%%",
        status.cooling.panelHealthPercent);
    text.textDrawPx(buffer,coolantX+8*scale,y+h-47*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"IN %.1f MW",
        status.cooling.heatIntoCoolantMW);
    text.textDrawPx(buffer,coolantX+8*scale,y+h-31*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"OUT %.1f MW",
        status.cooling.radiatedPowerMW);
    text.textDrawPx(buffer,coolantX+8*scale,y+h-15*scale,caption,amber);
}

} // namespace render::cockpit
