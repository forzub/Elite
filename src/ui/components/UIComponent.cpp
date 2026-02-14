// #include "UIComponent.h"

// void UIComponent::addChild(std::unique_ptr<UIComponent> child)
// {
//     children.push_back(std::move(child));
// }

// void UIComponent::renderToTexture(const Viewport& vp)
// {
//     for (auto& c : children)
//         if (c->visible)
//             c->renderToTexture(vp);
// }

// void UIComponent::render(const Viewport& vp)
// {
//     renderChildren(vp);
// }

// void UIComponent::renderChildren(const Viewport& vp)
// {
//     for (auto& c : children)
//         if (c->visible)
//             c->render(vp);
// }

// void UIComponent::render(
//     const Viewport& vp,
//     float parentX,
//     float parentY,
//     float parentW,
//     float parentH
// )
// {
//     float px = parentX + position.x * parentW;
//     float py = parentY + position.y * parentH;
//     float pw = size.x * parentW;
//     float ph = size.y * parentH;

//     for (auto& child : children)
//         if (child->visible)
//             child->render(vp, px, py, pw, ph);
// }

#include "UIComponent.h"

void UIComponent::render(
    const Viewport& vp,
    float parentX,
    float parentY,
    float parentW,
    float parentH
)
{
    float px = parentX + position.x * parentW;
    float py = parentY + position.y * parentH;
    float pw = size.x * parentW;
    float ph = size.y * parentH;

    renderChildren(vp, px, py, pw, ph);
}

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
