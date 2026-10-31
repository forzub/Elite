#include "src/render/navigation/NavigationCoordinateOverlay.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "render/Font.h"
#include "render/HUD/TextRenderer.h"
#include "src/render/HUD/HudPrimitiveBatch.h"

namespace render::navigation
{
namespace
{

glm::vec4 colorForRole(
    NavigationCoordinateRole role
)
{
    switch (role)
    {
        case NavigationCoordinateRole::Player:
            return glm::vec4(
                0.82f,
                0.72f,
                0.35f,
                0.72f
            );

        case NavigationCoordinateRole::Selected:
            return glm::vec4(
                0.32f,
                0.55f,
                0.82f,
                0.68f
            );

        case NavigationCoordinateRole::Hovered:
            return glm::vec4(
                0.42f,
                0.76f,
                0.86f,
                0.68f
            );
    }

    return glm::vec4(
        0.72f,
        0.78f,
        0.84f,
        0.68f
    );
}

// A rounded rectangle: optional fill (triangle fan), then the wire outline.
void roundedFrame(
    render::hud::HudPrimitiveBatch& batch,
    const NavigationOverlayButtonBounds& b,
    float radius,
    float lineWidth,
    const glm::vec4& line,
    const glm::vec4& fill
)
{
    constexpr int ArcSegments = 6;
    radius = std::min({radius, (b.right - b.left) * 0.5f, (b.bottom - b.top) * 0.5f});
    std::vector<glm::vec2> outline;
    const glm::vec2 centres[4] = {
        {b.right - radius, b.top + radius},
        {b.right - radius, b.bottom - radius},
        {b.left + radius, b.bottom - radius},
        {b.left + radius, b.top + radius}};
    for (int corner = 0; corner < 4; ++corner)
    {
        const float start = -1.5707963f + 1.5707963f * static_cast<float>(corner);
        for (int i = 0; i <= ArcSegments; ++i)
        {
            const float a = start + 1.5707963f * static_cast<float>(i) / static_cast<float>(ArcSegments);
            outline.push_back(centres[corner] + glm::vec2(std::cos(a), std::sin(a)) * radius);
        }
    }
    if (fill.a > 0.0f)
    {
        const glm::vec2 centre((b.left + b.right) * 0.5f, (b.top + b.bottom) * 0.5f);
        for (std::size_t i = 0; i < outline.size(); ++i)
            batch.filledTriangle(centre, outline[i], outline[(i + 1) % outline.size()], fill);
    }
    for (std::size_t i = 0; i < outline.size(); ++i)
        batch.line(outline[i], outline[(i + 1) % outline.size()], lineWidth, line);
}

} // namespace

NavigationCoordinateOverlay::
NavigationCoordinateOverlay() = default;

NavigationCoordinateOverlay::
~NavigationCoordinateOverlay() = default;

NavigationOverlayButtonBounds
NavigationCoordinateOverlay::levelZeroButtonBounds(
    const Viewport& viewport
)
{
    const NavigationOverlayVisualSettings visuals;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                visuals.referenceHeightPx,
            visuals.minimumScreenScale,
            visuals.maximumScreenScale
        );

    const float width =
        visuals.levelZeroButtonWidthPx *
        screenScale;

    const float height =
        visuals.levelZeroButtonHeightPx *
        screenScale;

    const float right =
        static_cast<float>(viewport.width) -
        visuals.levelZeroButtonRightPx *
        screenScale;

    const float bottom =
        static_cast<float>(viewport.height) -
        visuals.levelZeroButtonBottomPx *
        screenScale;

    return {
        right - width,
        bottom - height,
        right,
        bottom
    };
}


NavigationOverlayButtonBounds
NavigationCoordinateOverlay::trackButtonBounds(
    const Viewport& viewport
)
{
    const NavigationOverlayVisualSettings visuals;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                visuals.referenceHeightPx,
            visuals.minimumScreenScale,
            visuals.maximumScreenScale
        );

    const NavigationOverlayButtonBounds levelBounds =
        levelZeroButtonBounds(viewport);

    const float width =
        visuals.trackButtonWidthPx * screenScale;

    const float gap =
        visuals.overlayButtonGapPx * screenScale;

    return {
        levelBounds.left - gap - width,
        levelBounds.top,
        levelBounds.left - gap,
        levelBounds.bottom
    };
}

