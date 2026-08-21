#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/NavigationTrackingState.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/game/presentation/NavigationHudPresentation.h"
#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/system_map/LocalMapPresentationBuilder.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapMode.h"
#include "src/game/system_map/MapObjectOverlay.h"
#include "src/game/system_map/MapTransitionController.h"
#include "src/game/system_map/SystemMapInteraction.h"
#include "src/game/system_map/SystemMapFrameInteractionContext.h"
#include "src/game/system_map/SystemMapFrameData.h"
#include "src/game/system_map/SystemMapPresentationBuilder.h"
#include "src/game/system_map/SystemMapView.h"

namespace
{
using game::navigation::CubicGridIndex;
using game::navigation::CubicNavigationCell;
using game::navigation::CubicNavigationGrid;
using game::navigation::CubicNavigationGridDefinition;
using game::navigation::CubicNavigationLevelAction;
using game::navigation::GalaxyNavigationGrid;
using game::navigation::SystemNavigationGrid;
using game::presentation::SystemMapPanelAction;
using game::presentation::SystemMapPanelActionType;
using game::presentation::SystemMapPanelCommandType;
using game::presentation::SystemMapPanelPresentation;
using game::system_map::DetailMapView;
using game::system_map::HubMapView;
using game::system_map::GalaxyMapView;
using game::system_map::LocalMapCameraSnapshot;
using game::system_map::LocalMapPresentationBuilder;
using game::system_map::MapMode;
using game::system_map::MapObjectInfoKind;
using game::system_map::MapObjectInfoPanelState;
using game::system_map::MapObjectOverlayFrame;
using game::system_map::MapObjectOverlayItem;
using game::system_map::MapObjectOverlayState;
using game::system_map::mapObjectGlyphScale;
using game::system_map::mapObjectVelocityArrowLengthScale;
using game::system_map::MapObjectVelocityMode;
using game::system_map::stellarAzimuthElevationDeg;
using game::system_map::SystemMapCameraBodyTarget;
using game::system_map::SystemMapHubSelection;
using game::system_map::SystemMapInputFrame;
using game::system_map::SystemMapInteraction;
using game::system_map::SystemMapFrameData;
using game::system_map::SystemMapFrameInteractionContext;
using game::system_map::SystemMapInteractionContext;
using game::system_map::SystemMapPresentationBuilder;
using game::system_map::SystemMapView;

class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

[[noreturn]] void fail(
    const char* expression,
    const char* file,
    int line,
    const std::string& detail = {}
)
{
    std::ostringstream out;
    out << file << ':' << line << ": check failed: " << expression;

    if (!detail.empty())
        out << " (" << detail << ')';

    throw TestFailure(out.str());
}

#define REQUIRE(expression) \
    do \
    { \
        if (!(expression)) \
            fail(#expression, __FILE__, __LINE__); \
    } while (false)

void requireNear(
    double actual,
    double expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line
)
{
    if (std::isfinite(actual) &&
        std::isfinite(expected) &&
        std::abs(actual - expected) <= epsilon)
    {
        return;
    }

    std::ostringstream detail;
    detail << std::setprecision(17)
           << "actual=" << actual
           << ", expected=" << expected
           << ", epsilon=" << epsilon;

    fail(expression, file, line, detail.str());
}

#define REQUIRE_NEAR(actual, expected, epsilon) \
    requireNear( \
        static_cast<double>(actual), \
        static_cast<double>(expected), \
        static_cast<double>(epsilon), \
        #actual " ~= " #expected, \
        __FILE__, \
        __LINE__ \
    )

void requireClose(
    double actual,
    double expected,
    double absoluteTolerance,
    double relativeTolerance,
    const char* expression,
    const char* file,
    int line
)
{
    const double scale = std::max(std::abs(actual), std::abs(expected));
    const double allowedDelta =
        absoluteTolerance + relativeTolerance * scale;

    if (std::isfinite(actual) &&
        std::isfinite(expected) &&
        std::abs(actual - expected) <= allowedDelta)
    {
        return;
    }

    std::ostringstream detail;
    detail << std::setprecision(17)
           << "actual=" << actual
           << ", expected=" << expected
           << ", |delta|=" << std::abs(actual - expected)
           << ", allowed=" << allowedDelta
           << " (absolute=" << absoluteTolerance
           << ", relative=" << relativeTolerance << ')';

    fail(expression, file, line, detail.str());
}

#define REQUIRE_CLOSE(actual, expected, absoluteTolerance, relativeTolerance) \
    requireClose( \
        static_cast<double>(actual), \
        static_cast<double>(expected), \
        static_cast<double>(absoluteTolerance), \
        static_cast<double>(relativeTolerance), \
        #actual " ~= " #expected, \
        __FILE__, \
        __LINE__ \
    )

void requireVectorNear(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line
)
{
    const glm::dvec3 delta = actual - expected;

    if (std::isfinite(actual.x) &&
        std::isfinite(actual.y) &&
        std::isfinite(actual.z) &&
        glm::length(delta) <= epsilon)
    {
        return;
    }

    std::ostringstream detail;
    detail << std::setprecision(17)
           << "actual=(" << actual.x << ", " << actual.y << ", " << actual.z
           << "), expected=(" << expected.x << ", " << expected.y << ", "
           << expected.z << "), |delta|=" << glm::length(delta)
           << ", epsilon=" << epsilon;

    fail(expression, file, line, detail.str());
}

#define REQUIRE_VEC_NEAR(actual, expected, epsilon) \
    requireVectorNear( \
        (actual), \
        (expected), \
        static_cast<double>(epsilon), \
        #actual " ~= " #expected, \
        __FILE__, \
        __LINE__ \
    )

void requireVectorClose(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double absoluteTolerance,
    double relativeTolerance,
    const char* expression,
    const char* file,
    int line
)
{
    const glm::dvec3 delta = actual - expected;
    const double deltaLength = glm::length(delta);
    const double scale = std::max(glm::length(actual), glm::length(expected));
    const double allowedDelta =
        absoluteTolerance + relativeTolerance * scale;

    if (std::isfinite(actual.x) &&
        std::isfinite(actual.y) &&
        std::isfinite(actual.z) &&
        std::isfinite(expected.x) &&
        std::isfinite(expected.y) &&
        std::isfinite(expected.z) &&
        deltaLength <= allowedDelta)
    {
        return;
    }

    std::ostringstream detail;
    detail << std::setprecision(17)
           << "actual=(" << actual.x << ", " << actual.y << ", " << actual.z
           << "), expected=(" << expected.x << ", " << expected.y << ", "
           << expected.z << "), |delta|=" << deltaLength
           << ", allowed=" << allowedDelta
           << " (absolute=" << absoluteTolerance
           << ", relative=" << relativeTolerance << ')';

    fail(expression, file, line, detail.str());
}

#define REQUIRE_VEC_CLOSE( \
    actual, \
    expected, \
    absoluteTolerance, \
    relativeTolerance \
) \
    requireVectorClose( \
        (actual), \
        (expected), \
        static_cast<double>(absoluteTolerance), \
        static_cast<double>(relativeTolerance), \
        #actual " ~= " #expected, \
        __FILE__, \
        __LINE__ \
    )

bool finiteVector(const glm::dvec3& value)
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

Viewport testViewport()
{
    Viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 1200;
    viewport.height = 800;
    return viewport;
}

SystemMapInputFrame inputFrame(
    const Viewport& viewport,
    double localX,
    double localY,
    bool leftDown,
    bool rightDown,
    double nowSeconds
)
{
    SystemMapInputFrame frame;
    frame.viewport = viewport;
    frame.localMouseX = localX;
    frame.localMouseY = localY;
    frame.mouseX = localX + static_cast<double>(viewport.x);
    frame.mouseY = localY + static_cast<double>(viewport.y);
    frame.inside = true;
    frame.leftDown = leftDown;
    frame.rightDown = rightDown;
    frame.nowSeconds = nowSeconds;
    return frame;
}

SystemMapView makeSystemView()
{
    SystemMapView view;
    view.state().navigationGrid.activateSystem(42);
    view.state().lastScale = 1.0f;
    return view;
}

class MockSystemMapInteractionContext final
    : public SystemMapInteractionContext
{
public:
    std::optional<std::string> bodyPick;
    std::optional<SystemMapHubSelection> hubPick;
    std::optional<SystemMapCameraBodyTarget> cameraBodyTarget;

    std::unordered_map<std::string, glm::dvec3> bodyPositions;
    std::unordered_map<std::string, glm::dvec3> objectPositions;

    std::optional<std::string> pickSystemBodyId(
        double,
        double
    ) const override
    {
        return bodyPick;
    }

    std::optional<SystemMapHubSelection> pickSystemHubSelection(
        double,
        double
    ) const override
    {
        return hubPick;
    }

    std::optional<SystemMapCameraBodyTarget>
    pickSystemCameraBodyTarget(
        double,
        double,
        const Viewport&
    ) const override
    {
        return cameraBodyTarget;
    }

    std::optional<glm::dvec3> systemBodyAbsolutePosition(
        const std::string& bodyId
    ) const override
    {
        const auto found = bodyPositions.find(bodyId);

        if (found == bodyPositions.end())
            return std::nullopt;

        return found->second;
    }

    std::optional<glm::dvec3> systemObjectAbsolutePosition(
        const std::string& objectId
    ) const override
    {
        const auto found = objectPositions.find(objectId);

        if (found == objectPositions.end())
            return std::nullopt;

        return found->second;
    }
};

void testOrbitPreservesCompleteCameraPose()
{
    SystemMapView view = makeSystemView();
    auto& camera = view.state().camera;

    camera.target = glm::dvec3(12.0, -4.0, 3.0);
    camera.yaw = 0.45f;
    camera.pitch = 0.31f;
    camera.distance = 37.0f;

    const glm::dvec3 pivot(2.0, 5.0, -1.0);
    const glm::dvec3 oldTargetVector = camera.target - pivot;
    const glm::dvec3 oldEyeVector = view.cameraEyeAbsolute() - pivot;
    const float oldDistance = camera.distance;

    view.orbitCameraAroundPivot(
        pivot,
        0.27f,
        -0.18f,
        view.controls().pitchLimitRad
    );

    const glm::dvec3 newTargetVector = camera.target - pivot;
    const glm::dvec3 newEyeVector = view.cameraEyeAbsolute() - pivot;

    /*
        Camera angles are stored as float. Rebuilding the old and new
        orthonormal bases therefore preserves a rigid orbit to float-scale
        precision, not to double machine epsilon.
    */
    REQUIRE_CLOSE(
        glm::length(newTargetVector),
        glm::length(oldTargetVector),
        1.0e-7,
        1.0e-7
    );

    REQUIRE_CLOSE(
        glm::length(newEyeVector),
        glm::length(oldEyeVector),
        1.0e-7,
        1.0e-7
    );

    REQUIRE_NEAR(camera.distance, oldDistance, 1.0e-7);
    REQUIRE(std::abs(camera.pitch) <= view.controls().pitchLimitRad);
    REQUIRE(finiteVector(camera.target));
    REQUIRE(finiteVector(view.cameraEyeAbsolute()));
}

void testZoomScalesPoseAndRespectsBodyClearance()
{
    {
        SystemMapView view = makeSystemView();
        auto& camera = view.state().camera;

        camera.target = glm::dvec3(9.0, -3.0, 5.0);
        camera.distance = 40.0f;

        const glm::dvec3 pivot(-2.0, 4.0, 1.0);
        const glm::dvec3 oldTargetVector = camera.target - pivot;
        const glm::dvec3 oldEyeVector = view.cameraEyeAbsolute() - pivot;
        const float oldDistance = camera.distance;
        constexpr float zoomFactor = 0.60f;

        view.zoomCameraAroundPivot(
            pivot,
            zoomFactor,
            0.001f,
            1000.0f
        );

        REQUIRE_NEAR(camera.distance, oldDistance * zoomFactor, 1.0e-5);

        /*
            zoomCameraAroundPivot stores distance as float and then derives
            the pose scale from that stored value. Verify the effective
            scale actually applied to the complete pose.
        */
        const double effectivePoseScale =
            static_cast<double>(camera.distance) /
            static_cast<double>(oldDistance);

        REQUIRE_VEC_NEAR(
            camera.target - pivot,
            oldTargetVector * effectivePoseScale,
            1.0e-10
        );
        REQUIRE_VEC_CLOSE(
            view.cameraEyeAbsolute() - pivot,
            oldEyeVector * effectivePoseScale,
            1.0e-8,
            1.0e-7
        );
    }

    {
        SystemMapView view = makeSystemView();
        auto& camera = view.state().camera;

        const glm::dvec3 bodyCenter(0.0);
        camera.target = bodyCenter;
        camera.distance = 30.0f;

        const double beforeEyeDistance =
            glm::length(view.cameraEyeAbsolute() - bodyCenter);

        const double safeSurfaceDistance =
            beforeEyeDistance * 0.70;

        constexpr float zoomFactor = 0.25f;

        view.zoomCameraAroundPivot(
            bodyCenter,
            zoomFactor,
            SystemMapView::minimumCameraHalfHeight,
            1000.0f,
            safeSurfaceDistance
        );

        const double afterEyeDistance =
            glm::length(view.cameraEyeAbsolute() - bodyCenter);

        const double expectedEyeDistance =
            safeSurfaceDistance +
            (beforeEyeDistance - safeSurfaceDistance) *
                static_cast<double>(zoomFactor);

        REQUIRE(afterEyeDistance > safeSurfaceDistance);
        REQUIRE_CLOSE(
            afterEyeDistance,
            expectedEyeDistance,
            1.0e-7,
            1.0e-7
        );

        for (int i = 0; i < 64; ++i)
        {
            view.zoomCameraAroundPivot(
                bodyCenter,
                0.5f,
                SystemMapView::minimumCameraHalfHeight,
                1000.0f,
                safeSurfaceDistance
            );
        }

        REQUIRE(
            glm::length(view.cameraEyeAbsolute() - bodyCenter) >=
            safeSurfaceDistance - 1.0e-9
        );
        REQUIRE(finiteVector(view.cameraEyeAbsolute()));
    }
}

void testRotationPivotPriority()
{
    const Viewport viewport = testViewport();
    const SystemMapInteraction interaction;
    double pendingScrollY = 0.0;

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;

        const CubicNavigationCell selected =
            view.state().navigationGrid.cell(
                CubicGridIndex{1, 0, 0},
                view.state().navigationGrid.level()
            );

        view.state().navigationGrid.selectCell(selected);
        view.state().navigationCellExplicitlySelected = true;

        SystemMapCameraBodyTarget body;
        body.bodyId = "planet";
        body.absolutePosition = glm::dvec3(7.0, 8.0, 9.0);
        body.physicalRadiusWorld = 2.0;
        context.cameraBodyTarget = body;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, true, false, 1.0),
            pendingScrollY
        );

        REQUIRE(view.state().orbitPivotActive);
        REQUIRE_VEC_NEAR(
            view.state().orbitPivotAbsolute,
            body.absolutePosition,
            1.0e-12
        );
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;

        const CubicNavigationCell selected =
            view.state().navigationGrid.cell(
                CubicGridIndex{1, 0, 0},
                view.state().navigationGrid.level()
            );

        view.state().navigationGrid.selectCell(selected);
        view.state().navigationCellExplicitlySelected = true;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, true, false, 2.0),
            pendingScrollY
        );

        REQUIRE_VEC_NEAR(
            view.state().orbitPivotAbsolute,
            selected.center,
            1.0e-12
        );
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        view.state().navigationCellExplicitlySelected = false;

        const glm::dvec3 fallback =
            view.targetPlanePointFromScreen(
                viewport,
                610.0,
                400.0
            );

        REQUIRE(glm::length(fallback) > 0.0);

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 610.0, 400.0, true, false, 3.0),
            pendingScrollY
        );

        REQUIRE_VEC_NEAR(
            view.state().orbitPivotAbsolute,
            view.state().navigationGrid.anchorCell().center,
            1.0e-12
        );
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        view.state().navigationGrid.setEnabled(false);

        const glm::dvec3 expected =
            view.targetPlanePointFromScreen(
                viewport,
                650.0,
                420.0
            );

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 650.0, 420.0, true, false, 4.0),
            pendingScrollY
        );

        REQUIRE_VEC_NEAR(
            view.state().orbitPivotAbsolute,
            expected,
            1.0e-12
        );
    }
}

