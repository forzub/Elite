// #pragma once
// #include "UIComponent.h"

// class UIContainer : public UIComponent
// {
// public:
//     void add(std::unique_ptr<UIComponent> item);

//     void renderToTexture(const Viewport& vp) override;
//     void render(const Viewport& vp) override;

// private:
//     std::vector<std::unique_ptr<UIComponent>> items;
// };

#pragma once
#include "UIComponent.h"

class UIContainer : public UIComponent
{
};