NavigationOverlayButtonBounds
NavigationCoordinateOverlay::upButtonBounds(
    const Viewport& viewport,
    bool besideTrack
)
{
    const NavigationOverlayVisualSettings visuals;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                visuals.referenceHeightPx,
            visuals.minimumScreenScale,
            visuals.maximumScreenScale
        );

    const NavigationOverlayButtonBounds levelBounds =
        besideTrack ? trackButtonBounds(viewport) : levelZeroButtonBounds(viewport);

    const float width =
        visuals.upButtonWidthPx * screenScale;

    const float gap =
        visuals.overlayButtonGapPx * screenScale;

    return {
        levelBounds.left - gap - width,
        levelBounds.top,
        levelBounds.left - gap,
        levelBounds.bottom
    };
}

NavigationOverlayButtonBounds
NavigationCoordinateOverlay::backButtonBounds(
    const Viewport& viewport,
    const NavigationOverlayButtonBounds* rightNeighbour
)
{
    const NavigationOverlayVisualSettings visuals;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                visuals.referenceHeightPx,
            visuals.minimumScreenScale,
            visuals.maximumScreenScale
        );

    const NavigationOverlayButtonBounds anchor = levelZeroButtonBounds(viewport);
    const float width = visuals.backButtonWidthPx * screenScale;
    const float gap = visuals.overlayButtonGapPx * screenScale;
    const float right = rightNeighbour ? rightNeighbour->left - gap : anchor.right;
    return {right - width, anchor.top, right, anchor.bottom};
}

float NavigationCoordinateOverlay::screenScaleFor(
    const Viewport& viewport
) const
{
    return std::clamp(
        static_cast<float>(viewport.height) / m_visuals.referenceHeightPx,
        m_visuals.minimumScreenScale,
        m_visuals.maximumScreenScale
    );
}

void NavigationCoordinateOverlay::ensureBatch()
{
    if (!m_batch)
        m_batch = std::make_unique<render::hud::HudPrimitiveBatch>();
}

void NavigationCoordinateOverlay::drawButtons(
    const Viewport& viewport,
    const std::vector<NavigationOverlayButton>& buttons
)
{
    if (buttons.empty())
        return;
    ensureFont();
    ensureBatch();
    if (!m_font)
        return;

    const float screenScale = screenScaleFor(viewport);
    const float scale = m_visuals.bodyBaseScale * m_visuals.coordinateTextScale * screenScale;

    m_batch->begin(viewport.width, viewport.height);
    for (const auto& button : buttons)
    {
        glm::vec4 line {0.44f, 0.62f, 0.74f, 0.62f};
        glm::vec4 fill {0.10f, 0.20f, 0.28f, 0.10f};
        if (!button.enabled)
        {
            line = {0.30f, 0.36f, 0.40f, 0.32f};
            fill.a = 0.0f;
        }
        else if (button.active)
        {
            line = button.hovered ? glm::vec4(0.96f, 0.82f, 0.42f, 0.95f) : glm::vec4(0.88f, 0.70f, 0.30f, 0.80f);
            fill = {0.40f, 0.30f, 0.10f, button.hovered ? 0.26f : 0.16f};
        }
        else if (button.hovered)
        {
            line = {0.64f, 0.84f, 0.96f, 0.92f};
            fill = {0.20f, 0.42f, 0.56f, 0.24f};
        }
        roundedFrame(*m_batch, button.bounds, m_visuals.buttonCornerRadiusPx * screenScale,
                     m_visuals.buttonLineWidthPx * screenScale, line, fill);
    }
    m_batch->flush();

    TextRenderer& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    for (const auto& button : buttons)
    {
        glm::vec4 color {0.56f, 0.76f, 0.88f, 0.84f};
        if (!button.enabled)
            color = {0.30f, 0.36f, 0.40f, 0.40f};
        else if (button.active)
            color = button.hovered ? glm::vec4(0.98f, 0.86f, 0.48f, 1.0f) : glm::vec4(0.90f, 0.74f, 0.34f, 0.92f);
        else if (button.hovered)
            color = {0.74f, 0.90f, 1.0f, 0.98f};
        const float width = m_font->measureText(button.label) * scale;
        text.textDraw(
            *m_font,
            button.label,
            button.bounds.left + (button.bounds.right - button.bounds.left - width) * 0.5f,
            button.bounds.top + m_visuals.levelZeroButtonBaselinePx * screenScale,
            color,
            scale
        );
    }
    text.endFrame();
}

