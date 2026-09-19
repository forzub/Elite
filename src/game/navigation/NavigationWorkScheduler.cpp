#include "NavigationWorkScheduler.h"

#include <algorithm>
#include <limits>

namespace game::navigation
{
namespace
{

std::size_t queueIndex(
    NavigationPlannerJob::Priority priority
) noexcept
{
    return static_cast<std::size_t>(priority);
}

bool validPriority(NavigationPlannerJob::Priority priority) noexcept
{
    return queueIndex(priority) <=
        queueIndex(NavigationPlannerJob::Priority::Background);
}

bool validScope(NavigationPlannerJob::Scope scope) noexcept
{
    return
        scope == NavigationPlannerJob::Scope::LocalHorizon ||
        scope == NavigationPlannerJob::Scope::FullRoute;
}

} // namespace

NavigationWorkScheduler::NavigationWorkScheduler() noexcept
    : NavigationWorkScheduler(Policy {})
{
}

NavigationWorkScheduler::NavigationWorkScheduler(
    const Policy& policy
) noexcept
    : policy_(policy)
{
    actors_.reserve(2048);
    inFlightTicketToActor_.reserve(128);
}

bool NavigationWorkScheduler::validPolicy() const noexcept
{
    return
        policy_.maxPendingJobs > 0 &&
        policy_.backgroundToUrgentAfterTicks >=
            policy_.backgroundToNormalAfterTicks;
}

bool NavigationWorkScheduler::setCurrentWorldRevision(
    std::uint64_t worldRevision
) noexcept
{
    if (worldRevision == 0 ||
        (currentWorldRevision_ != 0 &&
         worldRevision < currentWorldRevision_))
    {
        return false;
    }

    currentWorldRevision_ = worldRevision;
    return true;
}

bool NavigationWorkScheduler::publishActorRevision(
    const ActorRevisionStamp& stamp
) noexcept
{
    if (stamp.actorId == 0 ||
        stamp.objectiveRevision == 0 ||
        stamp.capabilityRevision == 0)
    {
        return false;
    }

    ActorState& actor = actors_[stamp.actorId];
    if (actor.hasRevisionStamp)
    {
        if (stamp.objectiveRevision < actor.revision.objectiveRevision ||
            stamp.capabilityRevision < actor.revision.capabilityRevision)
        {
            return false;
        }

        // Route revision belongs to one objective and may restart when the
        // objective revision advances.
        if (stamp.objectiveRevision == actor.revision.objectiveRevision &&
            stamp.routeRevision < actor.revision.routeRevision)
        {
            return false;
        }
    }

    actor.hasRevisionStamp = true;
    actor.revision = stamp;
    return true;
}

bool NavigationWorkScheduler::validJob(
    const NavigationPlannerJob& job
) const noexcept
{
    return
        validPolicy() &&
        currentWorldRevision_ != 0 &&
        job.actorId != 0 &&
        job.jobRevision != 0 &&
        job.objectiveRevision != 0 &&
        job.worldRevision != 0 &&
        job.capabilityRevision != 0 &&
        validPriority(job.priority) &&
        validScope(job.scope) &&
        job.estimatedCostUnits > 0;
}

bool NavigationWorkScheduler::fresh(
    const NavigationPlannerJob& job,
    const ActorState* actor
) const noexcept
{
    if (currentWorldRevision_ == 0 ||
        job.worldRevision != currentWorldRevision_)
    {
        return false;
    }

    if (actor == nullptr || !actor->hasRevisionStamp)
        return true;

    if (job.objectiveRevision != actor->revision.objectiveRevision ||
        job.capabilityRevision != actor->revision.capabilityRevision)
    {
        return false;
    }

    return
        job.routeRevision == 0 ||
        job.routeRevision == actor->revision.routeRevision;
}

bool NavigationWorkScheduler::sameIdentity(
    const NavigationPlannerJob& a,
    const NavigationPlannerJob& b
) const noexcept
{
    return
        a.actorId == b.actorId &&
        a.jobRevision == b.jobRevision &&
        a.objectiveRevision == b.objectiveRevision &&
        a.worldRevision == b.worldRevision &&
        a.capabilityRevision == b.capabilityRevision &&
        a.routeRevision == b.routeRevision &&
        a.scope == b.scope &&
        a.guidanceOnly == b.guidanceOnly;
}

NavigationWorkScheduler::EnqueueResult NavigationWorkScheduler::enqueue(
    const NavigationPlannerJob& job,
    std::uint64_t schedulingTick
)
{
    EnqueueResult result;

    if (!validJob(job))
        return result;

    ActorState& actor = actors_[job.actorId];

    if (!fresh(job, &actor))
    {
        result.status = EnqueueStatus::Stale;
        ++totals_.staleRejected;
        return result;
    }

    if (actor.inFlightTicket != 0 &&
        sameIdentity(job, actor.inFlightJob))
    {
        result.status = EnqueueStatus::Duplicate;
        result.ticket = actor.inFlightTicket;
        ++totals_.duplicates;
        return result;
    }

    if (job.jobRevision < actor.latestJobRevision)
    {
        result.status = EnqueueStatus::Stale;
        ++totals_.staleRejected;
        return result;
    }

    if (actor.pendingTicket != 0)
    {
        if (sameIdentity(job, actor.pendingJob) &&
            queueIndex(job.priority) >=
                queueIndex(actor.pendingJob.priority))
        {
            result.status = EnqueueStatus::Duplicate;
            result.ticket = actor.pendingTicket;
            ++totals_.duplicates;
            return result;
        }

        // Replacement is O(1)-like at the actor slot. The old queue record is
        // left as a lazy tombstone and never searched/relinked here.
        result.status = EnqueueStatus::Replaced;
        ++totals_.replaced;
    }
    else
    {
        if (pendingCount_ >= policy_.maxPendingJobs)
        {
            result.status = EnqueueStatus::CapacityExceeded;
            ++totals_.capacityRejected;
            return result;
        }

        result.status = EnqueueStatus::Accepted;
        ++totals_.accepted;
        ++pendingCount_;
    }

    if (nextTicket_ == 0)
        nextTicket_ = 1;

    PendingRecord record;
    record.job = job;
    record.ticket = nextTicket_++;
    record.enqueuedTick = schedulingTick;

    queues_[queueIndex(job.priority)].push_back(record);

    actor.latestJobRevision =
        std::max(actor.latestJobRevision, job.jobRevision);
    actor.pendingTicket = record.ticket;
    actor.pendingJob = job;

    result.ticket = record.ticket;
    return result;
}

std::uint8_t NavigationWorkScheduler::effectivePriority(
    const PendingRecord& record,
    std::uint64_t schedulingTick
) const noexcept
{
    const std::uint64_t age =
        schedulingTick >= record.enqueuedTick
            ? schedulingTick - record.enqueuedTick
            : 0;

    switch (record.job.priority)
    {
    case NavigationPlannerJob::Priority::Urgent:
        return 0;

    case NavigationPlannerJob::Priority::Normal:
        return
            age >= policy_.normalToUrgentAfterTicks
                ? 0
                : 1;

    case NavigationPlannerJob::Priority::Background:
        if (age >= policy_.backgroundToUrgentAfterTicks)
            return 0;
        if (age >= policy_.backgroundToNormalAfterTicks)
            return 1;
        return 2;
    }

    return 2;
}

void NavigationWorkScheduler::pruneQueueFront(
    std::size_t index,
    DispatchResult& result
)
{
    auto& queue = queues_[index];

    while (!queue.empty())
    {
        const PendingRecord& record = queue.front();
        auto actorIt = actors_.find(record.job.actorId);

        if (actorIt == actors_.end() ||
            actorIt->second.pendingTicket != record.ticket)
        {
            queue.pop_front();
            ++result.supersededDiscarded;
            ++totals_.supersededDiscarded;
            continue;
        }

        if (!fresh(record.job, &actorIt->second))
        {
            actorIt->second.pendingTicket = 0;
            actorIt->second.pendingJob = NavigationPlannerJob {};
            if (pendingCount_ > 0)
                --pendingCount_;

            queue.pop_front();
            ++result.staleDiscarded;
            ++totals_.staleDiscarded;
            continue;
        }

        break;
    }
}

NavigationWorkScheduler::Candidate
NavigationWorkScheduler::selectCandidate(
    std::uint64_t schedulingTick,
    std::uint32_t remainingCostUnits,
    DispatchResult& result
)
{
    Candidate best;

    for (std::size_t i = 0; i < queues_.size(); ++i)
    {
        pruneQueueFront(i, result);
        auto& queue = queues_[i];
        if (queue.empty())
            continue;

        const PendingRecord& record = queue.front();
        auto actorIt = actors_.find(record.job.actorId);
        if (actorIt == actors_.end())
            continue;

        // Never run two planner jobs for one actor concurrently. A newer job
        // may wait behind an older in-flight result; complete() will classify
        // that older result stale if revisions changed.
        if (actorIt->second.inFlightTicket != 0)
            continue;

        if (record.job.estimatedCostUnits > remainingCostUnits)
            continue;

        const std::uint8_t effective =
            effectivePriority(record, schedulingTick);

        if (!best.valid ||
            effective < best.effectivePriority ||
            (effective == best.effectivePriority &&
             record.ticket < best.ticket))
        {
            best.valid = true;
            best.queueIndex = i;
            best.effectivePriority = effective;
            best.ticket = record.ticket;
        }
    }

    return best;
}

NavigationWorkScheduler::DispatchResult
NavigationWorkScheduler::dispatchSlice(
    std::uint64_t schedulingTick,
    const DispatchBudget& budget
)
{
    DispatchResult result;

    if (!validPolicy() ||
        budget.maxJobs == 0 ||
        budget.maxJobs > kMaxJobsPerSlice ||
        budget.maxCostUnits == 0)
    {
        result.invalidInput = true;
        result.pendingAfterDispatch = pendingCount_;
        result.inFlightAfterDispatch = inFlightCount_;
        return result;
    }

    while (result.count < budget.maxJobs)
    {
        const std::uint32_t remainingCost =
            budget.maxCostUnits - result.costUnits;

        const Candidate candidate =
            selectCandidate(
                schedulingTick,
                remainingCost,
                result
            );

        if (!candidate.valid)
            break;

        auto& queue = queues_[candidate.queueIndex];
        PendingRecord record = queue.front();
        queue.pop_front();

        auto actorIt = actors_.find(record.job.actorId);
        if (actorIt == actors_.end() ||
            actorIt->second.pendingTicket != record.ticket)
        {
            ++result.supersededDiscarded;
            ++totals_.supersededDiscarded;
            continue;
        }

        ActorState& actor = actorIt->second;
        actor.pendingTicket = 0;
        actor.pendingJob = NavigationPlannerJob {};
        actor.inFlightTicket = record.ticket;
        actor.inFlightJob = record.job;

        inFlightTicketToActor_[record.ticket] =
            record.job.actorId;

        if (pendingCount_ > 0)
            --pendingCount_;
        ++inFlightCount_;

        DispatchItem& item = result.items[result.count++];
        item.job = record.job;
        item.ticket = record.ticket;

        result.costUnits += record.job.estimatedCostUnits;
        ++totals_.dispatched;

        if (result.costUnits >= budget.maxCostUnits)
            break;
    }

    result.pendingAfterDispatch = pendingCount_;
    result.inFlightAfterDispatch = inFlightCount_;
    return result;
}

NavigationWorkScheduler::CompletionStatus
NavigationWorkScheduler::complete(
    std::uint64_t ticket
) noexcept
{
    const auto ticketIt =
        inFlightTicketToActor_.find(ticket);
    if (ticketIt == inFlightTicketToActor_.end())
        return CompletionStatus::UnknownTicket;

    const std::uint64_t actorId = ticketIt->second;
    auto actorIt = actors_.find(actorId);
    if (actorIt == actors_.end() ||
        actorIt->second.inFlightTicket != ticket)
    {
        inFlightTicketToActor_.erase(ticketIt);
        return CompletionStatus::UnknownTicket;
    }

    ActorState& actor = actorIt->second;
    const NavigationPlannerJob completed = actor.inFlightJob;

    const bool current =
        fresh(completed, &actor) &&
        completed.jobRevision == actor.latestJobRevision;

    actor.inFlightTicket = 0;
    actor.inFlightJob = NavigationPlannerJob {};
    inFlightTicketToActor_.erase(ticketIt);

    if (inFlightCount_ > 0)
        --inFlightCount_;

    if (current)
    {
        ++totals_.completedCurrent;
        return CompletionStatus::CompletedCurrent;
    }

    ++totals_.completedStale;
    return CompletionStatus::CompletedStale;
}

NavigationWorkScheduler::Stats
NavigationWorkScheduler::stats() const noexcept
{
    Stats out = totals_;
    out.pending = pendingCount_;
    out.inFlight = inFlightCount_;
    return out;
}

} // namespace game::navigation
