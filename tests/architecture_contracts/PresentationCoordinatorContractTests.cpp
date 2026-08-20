#include "src/ui/presentation/GamePresentationCoordinator.h"
#include "src/ui/services/ServiceUiDefinition.h"
#include "src/ui/presentation/PresentationFunctionKeyRouter.h"

#include <cassert>
#include <cstdint>

int main()
{
    GamePresentationCoordinator ui;

    const GameUiTarget front =
        GameUiTarget::forFlight(FlightPresentationView::Front);
    const GameUiTarget rear =
        GameUiTarget::forFlight(FlightPresentationView::Rear);
    assert(ui.requestTarget(front));
    const std::uint64_t frontRequestSerial = ui.requestedSerial();
    assert(frontRequestSerial != 0);
    assert(ui.armSceneTarget(front));
    assert(ui.commitRequested(front));
    assert(!ui.requestTarget(front));

    // Latest request wins while the committed presentation remains unchanged.
    const GameUiTarget galaxy =
        GameUiTarget::forNavigation(NavigationPresentationView::Galaxy);
    const GameUiTarget system =
        GameUiTarget::forNavigation(NavigationPresentationView::System);
    assert(ui.requestTarget(galaxy));
    const std::uint64_t galaxyRequestSerial = ui.requestedSerial();
    assert(galaxyRequestSerial != frontRequestSerial);
    assert(ui.requestTarget(system));
    assert(ui.requestedSerial() != galaxyRequestSerial);
    assert(ui.requestedTarget() == system);
    assert(ui.committedTarget() == front);
    assert(ui.armSceneTarget(system));
    assert(ui.commitRequested(system));

    assert(ui.requestTarget(rear));
    assert(ui.armSceneTarget(rear));
    assert(ui.commitRequested(rear));

    // Full-screen documents are prepared per physical back surface. A stale
    // acknowledgement from the old generation/surface cannot prepare the new
    // target.
    const GameUiTarget menu = GameUiTarget::forMode(GameUiMode::MainMenu);
    assert(ui.requestTarget(menu));
    const std::uint64_t menuSerial = ui.beginDocumentPreparation(0, menu);
    assert(menuSerial != 0);
    assert(!ui.acknowledgeDocumentPrepared(0, menu, menuSerial));
    assert(ui.acknowledgeDocumentLoaded(0, menu, menuSerial));
    assert(ui.acknowledgeDocumentPrepared(0, menu, menuSerial));
    assert(ui.documentSurface(0).prepared);

    const GameUiTarget loading = GameUiTarget::forMode(GameUiMode::Loading);
    assert(ui.requestTarget(loading));
    const std::uint64_t loadingSerial = ui.beginDocumentPreparation(1, loading);
    assert(loadingSerial != 0 && loadingSerial != menuSerial);
    assert(!ui.acknowledgeDocumentLoaded(1, loading, menuSerial));
    assert(ui.acknowledgeDocumentLoaded(1, loading, loadingSerial));
    assert(ui.acknowledgeDocumentPrepared(1, loading, loadingSerial));
    assert(ui.commitRequested(loading));
    ui.parkScene();
    assert(ui.sceneTarget().mode == GameUiMode::None);
    assert(ui.committedTarget() == loading);

    // A later direct selector can arm a new scene under the still committed
    // fullscreen document without changing visible presentation semantics.
    assert(ui.requestTarget(galaxy));
    assert(ui.armSceneTarget(galaxy));
    assert(ui.committedTarget() == loading);
    assert(ui.sceneTarget() == galaxy);

    ui.noteGeometryChange(10.0);
    assert(ui.geometryChangeActive(10.01));
    assert(!ui.consumeSettledGeometryChange(10.05));
    assert(ui.consumeSettledGeometryChange(10.08));

    using ui::presentation::directTargetForFunctionKey;
    assert(directTargetForFunctionKey(1) ==
        GameUiTarget::forFlight(FlightPresentationView::Front));
    assert(directTargetForFunctionKey(2) ==
        GameUiTarget::forFlight(FlightPresentationView::Rear));
    assert(directTargetForFunctionKey(3) ==
        GameUiTarget::forFlight(FlightPresentationView::FrontDrone));
    assert(directTargetForFunctionKey(4) ==
        GameUiTarget::forFlight(FlightPresentationView::Drone));
    assert(directTargetForFunctionKey(9) ==
        GameUiTarget::forNavigation(NavigationPresentationView::Galaxy));
    assert(directTargetForFunctionKey(10) ==
        GameUiTarget::forNavigation(NavigationPresentationView::System));
    assert(directTargetForFunctionKey(11) ==
        GameUiTarget::forNavigation(NavigationPresentationView::Detail));
    assert(directTargetForFunctionKey(12) ==
        GameUiTarget::forNavigation(NavigationPresentationView::Local));
    assert(directTargetForFunctionKey(5) ==
        GameUiTarget::forService(ui::services::ServiceUiId::GovernmentServices));
    assert(directTargetForFunctionKey(6) ==
        GameUiTarget::forService(ui::services::ServiceUiId::Shipyard));
    assert(directTargetForFunctionKey(7) ==
        GameUiTarget::forService(ui::services::ServiceUiId::RepairRefit));
    assert(directTargetForFunctionKey(8) ==
        GameUiTarget::forService(ui::services::ServiceUiId::Trade));

    const int serviceKeys[] = {5, 6, 7, 8};
    for (const int key : serviceKeys)
    {
        const auto* definition =
            ui::services::findServiceUiDefinitionByFunctionKey(key);
        assert(definition != nullptr);
        assert(definition->id != ui::services::ServiceUiId::None);
        assert(directTargetForFunctionKey(key) ==
            GameUiTarget::forService(definition->id));
    }

    // Freeze the user-visible selector semantics. A repeated key is a no-op.
    // Rapid keys replace only the unpublished destination; the visible source
    // remains committed until the final destination is prepared and committed.
    const auto* tradeDef =
        ui::services::findServiceUiDefinitionByFunctionKey(8);
    assert(tradeDef != nullptr);
    const GameUiTarget trade = GameUiTarget::forService(tradeDef->id);
    assert(!front.isFullScreenWebDocument());
    assert(!galaxy.isFullScreenWebDocument());
    assert(!trade.isFullScreenWebDocument());
    assert(menu.isFullScreenWebDocument());
    assert(loading.isFullScreenWebDocument());

    ui.forceCommit(trade);
    assert(ui.committedTarget() == trade);
    assert(ui.sceneTarget() == trade);
    assert(!ui.requestTarget(trade));

    const GameUiTarget local =
        GameUiTarget::forNavigation(NavigationPresentationView::Local);
    assert(ui.requestTarget(galaxy));
    assert(ui.committedTarget() == trade);
    assert(ui.requestTarget(system));
    assert(ui.committedTarget() == trade);
    assert(ui.requestTarget(local));
    assert(ui.requestedTarget() == local);
    assert(ui.committedTarget() == trade);
    assert(ui.armSceneTarget(local));
    assert(ui.committedTarget() == trade);
    assert(ui.commitRequested(local));
    assert(ui.committedTarget() == local);

    // Service -> pending Map -> Flight must never commit the abandoned Map.
    ui.forceCommit(trade);
    assert(ui.sceneTarget() == trade);
    assert(ui.requestTarget(galaxy));
    assert(ui.committedTarget() == trade);
    assert(ui.requestTarget(front));
    assert(ui.requestedTarget() == front);
    assert(ui.committedTarget() == trade);
    assert(ui.armSceneTarget(front));
    assert(ui.commitRequested(front));
    assert(ui.committedTarget() == front);

    // Flight -> Service is also a normal scene transaction; no document
    // preparation or scene parking is allowed for F5-F8.
    const GameUiTarget government =
        GameUiTarget::forService(ui::services::ServiceUiId::GovernmentServices);
    assert(ui.requestTarget(government));
    assert(ui.committedTarget() == front);
    assert(ui.armSceneTarget(government));
    assert(ui.sceneTarget() == government);
    assert(ui.commitRequested(government));
    assert(ui.committedTarget() == government);
    assert(ui.sceneTarget() == government);

    // Internal navigation drill uses the already prepared one-surface scene.
    // Synchronizing Galaxy -> System must leave no pending F9 request behind,
    // and F9 must immediately be selectable again from the new System state.
    ui.forceCommit(galaxy);
    assert(!ui.requestPending());
    ui.forceCommit(system);
    assert(ui.committedTarget() == system);
    assert(ui.sceneTarget() == system);
    assert(!ui.requestPending());
    assert(ui.requestTarget(galaxy));
    assert(ui.requestedTarget() == galaxy);
    assert(ui.committedTarget() == system);

    return 0;
}
