#pragma once

// THE platform boundary of the UI pointer (the UI component pass): the ONE place UI input reads GLFW - the cursor, the
// left button and the window / framebuffer sizes - and hands a ui::PointerSample in the UI screen to ui::UIInputRouter.
// Concrete widgets never read GLFW (tests/architecture_contracts/check_ui_input_boundary.py).

#include "render/types/Viewport.h"
#include "src/ui/components/UIInputRouter.h"

namespace ui
{
// the pointer now, in the UI screen of `viewport` (uiW x uiH: TextRenderer's screen for that viewport)
PointerSample readGlfwPointer(const Viewport& viewport, float uiW, float uiH);
} // namespace ui