void testZoomPivotIsCursorBodyOrCurrentTarget()
{
    const Viewport viewport = testViewport();
    const SystemMapInteraction interaction;

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        auto& camera = view.state().camera;

        camera.target = glm::dvec3(10.0, 0.0, 0.0);
        camera.distance = 50.0f;

        SystemMapCameraBodyTarget body;
        body.bodyId = "planet";
        body.absolutePosition = glm::dvec3(30.0, 0.0, 0.0);
        body.physicalRadiusWorld = 0.0;
        context.cameraBodyTarget = body;

        const glm::dvec3 oldTarget = camera.target;
        const float oldDistance = camera.distance;
        double pendingScrollY = 1.0;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, false, false, 1.0),
            pendingScrollY
        );

        const double poseScale =
            static_cast<double>(camera.distance) /
            static_cast<double>(oldDistance);

        const glm::dvec3 expectedTarget =
            body.absolutePosition +
            (oldTarget - body.absolutePosition) * poseScale;

        REQUIRE_VEC_NEAR(camera.target, expectedTarget, 1.0e-7);
        REQUIRE(pendingScrollY == 0.0);
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        auto& state = view.state();

        state.selectedBodyId = "previous-selection";
        state.camera.target = glm::dvec3(10.0, 2.0, -1.0);
        state.camera.distance = 50.0f;

        const glm::dvec3 oldTarget = state.camera.target;
        double pendingScrollY = 1.0;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, false, false, 2.0),
            pendingScrollY
        );

        REQUIRE_VEC_NEAR(state.camera.target, oldTarget, 1.0e-12);
        REQUIRE(state.camera.distance < 50.0f);
    }
}

CubicNavigationGrid makeTestGrid()
{
    CubicNavigationGridDefinition definition;
    definition.frame.id = "test";
    definition.frame.unit = "u";
    definition.subdivision = 5;
    definition.minimumLevel = 0;
    definition.initialLevel = 0;
    definition.maximumLevel = 3;
    definition.baseCellSize = 100.0;
    return CubicNavigationGrid(definition);
}

void testRefineCoarsenPreserveNavigationPoint()
{
    CubicNavigationGrid grid = makeTestGrid();
    const glm::dvec3 navigationPoint(37.0, -11.0, 8.0);

    const CubicNavigationCell implicitSelection =
        grid.selectedCell();

    const auto setAnchor = [](
        CubicNavigationGrid& targetGrid,
        const glm::dvec3& position
    )
    {
        targetGrid.setAnchorFromPosition(position);
    };

    REQUIRE(
        game::navigation::applyCubicNavigationLevelActionAtPosition(
            CubicNavigationLevelAction::Refine,
            grid,
            navigationPoint,
            setAnchor,
            false
        )
    );

    REQUIRE(grid.level() == 1);
    REQUIRE(
        grid.anchorIndex() ==
        grid.nearestIndexForPosition(navigationPoint, grid.level())
    );

    REQUIRE(grid.selectedCell().level == implicitSelection.level);
    REQUIRE(grid.selectedCell().index == implicitSelection.index);

    REQUIRE(
        game::navigation::applyCubicNavigationLevelActionAtPosition(
            CubicNavigationLevelAction::Refine,
            grid,
            navigationPoint,
            setAnchor,
            true
        )
    );

    REQUIRE(grid.level() == 2);
    REQUIRE(grid.selectedCell().level == 2);
    REQUIRE(grid.selectedCell().index == grid.anchorIndex());

    REQUIRE(
        game::navigation::applyCubicNavigationLevelActionAtPosition(
            CubicNavigationLevelAction::Coarsen,
            grid,
            navigationPoint,
            setAnchor,
            false
        )
    );

    REQUIRE(grid.level() == 1);
    REQUIRE(
        grid.anchorIndex() ==
        grid.nearestIndexForPosition(navigationPoint, grid.level())
    );
}

