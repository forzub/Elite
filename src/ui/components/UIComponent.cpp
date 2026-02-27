#include "UIComponent.h"


void UIComponent::renderChildren(
    const Viewport& vp,
    float x,
    float y,
    float w,
    float h
)
{
    for (auto& child : children)
        if (child->visible)
            child->render(vp, x, y, w, h);
}




void UIComponent::render(
    const Viewport& vp,
    float parentX,
    float parentY,
    float parentW,
    float parentH
)
{
    float px, py, pw, ph;
    computeLayout(parentX, parentY, parentW, parentH, px, py, pw, ph);
    renderChildren(vp, px, py, pw, ph);
}




void UIComponent::computeLayout(
    float parentX,
    float parentY,
    float parentW,
    float parentH,
    float& outX,
    float& outY,
    float& outW,
    float& outH
) const
{
    float px = parentX + position.x * parentW;
    float py = parentY + position.y * parentH;
    float pw = size.x * parentW;
    float ph = size.y * parentH;

    switch (anchor)
    {
        case UIAnchor::TopLeft:
            break;

        case UIAnchor::TopRight:
            px -= pw;
            break;

        case UIAnchor::BottomLeft:
            py -= ph;
            break;

        case UIAnchor::BottomRight:
            px -= pw;
            py -= ph;
            break;

        case UIAnchor::Center:
            px -= pw * 0.5f;
            py -= ph * 0.5f;
            break;
    }

    outX = px;
    outY = py;
    outW = pw;
    outH = ph;
}