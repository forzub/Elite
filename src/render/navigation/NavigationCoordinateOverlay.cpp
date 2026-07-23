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
    float levelAnnouncementAlpha
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
            )
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
                1080.0f,
            0.72f,
            1.35f
        );

    /*
        Navigation coordinates are primary instruments. Keep the entire
        block at twice the previous size while retaining viewport scaling.
    */
    const float navigationTextScale =
        2.0f;

    const float titleScale =
        0.72f *
        navigationTextScale *
        screenScale;

    const float bodyScale =
        0.66f *
        navigationTextScale *
        screenScale;

    /*
        The footer is an orientation instrument, not secondary fine print.
        Keep it close to twice the old size while retaining viewport scaling.
    */
    const float footerScale =
        bodyScale *
        1.85f;

    const float left =
        18.0f *
        screenScale;

    const float contentLeft =
        left +
        10.0f *
        screenScale;

    float baselineY =
        38.0f *
        screenScale;

    const float lineStep =
        34.0f *
        screenScale;

    const float blockGap =
        14.0f *
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
                28.0f * screenScale,
            footerColor,
            footerScale
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
            2.15f *
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
                    32.0f * screenScale
            );

        text.textDraw(
            *m_font,
            levelAnnouncement,
            noticeX,
            62.0f * screenScale,
            noticeColor,
            noticeScale
        );
    }

    text.endFrame();
}

} // namespace render::navigation