void testAnchorAndExplicitSelectionSemantics()
{
    const Viewport viewport = testViewport();
    const SystemMapInteraction interaction;

    {
        SystemMapView view = makeSystemView();

        REQUIRE(view.state().navigationGrid.hasSelectedCell());
        REQUIRE(!view.state().navigationCellExplicitlySelected);
        REQUIRE(!view.resolvedTerminalSelection().has_value());

        const int selectedLevel =
            std::min(
                1,
                view.state().navigationGrid
                    .definition()
                    .maximumLevel
            );

        const CubicGridIndex selectedIndex{2, -1, 3};
        view.state().navigationGrid.selectCell(
            view.state().navigationGrid.cell(
                selectedIndex,
                selectedLevel
            )
        );
        view.state().navigationCellExplicitlySelected = true;

        const auto resolved = view.resolvedTerminalSelection();
        REQUIRE(resolved.has_value());
        REQUIRE(
            resolved->level ==
            view.state().navigationGrid
                .definition()
                .maximumLevel
        );

        const std::int64_t subdivision =
            view.state().navigationGrid.subdivision();

        CubicGridIndex expectedIndex = selectedIndex;
        for (int level = selectedLevel;
             level < resolved->level;
             ++level)
        {
            expectedIndex.x *= subdivision;
            expectedIndex.y *= subdivision;
            expectedIndex.z *= subdivision;
        }

        REQUIRE(resolved->index == expectedIndex);
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        double pendingScrollY = 0.0;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, true, false, 10.0),
            pendingScrollY
        );
        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, false, false, 10.1),
            pendingScrollY
        );

        REQUIRE(view.state().navigationCellExplicitlySelected);
        REQUIRE(view.state().navigationGrid.hasSelectedCell());
        REQUIRE(view.resolvedTerminalSelection().has_value());
    }

    {
        SystemMapView view = makeSystemView();
        MockSystemMapInteractionContext context;
        double pendingScrollY = 0.0;

        context.bodyPick = "planet";
        context.bodyPositions.emplace(
            "planet",
            glm::dvec3(125.0, -50.0, 10.0)
        );

        view.state().navigationCellExplicitlySelected = true;

        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, true, false, 20.0),
            pendingScrollY
        );
        interaction.handleInput(
            view,
            context,
            inputFrame(viewport, 600.0, 400.0, false, false, 20.1),
            pendingScrollY
        );

        REQUIRE(view.state().selectedBodyId == "planet");
        REQUIRE(view.state().selectedHubId.empty());
        REQUIRE(!view.state().navigationCellExplicitlySelected);
        REQUIRE(!view.state().navigationGrid.hasSelectedCell());

        const CubicGridIndex expectedAnchor =
            view.state().navigationGrid.nearestIndexForPosition(
                context.bodyPositions.at("planet"),
                view.state().navigationGrid.level()
            );

        REQUIRE(view.state().navigationGrid.anchorIndex() == expectedAnchor);
    }
}

void testGalaxyAndSystemShareCubicNavigationCore()
{
    static_assert(
        std::is_same_v<
            game::navigation::GalaxyGridIndex,
            game::navigation::CubicGridIndex
        >
    );

    static_assert(
        std::is_same_v<
            game::navigation::GalaxyNavigationCell,
            game::navigation::CubicNavigationCell
        >
    );

    static_assert(
        std::is_base_of_v<CubicNavigationGrid, GalaxyNavigationGrid>
    );

    static_assert(
        std::is_base_of_v<CubicNavigationGrid, SystemNavigationGrid>
    );

    GalaxyNavigationGrid galaxy;
    CubicNavigationGrid& galaxyCore = galaxy;

    REQUIRE(galaxyCore.anchorState().level == galaxy.level());
    REQUIRE(galaxyCore.anchorState().index == galaxy.anchorIndex());
    REQUIRE(galaxy.hasSelectedCell());

    const CubicNavigationCell initialSelection = galaxy.selectedCell();
    const CubicNavigationCell hover = galaxy.cell(
        CubicGridIndex {1, 0, 0},
        galaxy.level()
    );

    REQUIRE(galaxy.isCellNavigable(hover));
    galaxy.setHoveredCell(hover);
    REQUIRE(galaxy.hasHoveredCell());

    REQUIRE(galaxy.refineAroundAnchor());
    REQUIRE(!galaxy.hasHoveredCell());
    REQUIRE(galaxy.hasSelectedCell());
    REQUIRE(galaxy.selectedCell().level == initialSelection.level);
    REQUIRE(galaxy.selectedCell().index == initialSelection.index);
    REQUIRE(galaxy.anchorState().level == initialSelection.level + 1);

    const CubicGridIndex anchorBeforeInvalid = galaxy.anchorIndex();
    const CubicNavigationCell invalid = galaxy.cell(
        CubicGridIndex {1000000, 0, 0},
        galaxy.level()
    );

    REQUIRE(!galaxy.isCellNavigable(invalid));
    galaxy.setAnchorIndex(invalid.index);
    galaxy.selectCell(invalid);
    galaxy.setHoveredCell(invalid);

    REQUIRE(galaxy.anchorIndex() == anchorBeforeInvalid);
    REQUIRE(galaxy.selectedCell().index == initialSelection.index);
    REQUIRE(!galaxy.hasHoveredCell());

    const CubicNavigationCell galaxyAnchor = galaxy.anchorCell();
    REQUIRE_VEC_NEAR(
        galaxyAnchor.center,
        galaxy.cellCenterLy(galaxyAnchor.index, galaxyAnchor.level),
        1.0e-12
    );
    REQUIRE_NEAR(
        galaxyAnchor.size,
        galaxy.cellSizeLy(galaxyAnchor.level),
        1.0e-12
    );

    SystemNavigationGrid system;
    system.activateSystem(42);

    const CubicNavigationCell explicitSystemSelection = system.cell(
        CubicGridIndex {2, -1, 3},
        system.level()
    );

    system.selectCell(explicitSystemSelection);
    system.setHoveredCell(system.cell(CubicGridIndex {1, 0, 0}, system.level()));

    REQUIRE(system.refineAroundAnchor());
    REQUIRE(!system.hasHoveredCell());
    REQUIRE(system.selectedCell().level == explicitSystemSelection.level);
    REQUIRE(system.selectedCell().index == explicitSystemSelection.index);
    REQUIRE(system.anchorState().level == explicitSystemSelection.level + 1);
}

void testPresentationBuilderPreparesStateBeforeRender()
{
    using world::celestial::BodyType;
    using world::celestial::SystemMapBody;
    using world::celestial::SystemMapObject;
    using world::celestial::SystemMapObjectKind;
    using world::celestial::SystemMapSnapshot;

    const Viewport viewport = testViewport();
    const SystemMapPresentationBuilder builder;

    SystemMapView view;
    view.state().selectedBodyId = "stale-body";
    view.state().selectedHubId = "stale-hub";
    view.state().selectedHubParentBodyId = "stale-parent";
    view.state().navigationCellExplicitlySelected = true;

    SystemMapSnapshot snapshot;
    snapshot.systemId = 77;
    snapshot.universeTimeSeconds = 1000.0;
    snapshot.universeTimeScale = 2.0;

    SystemMapBody star;
    star.id = "star";
    star.name = "Star";
    star.type = BodyType::Star;
    snapshot.bodies.push_back(star);

    SystemMapBody planet;
    planet.id = "planet";
    planet.name = "Planet";
    planet.parentId = "star";
    planet.type = BodyType::Planet;
    planet.positionAu = glm::dvec3(2.0, 0.0, 0.0);
    planet.orbitCenterAu = glm::dvec3(0.0);
    planet.orbitRadiusAu = 2.0;
    planet.drawOrbit = true;
    planet.orbitalPeriodDays = 10.0;
    planet.dayLengthHours = 20.0;
    snapshot.bodies.push_back(planet);

    SystemMapObject hub;
    hub.stableId = "hub-alpha";
    hub.name = "Hub Alpha";
    hub.parentBodyId = "planet";
    hub.kind = SystemMapObjectKind::Hub;
    snapshot.objects.push_back(hub);

    const auto first =
        builder.build(
            view,
            viewport,
            snapshot,
            50.0
        );

    REQUIRE(first.systemId == snapshot.systemId);
    REQUIRE_NEAR(first.timeSeconds, 1000.0, 1.0e-10);
    REQUIRE_CLOSE(first.systemScale, 35.0, 1.0e-6, 1.0e-7);
    REQUIRE(first.bodies.size() == 2);

    REQUIRE(view.state().navigationGrid.systemId() == 77);
    REQUIRE(view.state().lastCameraFitSystemId == 77);
    REQUIRE_VEC_NEAR(view.state().camera.target, glm::dvec3(0.0), 1.0e-12);
    REQUIRE_CLOSE(
        view.state().camera.distance,
        view.controls().fittedSystemRadiusWorld *
            view.controls().initialFitPadding,
        1.0e-6,
        1.0e-7
    );
    REQUIRE(view.state().selectedBodyId.empty());
    REQUIRE(view.state().selectedHubId.empty());
    REQUIRE(view.state().selectedHubParentBodyId.empty());
    REQUIRE(!view.state().navigationCellExplicitlySelected);

    const auto firstPlanet =
        std::find_if(
            first.bodies.begin(),
            first.bodies.end(),
            [](const auto& body)
            {
                return body.id == "planet";
            }
        );

    REQUIRE(firstPlanet != first.bodies.end());

    const glm::dvec3 preservedTarget(8.0, -3.0, 1.5);
    view.state().camera.target = preservedTarget;
    view.state().camera.distance = 17.0f;
    view.state().selectedBodyId = "planet";
    view.state().selectedHubId = "hub-alpha";
    view.state().selectedHubParentBodyId = "planet";

    const auto second =
        builder.build(
            view,
            viewport,
            snapshot,
            55.0
        );

    /*
        wallNowSeconds drives hover/presentation timing only. A second build
        from the same world snapshot must not locally advance universe time.
    */
    REQUIRE_NEAR(second.timeSeconds, 1000.0, 1.0e-10);
    REQUIRE_VEC_NEAR(view.state().camera.target, preservedTarget, 1.0e-12);
    REQUIRE_NEAR(view.state().camera.distance, 17.0f, 1.0e-7);
    REQUIRE(view.state().selectedBodyId == "planet");
    REQUIRE(view.state().selectedHubId == "hub-alpha");
    REQUIRE(view.state().selectedHubParentBodyId == "planet");

    const auto secondPlanet =
        std::find_if(
            second.bodies.begin(),
            second.bodies.end(),
            [](const auto& body)
            {
                return body.id == "planet";
            }
        );

    REQUIRE(secondPlanet != second.bodies.end());
    REQUIRE_VEC_NEAR(
        secondPlanet->positionAu,
        firstPlanet->positionAu,
        1.0e-15
    );

    view.state().selectedBodyId = "star";
    view.state().selectedHubId = "missing-hub";
    view.state().selectedHubParentBodyId = "planet";

    builder.build(
        view,
        viewport,
        snapshot,
        56.0
    );

    REQUIRE(view.state().selectedBodyId.empty());
    REQUIRE(view.state().selectedHubId.empty());
    REQUIRE(view.state().selectedHubParentBodyId.empty());
}

