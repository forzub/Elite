#pragma once

#include <optional>

#include "src/ui/presentation/GamePresentationCoordinator.h"

namespace ui::presentation
{
// Plain F1-F12 are direct presentation selectors. Modifier chords are handled
// separately by their feature policy and never change this mapping.
std::optional<GameUiTarget> directTargetForFunctionKey(int functionKey);
}
