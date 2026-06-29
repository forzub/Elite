#pragma once

#include "render/types/Viewport.h"

struct Application;
class HtmlUiManager;

struct StateContext
{
    Application* app = nullptr;
    float dt = 0.0f;

    Viewport viewport() const;
    
    float aspect() const;
    HtmlUiManager& htmlUi() const;


};

extern StateContext* g_stateContext;