void completeTransition(
    MapTransitionController& transition,
    double startedAt
)
{
    REQUIRE(transition.needsOutgoingCapture());
    transition.outgoingCaptured(startedAt);
    REQUIRE(transition.active());
    REQUIRE(transition.blocksInput());
    REQUIRE(transition.needsIncomingWarmup());
    REQUIRE_NEAR(transition.outgoingAlpha(), 1.0f, 1.0e-7);

    // One complete incoming frame stays fully covered by the captured
    // outgoing image before blend timing starts.
    transition.update(startedAt + 0.5);
    REQUIRE_NEAR(transition.outgoingAlpha(), 1.0f, 1.0e-7);
    transition.incomingFrameRendered(startedAt);
    REQUIRE(!transition.needsIncomingWarmup());

    const double durationSeconds =
        MapTransitionPresets::modeChange().durationSeconds;

    transition.update(
        startedAt + durationSeconds * 0.5
    );

    REQUIRE(transition.active());
    REQUIRE(transition.blocksInput());
    REQUIRE(transition.outgoingAlpha() > 0.0f);
    REQUIRE(transition.outgoingAlpha() < 1.0f);

    /*
        Do not require decimal duration arithmetic to land on the exact
        binary floating-point endpoint. A real frame also arrives after it.
    */
    transition.update(
        startedAt + durationSeconds + 1.0e-6
    );

    REQUIRE(!transition.active());
    REQUIRE(!transition.blocksInput());
    REQUIRE_NEAR(transition.outgoingAlpha(), 0.0f, 1.0e-7);
}

void testLocalPresentationBuilderPreparesDetailAndHub()
{
    using world::celestial::DetailMapSnapshot;
    using world::celestial::DetailObjectClass;
    using world::celestial::DetailSceneKind;
    using world::celestial::HubMapSnapshot;
    using world::celestial::LocalSceneObject;

    const Viewport viewport = testViewport();
    const LocalMapPresentationBuilder builder;

    DetailMapView detailView;
    detailView.camera().zoom = 0.1;
    detailView.camera().pan = glm::dvec2(75.0, -35.0);
    detailView.selectHub("stale-hub", "stale-parent");

    DetailMapSnapshot invalidDetail;
    invalidDetail.valid = false;

    const auto invalidPresentation =
        builder.buildDetail(
            detailView,
            viewport,
            invalidDetail
        );

    REQUIRE(!invalidPresentation.valid);
    REQUIRE(detailView.state().selectedHubId == "stale-hub");

    DetailMapSnapshot spatial;
    spatial.valid = true;
    spatial.hasCentralBody = false;
    spatial.detailTarget.sceneKind =
        DetailSceneKind::SpatialVolume;
    spatial.detailHalfExtentMeters = 1000.0;

    const auto spatialPresentation =
        builder.buildDetail(
            detailView,
            viewport,
            spatial
        );

    REQUIRE(spatialPresentation.valid);
    REQUIRE(spatialPresentation.sceneIsSpatialVolume);
    REQUIRE_CLOSE(
        detailView.camera().zoom,
        detailView.controls().spatialVolumeMinimumZoom,
        1.0e-12,
        1.0e-12
    );
    REQUIRE_VEC_NEAR(
        glm::dvec3(detailView.camera().pan, 0.0),
        glm::dvec3(0.0),
        1.0e-12
    );
    REQUIRE(detailView.state().selectedHubId.empty());
    REQUIRE(spatialPresentation.frame.hubScreenPoints.empty());
    REQUIRE(spatialPresentation.scale > 0.0);

    DetailMapSnapshot bodyScene;
    bodyScene.valid = true;
    bodyScene.hasCentralBody = true;
    bodyScene.planetBodyId = "planet";
    bodyScene.planetRadiusMeters = 1000.0;
    bodyScene.detailTarget.sceneKind =
        DetailSceneKind::CelestialBody;

    LocalSceneObject hub;
    hub.valid = true;
    hub.objectClass = DetailObjectClass::Hub;
    hub.kind = "hub";
    hub.stableId = "hub-alpha";
    hub.name = "Hub Alpha";
    hub.positionMeters = glm::dvec3(1500.0, 0.0, 0.0);
    bodyScene.scene.objects.push_back(hub);

    detailView.selectHub("hub-alpha", "planet");

    const auto bodyPresentation =
        builder.buildDetail(
            detailView,
            viewport,
            bodyScene
        );

    REQUIRE(!bodyPresentation.sceneIsSpatialVolume);
    REQUIRE(bodyPresentation.selectedHubId == "hub-alpha");
    REQUIRE(bodyPresentation.selectedHubParentBodyId == "planet");
    REQUIRE(bodyPresentation.frame.hubScreenPoints.size() == 1);
    REQUIRE(
        bodyPresentation.frame.hubScreenPoints.front().hubId ==
        "hub-alpha"
    );

    HubMapView hubView;
    hubView.beginScene();

    HubMapSnapshot hubScene;
    hubScene.valid = true;
    hubScene.systemId = 77;
    hubScene.hubId = "hub-alpha";
    hubScene.scene.halfExtentMeters = 4000.0;

    LocalSceneObject module;
    module.valid = true;
    module.objectClass = DetailObjectClass::Hub;
    module.name = "Prime module";
    module.prime = true;
    module.positionMeters = glm::dvec3(300.0, 0.0, 0.0);
    module.sizeMeters = glm::dvec3(200.0, 100.0, 300.0);
    LocalSceneObject ship;
    ship.valid = true;
    ship.objectClass = DetailObjectClass::Ship;
    ship.name = "Player ship";
    ship.player = true;
    ship.positionMeters = glm::dvec3(-500.0, 0.0, 0.0);

    // Snapshot order is deliberately reversed. Presentation order must stay
    // compatible with the old render path: modules first, ships second.
    hubScene.scene.objects.push_back(ship);
    hubScene.scene.objects.push_back(module);

    const auto hubPresentation =
        builder.buildHub(
            hubView,
            viewport,
            hubScene
        );

    REQUIRE(hubPresentation.valid);
    REQUIRE(hubPresentation.systemId == 77);
    REQUIRE(hubPresentation.hubId == "hub-alpha");
    REQUIRE(hubPresentation.scale > 0.0);
    REQUIRE(hubPresentation.frame.pickables.size() == 2);
    REQUIRE(hubPresentation.frame.pickables[0].priority == 20);
    REQUIRE(hubPresentation.frame.pickables[1].priority == 100);
}



void testPreparedFrameDrivesSystemPicking()
{
    SystemMapView view = makeSystemView();
    SystemMapFrameData frame;

    game::system_map::SystemMapBodyScreenPoint bodyPoint;
    bodyPoint.bodyId = "planet";
    bodyPoint.name = "Planet";
    bodyPoint.screen = glm::vec2(320.0f, 240.0f);
    bodyPoint.depth = 0.0f;
    bodyPoint.visible = true;
    bodyPoint.screenRadiusPx = 18.0f;
    frame.bodyScreenPoints.push_back(bodyPoint);
    frame.bodyAbsolutePositionById.emplace(
        "planet",
        glm::dvec3(12.0, 3.0, -4.0)
    );
    frame.bodyPhysicalRadiusWorldById.emplace(
        "planet",
        2.5f
    );

    game::system_map::SystemMapOrbitPivotScreenPoint pivotPoint;
    pivotPoint.bodyId = "planet";
    pivotPoint.screen = bodyPoint.screen;
    pivotPoint.depth = 0.0f;
    pivotPoint.cameraDepthWorld = 25.0;
    pivotPoint.visible = true;
    pivotPoint.screenRadiusPx = 18.0f;
    frame.orbitPivotScreenPoints.push_back(pivotPoint);

    game::system_map::SystemMapHubScreenPoint hubPoint;
    hubPoint.hubId = "hub-alpha";
    hubPoint.parentBodyId = "planet";
    hubPoint.name = "Hub Alpha";
    hubPoint.screen = glm::vec2(500.0f, 260.0f);
    hubPoint.depth = 0.0f;
    hubPoint.visible = true;
    hubPoint.screenRadiusPx = 15.0f;
    frame.hubScreenPoints.push_back(hubPoint);
    frame.objectAbsolutePositionById.emplace(
        "hub-alpha",
        glm::dvec3(20.0, 0.0, 1.0)
    );

    const SystemMapFrameInteractionContext context(
        frame,
        view.controls()
    );

    REQUIRE(
        context.pickSystemBodyId(320.0, 240.0) ==
        std::optional<std::string>("planet")
    );

    const auto cameraTarget =
        context.pickSystemCameraBodyTarget(
            320.0,
            240.0,
            testViewport()
        );

    REQUIRE(cameraTarget.has_value());
    REQUIRE(cameraTarget->bodyId == "planet");
    REQUIRE_VEC_NEAR(
        cameraTarget->absolutePosition,
        glm::dvec3(12.0, 3.0, -4.0),
        1.0e-12
    );
    REQUIRE_NEAR(cameraTarget->physicalRadiusWorld, 2.5, 1.0e-12);

    const auto hub =
        context.pickSystemHubSelection(500.0, 260.0);

    REQUIRE(hub.has_value());
    REQUIRE(hub->hubId == "hub-alpha");
    REQUIRE(hub->parentBodyId == "planet");

    const auto hubPosition =
        context.systemObjectAbsolutePosition("hub-alpha");

    REQUIRE(hubPosition.has_value());
    REQUIRE_VEC_NEAR(
        *hubPosition,
        glm::dvec3(20.0, 0.0, 1.0),
        1.0e-12
    );
}

