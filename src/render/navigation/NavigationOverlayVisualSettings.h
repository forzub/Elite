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

    /*
        Map buttons: a transparent wire bar with rounded corners, the text
        or arrow inside. One row at the bottom right, right to left:
        LEVEL 0, TRACK (System), up (one level up), back (previous map).
    */
    float levelZeroButtonWidthPx = 170.0f;
    float levelZeroButtonHeightPx = 36.0f;
    float levelZeroButtonRightPx = 18.0f;
    float levelZeroButtonBottomPx = 34.0f;
    float levelZeroButtonBaselinePx = 25.0f;
    float trackButtonWidthPx = 150.0f;
    float overlayButtonGapPx = 10.0f;
    float buttonCornerRadiusPx = 9.0f;
    float buttonLineWidthPx = 1.4f;

    // Hierarchy: "up" left of LEVEL 0 (System: left of TRACK), "back" left
    // of "up"; breadcrumbs above the footer.
    float upButtonWidthPx = 60.0f;
    float backButtonWidthPx = 60.0f;
    float breadcrumbAboveFooterPx = 30.0f;
    float breadcrumbGapPx = 8.0f;

    float levelAnnouncementScale = 2.15f;
    float levelAnnouncementRightPx = 32.0f;
    float levelAnnouncementBaselinePx = 62.0f;
};

} // namespace render::navigation
