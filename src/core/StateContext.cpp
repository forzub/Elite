#include "core/StateContext.h"
#include "core/Application.h"

StateContext* g_stateContext = nullptr;

Viewport StateContext::viewport() const
{
    return app->viewport();
}

float StateContext::aspect() const
{
    Viewport vp = viewport();
        return (vp.height > 0)
            ? float(vp.width) / float(vp.height)
            : 1.0f;
}