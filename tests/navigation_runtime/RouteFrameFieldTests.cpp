#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/RouteFrameField.h"

namespace
{
bool near(double a, double b, double eps = 1.0e-6)
{
    return std::abs(a - b) <= eps;
}

bool nearVec(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double eps = 1.0e-5
)
{
    return glm::length(a - b) <= eps;
}

int fail(const char* message)
{
    std::cerr << "ROUTE FRAME FIELD TESTS: FAIL: "
              << message << std::endl;
    return EXIT_FAILURE;
}
}

int main()
{
    using game::navigation::RouteFrameField;
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 3000.0;
    line.startMeters = glm::dvec3(0.0, 0.0, 0.0);
    line.endMeters = glm::dvec3(3000.0, 0.0, 0.0);
    line.startForward = glm::dvec3(1.0, 0.0, 0.0);
    line.endForward = line.startForward;

    std::vector<RouteFrameField::Anchor> anchors;

    RouteFrameField::Anchor blue;
    blue.progressMeters = 1000.0;
    blue.upReference = glm::dvec3(0.0, 0.0, 1.0);
    blue.liveDockPhaseWeight = 0.0;
    anchors.push_back(blue);

    const auto field = RouteFrameField::build(
        {line},
        glm::dvec3(0.0, 1.0, 0.0),
        anchors,
        25.0
    );

    if (field.empty())
        return fail("field was empty");

    const auto blueUp =
        RouteFrameField::upAtProgress(field, 1000.0);
    if (!nearVec(blueUp, glm::dvec3(0.0, 0.0, 1.0), 1.0e-4))
        return fail("BLUE anchor did not own route up");

    const auto terminalUp =
        RouteFrameField::upAtProgress(field, 3000.0);
    if (!nearVec(terminalUp, glm::dvec3(0.0, 1.0, 0.0), 1.0e-4))
        return fail("terminal dock anchor did not own route up");

    const double beforeBlue =
        RouteFrameField::liveDockPhaseWeightAtProgress(field, 500.0);
    const double atBlue =
        RouteFrameField::liveDockPhaseWeightAtProgress(field, 1000.0);
    const double between =
        RouteFrameField::liveDockPhaseWeightAtProgress(field, 2000.0);
    const double atDock =
        RouteFrameField::liveDockPhaseWeightAtProgress(field, 3000.0);

    if (!near(beforeBlue, 0.0) || !near(atBlue, 0.0))
        return fail("dock phase leaked into BLUE-owned approach");

    if (!(between > 0.45 && between < 0.55))
        return fail("dock phase did not blend through transition span");

    if (!near(atDock, 1.0))
        return fail("terminal dock phase ownership was not full");

    double progress = -1.0;
    if (!RouteFrameField::progressAtPoint(
            {line},
            glm::dvec3(1250.0, 0.0, 0.0),
            progress))
    {
        return fail("could not resolve semantic point on route");
    }

    if (!near(progress, 1250.0, 1.0e-4))
        return fail("semantic point resolved to wrong route progress");

    std::cout << "ROUTE FRAME FIELD TESTS: PASS" << std::endl;
    return EXIT_SUCCESS;
}
