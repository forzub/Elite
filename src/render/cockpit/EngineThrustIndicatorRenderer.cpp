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

    // Installation diagram replaces legacy vertical-bar instrument when the
    // MotherComputer supplies reactor-group topology.
    if (!presentation.drives.empty())
    {
        auto& text=TextRenderer::instance();
        const auto& style=resolveEngineStatusWidgetStyle(
            presentation.styleId);
        const float scale=std::clamp(
            static_cast<float>(viewport.height)/1440.0f,0.72f,1.40f);
        const float x=static_cast<float>(viewport.width)*0.025f;
        const float bottom=static_cast<float>(viewport.height)*0.965f;
        const float w=260.0f*scale;
        const float h=165.0f*scale;
        const float y=bottom-h;
        roundedOutlinePx(text,x,y,w,h,7*scale,1*scale,
            style.panelBorder,style.panelBackground);
        const int font=std::max(11,static_cast<int>(13*scale));
        const float pulse=presentation.warning?warningPulse(style):1.0f;
        drawCheckEngineIcon(text,x+8*scale,y+6*scale,17*scale,
            presentation.warning?withAlpha(style.warning,pulse):
                style.idleWarningIcon);
        text.textDrawPx(presentation.modelLabel,x+31*scale,y+8*scale,
            font,style.label);

        const bool newtonian=presentation.modelLabel.find("NEWTONIAN")
            !=std::string::npos;
        const float top=y+30*scale;
        // Reserve a separate bottom lane for 8 RCS indicators and the
        // argon-pressure strip. In Newtonian-Y/X, packing four generator
        // rows plus every multi-line heat caption caused overlap.
        const float rowStep=(newtonian?88.0f:128.0f)*scale/
            static_cast<float>(std::max<std::size_t>(
                1,presentation.drives.size()));
        const float sq=12.0f*scale;
        const float segment=83.0f*scale;
        const glm::vec4 cold(0.08f,0.19f,0.36f,1.0f);
        const glm::vec4 hot(1.0f,0.22f,0.12f,1.0f);
        const glm::vec4 normal(0.96f,0.49f,0.12f,1.0f);
        const glm::vec4 dead(0.02f,0.02f,0.03f,1.0f);
        const glm::vec4 green(0.18f,0.85f,0.36f,1.0f);
        const glm::vec4 yellow(0.95f,0.79f,0.10f,1.0f);
        const auto findBar=[&](const std::string& id)
            -> const game::presentation::EngineThrustBarPresentation*
        {
            for(const auto& group:presentation.groups)
                for(const auto& bar:group.bars)
                    if(bar.slotId==id) return &bar;
            return nullptr;
        };
        const auto drawMotor=[&](const std::string& id,
            float rx,float centerY,bool outwardRight)
        {
            const auto* bar=findBar(id);
            if(!bar) return;
            const float available=std::clamp(bar->capability01,0.0f,1.0f);
            const float length=segment*available;
            const float bx=outwardRight?rx:rx-length;
            const float fullBx=outwardRight?rx:rx-segment;
            const float bh=7.0f*scale;
            // Full-size neutral track is always visible. A motor with zero
            // actual available thrust is entirely GREY, never orange; a
            // partially damaged motor has only its reachable portion amber.
            const glm::vec4 unavailable(0.30f,0.33f,0.38f,0.90f);
            roundedOutlinePx(text,fullBx,centerY-bh*0.5f,
                segment,bh,1*scale,1*scale,unavailable,style.track);
            if(available>0.001f)
                roundedOutlinePx(text,bx,centerY-bh*0.5f,
                    length,bh,1*scale,1*scale,normal,style.track);
            // Filled from generator outward, with real executed motor thrust.
            const float fired=available>0.0f
                ? std::clamp(bar->thrust01/available,0.0f,1.0f):0.0f;
            const float fill=std::max(0.0f,length-2*scale)*fired;
            if(fill>0.0f)
                text.solidRectPx(outwardRight?bx+scale:bx+length-scale-fill,
                    centerY-bh*0.5f+scale,fill,
                    std::max(1.0f,bh-2*scale),style.thrust);
            // Real vectoring angle complements thrust: manoeuvring Assisted
            // engines still show their executed thrust on these bars.
            if(bar->hasNozzleAngle && bar->thrust01>0.005f &&
                bar->nozzleAngleDeg>0.5f)
            {
                char angle[20];
                std::snprintf(angle,sizeof(angle),"%.0f deg",
                    bar->nozzleAngleDeg);
                const int angleFont=std::max(11,
                    static_cast<int>(std::lround(12.5f*scale)));
                text.textDrawPx(angle,outwardRight?rx+8*scale:rx-31*scale,
                    centerY+6*scale+angleFont/3.0f,
                    angleFont,style.label);
            }
            // A damaged motor does not move its physical radiator pad.
            const float padX=outwardRight?rx+segment:rx-segment;
            glm::vec4 padColor=style.track;
            if(bar->hasEmitterIntegrity)
            {
                const float integrity=std::clamp(
                    bar->emitterIntegrity01,0.0f,1.0f);
                // The pad thermal-balance status is not yet replicated;
                // integrity is shown, while heat-condition color is unknown.
                // Match the equipment-damage presentation thresholds:
                // intact/usable = normal, damaged = amber, critical = red.
                padColor=integrity<=0.0f?dead:
                    integrity<0.30f?hot:
                    integrity<0.70f?yellow:green;
                char pct[16];
                std::snprintf(pct,sizeof(pct),"%.0f%%",integrity*100.0f);
                text.textDrawPx(pct,padX-12*scale,
                    centerY-17*scale,std::max(9,static_cast<int>(10*scale)),
                    style.label);
            }
            roundedOutlinePx(text,padX-4*scale,centerY-4*scale,
                8*scale,8*scale,4*scale,1*scale,
                style.panelBorder,padColor);
        };
        for(std::size_t i=0;i<presentation.drives.size();++i)
        {
            const auto& drive=presentation.drives[i];
            const float cy=top+(i+0.5f)*rowStep;
            const float reactorX=x+(newtonian?225.0f:130.0f)*scale;
            const float rx=reactorX-sq*0.5f;
            glm::vec4 color=drive.failed?dead:cold;
            if(drive.installed && !drive.failed)
            {
                // Unknown temperature is explicitly unclassified, not a
                // fictional assertion that the generator is at nominal heat.
                if(drive.hasCoreTemperature)
                    color=drive.coreTemperatureK>drive.safeCoreTemperatureK
                        ? hot:normal;
            }
            roundedOutlinePx(text,rx,cy-sq*0.5f,sq,sq,1*scale,
                1*scale,style.panelBorder,style.track);
            // Separate ER-5 radiator-pad status. A broken reactor core and
            // its damaged radiator are different physical faults.
            if(drive.hasPadIntegrity)
            {
                const float intact=std::clamp(drive.padIntegrity01,0.0f,1.0f);
                const glm::vec4 padColor=intact<=0.0f?dead:
                    intact<0.30f?hot:intact<0.70f?yellow:green;
                roundedOutlinePx(text,rx+sq+4*scale,cy-sq*0.5f,
                    7*scale,7*scale,1*scale,1*scale,
                    style.panelBorder,padColor);
            }
            // Ignition lamp: verified reactor state from server snapshot.
            // Green = energized, amber = 10 s restart, dark = switched off.
            const glm::vec4 ignitionColor=drive.ignitionStarting
                ? yellow : (drive.ignitionOn ? green : dead);
            roundedOutlinePx(text,rx+sq*0.5f-3.5f*scale,
                cy-sq*0.5f-10*scale,7*scale,7*scale,3*scale,
                1*scale,style.panelBorder,ignitionColor);
            if(drive.installed && drive.integrity01>0.0f)
            {
                const float fraction=std::clamp(
                    drive.integrity01,0.0f,1.0f);
                text.solidRectPx(rx+scale,
                    cy+sq*0.5f-scale-(sq-2*scale)*fraction,
                    sq-2*scale,(sq-2*scale)*fraction,color);
            }
            char temp[24];
            if(drive.hasCoreTemperature)
                std::snprintf(temp,sizeof(temp),"%.0f K",
                    drive.coreTemperatureK);
            else if(drive.simulatedPower)
                std::snprintf(temp,sizeof(temp),"SIM ER-5");
            else std::snprintf(temp,sizeof(temp),"-- K");
            text.textDrawPx(temp,rx-12*scale,cy-18*scale,
                std::max(9,static_cast<int>(10*scale)),style.label);
            // Actual measured heat balance, not an inferred temperature trend.
            // G = generator waste; P = heat sent to pads; S = stored thermal
            // power (positive => current overheating risk, even at fixed K).
            if(drive.hasCoreTemperature &&
               (!newtonian || presentation.drives.size()<=2))
            {
                char heat[96];
                std::snprintf(heat,sizeof(heat),
                    "G %.0f  P %.0f  S %+.0f MW",
                    drive.generatedHeatMW,drive.padHeatMW,
                    drive.storedHeatMW);
                const int powerFont=std::max(10,
                    static_cast<int>(std::lround(11.5f*scale)));
                text.textDrawPx(heat,x+27*scale,
                    cy+14*scale+powerFont/3.0f,
                    powerFont,
                    drive.storedHeatMW>0.1f?hot:style.label);
                // Remaining headroom to the safe core temperature, in K.
                char margin[32];
                std::snprintf(margin,sizeof(margin),"%.0f K left",
                    drive.safeCoreTemperatureK-drive.coreTemperatureK);
                if(!newtonian || presentation.drives.size()<=2)
                    text.textDrawPx(margin,newtonian
                        ?x+12*scale:x+176*scale,cy-18*scale,
                        std::max(9,static_cast<int>(9*scale)),
                        drive.safeCoreTemperatureK<=drive.coreTemperatureK
                            ?hot:style.label);
            }
            // Tight Y/X columns: keep the factual G/P/S readings, but
            // use one compact label on the LEFT of each main bar instead
            // of a 32px-tall secondary line that hit the next generator.
            if(newtonian && presentation.drives.size()>2 &&
                drive.hasCoreTemperature)
            {
                char heat[80];
                std::snprintf(heat,sizeof(heat),"G%.0f/P%.0f/S%+.0f",
                    drive.generatedHeatMW,drive.padHeatMW,
                    drive.storedHeatMW);
                text.textDrawPx(heat,x+12*scale,cy+3*scale,
                    std::max(8,static_cast<int>(9*scale)),
                    drive.storedHeatMW>0.1f?hot:style.label);
            }
            if(newtonian)
            {
                if(!drive.poweredEngineSlots.empty())
                    drawMotor(drive.poweredEngineSlots.front(),
                        rx,cy,false);
            }
            else
            {
                for(const auto& id:drive.poweredEngineSlots)
                {
                    const auto* bar=findBar(id);
                    if(!bar)continue;
                    // Assisted: aft motor LEFT, fore motor RIGHT.
                    // Both thrust fills start at the reactor and move outward.
                    if(bar->fillDirection==
                        game::presentation::EngineThrustFillDirection::BottomUp)
                        drawMotor(id,rx,cy,false);
                    else drawMotor(id,rx+sq,cy,true);
                }
            }
        }
        if(newtonian)
        {
            const float dotY=y+h-30*scale;
            // Eight physical display positions are reserved on all Newtonian
            // layouts. Uninstalled positions remain dark; no fake healthy
            // thrusters are added to Newtonian-I's four-cluster topology.
            constexpr std::size_t count=8;
            for(std::size_t i=0;i<count;++i)
            {
                const auto* rcs=i<presentation.rcsSlots.size()
                    ? &presentation.rcsSlots[i] : nullptr;
                const float cx=x+14*scale+
                    (i+0.5f)*(w-28*scale)/static_cast<float>(count);
                const glm::vec4 color=!rcs || !rcs->known?style.track:
                    rcs->integrity01<=0.0f?dead:
                    rcs->firingKnown && rcs->firing01>0.005f
                        ? normal:
                    rcs->integrity01<0.5f?hot:
                    rcs->integrity01<0.9f?yellow:green;
                roundedOutlinePx(text,cx-4*scale,dotY-4*scale,
                    8*scale,8*scale,4*scale,1*scale,
                    style.panelBorder,color);
            }

            // Gas is a CLOSED argon loop. This strip is ACTIVE manifold
            // pressure (available flow), not disappearing propellant mass.
            // Charge rises only when the ER-5-driven return compressor runs;
            // damage and no-power come from the same server cockpit sample.
            const float barY=y+h-12.0f*scale;
            const float barX=x+42.0f*scale;
            const float barW=w-88.0f*scale;
            const float barH=6.0f*scale;
            const float pressure=std::clamp(
                presentation.rcsManifold01,0.0f,1.0f);
            roundedOutlinePx(text,barX,barY,barW,barH,2*scale,
                1*scale,style.panelBorder,style.track);
            if(presentation.rcsManifoldKnown)
            {
                const bool refilling=
                    presentation.rcsNetFlow01PerSecond>1.0e-5f;
                const bool draining=
                    presentation.rcsNetFlow01PerSecond < -1.0e-5f;
                const glm::vec4 fillColor=
                    presentation.rcsManifoldDepleted?hot:
                    refilling?green:yellow;
                const float fillW=std::max(
                    0.0f,(barW-2*scale)*pressure);
                if(fillW>0.0f)
                    text.solidRectPx(barX+scale,barY+scale,fillW,
                        std::max(1.0f,barH-2*scale),fillColor);
                text.textDrawPx(refilling?"AR+":draining?"AR-":"AR",
                    x+10*scale,
                    y+h-7*scale,
                    std::max(9,static_cast<int>(10*scale)),style.label);
                char percent[16];
                std::snprintf(percent,sizeof(percent),"%.0f%%",
                    pressure*100.0f);
                text.textDrawPx(percent,x+w-41*scale,y+h-7*scale,
                    std::max(9,static_cast<int>(10*scale)),
                    presentation.rcsManifoldDepleted?hot:style.label);
            }
            else
            {
                text.textDrawPx("AR --",x+10*scale,y+h-7*scale,
                    std::max(9,static_cast<int>(10*scale)),style.track);
            }
        }
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
            labelWidth + iconSize + 7.0f * scale,
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
        contentX,
        contentY,
        iconSize,
        presentation.warning
            ? withAlpha(style.warning, pulse)
            : style.idleWarningIcon
    );

    // Fault icon and construction label share one header row. Keep the
    // existing bar baseline and panel height unchanged.
    const float labelY = contentY +
        (iconSize - static_cast<float>(labelPx)) * 0.5f;

    text.textDrawPx(
        presentation.modelLabel,
        contentX + iconSize + 7.0f * scale,
        labelY,
        labelPx,
        style.label
    );

    const float barsY = panelY + panelHeight - padding - barHeight;

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
        barsWidth,
        text.measureTextPx(engines.modelLabel,labelPx)+iconSize+7.0f*scale
    })+2.0f*padding;
    const float h=padding+iconSize+iconGap+static_cast<float>(labelPx)+
        labelGap+barHeight+padding;
    const float w=142.0f*scale;
    const float displayH=engines.drives.empty()?h:165.0f*scale;
    const float gap=9.0f*scale;
    const float reactorX=x+(engines.drives.empty()?engineWidth:260.0f*scale)+gap;
    const float coolantX=reactorX+w+gap;
    const float y=bottom-displayH;
    auto panel=[&](float px) {
        roundedOutlinePx(text,px,y,w,displayH,7.0f*scale,1.0f*scale,
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
    const float chainY=y+19*scale;
    // Fill the available reactor panel width proportionally. The two fault
    // lamps belong BELOW the pump and coolant icons, not at the right edge.
    const float chainLeft=reactorX+10*scale;
    const float chainRight=reactorX+w-10*scale;
    const float unit=(chainRight-chainLeft)/(101.0f*scale);
    const auto cx=[&](float v){return chainLeft+v*scale*unit;};
    const auto cw=[&](float v){return v*scale*unit;};
    const float r=std::clamp(static_cast<float>(
        status.reactor.criticalTemp>0
            ? status.reactor.temperature/status.reactor.criticalTemp : 0),
        0.0f,1.0f);
    roundedOutlinePx(text,cx(0),chainY,cw(15),37*scale,
        3*scale,1*scale,amber,style.track);
    text.solidRectPx(cx(2),chainY+(35-31*r)*scale,
        cw(11),31*r*scale,r>0.85f?warn:cyan);
    text.solidRectPx(cx(15),chainY+17*scale,cw(17),2*scale,amber);
    roundedOutlinePx(text,cx(32),chainY+9*scale,
        cw(22),20*scale,2*scale,1*scale,amber,style.track);
    const bool pumping=status.cooling.reactorPumpActive &&
        status.cooling.allocatedPowerMW>0.0;
    if(pumping)
        text.solidRectPx(cx(36),chainY+13*scale,
            cw(14),12*scale,style.capacity);
    text.solidRectPx(cx(54),chainY+17*scale,cw(14),2*scale,amber);
    roundedOutlinePx(text,cx(68),chainY+9*scale,
        cw(23),20*scale,3*scale,1*scale,cyan,style.track);
    text.solidRectPx(cx(71),chainY+19*scale,cw(17),7*scale,cyan);
    const bool radiatorAlarm=status.cooling.panelThermalCritical ||
        status.cooling.criticalPanelCount>0;
    const bool reactorAlarm=status.reactor.status>=1 ||
        status.reactor.integrity<0.95 ||
        status.reactor.temperature<status.reactor.minOperatingTemperatureK;
    const float blink=warningPulse(style);
    if(reactorAlarm)
        text.solidRectPx(cx(39),chainY+32*scale,
            cw(7),5*scale,withAlpha(warn,blink));
    if(radiatorAlarm)
        text.solidRectPx(cx(76),chainY+32*scale,
            cw(7),5*scale,withAlpha(warn,blink));
    std::snprintf(buffer,sizeof(buffer),"%.0f K",status.reactor.temperature);
    text.textDrawPx(buffer,reactorX+8*scale,y+displayH-52*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"%.0f MW  %.0f%%",
        status.reactor.outputMW,status.reactor.efficiency01*100.0);
    text.textDrawPx(buffer,reactorX+8*scale,y+displayH-34*scale,caption,amber);
    text.textDrawPx("PWR / EFF",reactorX+8*scale,y+displayH-16*scale,tiny,amber);

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
    text.textDrawPx(buffer,coolantX+8*scale,y+displayH-63*scale,tiny,amber);
    std::snprintf(buffer,sizeof(buffer),"OK %.0f%%",
        status.cooling.panelHealthPercent);
    text.textDrawPx(buffer,coolantX+8*scale,y+displayH-47*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"IN %.1f MW",
        status.cooling.heatIntoCoolantMW);
    text.textDrawPx(buffer,coolantX+8*scale,y+displayH-31*scale,caption,amber);
    std::snprintf(buffer,sizeof(buffer),"OUT %.1f MW",
        status.cooling.radiatedPowerMW);
    text.textDrawPx(buffer,coolantX+8*scale,y+displayH-15*scale,caption,amber);
}

} // namespace render::cockpit
