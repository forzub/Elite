#include "src/game/navigation/NavigationWorkScheduler.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Scheduler = game::navigation::NavigationWorkScheduler;
using Job = game::navigation::NavigationPlannerJob;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Job makeJob(
    std::uint64_t actorId,
    std::uint64_t jobRevision,
    std::uint64_t objectiveRevision,
    std::uint64_t worldRevision,
    std::uint64_t capabilityRevision,
    Job::Priority priority = Job::Priority::Normal
)
{
    Job job;
    job.actorId = actorId;
    job.jobRevision = jobRevision;
    job.objectiveRevision = objectiveRevision;
    job.worldRevision = worldRevision;
    job.capabilityRevision = capabilityRevision;
    job.routeRevision = 1;
    job.priority = priority;
    job.scope = Job::Scope::LocalHorizon;
    job.trigger = Job::Trigger::ProgramCompleted;
    job.estimatedCostUnits = 1;
    return job;
}

void publish(
    Scheduler& scheduler,
    std::uint64_t actorId,
    std::uint64_t objectiveRevision,
    std::uint64_t capabilityRevision,
    std::uint64_t routeRevision = 1
)
{
    Scheduler::ActorRevisionStamp stamp;
    stamp.actorId = actorId;
    stamp.objectiveRevision = objectiveRevision;
    stamp.capabilityRevision = capabilityRevision;
    stamp.routeRevision = routeRevision;

    require(
        scheduler.publishActorRevision(stamp),
        "actor revision publication failed"
    );
}

