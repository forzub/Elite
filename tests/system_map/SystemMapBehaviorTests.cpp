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
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/game/system_map/LocalMapPresentationBuilder.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapMode.h"
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
using game::system_map::DetailMapView;
using game::system_map::HubMapView;
using game::system_map::LocalMapCameraSnapshot;
using game::system_map::LocalMapPresentationBuilder;
using game::system_map::MapMode;
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

    REQUIRE_NEAR(second.timeSeconds, 1010.0, 1.0e-10);
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
    REQUIRE(
        glm::length(
            secondPlanet->positionAu -
            firstPlanet->positionAu
        ) > 0.0
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
    REQUIRE_NEAR(transition.outgoingAlpha(), 1.0f, 1.0e-7);

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
        {"presentation builder prepares state before render", testPresentationBuilderPreparesStateBeforeRender},
        {"local presentation builder prepares Detail and Hub", testLocalPresentationBuilderPreparesDetailAndHub},
        {"prepared frame drives System picking", testPreparedFrameDrivesSystemPicking},
        {"camera snapshots own projection contracts", testCameraSnapshotsOwnProjectionContracts},
        {"Galaxy -> System -> Detail -> Hub transition sequence", testGalaxySystemDetailHubTransitionSequence},
        {"mouse and scroll trace is repeatable", testMouseAndScrollTraceIsRepeatable}
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
