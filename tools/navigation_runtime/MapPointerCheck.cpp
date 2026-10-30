// DEV check: the map pointer contract for celestial cards (System Map) on the
// production MapObjectOverlayState. Expected:
//  E1 single click on a body: selected (activated), card NOT opened;
//  E2 double click: card opens; the next double click closes it; selection kept;
//  E3 single click on another body: the first body's card closes, the new one
//     stays closed; a double click on the other body opens only its card;
//  E4 the card never outlives the body's selection (empty-space click / other
//     object): closeCelestialCardsNotSelected closes it.
//
//   EliteMapPointerCheck -> PASS/FAIL lines, exit 0/1

#include <cstdio>
#include <string>

#include "src/game/system_map/MapObjectOverlay.h"

namespace
{
int failures = 0;
void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", what.c_str());
    if (!condition)
        ++failures;
}

using game::system_map::MapObjectInfoKind;
using game::system_map::MapObjectOverlayFrame;
using game::system_map::MapObjectOverlayItem;

MapObjectOverlayItem body(const std::string& id, double x)
{
    MapObjectOverlayItem item;
    item.objectId = "celestial:" + id;
    item.semanticTargetId = id;
    item.name = id;
    item.infoKind = MapObjectInfoKind::Celestial;
    item.screenPx = {x, 300.0};
    item.hitRadiusPx = 14.0;
    item.visible = true;
    item.physicalSizeMeters = 1.0e7;
    return item;
}
} // namespace

int main()
{
    game::system_map::MapObjectOverlayState overlay;
    MapObjectOverlayFrame frame;
    frame.items.push_back(body("earth", 200.0));
    frame.items.push_back(body("mars", 600.0));
    const glm::dvec2 viewport(1280.0, 720.0);
    double t = 10.0;
    const auto click = [&](double x, double y) {
        const auto press = overlay.handlePointer(frame, viewport, {x, y}, true, true, 0.0, t);
        overlay.handlePointer(frame, viewport, {x, y}, true, false, 0.0, t + 0.05);
        t += 0.12;
        return press;
    };
    const auto pause = [&]() { t += 1.0; };

    auto r = click(200.0, 300.0);
    check(r.activatedObjectId == "celestial:earth" && !overlay.isOpen("celestial:earth"),
          "E1 single click: body selected, card closed");
    pause();
    click(200.0, 300.0);
    click(200.0, 300.0);
    check(overlay.isOpen("celestial:earth"), "E2 first double click: card open");
    pause();
    click(200.0, 300.0);
    r = click(200.0, 300.0);
    check(!overlay.isOpen("celestial:earth") && r.activatedObjectId == "celestial:earth",
          "E2 second double click: card closed, body still activated");
    pause();
    click(200.0, 300.0);
    click(200.0, 300.0);
    check(overlay.isOpen("celestial:earth"), "E2 third double click: card open again");
    pause();
    r = click(600.0, 300.0);
    check(!overlay.isOpen("celestial:earth") && !overlay.isOpen("celestial:mars") &&
              r.activatedObjectId == "celestial:mars",
          "E3 single click on another body: old card closed, new one only selected");
    pause();
    click(200.0, 300.0);
    click(200.0, 300.0);
    pause();
    click(600.0, 300.0);
    click(600.0, 300.0);
    check(overlay.isOpen("celestial:mars") && !overlay.isOpen("celestial:earth"),
          "E3 double click on another body: only its card open");
    overlay.closeCelestialCardsNotSelected(frame, "");
    check(!overlay.isOpen("celestial:mars"), "E4 selection dropped (empty space): card closed");

    std::printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
