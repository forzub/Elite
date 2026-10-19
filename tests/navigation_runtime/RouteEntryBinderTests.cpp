#include "src/game/navigation/planner/RouteEntryBinder.h"
#include "src/game/navigation/planner/BallisticEntryHandoffPolicy.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using game::navigation::planner::BallisticPrefixDisposition;
using game::navigation::planner::RouteCurveKind;
using game::navigation::planner::RouteCurveSegment;
using game::navigation::planner::RouteEntryBinder;
using game::navigation::planner::RouteGate;
using game::navigation::planner::RoutePlan;
using game::navigation::planner::RoutePlanDisposition;
using game::navigation::planner::RoutePlanFailureCode;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

RoutePlan straightPrefixPlan()
{
    RoutePlan plan;
    plan.disposition = RoutePlanDisposition::Ready;
    plan.failureCode = RoutePlanFailureCode::None;
    plan.failure.clear();

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 1000.0;
    line.startMeters = {0.0, 0.0, 0.0};
    line.endMeters = {1000.0, 0.0, 0.0};
    line.startForward = {1.0, 0.0, 0.0};
    line.endForward = {1.0, 0.0, 0.0};
    line.maxSpeedMps = 200.0;
    plan.routeCurves.push_back(line);

    RouteGate a;
    a.positionMeters = line.startMeters;
    a.forward = line.startForward;
    a.speedMps = 200.0;

    RouteGate b;
    b.positionMeters = line.endMeters;
    b.forward = line.endForward;
    b.speedMps = 200.0;

    plan.gates = {a, b};
    plan.executionGates = {a, b};
    return plan;
}

void testLivePositionMayAdvanceAlongBallisticPrefix()
{
    const auto plan = straightPrefixPlan();

    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {250.0, 0.0, 0.0},
            5.0
        );

    require(
        status.disposition == BallisticPrefixDisposition::OnPrefix,
        "live ballistic craft on authored first straight was rejected"
    );
    require(
        std::abs(status.alongMeters - 250.0) < 1.0e-9,
        "ballistic-prefix along distance is wrong"
    );
    require(
        std::abs(status.remainingMeters - 750.0) < 1.0e-9,
        "ballistic-prefix remaining distance is wrong"
    );
}

void testProjectedSecondRunReusesExistingStraight()
{
    using game::navigation::planner::BallisticEntryHandoffPolicy;
    auto plan = straightPrefixPlan();

    // Actual 500 m/s second run after a 12.115-second worker.
    // Current craft is 6129.79 m along an authored 23226.9 m prefix.
    // It will still be on the very same safe straight at connector ETA.
    plan.routeCurves.front().endMeters = {23226.9, 0.0, 0.0};
    plan.routeCurves.front().endProgressMeters = 23226.9;
    require(
        BallisticEntryHandoffPolicy::canReuseStraightPrefix(
            plan,
            {6129.79, 43.8855, 0.0},
            {500.0, 0.0, 0.0},
            45.0,
            BallisticEntryHandoffPolicy::forecastLeadSeconds(0.0)
        ),
        "500 m/s still-on-prefix flight triggered an unnecessary rebind"
    );
    require(
        std::abs(
            BallisticEntryHandoffPolicy::forecastLeadSeconds(2.0) - 2.5
        ) < 1.0e-9,
        "observed connector-worker latency was not used for handoff"
    );
    require(
        std::abs(
            BallisticEntryHandoffPolicy::forecastLeadSeconds(0.0) - 0.75
        ) < 1.0e-9,
        "first connector forecast did not use bounded bootstrap"
    );
}

void testForecastCatchesStaleUpcomingHandoff()
{
    using game::navigation::planner::BallisticEntryHandoffPolicy;
    const auto plan = straightPrefixPlan();
    require(
        !BallisticEntryHandoffPolicy::canReuseStraightPrefix(
            plan, {950.0, 0.0, 0.0},
            {500.0, 0.0, 0.0}, 20.0, 0.75
        ),
        "near-end 500 m/s craft was adopted though forecast passes first turn"
    );
    require(
        !BallisticEntryHandoffPolicy::canReuseStraightPrefix(
            plan, {250.0, 0.0, 0.0},
            {500.0, 100.0, 0.0}, 20.0, 0.75
        ),
        "predicted lateral departure was accepted as ballistic safe entry"
    );
    require(
        BallisticEntryHandoffPolicy::canReuseStraightPrefix(
            plan, {250.0, 0.0, 0.0},
            {500.0, 0.0, 0.0}, 20.0, 0.75
        ),
        "safe matching 500 m/s straight was spuriously rejected"
    );
}

void testObservedLiveDockingHandoffIsAccepted()
{
    const auto plan = straightPrefixPlan();
    // Actual incident: along=-0.000147163 m, cross=0.000195785 m,
    // prefix=1000 m and effectively zero ship speed after a 9.7 s worker.
    const auto status = RouteEntryBinder::inspectBallisticPrefix(
        plan, {-0.000147163, 0.000195785, 0.0}, 20.0
    );
    require(
        status.disposition == BallisticPrefixDisposition::OnPrefix,
        "actual live docking handoff was rejected again"
    );
    require(
        status.alongMeters == 0.0 &&
            status.remainingMeters == 1000.0,
        "actual live docking handoff should begin at clamped route start"
    );
}

