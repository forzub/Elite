#include "src/render/navigation/NavigationCoordinateOverlay.h"

#include <algorithm>
#include <filesystem>

#include <glm/glm.hpp>

#include "render/Font.h"
#include "render/HUD/TextRenderer.h"

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
            "[  LEVEL 0  ]";

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
                ? "[ TRACK ON ]"
                : "[  TRACK  ]";

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
