#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace game::navigation::debug
{

class NavigationStressHarness final
{
public:
    enum class Target
    {
        None = 0,
        CylinderRear,
        CubeFront
    };

    struct Observation
    {
        bool worldReady = false;
        double universeSeconds = 0.0;
        double speedMps = 0.0;
        double angularSpeedRadPerSec = 0.0;

        int dockingPhase = 0;
        bool requestPending = false;
        std::uint64_t requestSerial = 0;
        std::string requestTarget;
        bool autopilotActive = false;

        std::size_t pageIndex = 0;
        std::size_t segmentIndex = 0;
        std::size_t checkpointIndex = 0;
        std::size_t routeCurveIndex = 0;
        std::uint64_t speedProfileRevision = 0;
        bool failed = false;
        std::string failureReason;
        int workerCount = 0;
        std::uint64_t jobSerial = 0;
        bool jobReady = false;
        bool jobCancelled = false;
        bool jobPlanValid = false;
        int jobStage = -1;
        int jobPermille = -1;
        std::string jobResult;

        bool safetyCourseActive = false;
        bool safetyStopFallback = false;

        bool routeValid = false;
        double routeProgressMeters = 0.0;
        std::size_t routeCurveCount = 0;

        // Semantic BLUE proving state from RoutePlan::stages.
        bool blueStagePresent = false;
        bool blueHardContainment = false;
        int bluePhase = 0; // 0 absent, 1 approach, 2 transit, 3 exited
        double blueStartProgressMeters = 0.0;
        double blueEndProgressMeters = 0.0;
        std::string blueVolumeId;
        std::size_t executionGateCount = 0;
        int firstCurveKind = -1;
        double firstCurveLengthMeters = 0.0;
        double routeLengthMeters = 0.0;
        std::string routeDiagnostic;
    };

    struct Action
    {
        bool request = false;
        Target target = Target::None;
        bool clear = false;
    };

    NavigationStressHarness()
    {
        const char* enabled = std::getenv("ELITE_NAV_STRESS");
        m_enabled =
            enabled != nullptr &&
            std::string(enabled) != "0" &&
            std::string(enabled) != "";

        const char* report = std::getenv("ELITE_NAV_STRESS_REPORT");
        if (report && *report)
            m_reportPath = report;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool complete() const noexcept { return m_complete; }
    int currentStep() const noexcept { return m_step; }
    static constexpr int totalSteps() noexcept { return 12; }
    const std::string& reportPath() const noexcept { return m_reportPath; }

    Action update(const Observation& o)
    {
        Action action;
        if (!m_enabled || m_complete)
            return action;

        ensureOpen();
        if (!m_started)
        {
            if (!o.worldReady)
                return action;

            m_started = true;
            m_startedAt = o.universeSeconds;
            m_stepStartedAt = o.universeSeconds;
            logEvent(o, "START scenario=hot-replan-blue-transit-v2");
        }

        sample(o);
        detectTransitions(o);

        // A step is deliberately condition-driven where possible. Timeouts
        // prevent one failure from hiding all later transition cases.
        const double stepAge = o.universeSeconds - m_stepStartedAt;

        switch (m_step)
        {
            case 0:
                if (o.universeSeconds - m_startedAt >= 1.0)
                    return issue(o, Target::CylinderRear, "cold-cylinder");
                break;

            case 1:
                if (o.failed || o.speedMps >= 100.0 || stepAge >= 12.0)
                {
                    return issue(
                        o,
                        Target::CubeFront,
                        o.speedMps >= 100.0
                            ? "switch-low-speed"
                            : o.failed
                                ? "switch-low-speed-after-failure"
                                : "switch-low-speed-timeout"
                    );
                }
                break;

            case 2:
                if (o.failed || o.speedMps >= 280.0 || stepAge >= 14.0)
                {
                    return issue(
                        o,
                        Target::CylinderRear,
                        o.speedMps >= 280.0
                            ? "switch-mid-speed"
                            : o.failed
                                ? "switch-mid-speed-after-failure"
                                : "switch-mid-speed-timeout"
                    );
                }
                break;

            case 3:
                if (o.failed || o.speedMps >= 390.0 || stepAge >= 14.0)
                {
                    return issue(
                        o,
                        Target::CubeFront,
                        o.speedMps >= 390.0
                            ? "switch-high-speed"
                            : o.failed
                                ? "switch-high-speed-after-failure"
                                : "switch-high-speed-timeout"
                    );
                }
                break;

            case 4:
                // Early interruption: hit the planner/follower handoff while
                // the newly accepted route has barely started.
                if (o.failed ||
                    (o.dockingPhase == 4 && stepAge >= 1.0) ||
                    stepAge >= 12.0)
                {
                    return issue(
                        o,
                        Target::CylinderRear,
                        o.dockingPhase == 4
                            ? "switch-early-execution"
                            : o.failed
                                ? "switch-early-after-failure"
                                : "switch-early-timeout"
                    );
                }
                break;

            case 5:
                // Give this route enough time to reach authored curvature.
                if (o.failed ||
                    (o.dockingPhase == 4 && stepAge >= 4.0) ||
                    stepAge >= 14.0)
                {
                    return issue(
                        o,
                        Target::CubeFront,
                        o.dockingPhase == 4
                            ? "switch-during-flight"
                            : o.failed
                                ? "switch-during-flight-after-failure"
                                : "switch-during-flight-timeout"
                    );
                }
                break;

            default:
                // Five rapid alternating transitions exercise cancellation,
                // stale workers, request serial ownership and state reset.
                if (m_step >= 6 && m_step < 11)
                {
                    if (o.failed || stepAge >= 1.5)
                    {
                        const Target next =
                            (m_step % 2 == 0)
                                ? Target::CylinderRear
                                : Target::CubeFront;
                        return issue(o, next, "rapid-switch");
                    }
                }
                else if (m_step == 11)
                {
                    // Final request is not another cancellation probe. It is a
                    // dedicated end-to-end BLUE traversal proof and therefore
                    // must be allowed to fly through the authored volume.
                    if (o.failed || stepAge >= 1.5)
                    {
                        return issue(
                            o,
                            Target::CubeFront,
                            "blue-proving-run"
                        );
                    }
                }
                else if (m_step == 12)
                {
                    if (o.failed)
                    {
                        logSummary(
                            o,
                            "FAILED",
                            "blue-proving-terminal-failure"
                        );
                        m_complete = true;
                        flush();
                        break;
                    }

                    if (o.dockingPhase == 4 &&
                        !o.blueStagePresent &&
                        stepAge >= 1.0)
                    {
                        logSummary(
                            o,
                            "FAILED",
                            "accepted cube route has no semantic BLUE stage"
                        );
                        m_complete = true;
                        flush();
                        break;
                    }

                    if (o.blueStagePresent &&
                        !o.blueHardContainment &&
                        o.dockingPhase == 4)
                    {
                        logSummary(
                            o,
                            "FAILED",
                            "semantic BLUE stage lost hard containment"
                        );
                        m_complete = true;
                        flush();
                        break;
                    }

                    if (m_blueEntered && m_blueExited)
                    {
                        if (m_safetyCourseCorrections > 0 &&
                            m_planningSafetyOverlapSamples == 0)
                        {
                            logSummary(
                                o,
                                "FAILED",
                                "safe-course correction never overlapped async planning"
                            );
                            m_complete = true;
                            flush();
                            break;
                        }

                        logSummary(o, "COMPLETE", std::string{});
                        m_complete = true;
                        flush();
                        break;
                    }

                    // Approach and transit are different contracts.
                    // The old single 180 s timer could fire one second after
                    // BLUE_ENTER when a bloated route consumed the whole
                    // budget on approach.
                    if (!m_blueEntered &&
                        stepAge >= 180.0)
                    {
                        logSummary(
                            o,
                            "FAILED",
                            "BLUE volume was not reached within 180 seconds"
                        );
                        m_complete = true;
                        flush();
                    }
                    else if (m_blueEntered &&
                             !m_blueExited &&
                             m_blueEnteredAt > -1.0e20 &&
                             o.universeSeconds -
                                 m_blueEnteredAt >= 30.0)
                    {
                        logSummary(
                            o,
                            "FAILED",
                            "BLUE transit was not completed within 30 seconds after entry"
                        );
                        m_complete = true;
                        flush();
                    }
                }
                break;
        }

        return action;
    }

private:
    static std::string escaped(std::string value)
    {
        for (char& c : value)
        {
            if (c == '\n' || c == '\r' || c == '\t')
                c = ' ';
        }
        return value;
    }

    static const char* targetName(Target target)
    {
        switch (target)
        {
            case Target::CylinderRear: return "cylinder-rear";
            case Target::CubeFront: return "cube-front";
            default: return "none";
        }
    }

    void ensureOpen()
    {
        if (m_stream.is_open())
            return;

        try
        {
            const std::filesystem::path path(m_reportPath);
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
        }
        catch (...) {}

        m_stream.open(
            m_reportPath,
            std::ios::out | std::ios::trunc
        );

        if (m_stream.is_open())
        {
            m_stream
                << "# Elite navigation runtime stress report\n"
                << "# scenario=hot-replan-blue-transit-v2\n"
                << "# columns are key=value; SAMPLE cadence ~= 0.25 s\n";
            flush();
        }
    }

    void flush()
    {
        if (m_stream.is_open())
            m_stream.flush();
    }

    void writeCommon(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << std::fixed << std::setprecision(6)
            << " t=" << o.universeSeconds
            << " step=" << m_step
            << " v=" << o.speedMps
            << " omega=" << o.angularSpeedRadPerSec
            << " phase=" << o.dockingPhase
            << " pending=" << (o.requestPending ? 1 : 0)
            << " serial=" << o.requestSerial
            << " target=" << (o.requestTarget.empty() ? "none" : o.requestTarget)
            << " autopilot=" << (o.autopilotActive ? 1 : 0)
            << " page=" << o.pageIndex
            << " segment=" << o.segmentIndex
            << " checkpoint=" << o.checkpointIndex
            << " route_curve=" << o.routeCurveIndex
            << " profile_rev=" << o.speedProfileRevision
            << " failed=" << (o.failed ? 1 : 0)
            << " workers=" << o.workerCount
            << " job_serial=" << o.jobSerial
            << " job_ready=" << (o.jobReady ? 1 : 0)
            << " job_cancelled=" << (o.jobCancelled ? 1 : 0)
            << " job_plan_valid=" << (o.jobPlanValid ? 1 : 0)
            << " safety_active=" << (o.safetyCourseActive ? 1 : 0)
            << " safety_stop=" << (o.safetyStopFallback ? 1 : 0)
            << " job_stage=" << o.jobStage
            << " job_permille=" << o.jobPermille
            << " route_valid=" << (o.routeValid ? 1 : 0)
            << " route_progress_m=" << o.routeProgressMeters
            << " blue_stage=" << (o.blueStagePresent ? 1 : 0)
            << " blue_hard=" << (o.blueHardContainment ? 1 : 0)
            << " blue_phase=" << o.bluePhase
            << " blue_start_m=" << o.blueStartProgressMeters
            << " blue_end_m=" << o.blueEndProgressMeters
            << " blue_volume="
            << (o.blueVolumeId.empty() ? "none" : o.blueVolumeId)
            << " curves=" << o.routeCurveCount
            << " exec_gates=" << o.executionGateCount
            << " first_kind=" << o.firstCurveKind
            << " first_len=" << o.firstCurveLengthMeters
            << " route_len=" << o.routeLengthMeters;
    }

    void sample(const Observation& o)
    {
        if (o.universeSeconds - m_lastSampleAt < 0.25)
            return;
        m_lastSampleAt = o.universeSeconds;

        if (!m_stream.is_open())
            return;

        m_stream << "SAMPLE";
        writeCommon(o);
        if (!o.failureReason.empty())
            m_stream << " failure=\"" << escaped(o.failureReason) << "\"";
        if (!o.routeDiagnostic.empty())
            m_stream << " diag=\"" << escaped(o.routeDiagnostic) << "\"";
        if (!o.jobResult.empty())
            m_stream << " job_result=\"" << escaped(o.jobResult) << "\"";
        m_stream << "\n";
        flush();

        m_maxSpeed = std::max(m_maxSpeed, o.speedMps);
        m_maxOmega = std::max(m_maxOmega, o.angularSpeedRadPerSec);
        m_maxWorkers = std::max(m_maxWorkers, o.workerCount);

        // Contract observations are diagnostic rather than fatal so the test
        // keeps running and collects the later failures too.
        if (o.routeValid && o.speedMps > 1.0)
        {
            if (o.firstCurveKind != 0)
                ++m_nonLineMovingStarts;
            if (o.firstCurveLengthMeters <= 1.0)
                ++m_missingBallisticLeadSamples;
        }
    }

    void detectTransitions(const Observation& o)
    {
        if (o.dockingPhase != m_lastPhase ||
            o.requestSerial != m_lastSerial ||
            o.failed != m_lastFailed ||
            o.workerCount != m_lastWorkers)
        {
            logEvent(o, "STATE");
            m_lastPhase = o.dockingPhase;
            m_lastSerial = o.requestSerial;
            m_lastFailed = o.failed;
            m_lastWorkers = o.workerCount;
        }

        if (o.failed && !m_failureLatched)
        {
            ++m_failures;
            m_failureLatched = true;
            logEvent(
                o,
                std::string("FAIL reason=\"") +
                    escaped(o.failureReason) + "\""
            );
        }
        if (!o.failed)
            m_failureLatched = false;

        // A retryable planner miss is not a terminal client failure, but it
        // must never disappear from the stress summary. Count each completed
        // invalid worker once, including repeated attempts for the same serial.
        if (o.jobReady &&
            !m_lastJobReady &&
            !o.jobPlanValid &&
            !o.jobCancelled)
        {
            ++m_plannerRetries;
        }
        m_lastJobReady = o.jobReady;

        if (o.safetyCourseActive && !m_lastSafetyCourseActive)
            ++m_safetyCourseCorrections;

        if (o.safetyCourseActive && o.workerCount > 0)
            ++m_planningSafetyOverlapSamples;

        if (o.safetyStopFallback && !m_lastSafetyStopFallback)
            ++m_emergencyStops;

        m_lastSafetyCourseActive = o.safetyCourseActive;
        m_lastSafetyStopFallback = o.safetyStopFallback;

        if (o.bluePhase != m_lastBluePhase)
        {
            if (o.bluePhase == 1)
            {
                logEvent(o, "BLUE_APPROACH");
            }
            else if (o.bluePhase == 2)
            {
                m_blueEntered = true;
                if (m_blueEnteredAt < -1.0e20)
                    m_blueEnteredAt = o.universeSeconds;
                logEvent(o, "BLUE_ENTER");
            }
            else if (o.bluePhase == 3)
            {
                if (m_blueEntered)
                    m_blueExited = true;
                logEvent(o, "BLUE_EXIT");
            }
            m_lastBluePhase = o.bluePhase;
        }
    }

    Action issue(
        const Observation& o,
        Target target,
        const char* reason
    )
    {
        logEvent(
            o,
            std::string("REQUEST target=") +
                targetName(target) +
                " reason=" + reason
        );

        Action action;
        action.request = true;
        action.target = target;

        ++m_step;
        m_stepStartedAt = o.universeSeconds;
        ++m_requests;

        if (std::string(reason) == "blue-proving-run")
        {
            m_blueEntered = false;
            m_blueExited = false;
            m_blueEnteredAt = -1.0e30;
            m_lastBluePhase = 0;
        }

        return action;
    }

    void logEvent(
        const Observation& o,
        const std::string& event
    )
    {
        if (!m_stream.is_open())
            return;
        m_stream << "EVENT";
        writeCommon(o);
        m_stream << " " << event;
        if (!o.jobResult.empty())
            m_stream << " job_result=\"" << escaped(o.jobResult) << "\"";
        m_stream << "\n";
        flush();
    }

    void logSummary(
        const Observation& o,
        const char* status,
        const std::string& reason
    )
    {
        if (!m_stream.is_open())
            return;

        m_stream << "SUMMARY";
        writeCommon(o);
        m_stream
            << " requests=" << m_requests
            << " failures=" << m_failures
            << " planner_retries=" << m_plannerRetries
            << " safety_course_corrections="
            << m_safetyCourseCorrections
            << " planning_safety_overlap_samples="
            << m_planningSafetyOverlapSamples
            << " emergency_stops=" << m_emergencyStops
            << " max_speed=" << m_maxSpeed
            << " max_omega=" << m_maxOmega
            << " max_workers=" << m_maxWorkers
            << " moving_non_line_samples=" << m_nonLineMovingStarts
            << " missing_ballistic_lead_samples="
            << m_missingBallisticLeadSamples
            << " blue_entered=" << (m_blueEntered ? 1 : 0)
            << " blue_exited=" << (m_blueExited ? 1 : 0)
            << " status=" << status;
        if (!reason.empty())
            m_stream << " reason=\"" << escaped(reason) << "\"";
        m_stream << "\n";
        flush();
    }

private:
    bool m_enabled = false;
    bool m_started = false;
    bool m_complete = false;
    bool m_failureLatched = false;

    std::string m_reportPath =
        "D:/tmp/elite_navigation_stress.log";
    std::ofstream m_stream;

    int m_step = 0;
    double m_startedAt = 0.0;
    double m_stepStartedAt = 0.0;
    double m_lastSampleAt = -1.0e30;

    int m_lastPhase = -1;
    std::uint64_t m_lastSerial = 0;
    bool m_lastFailed = false;
    int m_lastWorkers = -1;
    bool m_lastJobReady = false;
    bool m_lastSafetyCourseActive = false;
    bool m_lastSafetyStopFallback = false;
    int m_lastBluePhase = 0;
    bool m_blueEntered = false;
    bool m_blueExited = false;
    double m_blueEnteredAt = -1.0e30;

    int m_requests = 0;
    int m_failures = 0;
    int m_plannerRetries = 0;
    int m_safetyCourseCorrections = 0;
    int m_planningSafetyOverlapSamples = 0;
    int m_emergencyStops = 0;
    double m_maxSpeed = 0.0;
    double m_maxOmega = 0.0;
    int m_maxWorkers = 0;
    int m_nonLineMovingStarts = 0;
    int m_missingBallisticLeadSamples = 0;
};

} // namespace game::navigation::debug
