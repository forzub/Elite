# UI component architecture (author's brief 2026-11-24)

## Model

- **One resolved geometry.** Every `UIComponent` has one `ui::UIRect` (`resolvedRect()`), in the UI screen's pixels
  (TextRenderer's space for the game viewport, origin top-left). The same rect is used to draw, hit-test, lay out the
  children and show the debug bounds. Hand-made hit boxes beside the drawing are forbidden.
- **Layout.** By default it uses `position` / `size` (fractions of the parent's rect) and `anchor`
  (`UIComponent::layout` → `computeLayout`). A composite component with its own arrangement places its children with
  `setResolvedRect` (for example `CockpitRadioWidget::layoutPanel`). `UIText` uses the same layout, so it honours the
  anchor and size. There is one `UIAnchor`; the unused `game::UIAnchor` was removed.
- **Input.** `ui::UIInputRouter` routes pointer events. It hit-tests the tree front to back (the last child drawn is hit
  first) and delivers the event to the deepest interactive child. Events: `onPointerDown/Up/Move`, `onScroll`. The result
  is `Ignored`, `Handled` or `HandledCapture`.
- **Pointer capture** belongs to the router. A press answered with `HandledCapture` owns the pointer until it is released:
  moves outside the component's rect still reach it (for example a slider drag). Widgets keep no drag flags.
- **Platform boundary.** `src/ui/input/GlfwPointerSource.cpp` is the only UI place that reads GLFW. It converts the
  coordinates once (`UIInputRouter::windowToUi`): window → framebuffer scale → the letterboxed game viewport (GL
  bottom-left to top-left) → UI screen.
- **Function vs presentation.** A widget is not the device. It receives read-only presentation state and emits actions.
  The caller applies them, for example `RadioPresentation` / `RadioAction` → `SpaceState::updateCockpitRadioUi` →
  `audio::CockpitRadio`.
- **Frame order.** First layout + routed input + actions (the update step), then `draw(UIDrawContext&)`. Draw is pure
  presentation: no GLFW, no application calls.
- **Style vs behaviour.** Sizes and colours live in plain style structs (`ui::UIWidgetStyle`, `RadioWidgetStyle`).
  Data-driven style files come later.
- **Composite.** A child widget exists only where something has its own layout, state, interaction or reusable look.
  Backgrounds and frames are drawn by the parent.

## Widgets

`UILabel`, `UIButton`, `UISlider` (`src/ui/components/UIWidgets.h`; their drawing is in `UIWidgetsDraw.cpp`).
The magnitola (`src/render/cockpit/CockpitRadioWidget*`) is the reference composite:

- caption
- power, source
- previous, display, next
- remove (playlist only), header, signal indicator
- volume −, slider, volume +

## Invariants (tests)

- `tests/navigation_runtime/UiRadioWidgetTests.cpp` (ctest `ui_radio_widget`). It lays the widget out at 1920×1080,
  1280×720 and 3840×2160, with and without the playlist mode, and checks:
  - a click at the centre of every drawn child gives exactly its action;
  - hidden children take no click;
  - the slider keeps the capture outside its rect;
  - `windowToUi` inverts a 21:9 letterbox and a HiDPI + bottom letterbox.
- `tests/architecture_contracts/check_ui_input_boundary.py`:
  - no `glfwGetCursorPos` / `glfwGetMouseButton` in `src/render`, `src/ui`, `src/game/system_map` outside the listed
    boundary and legacy files;
  - the radio has no hand-made hit boxes;
  - its drawing file emits no actions and reads no input.
- `ELITE_DEV_UI_BOUNDS=1` draws every resolved rect with its id. The hovered rect is green, the captured one red.

## Inventory (where the rest stands)

| Widget | Class | Next |
|---|---|---|
| Radar | A (already a `UIComponent`) | move to `layout` + `draw` |
| Service panels (`ServicePanelsRenderer m_hits`) | B | the hit list maps onto children; its cursor conversion skips the framebuffer scale / GL flip |
| Map panel (`InSessionPresentationRenderer::makePanelLayout`) | B | the same conversion gap |
| Engine indicators | C (legacy renderer) | its `GroupFrame` is the TRANSITIONAL anchor of the radio (`EngineGroupAnchor`); a `CockpitBottomLeftCluster` container owns both later |
| HUD navigation indicators | C | — |
| Docking terminal | C (the model is already separate) | a renderer adapter |
| System map input | C/D | reads GLFW itself (listed in the boundary check) |