void testCameraSnapshotsOwnProjectionContracts()
{
    const Viewport viewport = testViewport();

    SystemMapView systemView = makeSystemView();
    systemView.state().camera.target = glm::dvec3(7.0, -3.0, 4.0);
    systemView.state().camera.yaw = 0.73f;
    systemView.state().camera.pitch = -0.41f;
    systemView.state().camera.distance = 86.0f;

    const auto systemCamera =
        systemView.cameraSnapshot(viewport);

    REQUIRE_VEC_NEAR(
        systemCamera.targetAbsolute,
        systemView.state().camera.target,
        1.0e-12
    );
    REQUIRE_VEC_NEAR(
        systemCamera.basis.direction,
        systemView.cameraDirectionWorld(),
        1.0e-12
    );
    REQUIRE_VEC_NEAR(
        systemCamera.basis.up,
        systemView.cameraUpWorld(),
        1.0e-12
    );
    REQUIRE_VEC_NEAR(
        systemCamera.basis.right,
        systemView.cameraRightWorld(),
        1.0e-12
    );
    REQUIRE_NEAR(
        glm::dot(
            systemCamera.basis.direction,
            systemCamera.basis.up
        ),
        0.0,
        1.0e-6
    );
    REQUIRE_NEAR(
        glm::length(systemCamera.basis.direction),
        1.0,
        1.0e-6
    );
    REQUIRE_NEAR(
        glm::length(systemCamera.basis.up),
        1.0,
        1.0e-6
    );

    const glm::mat4 expectedView = systemView.viewMatrix();
    const glm::mat4 expectedProjection =
        systemView.projectionMatrix(viewport);

    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            REQUIRE_NEAR(
                systemCamera.view[column][row],
                expectedView[column][row],
                1.0e-6
            );
            REQUIRE_NEAR(
                systemCamera.projection[column][row],
                expectedProjection[column][row],
                1.0e-6
            );
        }
    }

    LocalMapCameraSnapshot localCamera;
    localCamera.state.yaw = 0.42;
    localCamera.state.pitch = -0.31;
    localCamera.state.zoom = 1.7;
    localCamera.state.pan = glm::dvec2(12.0, -9.0);
    localCamera.scale = 0.035;
    localCamera.centerPx = glm::dvec2(640.0, 360.0);
    localCamera.originMeters = glm::dvec3(20.0, -8.0, 11.0);

    const glm::dvec3 cameraPlanePoint(
        17.0,
        -6.0,
        0.0
    );
    const glm::dvec3 worldPoint =
        localCamera.originMeters +
        localCamera.vectorFromCamera(cameraPlanePoint);

    const glm::dvec2 screenPoint =
        localCamera.project(worldPoint);

    REQUIRE_VEC_NEAR(
        localCamera.unprojectPlane(screenPoint),
        worldPoint,
        1.0e-9
    );

    localCamera.perspectiveEnabled = true;
    localCamera.perspectiveCameraDistanceMeters = 1000.0;

    const glm::dvec3 nearPoint =
        localCamera.originMeters +
        localCamera.vectorFromCamera(
            glm::dvec3(20.0, 0.0, 100.0)
        );
    const glm::dvec3 farPoint =
        localCamera.originMeters +
        localCamera.vectorFromCamera(
            glm::dvec3(20.0, 0.0, -100.0)
        );

    const double nearOffset =
        std::abs(
            localCamera.project(nearPoint).x -
            localCamera.centerPx.x -
            localCamera.state.pan.x
        );
    const double farOffset =
        std::abs(
            localCamera.project(farPoint).x -
            localCamera.centerPx.x -
            localCamera.state.pan.x
        );

    REQUIRE(nearOffset > farOffset);
}

void testGalaxyTerminalCubeEntersSystemOrEmptySector()
{
    world::celestial::GalaxyMapSnapshot galaxy;
    world::celestial::GalaxyMapSystem known;
    known.id = 42;
    known.name = "Known";
    known.starType = "G";
    known.positionLy = glm::dvec3(0.25, 0.10, -0.05);
    galaxy.systems.push_back(known);

    world::celestial::PlayerNavigationState navigation;
    navigation.currentSystemId = known.id;
    navigation.systemLocalAu = glm::dvec3(0.0);

    GalaxyMapView view;
    view.onEntered(galaxy, navigation);

    auto& grid = view.state().navigationGrid;
    const auto setAnchor = [](
        game::navigation::GalaxyNavigationGrid& targetGrid,
        const glm::dvec3& position)
    {
        targetGrid.setAnchorFromPositionLy(position);
    };

    while (grid.canRefine())
    {
        REQUIRE(
            game::navigation::applyCubicNavigationLevelActionAtPosition(
                CubicNavigationLevelAction::Refine,
                grid,
                known.positionLy,
                setAnchor,
                true));
    }

    REQUIRE(grid.level() == grid.maximumLevel());
    REQUIRE(
        game::navigation::cubicNavigationWheelAction(
            1.0f,
            10000.0f,
            1200,
            800,
            grid.canRefine(),
            grid.canCoarsen(),
            true) == CubicNavigationLevelAction::EnterChildMap);

    const auto knownIntent = view.entryIntentForPosition(
        galaxy,
        known.positionLy,
        known.id);
    REQUIRE(knownIntent.entersKnownSystem());
    REQUIRE(knownIntent.systemId == known.id);

    const auto knownIndex = grid.nearestIndexForPositionLy(
        known.positionLy,
        grid.maximumLevel());
    bool foundEmpty = false;
    for (int dx = 1; dx <= 4 && !foundEmpty; ++dx)
    {
        auto candidateIndex = knownIndex;
        candidateIndex.x += dx;
        if (!grid.isCellNavigable(candidateIndex, grid.maximumLevel()))
            continue;

        const auto candidateCell = grid.cell(candidateIndex, grid.maximumLevel());
        const auto emptyIntent = view.entryIntentForPosition(
            galaxy,
            candidateCell.center,
            -1);
        if (emptyIntent.entersEmptySector())
        {
            foundEmpty = true;
            REQUIRE_VEC_NEAR(emptyIntent.positionLy, candidateCell.center, 1.0e-12);
        }
    }
    REQUIRE(foundEmpty);
}

void requireNavigationAction(
    const game::presentation::SystemMapPanelNavigationAction& actual,
    SystemMapPanelActionType action,
    bool enabled
)
{
    REQUIRE(actual.action == action);
    REQUIRE(actual.enabled == enabled);
}

void testSystemMapPanelNavigationActionMatrix()
{
    using game::presentation::buildSystemMapPanelNavigationActions;

    SystemMapPanelPresentation panel;

    panel.mode = MapMode::Galaxy;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenSystem, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenDetail, false);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenHub, false);
    }

    panel.mode = MapMode::System;
    panel.canOpenDetail = false;
    panel.canOpenHub = false;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenGalaxy, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenDetail, false);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenHub, false);
    }

    panel.canOpenDetail = true;
    panel.canOpenHub = true;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenGalaxy, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenDetail, true);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenHub, true);
    }

    panel.mode = MapMode::Detail;
    panel.canOpenHub = false;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenSystem, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenGalaxy, true);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenHub, false);
    }

    panel.canOpenHub = true;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenSystem, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenGalaxy, true);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenHub, true);
    }

    panel.mode = MapMode::Hub;
    {
        const auto actions = buildSystemMapPanelNavigationActions(panel);
        requireNavigationAction(actions[0], SystemMapPanelActionType::OpenDetail, true);
        requireNavigationAction(actions[1], SystemMapPanelActionType::OpenSystem, true);
        requireNavigationAction(actions[2], SystemMapPanelActionType::OpenGalaxy, true);
    }
}

void requirePanelCommand(
    const SystemMapPanelAction& action,
    MapMode mode,
    SystemMapPanelCommandType expected,
    int expectedSystemId = -1
)
{
    const auto command = game::presentation::resolveSystemMapPanelAction(
        action,
        mode
    );
    REQUIRE(command.type == expected);
    REQUIRE(command.systemId == expectedSystemId);
}

void testSystemMapPanelCommandSemantics()
{
    const SystemMapPanelAction select{
        SystemMapPanelActionType::SelectSystem,
        42
    };
    for (MapMode mode : {
             MapMode::Galaxy,
             MapMode::System,
             MapMode::Detail,
             MapMode::Hub})
    {
        requirePanelCommand(
            select,
            mode,
            SystemMapPanelCommandType::SelectSystem,
            42
        );
    }

    const SystemMapPanelAction galaxy{
        SystemMapPanelActionType::OpenGalaxy,
        -1
    };
    requirePanelCommand(galaxy, MapMode::Galaxy, SystemMapPanelCommandType::None);
    requirePanelCommand(galaxy, MapMode::System, SystemMapPanelCommandType::Galaxy);
    requirePanelCommand(galaxy, MapMode::Detail, SystemMapPanelCommandType::Galaxy);
    requirePanelCommand(galaxy, MapMode::Hub, SystemMapPanelCommandType::Galaxy);

    const SystemMapPanelAction system{
        SystemMapPanelActionType::OpenSystem,
        -1
    };
    requirePanelCommand(
        system,
        MapMode::Galaxy,
        SystemMapPanelCommandType::OpenSelectedGalaxyTarget
    );
    requirePanelCommand(system, MapMode::System, SystemMapPanelCommandType::None);
    requirePanelCommand(system, MapMode::Detail, SystemMapPanelCommandType::LoadedSystem);
    requirePanelCommand(system, MapMode::Hub, SystemMapPanelCommandType::LoadedSystem);

    const SystemMapPanelAction detail{
        SystemMapPanelActionType::OpenDetail,
        -1
    };
    requirePanelCommand(detail, MapMode::Galaxy, SystemMapPanelCommandType::None);
    requirePanelCommand(detail, MapMode::System, SystemMapPanelCommandType::SelectedDetail);
    requirePanelCommand(detail, MapMode::Detail, SystemMapPanelCommandType::None);
    requirePanelCommand(detail, MapMode::Hub, SystemMapPanelCommandType::LoadedDetail);

    const SystemMapPanelAction hub{
        SystemMapPanelActionType::OpenHub,
        -1
    };
    requirePanelCommand(hub, MapMode::Galaxy, SystemMapPanelCommandType::None);
    requirePanelCommand(hub, MapMode::System, SystemMapPanelCommandType::Hub);
    requirePanelCommand(hub, MapMode::Detail, SystemMapPanelCommandType::Hub);
    requirePanelCommand(hub, MapMode::Hub, SystemMapPanelCommandType::None);
}

void testGalaxySystemDetailHubTransitionSequence()
{
    MapMode mode = MapMode::Galaxy;
    MapTransitionController transition;

    const auto transitionTo = [&](MapMode destination, double nowSeconds)
    {
        const MapMode oldMode = mode;

        REQUIRE(
            transition.begin(
                MapTransitionPresets::modeChange(),
                [&mode, destination]()
                {
                    mode = destination;
                }
            )
        );

        REQUIRE(mode == oldMode);
        completeTransition(transition, nowSeconds);
        REQUIRE(mode == destination);
    };

    transitionTo(MapMode::System, 10.0);
    transitionTo(MapMode::Detail, 20.0);
    transitionTo(MapMode::Hub, 30.0);

    REQUIRE(mode == MapMode::Hub);
}

struct TraceStep
{
    SystemMapInputFrame frame;
    double scrollDelta = 0.0;
};

struct TraceSnapshot
{
    glm::dvec3 target {0.0};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 0.0f;
    int level = 0;
    CubicGridIndex anchor;
    bool rotating = false;
    bool panning = false;
    bool explicitSelection = false;
};

TraceSnapshot snapshot(const SystemMapView& view)
{
    const auto& state = view.state();

    TraceSnapshot result;
    result.target = state.camera.target;
    result.yaw = state.camera.yaw;
    result.pitch = state.camera.pitch;
    result.distance = state.camera.distance;
    result.level = state.navigationGrid.level();
    result.anchor = state.navigationGrid.anchorIndex();
    result.rotating = state.camera.rotating;
    result.panning = state.camera.panning;
    result.explicitSelection = state.navigationCellExplicitlySelected;
    return result;
}