void testDuplicateSuppressionAndPriorityUpgrade()
{
    Scheduler scheduler;
    require(
        scheduler.setCurrentWorldRevision(10),
        "world revision setup failed"
    );
    publish(scheduler, 1, 1, 1);

    Job normal = makeJob(1, 1, 1, 10, 1);
    const auto first = scheduler.enqueue(normal, 100);
    require(
        first.status == Scheduler::EnqueueStatus::Accepted,
        "first job was not accepted"
    );

    const auto duplicate = scheduler.enqueue(normal, 101);
    require(
        duplicate.status == Scheduler::EnqueueStatus::Duplicate &&
        duplicate.ticket == first.ticket,
        "duplicate job was not suppressed"
    );

    Job urgent = normal;
    urgent.priority = Job::Priority::Urgent;
    urgent.trigger = Job::Trigger::DynamicHazardInvalidated;

    const auto upgrade = scheduler.enqueue(urgent, 102);
    require(
        upgrade.status == Scheduler::EnqueueStatus::Replaced &&
        upgrade.ticket != first.ticket,
        "urgent replacement did not supersede pending normal job"
    );

    Scheduler::DispatchBudget budget;
    budget.maxJobs = 1;
    budget.maxCostUnits = 1;

    const auto dispatched = scheduler.dispatchSlice(102, budget);
    require(dispatched.count == 1, "priority upgrade was not dispatched");
    require(
        dispatched.items[0].job.priority == Job::Priority::Urgent,
        "superseded normal job retained authority"
    );
    require(
        dispatched.supersededDiscarded == 1,
        "lazy tombstone was not discarded deterministically"
    );
    require(
        scheduler.complete(dispatched.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "current upgraded job did not complete as current"
    );
}

void testUrgencyAndAgePromotionAreDeterministic()
{
    Scheduler::Policy policy;
    policy.normalToUrgentAfterTicks = 5;
    policy.backgroundToNormalAfterTicks = 10;
    policy.backgroundToUrgentAfterTicks = 20;

    Scheduler scheduler(policy);
    require(scheduler.setCurrentWorldRevision(20), "world setup failed");

    publish(scheduler, 1, 1, 1);
    publish(scheduler, 2, 1, 1);
    publish(scheduler, 3, 1, 1);

    require(
        scheduler.enqueue(
            makeJob(1, 1, 1, 20, 1, Job::Priority::Background),
            0
        ).status == Scheduler::EnqueueStatus::Accepted,
        "background enqueue failed"
    );
    require(
        scheduler.enqueue(
            makeJob(2, 1, 1, 20, 1, Job::Priority::Urgent),
            5
        ).status == Scheduler::EnqueueStatus::Accepted,
        "urgent enqueue failed"
    );
    require(
        scheduler.enqueue(
            makeJob(3, 1, 1, 20, 1, Job::Priority::Normal),
            5
        ).status == Scheduler::EnqueueStatus::Accepted,
        "normal enqueue failed"
    );

    Scheduler::DispatchBudget one;
    one.maxJobs = 1;
    one.maxCostUnits = 1;

    auto first = scheduler.dispatchSlice(5, one);
    require(
        first.count == 1 && first.items[0].job.actorId == 2,
        "urgent work did not preempt normal/background work"
    );
    require(
        scheduler.complete(first.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "urgent completion failed"
    );

    auto aged = scheduler.dispatchSlice(21, one);
    require(
        aged.count == 1 && aged.items[0].job.actorId == 1,
        "old background work did not age into deterministic urgent service"
    );
    require(
        scheduler.complete(aged.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "aged completion failed"
    );

    auto last = scheduler.dispatchSlice(21, one);
    require(
        last.count == 1 && last.items[0].job.actorId == 3,
        "remaining normal work was lost"
    );
    require(
        scheduler.complete(last.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "normal completion failed"
    );
}

void testStaleJobsAreRejectedBeforePlannerWork()
{
    Scheduler scheduler;
    require(scheduler.setCurrentWorldRevision(30), "world setup failed");
    publish(scheduler, 10, 1, 1);

    require(
        scheduler.enqueue(makeJob(10, 1, 1, 30, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "initial stale-test job enqueue failed"
    );

    require(
        scheduler.setCurrentWorldRevision(31),
        "world revision advance failed"
    );

    Scheduler::DispatchBudget budget;
    budget.maxJobs = 8;
    budget.maxCostUnits = 8;

    const auto worldStale = scheduler.dispatchSlice(1, budget);
    require(worldStale.count == 0, "stale-world job reached dispatch");
    require(
        worldStale.staleDiscarded == 1,
        "stale-world job was not discarded before planner work"
    );

    Job actorStale = makeJob(10, 2, 1, 31, 1);
    require(
        scheduler.enqueue(actorStale, 2).status ==
            Scheduler::EnqueueStatus::Accepted,
        "actor stale-test job enqueue failed"
    );

    publish(scheduler, 10, 2, 1, 0);

    const auto objectiveStale = scheduler.dispatchSlice(3, budget);
    require(
        objectiveStale.count == 0 &&
        objectiveStale.staleDiscarded == 1,
        "stale objective job reached planner dispatch"
    );
}

void testInFlightResultIsRejectedAfterRevisionChange()
{
    Scheduler scheduler;
    require(scheduler.setCurrentWorldRevision(40), "world setup failed");
    publish(scheduler, 7, 1, 1);

    require(
        scheduler.enqueue(makeJob(7, 1, 1, 40, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "in-flight initial enqueue failed"
    );

    Scheduler::DispatchBudget one;
    one.maxJobs = 1;
    one.maxCostUnits = 1;

    const auto first = scheduler.dispatchSlice(0, one);
    require(first.count == 1, "initial job did not dispatch");

    publish(scheduler, 7, 2, 1, 0);

    Job replacement = makeJob(7, 2, 2, 40, 1);
    replacement.routeRevision = 0;
    replacement.scope = Job::Scope::FullRoute;
    replacement.trigger = Job::Trigger::ObjectiveChanged;
    replacement.priority = Job::Priority::Urgent;

    require(
        scheduler.enqueue(replacement, 1).status ==
            Scheduler::EnqueueStatus::Accepted,
        "new revision was not queued behind in-flight work"
    );

    require(
        scheduler.complete(first.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedStale,
        "old in-flight result was allowed to commit after objective change"
    );

    const auto second = scheduler.dispatchSlice(1, one);
    require(
        second.count == 1 &&
        second.items[0].job.objectiveRevision == 2,
        "new objective job did not dispatch after stale completion"
    );
    require(
        scheduler.complete(second.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "fresh replacement did not complete as current"
    );
}

void testCompletedRevisionCannotBeReplayed()
{
    Scheduler scheduler;
    require(scheduler.setCurrentWorldRevision(54), "world setup failed");
    publish(scheduler, 88, 1, 1);

    Job job = makeJob(88, 1, 1, 54, 1);
    require(
        scheduler.enqueue(job, 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "replay test initial enqueue failed"
    );

    Scheduler::DispatchBudget one;
    one.maxJobs = 1;
    one.maxCostUnits = 1;

    const auto dispatched = scheduler.dispatchSlice(0, one);
    require(dispatched.count == 1, "replay test did not dispatch");
    require(
        scheduler.complete(dispatched.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "replay test initial completion failed"
    );

    require(
        scheduler.enqueue(job, 1).status ==
            Scheduler::EnqueueStatus::Stale,
        "completed planner revision was replayed without a new scheduling event"
    );

    job.jobRevision = 2;
    require(
        scheduler.enqueue(job, 1).status ==
            Scheduler::EnqueueStatus::Accepted,
        "new planner revision was rejected after completed prior revision"
    );
}

void testCapacityPressureReclaimsStaleWorldJobs()
{
    Scheduler::Policy policy;
    policy.maxPendingJobs = 2;

    Scheduler scheduler(policy);
    require(scheduler.setCurrentWorldRevision(56), "world setup failed");

    publish(scheduler, 1, 1, 1);
    publish(scheduler, 2, 1, 1);
    publish(scheduler, 3, 1, 1);

    require(
        scheduler.enqueue(makeJob(1, 1, 1, 56, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "stale-capacity job 1 failed"
    );
    require(
        scheduler.enqueue(makeJob(2, 1, 1, 56, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "stale-capacity job 2 failed"
    );

    require(
        scheduler.setCurrentWorldRevision(57),
        "stale-capacity world advance failed"
    );

    Job current = makeJob(3, 1, 1, 57, 1);
    require(
        scheduler.enqueue(current, 1).status ==
            Scheduler::EnqueueStatus::Accepted,
        "stale jobs occupied capacity after world revision advanced"
    );

    const auto stats = scheduler.stats();
    require(
        stats.pending == 1 && stats.staleDiscarded >= 2,
        "capacity-pressure compaction did not reclaim stale pending work"
    );
}

void testReplacementStormKeepsPhysicalQueueBounded()
{
    Scheduler scheduler;
    require(scheduler.setCurrentWorldRevision(55), "world setup failed");
    publish(scheduler, 99, 1, 1);

    Job job = makeJob(
        99,
        1,
        1,
        55,
        1,
        Job::Priority::Background
    );

    require(
        scheduler.enqueue(job, 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "replacement-storm initial enqueue failed"
    );

    for (std::uint64_t revision = 2; revision <= 10000; ++revision)
    {
        job.jobRevision = revision;
        job.trigger =
            (revision % 2 == 0)
                ? Job::Trigger::ManualRefresh
                : Job::Trigger::ProgramCompleted;

        require(
            scheduler.enqueue(job, revision).status ==
                Scheduler::EnqueueStatus::Replaced,
            "replacement storm failed to supersede pending actor job"
        );
    }

    const auto stats = scheduler.stats();
    require(stats.pending == 1, "replacement storm created multiple active jobs");
    require(
        stats.queuedRecords <= 2048,
        "lazy scheduler tombstones grew without bounded compaction"
    );
    require(
        stats.queueCompactions > 0,
        "replacement storm never triggered amortized tombstone compaction"
    );

    Scheduler::DispatchBudget one;
    one.maxJobs = 1;
    one.maxCostUnits = 1;

    const auto dispatched = scheduler.dispatchSlice(10001, one);
    require(
        dispatched.count == 1 &&
        dispatched.items[0].job.jobRevision == 10000,
        "replacement storm dispatched anything except newest actor job"
    );
    require(
        scheduler.complete(dispatched.items[0].ticket) ==
            Scheduler::CompletionStatus::CompletedCurrent,
        "newest replacement-storm job did not complete current"
    );
}

void testCapacityIsBounded()
{
    Scheduler::Policy policy;
    policy.maxPendingJobs = 2;

    Scheduler scheduler(policy);
    require(scheduler.setCurrentWorldRevision(50), "world setup failed");

    for (std::uint64_t actor = 1; actor <= 3; ++actor)
        publish(scheduler, actor, 1, 1);

    require(
        scheduler.enqueue(makeJob(1, 1, 1, 50, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "capacity job 1 failed"
    );
    require(
        scheduler.enqueue(makeJob(2, 1, 1, 50, 1), 0).status ==
            Scheduler::EnqueueStatus::Accepted,
        "capacity job 2 failed"
    );
    require(
        scheduler.enqueue(makeJob(3, 1, 1, 50, 1), 0).status ==
            Scheduler::EnqueueStatus::CapacityExceeded,
        "scheduler exceeded configured pending capacity"
    );
}

void testFiveThousandActorQueueAndMeasure()
{
    constexpr std::uint64_t kActors = 5000;

    Scheduler scheduler;
    require(scheduler.setCurrentWorldRevision(60), "world setup failed");

    const auto totalStart = std::chrono::steady_clock::now();
    const auto enqueueStart = totalStart;

    for (std::uint64_t actor = 1; actor <= kActors; ++actor)
    {
        publish(scheduler, actor, 1, 1);
        const auto result = scheduler.enqueue(
            makeJob(
                actor,
                1,
                1,
                60,
                1,
                Job::Priority::Background
            ),
            0
        );
        require(
            result.status == Scheduler::EnqueueStatus::Accepted,
            "scale enqueue failed"
        );
    }

    const auto enqueueEnd = std::chrono::steady_clock::now();

    Scheduler::DispatchBudget batch;
    batch.maxJobs = Scheduler::kMaxJobsPerSlice;
    batch.maxCostUnits =
        static_cast<std::uint32_t>(Scheduler::kMaxJobsPerSlice);

    std::uint64_t expectedActor = 1;
    std::uint64_t completed = 0;
    std::uint64_t tick = 1;

    while (completed < kActors)
    {
        const auto slice = scheduler.dispatchSlice(tick++, batch);
        require(slice.count > 0, "scale queue stalled before completion");
        require(
            slice.count <= Scheduler::kMaxJobsPerSlice,
            "dispatch exceeded fixed slice capacity"
        );
        require(
            slice.costUnits <= batch.maxCostUnits,
            "dispatch exceeded deterministic cost budget"
        );

        for (std::size_t i = 0; i < slice.count; ++i)
        {
            require(
                slice.items[i].job.actorId == expectedActor,
                "scale dispatch order is not deterministic FIFO within priority"
            );
            ++expectedActor;
            ++completed;

            require(
                scheduler.complete(slice.items[i].ticket) ==
                    Scheduler::CompletionStatus::CompletedCurrent,
                "scale completion became stale unexpectedly"
            );
        }
    }

    const auto totalEnd = std::chrono::steady_clock::now();

    const auto finalStats = scheduler.stats();
    require(finalStats.pending == 0, "scale queue retained pending work");
    require(finalStats.inFlight == 0, "scale queue retained in-flight work");
    require(finalStats.queuedRecords == 0, "scale queue retained physical records");
    require(
        finalStats.dispatched == kActors &&
        finalStats.completedCurrent == kActors,
        "scale scheduler lost or duplicated jobs"
    );

    const auto enqueueUs =
        std::chrono::duration_cast<std::chrono::microseconds>(
            enqueueEnd - enqueueStart
        ).count();
    const auto dispatchUs =
        std::chrono::duration_cast<std::chrono::microseconds>(
            totalEnd - enqueueEnd
        ).count();
    const auto totalUs =
        std::chrono::duration_cast<std::chrono::microseconds>(
            totalEnd - totalStart
        ).count();

    std::cout
        << "[TIMING] navigation_work_scheduler actors=" << kActors
        << " enqueue_us=" << enqueueUs
        << " dispatch_complete_us=" << dispatchUs
        << " total_us=" << totalUs
        << "\n";
}

} // namespace

int main()
{
    try
    {
        testDuplicateSuppressionAndPriorityUpgrade();
        testUrgencyAndAgePromotionAreDeterministic();
        testStaleJobsAreRejectedBeforePlannerWork();
        testInFlightResultIsRejectedAfterRevisionChange();
        testCompletedRevisionCannotBeReplayed();
        testCapacityPressureReclaimsStaleWorldJobs();
        testReplacementStormKeepsPhysicalQueueBounded();
        testCapacityIsBounded();
        testFiveThousandActorQueueAndMeasure();

        std::cout << "NAVIGATION WORK SCHEDULER TESTS: PASS\n";
        std::cout << " - duplicate/superseded work is bounded by actor slot\n";
        std::cout << " - urgent work preempts and aged work cannot starve\n";
        std::cout << " - stale revisions are rejected before dispatch and before commit\n";
        std::cout << " - completed revisions cannot replay and stale capacity is reclaimed\n";
        std::cout << " - dispatch respects fixed job/cost slice budgets\n";
        std::cout << " - replacement storms keep physical queue storage bounded\n";
        std::cout << " - 5000-actor queue drains deterministically without loss\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION WORK SCHEDULER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
