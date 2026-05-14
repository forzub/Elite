#pragma once

#include "ui/html/HtmlUiPanelId.h"

struct HtmlUiViewportState
{
    int width = 1280;
    int height = 720;
};

struct HtmlUiState
{
    HtmlUiPanelId activePanel = HtmlUiPanelId::None;
    HtmlUiViewportState viewport;
    bool debugVisible = false;
};