std::vector<TraceSnapshot> replayTrace(
    const std::vector<TraceStep>& trace
)
{
    SystemMapView view = makeSystemView();
    MockSystemMapInteractionContext context;
    const SystemMapInteraction interaction;
    double pendingScrollY = 0.0;

    std::vector<TraceSnapshot> snapshots;
    snapshots.reserve(trace.size());

    for (const TraceStep& step : trace)
    {
        pendingScrollY += step.scrollDelta;

        interaction.handleInput(
            view,
            context,
            step.frame,
            pendingScrollY
        );

        REQUIRE(pendingScrollY == 0.0 || step.scrollDelta == 0.0);
        REQUIRE(finiteVector(view.state().camera.target));
        REQUIRE(std::isfinite(view.state().camera.yaw));
        REQUIRE(std::isfinite(view.state().camera.pitch));
        REQUIRE(std::isfinite(view.state().camera.distance));
        REQUIRE(view.state().camera.distance > 0.0f);

        snapshots.push_back(snapshot(view));
    }

    return snapshots;
}

void requireSnapshotNear(
    const TraceSnapshot& actual,
    const TraceSnapshot& expected
)
{
    REQUIRE_VEC_NEAR(actual.target, expected.target, 1.0e-10);
    REQUIRE_NEAR(actual.yaw, expected.yaw, 1.0e-7);
    REQUIRE_NEAR(actual.pitch, expected.pitch, 1.0e-7);
    REQUIRE_NEAR(actual.distance, expected.distance, 1.0e-6);
    REQUIRE(actual.level == expected.level);
    REQUIRE(actual.anchor == expected.anchor);
    REQUIRE(actual.rotating == expected.rotating);
    REQUIRE(actual.panning == expected.panning);
    REQUIRE(actual.explicitSelection == expected.explicitSelection);
}

void testMouseAndScrollTraceIsRepeatable()
{
    const Viewport viewport = testViewport();

    std::vector<TraceStep> trace;
    trace.push_back({inputFrame(viewport, 600.0, 400.0, false, false, 1.0), 0.0});
    trace.push_back({inputFrame(viewport, 600.0, 400.0, true, false, 1.1), 0.0});
    trace.push_back({inputFrame(viewport, 624.0, 408.0, true, false, 1.2), 0.0});
    trace.push_back({inputFrame(viewport, 624.0, 408.0, false, false, 1.3), 0.0});
    trace.push_back({inputFrame(viewport, 624.0, 408.0, false, true, 1.4), 0.0});
    trace.push_back({inputFrame(viewport, 610.0, 420.0, false, true, 1.5), 0.0});
    trace.push_back({inputFrame(viewport, 610.0, 420.0, false, false, 1.6), 0.0});
    trace.push_back({inputFrame(viewport, 610.0, 420.0, false, false, 1.7), 1.0});
    trace.push_back({inputFrame(viewport, 610.0, 420.0, false, false, 1.8), -1.0});

    const std::vector<TraceSnapshot> first = replayTrace(trace);
    const std::vector<TraceSnapshot> second = replayTrace(trace);

    REQUIRE(first.size() == second.size());

    for (std::size_t i = 0; i < first.size(); ++i)
        requireSnapshotNear(first[i], second[i]);

    REQUIRE(first[2].yaw != first[1].yaw);
    REQUIRE(first[2].pitch != first[1].pitch);
    REQUIRE(glm::length(first[5].target - first[4].target) > 0.0);
    REQUIRE(first[7].distance < first[6].distance);
    REQUIRE(first[8].distance > first[7].distance);
}


void testCockpitNavigationTargetSpeedUsesCommonTravelFrame()
{
    game::navigation::DynamicMotionState motion;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.travelFrame.valid = true;
    motion.travelFrame.systemId = 0;
    motion.travelFrame.originMeters = glm::dvec3(0.0);
    motion.travelFrame.linearVelocityMps = glm::dvec3(1000.0, 0.0, 0.0);
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(45.0, 0.0, 0.0);

    const auto local =
        game::presentation::resolveCockpitNavigationTargetSpeed(
            motion,
            glm::dvec3(200.0, 0.0, 0.0),
            glm::dvec3(1180.0, 0.0, 0.0)
        );

    REQUIRE(local.mode == game::presentation::NavigationHudSpeedMode::Relative);
    REQUIRE_NEAR(local.speedMps, 180.0, 1.0e-9);

    // Closing speed would be 135 m/s here. The HUD must instead show the
    // target's own 180 m/s in the same frame as the player's 45 m/s HUD value.
    REQUIRE(std::abs(local.speedMps - 135.0) > 1.0);

    motion.mode = game::navigation::MotionMode::Cruise;
    const auto global =
        game::presentation::resolveCockpitNavigationTargetSpeed(
            motion,
            glm::dvec3(200.0, 0.0, 0.0),
            glm::dvec3(1180.0, 0.0, 0.0)
        );
    REQUIRE(global.mode == game::presentation::NavigationHudSpeedMode::Global);
    REQUIRE_NEAR(global.speedMps, 1180.0, 1.0e-9);

    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.travelFrame.valid = false;
    const auto noFrame =
        game::presentation::resolveCockpitNavigationTargetSpeed(
            motion,
            glm::dvec3(200.0, 0.0, 0.0),
            glm::dvec3(1180.0, 0.0, 0.0)
        );
    REQUIRE(noFrame.mode == game::presentation::NavigationHudSpeedMode::Global);
    REQUIRE_NEAR(noFrame.speedMps, 1180.0, 1.0e-9);
}

void testCockpitHubMarkerUsesPlayerPresentationFrame()
{
    ClientHubState hub;
    hub.id = "earth_orbital_hub";
    hub.systemId = 0;
    hub.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(1000.0, 0.0, 0.0)
        );
    hub.worldVelocityMps = glm::dvec3(10.0, 0.0, 0.0);

    ClientShipState player;
    player.renderReferenceFrame.valid = true;
    player.renderReferenceFrame.type =
        game::navigation::MotionMode::HubTactical;
    player.renderReferenceFrame.systemId = 0;
    // Production uses a ship-owned travel-frame identity here; Hub identity
    // is a separate field and must be the one used by HUD co-frame matching.
    player.renderReferenceFrame.frameId = "ship_travel_42";
    player.renderReferenceFrame.hubId = hub.id;
    player.renderReferenceFrame.originMeters =
        glm::dvec3(1250.0, 40.0, -20.0);
    player.renderReferenceFrame.velocityMetersPerSecond =
        glm::dvec3(30.0, 4.0, 2.0);

    const auto resolved =
        game::presentation::detail::resolveHubFromPlayerPresentationFrame(
            hub,
            player
        );

    REQUIRE(resolved.valid);
    const glm::dvec3 resolvedMeters =
        world::coordinates::fullMeters(resolved.worldPosition);
    REQUIRE_NEAR(resolvedMeters.x, 1250.0, 1.0e-9);
    REQUIRE_NEAR(resolvedMeters.y, 40.0, 1.0e-9);
    REQUIRE_NEAR(resolvedMeters.z, -20.0, 1.0e-9);
    REQUIRE_NEAR(resolved.worldVelocityMps.x, 30.0, 1.0e-9);

    // The replicated Hub snapshot is deliberately stale by 250 metres.
    // Reusing it would reintroduce the snapshot-rate HUD marker staircase.
    REQUIRE(
        glm::length(
            resolvedMeters -
            world::coordinates::fullMeters(hub.worldPosition)
        ) > 200.0
    );

    player.renderReferenceFrame.hubId = "another_hub";
    const auto mismatched =
        game::presentation::detail::resolveHubFromPlayerPresentationFrame(
            hub,
            player
        );
    REQUIRE(!mismatched.valid);
}

void testTacticalOverlayGlyphScaleAndAngles()
{
    REQUIRE_NEAR(mapObjectGlyphScale(100.0, 0.01), 1.0, 1.0e-12);

    const double closeScale = mapObjectGlyphScale(100.0, 0.50);
    REQUIRE(closeScale > 1.0);
    REQUIRE(closeScale <= 4.0);
    REQUIRE_NEAR(mapObjectGlyphScale(1000.0, 100.0), 4.0, 1.0e-12);

    REQUIRE_NEAR(
        mapObjectVelocityArrowLengthScale(0.0, MapObjectVelocityMode::Global),
        0.0,
        1.0e-12
    );
    const double globalSlow = mapObjectVelocityArrowLengthScale(
        100.0,
        MapObjectVelocityMode::Global
    );
    const double globalMedium = mapObjectVelocityArrowLengthScale(
        1000.0,
        MapObjectVelocityMode::Global
    );
    const double globalFast = mapObjectVelocityArrowLengthScale(
        10000.0,
        MapObjectVelocityMode::Global
    );
    REQUIRE(globalSlow > 0.0);
    REQUIRE(globalSlow < globalMedium);
    REQUIRE(globalMedium < globalFast);
    REQUIRE(globalFast < 1.0);
    REQUIRE_NEAR(
        mapObjectVelocityArrowLengthScale(100000.0, MapObjectVelocityMode::Global),
        1.0,
        1.0e-12
    );
    REQUIRE_NEAR(
        mapObjectVelocityArrowLengthScale(1000.0, MapObjectVelocityMode::Local),
        1.0,
        1.0e-12
    );

    const double localCrawl = mapObjectVelocityArrowLengthScale(
        1.0,
        MapObjectVelocityMode::Local
    );
    const double localFast = mapObjectVelocityArrowLengthScale(
        180.0,
        MapObjectVelocityMode::Local
    );
    REQUIRE(localCrawl > 0.0);
    REQUIRE(localFast > localCrawl * 5.0);
    REQUIRE_NEAR(
        mapObjectVelocityArrowLengthScale(125.0, MapObjectVelocityMode::Local),
        0.5,
        1.0e-12
    );
    REQUIRE_NEAR(
        mapObjectVelocityArrowLengthScale(50000.0, MapObjectVelocityMode::Global),
        0.5,
        1.0e-12
    );

    const auto [azForward, elForward] =
        stellarAzimuthElevationDeg(glm::dvec3(0.0, 0.0, 10.0));
    REQUIRE_NEAR(azForward, 0.0, 1.0e-9);
    REQUIRE_NEAR(elForward, 0.0, 1.0e-9);

    const auto [azRight, elRight] =
        stellarAzimuthElevationDeg(glm::dvec3(10.0, 0.0, 0.0));
    REQUIRE_NEAR(azRight, 90.0, 1.0e-9);
    REQUIRE_NEAR(elRight, 0.0, 1.0e-9);

    const auto [azUp, elUp] =
        stellarAzimuthElevationDeg(glm::dvec3(0.0, 10.0, 0.0));
    REQUIRE_NEAR(azUp, 0.0, 1.0e-9);
    REQUIRE_NEAR(elUp, 90.0, 1.0e-9);
}