void testSubMillimetreStartNoiseIsAccepted()
{
    const auto plan = straightPrefixPlan();

    // A real stationary docking handoff reported along=-0.000147 m and
    // cross-track <0.001 m. That must not trigger endless entry rebinding.
    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {-0.0005, 0.0, 0.0},
            5.0
        );

    require(
        status.disposition == BallisticPrefixDisposition::OnPrefix,
        "sub-millimetre start drift was mistaken for a stale connector"
    );
    require(
        std::abs(status.alongMeters) < 1.0e-12,
        "accepted start rounding must clamp progress to zero"
    );
    require(
        std::abs(status.remainingMeters - 1000.0) < 1.0e-9,
        "accepted start rounding must not inflate prefix remaining distance"
    );
}

void testSmallStationaryBallisticBackstepIsAccepted()
{
    const auto plan = straightPrefixPlan();
    const auto status = RouteEntryBinder::inspectBallisticPrefix(
        plan, {-0.005, 0.0, 0.0}, 5.0
    );
    require(
        status.disposition == BallisticPrefixDisposition::OnPrefix,
        "5 mm stationary backstep must remain inside 1 cm entry tolerance"
    );
    require(
        status.alongMeters == 0.0 &&
            status.remainingMeters == 1000.0,
        "stationary entry backstep did not clamp to true route start"
    );
}

void testSmallBackstepDoesNotBypassOffAxisRejection()
{
    const auto plan = straightPrefixPlan();
    const auto status = RouteEntryBinder::inspectBallisticPrefix(
        plan, {-0.005, 10.0, 0.0}, 5.0
    );
    require(
        status.disposition == BallisticPrefixDisposition::OffAxis,
        "entry backstep tolerance bypassed lateral corridor handoff check"
    );
}

void testRealNegativeStartOffsetStillRequiresRebind()
{
    const auto plan = straightPrefixPlan();
    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {-0.05, 0.0, 0.0},
            5.0
        );
    require(
        status.disposition == BallisticPrefixDisposition::BeforeStart,
        "real start offset beyond numerical tolerance was accepted"
    );
}

void testCentimetreEndpointDriftIsAcceptedWithoutInventingProgress()
{
    const auto plan = straightPrefixPlan();
    const auto status = RouteEntryBinder::inspectBallisticPrefix(
        plan, {1000.005, 0.0, 0.0}, 5.0
    );
    require(
        status.disposition == BallisticPrefixDisposition::OnPrefix,
        "5 mm overshoot of prefix endpoint triggered a false rebind"
    );
    require(
        status.alongMeters == 1000.0 &&
            status.remainingMeters == 0.0,
        "endpoint drift fabricated extra route progress"
    );
}

void testRealPastPrefixStillRequiresRebind()
{
    const auto plan = straightPrefixPlan();
    const auto status = RouteEntryBinder::inspectBallisticPrefix(
        plan, {1000.05, 0.0, 0.0}, 5.0
    );
    require(
        status.disposition == BallisticPrefixDisposition::PastPrefix,
        "5 cm true prefix overshoot was accepted"
    );
}

void testPastPrefixRequiresLateEntryRebind()
{
    const auto plan = straightPrefixPlan();

    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {1100.0, 0.0, 0.0},
            5.0
        );

    require(
        status.disposition == BallisticPrefixDisposition::PastPrefix,
        "craft beyond first straight was not marked for late-entry rebind"
    );
    require(
        status.remainingMeters == 0.0,
        "past-prefix result reported negative/positive remaining distance"
    );
}

void testOffAxisRequiresLateEntryRebind()
{
    const auto plan = straightPrefixPlan();

    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {250.0, 10.0, 0.0},
            5.0
        );

    require(
        status.disposition == BallisticPrefixDisposition::OffAxis,
        "off-axis live craft was incorrectly accepted on stale ballistic prefix"
    );
    require(
        std::abs(status.crossTrackMeters - 10.0) < 1.0e-9,
        "ballistic-prefix cross-track diagnostic is wrong"
    );
}

void testBeforeStartIsNotAcceptedAsOnPrefix()
{
    const auto plan = straightPrefixPlan();

    const auto status =
        RouteEntryBinder::inspectBallisticPrefix(
            plan,
            {-25.0, 0.0, 0.0},
            5.0
        );

    require(
        status.disposition == BallisticPrefixDisposition::BeforeStart,
        "position before authored start was incorrectly accepted as on-prefix"
    );
}

} // namespace

int main()
{
    try
    {
        testLivePositionMayAdvanceAlongBallisticPrefix();
        testProjectedSecondRunReusesExistingStraight();
        testForecastCatchesStaleUpcomingHandoff();
        testObservedLiveDockingHandoffIsAccepted();
        testSubMillimetreStartNoiseIsAccepted();
        testSmallStationaryBallisticBackstepIsAccepted();
        testSmallBackstepDoesNotBypassOffAxisRejection();
        testRealNegativeStartOffsetStillRequiresRebind();
        testCentimetreEndpointDriftIsAcceptedWithoutInventingProgress();
        testRealPastPrefixStillRequiresRebind();
        testPastPrefixRequiresLateEntryRebind();
        testOffAxisRequiresLateEntryRebind();
        testBeforeStartIsNotAcceptedAsOnPrefix();

        std::cout
            << "ROUTE ENTRY BINDER: PASS\n"
            << " - worker duration is irrelevant while live craft remains on spatial prefix\n"
            << " - past-prefix and off-axis states demand late-entry rebind\n"
            << " - no fixed planning-time projection is required\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ROUTE ENTRY BINDER: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