void NavigationCoordinateOverlay::drawContextMenu(
    const Viewport& viewport,
    const NavigationContextMenuView& menu
)
{
    ensureFont();
    ensureBatch();
    if (!m_font)
        return;

    const float screenScale = screenScaleFor(viewport);
    const float scale = m_visuals.bodyBaseScale * m_visuals.coordinateTextScale * screenScale * 0.92f;
    const NavigationOverlayButtonBounds& b = menu.bounds;

    m_batch->begin(viewport.width, viewport.height);
    roundedFrame(*m_batch, b, m_visuals.buttonCornerRadiusPx * screenScale, m_visuals.buttonLineWidthPx * screenScale,
                 glm::vec4(0.44f, 0.66f, 0.82f, 0.70f), glm::vec4(0.02f, 0.06f, 0.09f, 0.78f));
    // Title separator.
    const float separatorY = b.top + menu.headerHeightPx;
    m_batch->line({b.left + 8.0f, separatorY}, {b.right - 8.0f, separatorY}, 1.0f,
                  glm::vec4(0.40f, 0.60f, 0.74f, 0.40f));
    if (menu.hovered >= 0 && menu.hovered < static_cast<int>(menu.rows.size()))
    {
        const float top = separatorY + 3.0f + menu.rowHeightPx * static_cast<float>(menu.hovered);
        roundedFrame(*m_batch, {b.left + 4.0f, top, b.right - 4.0f, top + menu.rowHeightPx - 2.0f},
                     6.0f * screenScale, 1.0f, glm::vec4(0.58f, 0.80f, 0.94f, 0.70f),
                     glm::vec4(0.20f, 0.42f, 0.56f, 0.30f));
    }
    m_batch->flush();

    TextRenderer& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    text.textDraw(*m_font, menu.title, b.left + 12.0f, b.top + menu.headerHeightPx * 0.70f,
                  glm::vec4(0.90f, 0.80f, 0.48f, 0.95f), scale);
    for (std::size_t i = 0; i < menu.rows.size(); ++i)
    {
        const bool enabled = i >= menu.enabled.size() || menu.enabled[i];
        const bool hovered = static_cast<int>(i) == menu.hovered;
        const glm::vec4 color =
            !enabled ? glm::vec4(0.34f, 0.40f, 0.44f, 0.50f)
                     : (hovered ? glm::vec4(0.82f, 0.94f, 1.0f, 1.0f) : glm::vec4(0.60f, 0.80f, 0.92f, 0.90f));
        const float top = separatorY + 3.0f + menu.rowHeightPx * static_cast<float>(i);
        text.textDraw(*m_font, menu.rows[i], b.left + 16.0f, top + menu.rowHeightPx * 0.70f, color, scale);
    }
    text.endFrame();
}

namespace
{
    const char* const BreadcrumbSeparator = "  ›  ";
}

std::vector<NavigationOverlayButtonBounds>
NavigationCoordinateOverlay::breadcrumbBounds(
    const Viewport& viewport,
    const std::vector<std::string>& crumbs
)
{
    std::vector<NavigationOverlayButtonBounds> result;
    ensureFont();
    if (!m_font || viewport.height <= 0)
        return result;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                m_visuals.referenceHeightPx,
            m_visuals.minimumScreenScale,
            m_visuals.maximumScreenScale
        );
    const float scale =
        m_visuals.bodyBaseScale *
        m_visuals.coordinateTextScale *
        screenScale;
    const float baseline =
        static_cast<float>(viewport.height) -
        (m_visuals.footerBottomPx + m_visuals.breadcrumbAboveFooterPx) *
            screenScale;
    const float height = m_visuals.lineStepPx * screenScale;
    const float separator = m_font->measureText(BreadcrumbSeparator) * scale;

    float x = m_visuals.leftPx * screenScale;
    for (const std::string& crumb : crumbs)
    {
        const float width = m_font->measureText(crumb) * scale;
        result.push_back({x, baseline - height * 0.8f, x + width, baseline + height * 0.3f});
        x += width + separator;
    }
    return result;
}

