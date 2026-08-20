#include "src/ui/presentation/GamePresentationCoordinator.h"

#include <stdexcept>

GameUiTarget GamePresentationCoordinator::normalizeTarget(GameUiTarget target)
{
    if (!target.valid())
        throw std::invalid_argument("invalid GameUiTarget");
    return target;
}

std::size_t GamePresentationCoordinator::surfaceIndexChecked(int surfaceIndex)
{
    if (surfaceIndex < 0 || surfaceIndex >= 2)
        throw std::out_of_range("presentation document surface index");
    return static_cast<std::size_t>(surfaceIndex);
}

bool GamePresentationCoordinator::requestTarget(GameUiTarget target)
{
    target = normalizeTarget(target);
    if (target.mode == GameUiMode::None)
        return false;

    // Same committed/requested destination is deliberately not a toggle.
    if (m_requestedTarget == target)
        return false;

    m_requestedTarget = target;
    ++m_requestedSerial;
    if (m_requestedSerial == 0)
        ++m_requestedSerial;
    return true;
}


bool GamePresentationCoordinator::armSceneTarget(GameUiTarget target)
{
    target = normalizeTarget(target);
    if (target.mode != GameUiMode::Flight &&
        target.mode != GameUiMode::SystemMap &&
        target.mode != GameUiMode::ServicePanel)
        return false;
    if (m_requestedTarget != target || m_sceneTarget == target)
        return false;
    m_sceneTarget = target;
    return true;
}

bool GamePresentationCoordinator::commitRequested(GameUiTarget target)
{
    target = normalizeTarget(target);
    if (target.mode == GameUiMode::None || m_requestedTarget != target)
        return false;

    if (m_committedTarget == target)
        return false;

    m_committedTarget = target;
    return true;
}

void GamePresentationCoordinator::parkScene()
{
    // Main Menu / Loading / ESC WebUI own every visible pixel. F1-F12 are
    // scene-backed and never park behind another in-session presentation.
    m_sceneTarget = GameUiTarget::none();
}

void GamePresentationCoordinator::forceCommit(GameUiTarget target)
{
    target = normalizeTarget(target);
    m_committedTarget = target;
    m_requestedTarget = target;
    ++m_requestedSerial;
    if (m_requestedSerial == 0)
        ++m_requestedSerial;
    if (target.mode == GameUiMode::Flight ||
        target.mode == GameUiMode::SystemMap ||
        target.mode == GameUiMode::ServicePanel)
        m_sceneTarget = target;
}

void GamePresentationCoordinator::clearForShutdown()
{
    m_committedTarget = GameUiTarget::none();
    m_requestedTarget = GameUiTarget::none();
    m_sceneTarget = GameUiTarget::none();
    ++m_requestedSerial;
    if (m_requestedSerial == 0)
        ++m_requestedSerial;
}

std::uint64_t GamePresentationCoordinator::beginDocumentPreparation(
    int surfaceIndex,
    GameUiTarget target)
{
    target = normalizeTarget(target);
    if (!target.isFullScreenWebDocument())
        throw std::invalid_argument("target is not a full-screen web document");

    auto& surface = m_documentSurfaces[surfaceIndexChecked(surfaceIndex)];
    surface.target = target;
    surface.serial = m_nextNavigationSerial++;
    if (surface.serial == 0)
        surface.serial = m_nextNavigationSerial++;
    surface.loaded = false;
    surface.prepared = false;
    return surface.serial;
}

bool GamePresentationCoordinator::acknowledgeDocumentLoaded(
    int surfaceIndex,
    GameUiTarget target,
    std::uint64_t serial)
{
    target = normalizeTarget(target);
    auto& surface = m_documentSurfaces[surfaceIndexChecked(surfaceIndex)];
    if (serial == 0 || surface.serial != serial || surface.target != target)
        return false;

    surface.loaded = true;
    surface.prepared = false;
    return true;
}

bool GamePresentationCoordinator::acknowledgeDocumentPrepared(
    int surfaceIndex,
    GameUiTarget target,
    std::uint64_t serial)
{
    target = normalizeTarget(target);
    auto& surface = m_documentSurfaces[surfaceIndexChecked(surfaceIndex)];
    if (serial == 0 || surface.serial != serial || surface.target != target ||
        !surface.loaded)
    {
        return false;
    }

    surface.prepared = true;
    return true;
}

const GamePresentationCoordinator::DocumentSurfaceState&
GamePresentationCoordinator::documentSurface(int surfaceIndex) const
{
    return m_documentSurfaces[surfaceIndexChecked(surfaceIndex)];
}

void GamePresentationCoordinator::invalidateDocumentSurface(int surfaceIndex)
{
    auto& surface = m_documentSurfaces[surfaceIndexChecked(surfaceIndex)];
    surface = {};
}

void GamePresentationCoordinator::noteGeometryChange(double nowSeconds)
{
    m_geometrySettleDeadline = nowSeconds + 0.075;
    m_geometryChangePending = true;
}

bool GamePresentationCoordinator::geometryChangeActive(double nowSeconds) const
{
    return m_geometryChangePending && nowSeconds < m_geometrySettleDeadline;
}

bool GamePresentationCoordinator::consumeSettledGeometryChange(double nowSeconds)
{
    if (!m_geometryChangePending || nowSeconds < m_geometrySettleDeadline)
        return false;

    m_geometryChangePending = false;
    m_geometrySettleDeadline = 0.0;
    return true;
}