void testClientNavigationTrackingOwnsCardsBodiesAndWaypointPanels()
{
    game::navigation::NavigationTrackingState state;

    state.rememberTacticalObject(
        "entity:42",
        "Scout",
        "Needle",
        glm::vec4(0.4f, 0.8f, 1.0f, 1.0f)
    );

    const auto earthPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(1000.0, 2000.0, 3000.0)
        );
    state.rememberCelestialBody(
        "body:0:earth",
        0,
        "earth",
        "Planet",
        "Earth",
        earthPosition,
        glm::vec4(0.4f, 0.7f, 1.0f, 1.0f)
    );

    auto& finish = state.rememberWaypointCandidate(
        "waypoint_candidate:S:6:1:2:3",
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(5000.0, 0.0, 0.0)
        ),
        "S6[1,2,3]"
    );
    const std::uint64_t finishId = finish.id;
    const std::string finishSourceId = finish.sourceObjectId;
    state.toggleWaypointRole(finishSourceId, game::navigation::NavigationWaypointRole::Finish);

    auto& intermediate = state.rememberWaypointCandidate(
        "waypoint_candidate:G:4:4:5:6",
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(9000.0, 0.0, 0.0)
        ),
        "G4[4,5,6]"
    );
    const std::string intermediateSourceId = intermediate.sourceObjectId;
    state.toggleWaypointRole(intermediateSourceId, game::navigation::NavigationWaypointRole::Intermediate);

    auto& intermediateTwo = state.rememberWaypointCandidate(
        "waypoint_candidate:G:4:7:8:9",
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(12000.0, 0.0, 0.0)
        ),
        "G4[7,8,9]"
    );
    const std::string intermediateTwoSourceId = intermediateTwo.sourceObjectId;
    state.toggleWaypointRole(
        intermediateTwoSourceId,
        game::navigation::NavigationWaypointRole::Intermediate
    );

    state.reconcileOpenCards({
        "entity:42",
        "body:0:earth",
        finishSourceId,
        intermediateSourceId,
        intermediateTwoSourceId
    });
    REQUIRE(state.tacticalObjects().size() == 1);
    REQUIRE(state.celestialBodies().size() == 1);
    REQUIRE(state.waypoints().size() == 3);
    REQUIRE(state.waypoints().front().id == finishId);
    REQUIRE(state.waypoints().front().role == game::navigation::NavigationWaypointRole::Finish);
    REQUIRE(state.waypoints()[1].role == game::navigation::NavigationWaypointRole::Intermediate);
    REQUIRE(state.waypoints()[1].sequence == 1);
    REQUIRE(state.waypoints()[2].role == game::navigation::NavigationWaypointRole::Intermediate);
    REQUIRE(state.waypoints()[2].sequence == 2);

    state.reconcileOpenCards({
        "body:0:earth",
        finishSourceId,
        intermediateTwoSourceId
    });
    REQUIRE(state.tacticalObjects().empty());
    REQUIRE(state.celestialBodies().size() == 1);
    REQUIRE(state.waypoints().size() == 2);
    REQUIRE(state.waypoints()[1].sourceObjectId == intermediateTwoSourceId);
    REQUIRE(state.waypoints()[1].sequence == 1);
}


void testWaypointInfoAffordanceWinsWithoutAutoOpeningCard()
{
    MapObjectOverlayState state;
    MapObjectOverlayFrame frame;
    const glm::dvec2 viewport(1280.0, 720.0);

    MapObjectOverlayItem largeBodyLikeObject;
    largeBodyLikeObject.objectId = "body:earth";
    largeBodyLikeObject.infoKind = MapObjectInfoKind::Celestial;
    largeBodyLikeObject.screenPx = glm::dvec2(500.0, 300.0);
    largeBodyLikeObject.hitRadiusPx = 30.0;
    largeBodyLikeObject.physicalSizeMeters = 12000000.0;
    largeBodyLikeObject.visible = true;
    frame.items.push_back(largeBodyLikeObject);

    MapObjectOverlayItem waypoint;
    waypoint.objectId = "waypoint_candidate:S:6:1:2:3";
    waypoint.infoKind = MapObjectInfoKind::WaypointCandidate;
    waypoint.screenPx = glm::dvec2(500.0, 300.0);
    waypoint.hitRadiusPx = 12.0;
    waypoint.physicalSizeMeters = 1.0;
    waypoint.screenAffordance = true;
    waypoint.visible = true;
    frame.items.push_back(waypoint);

    REQUIRE(!state.isOpen(waypoint.objectId));

    const auto down = state.handlePointer(
        frame,
        viewport,
        waypoint.screenPx,
        true,
        true,
        largeBodyLikeObject.physicalSizeMeters
    );
    REQUIRE(down.consumed);
    REQUIRE(down.toggledObjectId == waypoint.objectId);
    REQUIRE(state.isOpen(waypoint.objectId));

    (void)state.handlePointer(
        frame,
        viewport,
        waypoint.screenPx,
        true,
        false,
        largeBodyLikeObject.physicalSizeMeters
    );
}

void testTacticalOverlaySupportsMultipleIndependentCards()
{
    MapObjectOverlayState state;
    const glm::dvec2 viewport(1280.0, 720.0);

    MapObjectOverlayItem first;
    first.objectId = "ship:alpha";
    first.screenPx = glm::dvec2(300.0, 260.0);
    first.visible = true;

    MapObjectOverlayItem second;
    second.objectId = "ship:beta";
    second.screenPx = glm::dvec2(700.0, 420.0);
    second.visible = true;

    state.toggle(first, viewport);
    state.toggle(second, viewport);

    REQUIRE(state.isOpen(first.objectId));
    REQUIRE(state.isOpen(second.objectId));
    REQUIRE(state.orderedPanels().size() == 2);

    const std::string firstTrack = state.trackLabelFor(first.objectId);
    REQUIRE(firstTrack == state.trackLabelFor(first.objectId));
    REQUIRE(firstTrack != state.trackLabelFor(second.objectId));

    state.close(first.objectId);
    REQUIRE(!state.isOpen(first.objectId));
    REQUIRE(state.isOpen(second.objectId));
    REQUIRE(state.orderedPanels().size() == 1);
}

void testTacticalOverlayPointerToggleAndDragAreCaptured()
{
    MapObjectOverlayState state;
    MapObjectOverlayFrame frame;
    MapObjectOverlayItem item;
    item.objectId = "ship:17";
    item.screenPx = glm::dvec2(260.0, 220.0);
    item.hitRadiusPx = 18.0;
    item.visible = true;
    frame.items.push_back(item);

    const glm::dvec2 viewport(1000.0, 700.0);

    auto result = state.handlePointer(
        frame, viewport, item.screenPx, true, true);
    REQUIRE(result.consumed);
    REQUIRE(result.activatedObjectId == item.objectId);
    REQUIRE(result.toggledObjectId == item.objectId);
    REQUIRE(state.isActive(item.objectId));
    REQUIRE(state.isOpen(item.objectId));

    // Holding the click remains captured by the overlay so map-camera input
    // cannot start underneath a card/object click.
    result = state.handlePointer(
        frame, viewport, item.screenPx, true, true);
    REQUIRE(result.consumed);
    state.handlePointer(frame, viewport, item.screenPx, true, false);

    auto panels = state.orderedPanels();
    REQUIRE(panels.size() == 1);
    const glm::dvec2 originalTopLeft = panels.front().topLeftPx;
    const glm::dvec2 headerPoint = originalTopLeft + glm::dvec2(12.0, 10.0);

    result = state.handlePointer(frame, viewport, headerPoint, true, true);
    REQUIRE(result.consumed);
    const glm::dvec2 draggedPoint = headerPoint + glm::dvec2(80.0, 45.0);
    result = state.handlePointer(frame, viewport, draggedPoint, true, true);
    REQUIRE(result.consumed);
    state.handlePointer(frame, viewport, draggedPoint, true, false);

    panels = state.orderedPanels();
    REQUIRE(panels.size() == 1);
    REQUIRE(glm::length(panels.front().topLeftPx - originalTopLeft) > 1.0);

    // Repeated click on the glyph closes exactly that card.
    result = state.handlePointer(frame, viewport, item.screenPx, true, true);
    REQUIRE(result.consumed);
    REQUIRE(result.toggledObjectId == item.objectId);
    REQUIRE(!state.isOpen(item.objectId));
    state.handlePointer(frame, viewport, item.screenPx, true, false);
}

void testTacticalOverlayCardClickReactivatesObjectWithoutTogglingCard()
{
    MapObjectOverlayState state;
    MapObjectOverlayFrame frame;
    const glm::dvec2 viewport(1000.0, 700.0);

    MapObjectOverlayItem hub;
    hub.objectId = "hub:earth";
    hub.screenPx = glm::dvec2(220.0, 220.0);
    hub.visible = true;
    frame.items.push_back(hub);

    MapObjectOverlayItem ship;
    ship.objectId = "ship:other";
    ship.screenPx = glm::dvec2(620.0, 420.0);
    ship.visible = true;
    frame.items.push_back(ship);

    state.toggle(hub, viewport);
    state.toggle(ship, viewport);
    state.activate(ship.objectId);
    REQUIRE(state.isActive(ship.objectId));
    REQUIRE(!state.isActive(hub.objectId));

    const auto panels = state.orderedPanels();
    const auto hubPanel = std::find_if(
        panels.begin(),
        panels.end(),
        [&](const auto& panel)
        {
            return panel.objectId == hub.objectId;
        }
    );
    REQUIRE(hubPanel != panels.end());

    const glm::dvec2 bodyPoint =
        hubPanel->topLeftPx + glm::dvec2(80.0, 70.0);
    auto result = state.handlePointer(
        frame,
        viewport,
        bodyPoint,
        true,
        true
    );
    REQUIRE(result.consumed);
    REQUIRE(result.activatedObjectId == hub.objectId);
    REQUIRE(result.toggledObjectId.empty());
    REQUIRE(state.isActive(hub.objectId));
    REQUIRE(state.isOpen(hub.objectId));
    REQUIRE(state.isOpen(ship.objectId));

    state.handlePointer(frame, viewport, bodyPoint, true, false);
}

