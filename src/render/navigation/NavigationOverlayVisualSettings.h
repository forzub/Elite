#pragma once

namespace render::navigation
{

struct NavigationOverlayVisualSettings
{
    /*
        Shared coordinate block and footer typography.

        Values are expressed for a 1080 px-high viewport and are scaled
        proportionally for other viewport sizes.
    */
    float referenceHeightPx = 1080.0f;
    float minimumScreenScale = 0.72f;
    float maximumScreenScale = 1.35f;

    /*
        The previous coordinate block used 2.0. 1.333333 makes it
        exactly 1.5 times smaller without changing the font itself.
    */
    float coordinateTextScale = 1.333333f;
    float titleBaseScale = 0.72f;
    float bodyBaseScale = 0.66f;

    /*
        Combined with coordinateTextScale this makes the footer exactly
        twice smaller than the previous bodyScale * 1.85 value.
    */
    float footerRelativeToBodyScale = 1.3875f;

    float leftPx = 18.0f;
    float contentIndentPx = 10.0f;
    float topBaselinePx = 38.0f;
    float lineStepPx = 22.666667f;
    float blockGapPx = 9.333333f;
    float footerBottomPx = 28.0f;

    float levelZeroButtonWidthPx = 190.0f;
    float levelZeroButtonHeightPx = 38.0f;
    float levelZeroButtonRightPx = 18.0f;
    float levelZeroButtonBottomPx = 10.0f;
    float levelZeroButtonBaselinePx = 27.0f;
    float trackButtonWidthPx = 150.0f;
    float overlayButtonGapPx = 10.0f;

    float levelAnnouncementScale = 2.15f;
    float levelAnnouncementRightPx = 32.0f;
    float levelAnnouncementBaselinePx = 62.0f;
};

} // namespace render::navigation
