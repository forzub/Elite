#pragma once

#include "render/types/Viewport.h"

struct LetterboxedViewport
{
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
};

inline LetterboxedViewport makeLetterboxedViewport(
    int framebufferWidth,
    int framebufferHeight,
    float targetAspect
)
{
    LetterboxedViewport out;

    if (framebufferWidth <= 0 || framebufferHeight <= 0 || targetAspect <= 0.0f)
        return out;

    const float windowAspect =
        static_cast<float>(framebufferWidth) /
        static_cast<float>(framebufferHeight);

    if (windowAspect > targetAspect)
    {
        // Окно слишком широкое: чёрные поля слева/справа.
        out.height = framebufferHeight;
        out.width = static_cast<int>(
            static_cast<float>(framebufferHeight) * targetAspect
        );

        out.x = (framebufferWidth - out.width) / 2;
        out.y = 0;
    }
    else
    {
        // Окно слишком высокое: чёрные поля сверху/снизу.
        out.width = framebufferWidth;
        out.height = static_cast<int>(
            static_cast<float>(framebufferWidth) / targetAspect
        );

        out.x = 0;
        out.y = (framebufferHeight - out.height) / 2;
    }

    if (out.width < 1) out.width = 1;
    if (out.height < 1) out.height = 1;

    return out;
}

inline Viewport toViewport(const LetterboxedViewport& lb)
{
    Viewport vp;
    vp.width = lb.width;
    vp.height = lb.height;
    vp.x = lb.x;
    vp.y = lb.y;
    return vp;
}