void testTacticalObjectSelectionClearsBodyCubeAndHubFocus()
{
    SystemMapView view = makeSystemView();
    auto& state = view.state();

    state.selectedBodyId = "earth";
    state.selectedHubId = "hub:earth";
    state.selectedHubParentBodyId = "earth";
    state.navigationGrid.selectCell(state.navigationGrid.anchorCell());
    state.navigationCellExplicitlySelected = true;
    state.selectedBodyTrackingEnabled = true;
    state.trackedBodyId = "earth";
    state.trackedBodyPositionValid = true;
    state.orbitPivotActive = true;

    SystemMapInteraction interaction;
    interaction.focusTacticalObjectSelection(view);

    REQUIRE(state.selectedBodyId.empty());
    REQUIRE(state.selectedHubId.empty());
    REQUIRE(state.selectedHubParentBodyId.empty());
    REQUIRE(!state.navigationGrid.hasSelectedCell());
    REQUIRE(!state.navigationCellExplicitlySelected);
    REQUIRE(!state.selectedBodyTrackingEnabled);
    REQUIRE(state.trackedBodyId.empty());
    REQUIRE(!state.trackedBodyPositionValid);
    REQUIRE(!state.orbitPivotActive);

    state.selectedBodyId = "earth";
    state.navigationGrid.selectCell(state.navigationGrid.anchorCell());
    state.navigationCellExplicitlySelected = true;

    interaction.focusTacticalObjectSelection(
        view,
        "hub:earth",
        "earth"
    );

    REQUIRE(state.selectedBodyId.empty());
    REQUIRE(state.selectedHubId == "hub:earth");
    REQUIRE(state.selectedHubParentBodyId == "earth");
    REQUIRE(!state.navigationGrid.hasSelectedCell());
    REQUIRE(!state.navigationCellExplicitlySelected);
}

void testTacticalOverlayCrowdedPickPrefersLargestObject()
{
    MapObjectOverlayState state;
    MapObjectOverlayFrame frame;

    MapObjectOverlayItem smallShip;
    smallShip.objectId = "ship:small";
    smallShip.screenPx = glm::dvec2(260.0, 220.0);
    smallShip.hitRadiusPx = 18.0;
    smallShip.physicalSizeMeters = 36.0;
    smallShip.visible = true;
    frame.items.push_back(smallShip);

    MapObjectOverlayItem largeHub;
    largeHub.objectId = "hub:large";
    largeHub.screenPx = glm::dvec2(266.0, 220.0);
    largeHub.hitRadiusPx = 18.0;
    largeHub.physicalSizeMeters = 4200.0;
    largeHub.visible = true;
    frame.items.push_back(largeHub);

    const glm::dvec2 viewport(1000.0, 700.0);
    const glm::dvec2 crowdedClick(260.0, 220.0);

    const auto result = state.handlePointer(
        frame,
        viewport,
        crowdedClick,
        true,
        true
    );

    REQUIRE(result.consumed);
    REQUIRE(result.toggledObjectId == largeHub.objectId);
    REQUIRE(state.isOpen(largeHub.objectId));
    REQUIRE(!state.isOpen(smallShip.objectId));

    state.handlePointer(
        frame,
        viewport,
        crowdedClick,
        true,
        false
    );

    // A larger body in the same direct System-map hit cluster owns the click
    // before tactical ship/Hub glyphs.
    MapObjectOverlayState bodyDominatedState;
    const auto suppressed = bodyDominatedState.handlePointer(
        frame,
        viewport,
        crowdedClick,
        true,
        true,
        12000000.0
    );
    REQUIRE(!suppressed.consumed);
    REQUIRE(!bodyDominatedState.isOpen(largeHub.objectId));
    REQUIRE(!bodyDominatedState.isOpen(smallShip.objectId));
}

void testPreparedSystemPickingPrefersLargestDirectSemanticObject()
{
    SystemMapFrameData frame;
    SystemMapView view = makeSystemView();

    game::system_map::SystemMapBodyScreenPoint largeBody;
    largeBody.bodyId = "planet-large";
    largeBody.name = "Large Planet";
    largeBody.screen = glm::vec2(420.0f, 260.0f);
    largeBody.depth = 0.0f;
    largeBody.visible = true;
    largeBody.screenRadiusPx = 14.0f;
    largeBody.physicalSizeMeters = 12000000.0;
    frame.bodyScreenPoints.push_back(largeBody);

    game::system_map::SystemMapBodyScreenPoint smallBody;
    smallBody.bodyId = "moon-small";
    smallBody.name = "Small Moon";
    smallBody.screen = glm::vec2(418.0f, 260.0f);
    smallBody.depth = 0.0f;
    smallBody.visible = true;
    smallBody.screenRadiusPx = 14.0f;
    smallBody.physicalSizeMeters = 3000000.0;
    frame.bodyScreenPoints.push_back(smallBody);

    game::system_map::SystemMapHubScreenPoint hub;
    hub.hubId = "hub-clustered";
    hub.parentBodyId = "planet-large";
    hub.name = "Clustered Hub";
    hub.screen = glm::vec2(419.0f, 260.0f);
    hub.depth = 0.0f;
    hub.visible = true;
    hub.screenRadiusPx = 15.0f;
    hub.physicalSizeMeters = 5000.0;
    frame.hubScreenPoints.push_back(hub);

    const SystemMapFrameInteractionContext context(
        frame,
        view.controls()
    );

    REQUIRE(
        context.pickSystemBodyId(419.0, 260.0) ==
        std::optional<std::string>("planet-large")
    );
    REQUIRE(
        !context.pickSystemHubSelection(419.0, 260.0).has_value()
    );
    REQUIRE_NEAR(
        context.largestDirectBodyPhysicalSizeMetersAt(419.0, 260.0),
        12000000.0,
        1.0e-9
    );

    // If only a smaller body competes with a physically larger Hub, the Hub
    // remains the semantic winner.
    frame.bodyScreenPoints[0].visible = false;
    frame.hubScreenPoints[0].physicalSizeMeters = 5000000.0;

    const SystemMapFrameInteractionContext hubWinsContext(
        frame,
        view.controls()
    );
    const auto hubPick =
        hubWinsContext.pickSystemHubSelection(419.0, 260.0);
    REQUIRE(hubPick.has_value());
    REQUIRE(hubPick->hubId == "hub-clustered");
}

void testTacticalOverlayTrajectorySeamDoesNotInventSamples()
{
    MapObjectOverlayFrame frame;
    REQUIRE(frame.trajectories.empty());

    MapObjectOverlayItem item;
    item.objectId = "ship:trajectory-test";
    item.velocityArrowMps = glm::dvec3(25.0, 0.0, 0.0);
    frame.items.push_back(item);

    // Current velocity is presentation data only; merely adding a moving
    // object must never manufacture history/prediction points.
    REQUIRE(frame.trajectories.empty());
}

void testHubMapAllowsCloseTacticalInspection()
{
    HubMapView view;
    REQUIRE(view.controls().maxZoom >= 64.0);
}

using TestFunction = void (*)();

struct TestCase
{
    const char* name;
    TestFunction function;
};

} // namespace

int main()
{
    const std::vector<TestCase> tests =
    {
        {"orbit preserves complete camera pose", testOrbitPreservesCompleteCameraPose},
        {"zoom scales pose and respects body clearance", testZoomScalesPoseAndRespectsBodyClearance},
        {"rotation pivot priority", testRotationPivotPriority},
        {"zoom pivot is cursor body or current target", testZoomPivotIsCursorBodyOrCurrentTarget},
        {"refine/coarsen preserve navigation point", testRefineCoarsenPreserveNavigationPoint},
        {"anchor and explicit selection semantics", testAnchorAndExplicitSelectionSemantics},
        {"Galaxy and System share cubic navigation core", testGalaxyAndSystemShareCubicNavigationCore},
        {"presentation builder prepares state before render", testPresentationBuilderPreparesStateBeforeRender},
        {"local presentation builder prepares Detail and Hub", testLocalPresentationBuilderPreparesDetailAndHub},
        {"prepared frame drives System picking", testPreparedFrameDrivesSystemPicking},
        {"prepared System crowded picking prefers largest direct semantic object", testPreparedSystemPickingPrefersLargestDirectSemanticObject},
        {"camera snapshots own projection contracts", testCameraSnapshotsOwnProjectionContracts},
        {"Galaxy terminal cube enters System/empty sector", testGalaxyTerminalCubeEntersSystemOrEmptySector},
        {"semantic map-panel navigation action matrix", testSystemMapPanelNavigationActionMatrix},
        {"semantic map-panel command routing", testSystemMapPanelCommandSemantics},
        {"Galaxy -> System -> Detail -> Hub transition sequence", testGalaxySystemDetailHubTransitionSequence},
        {"mouse and scroll trace is repeatable", testMouseAndScrollTraceIsRepeatable},
        {"cockpit target speed uses common travel frame", testCockpitNavigationTargetSpeedUsesCommonTravelFrame},
        {"cockpit Hub marker uses player presentation frame", testCockpitHubMarkerUsesPlayerPresentationFrame},
        {"tactical overlay glyph scale and stellar angles", testTacticalOverlayGlyphScaleAndAngles},
        {"client navigation tracking owns cards bodies and waypoint panels", testClientNavigationTrackingOwnsCardsBodiesAndWaypointPanels},
        {"waypoint info affordance wins without auto opening card", testWaypointInfoAffordanceWinsWithoutAutoOpeningCard},
        {"tactical overlay supports multiple independent cards", testTacticalOverlaySupportsMultipleIndependentCards},
        {"tactical overlay pointer toggle and drag are captured", testTacticalOverlayPointerToggleAndDragAreCaptured},
        {"tactical overlay card click reactivates object without toggling card", testTacticalOverlayCardClickReactivatesObjectWithoutTogglingCard},
        {"tactical object selection clears body cube and Hub focus", testTacticalObjectSelectionClearsBodyCubeAndHubFocus},
        {"tactical overlay crowded pick prefers largest object", testTacticalOverlayCrowdedPickPrefersLargestObject},
        {"tactical overlay trajectory seam does not invent samples", testTacticalOverlayTrajectorySeamDoesNotInventSamples},
        {"Hub map allows close tactical inspection", testHubMapAllowsCloseTacticalInspection}
    };

    int failed = 0;

    for (const TestCase& test : tests)
    {
        try
        {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "[FAIL] " << test.name << "\n       "
                      << error.what() << '\n';
        }
        catch (...)
        {
            ++failed;
            std::cerr << "[FAIL] " << test.name
                      << "\n       unknown exception\n";
        }
    }

    std::cout << "\n" << (tests.size() - static_cast<std::size_t>(failed))
              << '/' << tests.size() << " tests passed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
