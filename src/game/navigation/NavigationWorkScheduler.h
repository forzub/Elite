#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace game::navigation
{

// B14 value object. It describes planner work; it does not contain a callback
// and cannot invoke a planner by itself.
struct NavigationPlannerJob
{
    enum class Priority : std::uint8_t
    {
        Urgent = 0,
        Normal,
        Background
    };

    enum class Scope : std::uint8_t
    {
        LocalHorizon = 0,
        FullRoute
    };

    enum class Trigger : std::uint8_t
    {
        MissingProgram = 0,
        ProgramExpired,
        ProgramCompleted,
        TrackingErrorExceeded,
        StaticSafetyInvalidated,
        DynamicHazardInvalidated,
        CapabilityChanged,
        ObjectiveChanged,
        TopologyInvalidated,
        ManualRefresh,
        ReflexDeviation,
        AdvisoryPrewarm
    };

    std::uint64_t actorId = 0;
    std::uint64_t jobRevision = 0;
    std::uint64_t objectiveRevision = 0;
    std::uint64_t worldRevision = 0;
    std::uint64_t capabilityRevision = 0;

    // Zero means that this job does not depend on an already accepted route
    // revision (for example a FullRoute rebuild).
    std::uint64_t routeRevision = 0;

    Priority priority = Priority::Normal;
    Scope scope = Scope::LocalHorizon;
    Trigger trigger = Trigger::MissingProgram;

    bool guidanceOnly = false;

    // Deterministic scheduling budget unit. It is deliberately not wall-clock
    // time; runtime orchestration may additionally enforce an external time
    // budget without putting a clock inside this scheduling block.
    std::uint32_t estimatedCostUnits = 1;
};

// B14 — planner work scheduler.
//
// Responsibilities:
// - keep at most one pending job per actor;
// - suppress duplicates and supersede older actor jobs lazily;
// - reject stale revision work before expensive planning;
// - dispatch bounded deterministic slices;
// - prevent starvation by deterministic age promotion;
// - keep dispatched work in-flight until completion so a later worker pool can
//   reject stale results before commit.
//
// It owns no world geometry, no planner and no wall clock.
class NavigationWorkScheduler final
{
public:
    static constexpr std::size_t kMaxJobsPerSlice = 128;

    struct Policy
    {
        std::size_t maxPendingJobs = 16384;

        std::uint64_t normalToUrgentAfterTicks = 8;
        std::uint64_t backgroundToNormalAfterTicks = 30;
        std::uint64_t backgroundToUrgentAfterTicks = 120;
    };

    struct ActorRevisionStamp
    {
        std::uint64_t actorId = 0;
        std::uint64_t objectiveRevision = 0;
        std::uint64_t capabilityRevision = 0;
        std::uint64_t routeRevision = 0;
    };

    enum class EnqueueStatus : std::uint8_t
    {
        InvalidInput = 0,
        Accepted,
        Replaced,
        Duplicate,
        Stale,
        CapacityExceeded
    };

    struct EnqueueResult
    {
        EnqueueStatus status = EnqueueStatus::InvalidInput;
        std::uint64_t ticket = 0;
    };

    struct DispatchBudget
    {
        std::size_t maxJobs = 32;
        std::uint32_t maxCostUnits = 32;
    };

    struct DispatchItem
    {
        NavigationPlannerJob job {};
        std::uint64_t ticket = 0;
    };

    struct DispatchResult
    {
        std::array<DispatchItem, kMaxJobsPerSlice> items {};
        std::size_t count = 0;
        std::uint32_t costUnits = 0;

        std::size_t staleDiscarded = 0;
        std::size_t supersededDiscarded = 0;
        std::size_t pendingAfterDispatch = 0;
        std::size_t inFlightAfterDispatch = 0;

        bool invalidInput = false;
    };

    enum class CompletionStatus : std::uint8_t
    {
        UnknownTicket = 0,
        CompletedCurrent,
        CompletedStale
    };

    struct Stats
    {
        std::uint64_t accepted = 0;
        std::uint64_t replaced = 0;
        std::uint64_t duplicates = 0;
        std::uint64_t staleRejected = 0;
        std::uint64_t capacityRejected = 0;
        std::uint64_t staleDiscarded = 0;
        std::uint64_t supersededDiscarded = 0;
        std::uint64_t dispatched = 0;
        std::uint64_t completedCurrent = 0;
        std::uint64_t completedStale = 0;

        std::size_t pending = 0;
        std::size_t inFlight = 0;
    };

    NavigationWorkScheduler() noexcept;
    explicit NavigationWorkScheduler(const Policy& policy) noexcept;

    [[nodiscard]] bool setCurrentWorldRevision(
        std::uint64_t worldRevision
    ) noexcept;

    [[nodiscard]] bool publishActorRevision(
        const ActorRevisionStamp& stamp
    ) noexcept;

    [[nodiscard]] EnqueueResult enqueue(
        const NavigationPlannerJob& job,
        std::uint64_t schedulingTick
    );

    [[nodiscard]] DispatchResult dispatchSlice(
        std::uint64_t schedulingTick,
        const DispatchBudget& budget
    );

    [[nodiscard]] CompletionStatus complete(
        std::uint64_t ticket
    ) noexcept;

    [[nodiscard]] Stats stats() const noexcept;

private:
    struct PendingRecord
    {
        NavigationPlannerJob job {};
        std::uint64_t ticket = 0;
        std::uint64_t enqueuedTick = 0;
    };

    struct ActorState
    {
        bool hasRevisionStamp = false;
        ActorRevisionStamp revision {};

        std::uint64_t latestJobRevision = 0;
        std::uint64_t pendingTicket = 0;
        NavigationPlannerJob pendingJob {};

        std::uint64_t inFlightTicket = 0;
        NavigationPlannerJob inFlightJob {};
    };

    struct Candidate
    {
        bool valid = false;
        std::size_t queueIndex = 0;
        std::uint8_t effectivePriority = 0;
        std::uint64_t ticket = 0;
    };

    [[nodiscard]] bool validPolicy() const noexcept;
    [[nodiscard]] bool validJob(
        const NavigationPlannerJob& job
    ) const noexcept;
    [[nodiscard]] bool fresh(
        const NavigationPlannerJob& job,
        const ActorState* actor
    ) const noexcept;
    [[nodiscard]] bool sameIdentity(
        const NavigationPlannerJob& a,
        const NavigationPlannerJob& b
    ) const noexcept;
    [[nodiscard]] std::uint8_t effectivePriority(
        const PendingRecord& record,
        std::uint64_t schedulingTick
    ) const noexcept;

    void pruneQueueFront(
        std::size_t queueIndex,
        DispatchResult& result
    );

    [[nodiscard]] Candidate selectCandidate(
        std::uint64_t schedulingTick,
        std::uint32_t remainingCostUnits,
        DispatchResult& result
    );

    Policy policy_ {};
    std::uint64_t currentWorldRevision_ = 0;
    std::uint64_t nextTicket_ = 1;

    std::array<std::deque<PendingRecord>, 3> queues_ {};
    std::unordered_map<std::uint64_t, ActorState> actors_ {};
    std::unordered_map<std::uint64_t, std::uint64_t> inFlightTicketToActor_ {};

    std::size_t pendingCount_ = 0;
    std::size_t inFlightCount_ = 0;

    Stats totals_ {};
};

} // namespace game::navigation