void NavigationCoordinateOverlay::drawBreadcrumbs(
    const Viewport& viewport,
    const std::vector<std::string>& crumbs,
    int hoveredCrumb
)
{
    const auto bounds = breadcrumbBounds(viewport, crumbs);
    if (!m_font)
        return;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                m_visuals.referenceHeightPx,
            m_visuals.minimumScreenScale,
            m_visuals.maximumScreenScale
        );
    const float scale =
        m_visuals.bodyBaseScale *
        m_visuals.coordinateTextScale *
        screenScale;

    TextRenderer& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    const glm::vec4 normal {0.44f, 0.62f, 0.74f, 0.70f};
    const glm::vec4 hovered {0.64f, 0.84f, 0.96f, 0.95f};
    const glm::vec4 current {0.90f, 0.78f, 0.42f, 0.92f};
    const glm::vec4 separatorColor {0.40f, 0.54f, 0.64f, 0.55f};
    const float baseline =
        static_cast<float>(viewport.height) -
        (m_visuals.footerBottomPx + m_visuals.breadcrumbAboveFooterPx) *
            screenScale;

    for (std::size_t i = 0; i < crumbs.size() && i < bounds.size(); ++i)
    {
        const bool last = i + 1 == crumbs.size();
        const glm::vec4 color =
            last ? current : (static_cast<int>(i) == hoveredCrumb ? hovered : normal);
        text.textDraw(*m_font, crumbs[i], bounds[i].left, baseline, color, scale);
        if (!last)
            text.textDraw(*m_font, BreadcrumbSeparator, bounds[i].right, baseline, separatorColor, scale);
    }

    text.endFrame();
}

void NavigationCoordinateOverlay::ensureFont()
{
    if (m_font)
        return;

    /*
        В названиях могут быть латиница, кириллица
        и китайские иероглифы.
    */
    const std::vector<std::string> candidates =
    {
        "assets/fonts/NotoSansCJK-Regular.otf",
        "src/assets/fonts/NotoSansCJK-Regular.otf",
        "assets/fonts/Roboto-Medium.ttf",
        "src/assets/fonts/Roboto-Medium.ttf"
    };

    for (const std::string& path : candidates)
    {
        if (!std::filesystem::exists(path))
            continue;

        m_font =
            std::make_unique<Font>(
                path,
                18
            );

        return;
    }
}

