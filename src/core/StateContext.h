#pragma once

#include "render/types/Viewport.h"

struct Application;


struct StateContext
{
    Application* app = nullptr;
    float dt = 0.0f;

    Viewport viewport() const;
    
    float aspect() const;

};

extern StateContext* g_stateContext;
