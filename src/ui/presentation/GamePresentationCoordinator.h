#pragma once

#include "src/ui/services/ServiceUiDefinition.h"

#include <array>
#include <cstdint>

// Presentation is explicit. Flight is not the absence of UI: it is a peer
// destination selected by F1-F4. Likewise F5-F8 and F9-F12 are direct
// selectors into service and navigation domains.
enum class GameUiMode
{
    None,       // boot / terminal shutdown only
    Flight,
    MainMenu,
    Loading,
    SystemMap,
    SessionMenu,
    ServicePanel
};

enum class FlightPresentationView
{
    Front,
    Rear,
    FrontDrone,
    Drone
};

enum class NavigationPresentationView
{
    Galaxy,
    System,
    Detail,
    Local
};

struct GameUiTarget
{
    GameUiMode mode = GameUiMode::None;
    ui::services::ServiceUiId service = ui::services::ServiceUiId::None;
    FlightPresentationView flight = FlightPresentationView::Front;
    NavigationPresentationView navigation = NavigationPresentationView::Galaxy;

    static constexpr GameUiTarget none()
    {
        return {};
    }

    static constexpr GameUiTarget forMode(GameUiMode modeValue)
    {
        return { modeValue };
    }

    static constexpr GameUiTarget forFlight(FlightPresentationView view)
    {
        GameUiTarget target;
        target.mode = GameUiMode::Flight;
        target.flight = view;
        return target;
    }

    static constexpr GameUiTarget forNavigation(NavigationPresentationView view)
    {
        GameUiTarget target;
        target.mode = GameUiMode::SystemMap;
        target.navigation = view;
        return target;
    }

    static constexpr GameUiTarget forService(ui::services::ServiceUiId serviceValue)
    {
        GameUiTarget target;
        target.mode = GameUiMode::ServicePanel;
        target.service = serviceValue;
        return target;
    }

    constexpr bool valid() const
    {
        if (mode == GameUiMode::None)
            return service == ui::services::ServiceUiId::None;
        if (mode == GameUiMode::ServicePanel)
            return service != ui::services::ServiceUiId::None;
        return service == ui::services::ServiceUiId::None;
    }

    constexpr bool isFullScreenWebDocument() const
    {
        return mode == GameUiMode::MainMenu ||
               mode == GameUiMode::Loading ||
               mode == GameUiMode::SessionMenu;
    }
};

constexpr bool operator==(const GameUiTarget& a, const GameUiTarget& b)
{
    return a.mode == b.mode &&
           a.service == b.service &&
           a.flight == b.flight &&
           a.navigation == b.navigation;
}

constexpr bool operator!=(const GameUiTarget& a, const GameUiTarget& b)
{
    return !(a == b);
}

class GamePresentationCoordinator
{
public:
    struct DocumentSurfaceState
    {
        GameUiTarget target = GameUiTarget::none();
        std::uint64_t serial = 0;
        bool loaded = false;
        bool prepared = false;
    };

    GameUiMode mode() const { return m_committedTarget.mode; }
    GameUiTarget committedTarget() const { return m_committedTarget; }
    GameUiTarget requestedTarget() const { return m_requestedTarget; }
    GameUiTarget sceneTarget() const { return m_sceneTarget; }
    std::uint64_t requestedSerial() const { return m_requestedSerial; }

    bool isMode(GameUiMode modeValue) const
    {
        return m_committedTarget.mode == modeValue;
    }

    bool isCommitted(GameUiTarget target) const
    {
        return m_committedTarget == target;
    }

    bool requestPending() const
    {
        return m_requestedTarget != m_committedTarget;
    }

    // Direct-selector semantics. Repeating the same F-key is a no-op; a new
    // target while a transition is pending replaces the previous request.
    bool requestTarget(GameUiTarget target);
    bool armSceneTarget(GameUiTarget target);
    bool commitRequested(GameUiTarget target);
    void parkScene();
    void forceCommit(GameUiTarget target);
    void clearForShutdown();

    std::uint64_t beginDocumentPreparation(int surfaceIndex, GameUiTarget target);
    bool acknowledgeDocumentLoaded(
        int surfaceIndex,
        GameUiTarget target,
        std::uint64_t serial);
    bool acknowledgeDocumentPrepared(
        int surfaceIndex,
        GameUiTarget target,
        std::uint64_t serial);
    const DocumentSurfaceState& documentSurface(int surfaceIndex) const;
    void invalidateDocumentSurface(int surfaceIndex);

    void noteGeometryChange(double nowSeconds);
    bool geometryChangeActive(double nowSeconds) const;
    bool consumeSettledGeometryChange(double nowSeconds);


private:
    static GameUiTarget normalizeTarget(GameUiTarget target);
    static std::size_t surfaceIndexChecked(int surfaceIndex);

private:
    GameUiTarget m_committedTarget = GameUiTarget::none();
    GameUiTarget m_requestedTarget = GameUiTarget::none();
    GameUiTarget m_sceneTarget = GameUiTarget::none();
    std::array<DocumentSurfaceState, 2> m_documentSurfaces{};
    std::uint64_t m_nextNavigationSerial = 1;
    std::uint64_t m_requestedSerial = 0;
    double m_geometrySettleDeadline = 0.0;
    bool m_geometryChangePending = false;
};