void NavigationCoordinateOverlay::draw(
    const Viewport& viewport,
    const std::vector<NavigationCoordinateBlock>& blocks,
    const std::string& footerText,
    const std::string& levelAnnouncement,
    float levelAnnouncementAlpha,
    bool showLevelZeroButton,
    bool levelZeroButtonHovered,
    bool showTrackButton,
    bool trackButtonHovered,
    bool trackButtonActive,
    bool trackButtonEnabled
)
{
    if (viewport.width <= 0 ||
        viewport.height <= 0 ||
        (
            blocks.empty() &&
            footerText.empty() &&
            (
                levelAnnouncement.empty() ||
                levelAnnouncementAlpha <= 0.001f
            ) &&
            !showLevelZeroButton &&
            !showTrackButton
        ))
    {
        return;
    }

    ensureFont();

    if (!m_font)
        return;

    const float screenScale =
        std::clamp(
            static_cast<float>(viewport.height) /
                m_visuals.referenceHeightPx,
            m_visuals.minimumScreenScale,
            m_visuals.maximumScreenScale
        );

    const float navigationTextScale =
        m_visuals.coordinateTextScale;

    const float titleScale =
        m_visuals.titleBaseScale *
        navigationTextScale *
        screenScale;

    const float bodyScale =
        m_visuals.bodyBaseScale *
        navigationTextScale *
        screenScale;

    const float footerScale =
        bodyScale *
        m_visuals.footerRelativeToBodyScale;

    const float left =
        m_visuals.leftPx *
        screenScale;

    const float contentLeft =
        left +
        m_visuals.contentIndentPx *
        screenScale;

    float baselineY =
        m_visuals.topBaselinePx *
        screenScale;

    const float lineStep =
        m_visuals.lineStepPx *
        screenScale;

    const float blockGap =
        m_visuals.blockGapPx *
        screenScale;

    TextRenderer& text =
        TextRenderer::instance();

    text.beginFrameForViewport(
        viewport.width,
        viewport.height
    );

    for (const NavigationCoordinateBlock& block : blocks)
    {
        if (block.title.empty() &&
            block.regionNames.empty() &&
            block.addressLines.empty())
        {
            continue;
        }

        const glm::vec4 baseColor =
            colorForRole(
                block.role
            );

        glm::vec4 titleColor =
            baseColor;

        titleColor.a =
            std::min(
                1.0f,
                baseColor.a * 1.08f
            );

        glm::vec4 nameColor =
            baseColor;

        nameColor.a *=
            0.86f;

        glm::vec4 addressColor =
            baseColor;

        addressColor.a *=
            0.76f;

        if (!block.title.empty())
        {
            text.textDraw(
                *m_font,
                block.title + ":",
                left,
                baselineY,
                titleColor,
                titleScale
            );

            baselineY +=
                lineStep;
        }

        if (!block.regionNames.empty())
        {
            text.textDraw(
                *m_font,
                block.regionNames,
                contentLeft,
                baselineY,
                nameColor,
                bodyScale
            );

            baselineY +=
                lineStep;
        }

        for (const std::string& line :
             block.addressLines)
        {
            if (line.empty())
                continue;

            text.textDraw(
                *m_font,
                line,
                contentLeft,
                baselineY,
                addressColor,
                bodyScale
            );

            baselineY +=
                lineStep;
        }

        baselineY +=
            blockGap;
    }

    if (!footerText.empty())
    {
        const glm::vec4 footerColor {
            0.48f,
            0.61f,
            0.72f,
            0.58f
        };

        text.textDraw(
            *m_font,
            footerText,
            left,
            static_cast<float>(viewport.height) -
                m_visuals.footerBottomPx *
                    screenScale,
            footerColor,
            footerScale
        );
    }

    if (showLevelZeroButton)
    {
        const NavigationOverlayButtonBounds bounds =
            levelZeroButtonBounds(
                viewport
            );

        const glm::vec4 buttonColor =
            levelZeroButtonHovered
                ? glm::vec4(
                    0.64f,
                    0.84f,
                    0.96f,
                    0.92f
                  )
                : glm::vec4(
                    0.44f,
                    0.62f,
                    0.74f,
                    0.66f
                  );

        const float buttonScale =
            bodyScale;

        const std::string buttonText =
            "[  " + m_textProfile.level + " 0  ]";

        const float textWidth =
            m_font->measureText(
                buttonText
            ) *
            buttonScale;

        const float x =
            bounds.left +
            (
                bounds.right -
                bounds.left -
                textWidth
            ) *
            0.5f;

        const float y =
            bounds.top +
            m_visuals.levelZeroButtonBaselinePx *
            screenScale;

        text.textDraw(
            *m_font,
            buttonText,
            x,
            y,
            buttonColor,
            buttonScale
        );
    }

    if (showTrackButton)
    {
        const NavigationOverlayButtonBounds bounds =
            trackButtonBounds(
                viewport
            );

        glm::vec4 buttonColor;

        if (!trackButtonEnabled)
        {
            buttonColor = glm::vec4(
                0.30f,
                0.36f,
                0.40f,
                0.34f
            );
        }
        else if (trackButtonActive)
        {
            buttonColor = trackButtonHovered
                ? glm::vec4(
                    0.96f,
                    0.82f,
                    0.42f,
                    1.0f
                  )
                : glm::vec4(
                    0.88f,
                    0.70f,
                    0.30f,
                    0.88f
                  );
        }
        else
        {
            buttonColor = trackButtonHovered
                ? glm::vec4(
                    0.64f,
                    0.84f,
                    0.96f,
                    0.92f
                  )
                : glm::vec4(
                    0.44f,
                    0.62f,
                    0.74f,
                    0.66f
                  );
        }

        const float buttonScale =
            bodyScale;

        const std::string buttonText =
            trackButtonActive
                ? "[ " + m_textProfile.trackOn + " ]"
                : "[  " + m_textProfile.track + "  ]";

        const float textWidth =
            m_font->measureText(
                buttonText
            ) *
            buttonScale;

        const float x =
            bounds.left +
            (
                bounds.right -
                bounds.left -
                textWidth
            ) *
            0.5f;

        const float y =
            bounds.top +
            m_visuals.levelZeroButtonBaselinePx *
            screenScale;

        text.textDraw(
            *m_font,
            buttonText,
            x,
            y,
            buttonColor,
            buttonScale
        );
    }

    if (!levelAnnouncement.empty() &&
        levelAnnouncementAlpha > 0.001f)
    {
        glm::vec4 noticeColor {
            0.72f,
            0.84f,
            0.96f,
            std::clamp(
                levelAnnouncementAlpha,
                0.0f,
                1.0f
            ) * 0.90f
        };

        const float noticeScale =
            m_visuals.levelAnnouncementScale *
            screenScale;

        const float estimatedWidth =
            m_font->measureText(
                levelAnnouncement
            ) *
            noticeScale;

        const float noticeX =
            std::max(
                18.0f * screenScale,
                static_cast<float>(viewport.width) -
                    estimatedWidth -
                    m_visuals.levelAnnouncementRightPx *
                        screenScale
            );

        text.textDraw(
            *m_font,
            levelAnnouncement,
            noticeX,
            m_visuals.levelAnnouncementBaselinePx *
                screenScale,
            noticeColor,
            noticeScale
        );
    }

    text.endFrame();
}

} // namespace render::navigation
