#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/debug/RuntimeScenarioSelection.h"

namespace game::debug
{

class ShipRuntimeControlOracle final
{
public:
    using Model = game::ship::plant::PhysicalControlModelId;

    struct Observation
    {
        bool worldReady = false;
        double universeSeconds = 0.0;
        std::string physicalModelLabel;
        game::navigation::LocalFlightControlLaw law =
            game::navigation::defaultLocalFlightControlLaw();

        double forwardVelocityMps = 0.0;
        double rightVelocityMps = 0.0;
        double upVelocityMps = 0.0;
        double speedMps = 0.0;

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        double mainForwardAccelerationMps2 = 0.0;
        double mainRightAccelerationMps2 = 0.0;
        double mainUpAccelerationMps2 = 0.0;
        double manoeuvreRightAccelerationMps2 = 0.0;
        double manoeuvreUpAccelerationMps2 = 0.0;
        double assistedRightAccelerationMps2 = 0.0;
        double assistedUpAccelerationMps2 = 0.0;

        double gimbalYawRad = 0.0;
        double gimbalPitchRad = 0.0;
    };

    ShipRuntimeControlOracle() = default;

    void configure(const RuntimeScenarioSelection& selection)
    {
        m_enabled =
            selection.kind == RuntimeScenarioKind::ShipControl;

        const char* report =
            std::getenv("ELITE_SHIP_CONTROL_ORACLE_REPORT");
        if (report && *report)
            m_reportPath = report;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool complete() const noexcept { return m_complete; }
    bool failed() const noexcept { return m_failed; }
    const std::string& reportPath() const noexcept { return m_reportPath; }

    ShipControlState control() const noexcept
    {
        ShipControlState c;

        if (!m_enabled || m_complete || m_waitingForModel)
            return c;

        if (m_preparing)
        {
            c.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BrakeToStop;
            return c;
        }

        switch (m_step)
        {
            case 0:
                break;
            case 1:
                c.targetSpeedRate = 1.0f;
                break;
            case 2:
                break;
            case 3:
                c.strafeInput = 1.0f;
                break;
            case 4:
                break;
            case 5:
                c.liftInput = 1.0f;
                break;
            case 6:
                break;
            case 7:
                c.pitchInput = 1.0f;
                break;
            case 8:
                break;
            case 9:
                c.yawInput = 1.0f;
                break;
            case 10:
                break;
            case 11:
                c.rollInput = 1.0f;
                break;
            case 12:
                break;
            case 13:
                if (desiredLaw() ==
                    game::navigation::LocalFlightControlLaw::Newtonian)
                {
                    c.targetSpeedRate = 1.0f;
                    c.mainGimbalRightInput = 1.0f;
                }
                else
                {
                    c.strafeInput = 1.0f;
                }
                break;
            default:
                break;
        }

        return c;
    }

    bool takeModelRequest(Model& out) noexcept
    {
        if (!m_modelRequestPending)
            return false;

        out = desiredModel();
        m_modelRequestPending = false;
        return true;
    }

    void update(const Observation& o)
    {
        if (!m_enabled || m_complete || !o.worldReady)
            return;

        ensureOpen();

        Model actual {};
        if (!game::ship::plant::parsePhysicalControlModel(
                o.physicalModelLabel,
                actual
            ))
        {
            return;
        }

        if (actual != desiredModel())
        {
            if (!m_waitingForModel)
            {
                m_waitingForModel = true;
                m_modelRequestPending = true;
                logModelRequest(o);
            }
            return;
        }

        if (m_waitingForModel || !m_modelStarted)
        {
            m_waitingForModel = false;
            m_modelStarted = true;
            m_preparing = true;
            m_step = 0;
            logModelStart(o);
            return;
        }

        if (m_preparing)
        {
            if (!settled(o))
            {
                heartbeat(o, "PREPARE");
                return;
            }

            m_preparing = false;
            m_stepStart = o;
            logCommand(o);
            return;
        }

        if (!expectationSatisfied(o))
        {
            // This probe should establish lateral actuation almost at once.
            // Never hold full strafe until the ship reaches the 500 m/s
            // gameplay speed ceiling because an oracle channel is stale.
            if (m_step == 13 && o.speedMps >= 100.0)
            {
                logRunawayFailure(o);
                m_failed = true;
                m_complete = true; // stop emitting flight commands
                std::error_code ec;
                std::filesystem::remove(
                    "D:/tmp/elite/ship_control_oracle.enable", ec
                );
                return;
            }
            heartbeat(o, "WAIT");
            return;
        }

        logActualAndPass(o);
        ++m_step;

        if (m_step >= StepCount)
        {
            logModelComplete(o);
            ++m_modelIndex;
            m_modelStarted = false;
            m_preparing = false;

            if (m_modelIndex >= static_cast<int>(Models.size()))
            {
                m_complete = true;
                if (m_stream.is_open())
                {
                    m_stream
                        << "SUMMARY status=COMPLETE models=6\n";
                    m_stream.flush();
                }

                std::error_code ec;
                std::filesystem::remove(
                    "D:/tmp/elite/ship_control_oracle.enable",
                    ec
                );
                return;
            }

            m_waitingForModel = true;
            m_modelRequestPending = true;
            logModelRequest(o);
            return;
        }

        m_stepStart = o;
        logCommand(o);
    }

private:
    static constexpr int StepCount = 14;
    static constexpr std::array<Model, 6> Models = {
        Model::AssistedI,
        Model::AssistedY,
        Model::AssistedX,
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    Model desiredModel() const noexcept
    {
        return Models[
            std::clamp(
                m_modelIndex,
                0,
                static_cast<int>(Models.size()) - 1
            )
        ];
    }

    game::navigation::LocalFlightControlLaw desiredLaw() const noexcept
    {
        return game::ship::plant::nativeFlightLaw(desiredModel());
    }

    static bool settled(const Observation& o) noexcept
    {
        return
            o.speedMps < 0.10 &&
            std::abs(o.pitchRateRadPerSec) < 0.02 &&
            std::abs(o.yawRateRadPerSec) < 0.02 &&
            std::abs(o.rollRateRadPerSec) < 0.02;
    }

    static const char* commandName(int step) noexcept
    {
        switch (step)
        {
            case 0: return "neutral";
            case 1: return "forward_burn";
            case 2: return "coast";
            case 3: return "strafe_right";
            case 4: return "release_strafe";
            case 5: return "lift_up";
            case 6: return "release_lift";
            case 7: return "pitch_positive";
            case 8: return "release_pitch";
            case 9: return "yaw_positive";
            case 10: return "release_yaw";
            case 11: return "roll_positive";
            case 12: return "release_roll";
            case 13: return "family_vectoring_probe";
            default: return "done";
        }
    }

    const char* expectedText() const noexcept
    {
        switch (m_step)
        {
            case 0:
                return "neutral ship remains stopped and stable";
            case 1:
                return "forward command produces main thrust and at least 5 m/s forward velocity";
            case 2:
                return "released longitudinal command produces no meaningful lateral/up propulsion acceleration";
            case 3:
                return "strafe-right command produces at least 1 m/s body-right velocity";
            case 4:
                return "after strafe release body-right velocity returns below 0.20 m/s";
            case 5:
                return "lift-up command produces at least 1 m/s body-up velocity";
            case 6:
                return "after lift release body-up velocity returns below 0.20 m/s";
            case 7:
                return "positive pitch command produces pitch rate above 0.20 rad/s";
            case 8:
                return "after pitch release pitch rate returns below 0.05 rad/s";
            case 9:
                return "positive yaw command produces yaw rate above 0.20 rad/s";
            case 10:
                return "after yaw release yaw rate returns below 0.05 rad/s";
            case 11:
                return "positive roll command produces roll rate above 0.20 rad/s";
            case 12:
                return "after roll release roll rate returns below 0.05 rad/s";
            case 13:
                return desiredLaw() ==
                        game::navigation::LocalFlightControlLaw::Newtonian
                    ? "Newtonian main+gimbal-right produces nonzero yaw gimbal and rightward main-thrust component"
                    : "Assisted strafe increases body-right speed using executed main-engine vectoring";
            default:
                return "complete";
        }
    }

    bool expectationSatisfied(const Observation& o) const noexcept
    {
        constexpr double TinyA = 1.0e-3;

        switch (m_step)
        {
            case 0:
                return settled(o);

            case 1:
                return
                    o.forwardVelocityMps >= 5.0 &&
                    std::abs(o.mainForwardAccelerationMps2) > TinyA;

            case 2:
                return
                    std::abs(o.mainRightAccelerationMps2) < 0.10 &&
                    std::abs(o.mainUpAccelerationMps2) < 0.10 &&
                    std::abs(o.manoeuvreRightAccelerationMps2) < 0.10 &&
                    std::abs(o.manoeuvreUpAccelerationMps2) < 0.10;

            case 3:
                return o.rightVelocityMps >= 1.0;

            case 4:
                return std::abs(o.rightVelocityMps) <= 0.20;

            case 5:
                return o.upVelocityMps >= 1.0;

            case 6:
                return std::abs(o.upVelocityMps) <= 0.20;

            case 7:
                return o.pitchRateRadPerSec >= 0.20;

            case 8:
                return std::abs(o.pitchRateRadPerSec) <= 0.05;

            case 9:
                return o.yawRateRadPerSec >= 0.20;

            case 10:
                return std::abs(o.yawRateRadPerSec) <= 0.05;

            case 11:
                return o.rollRateRadPerSec >= 0.20;

            case 12:
                return std::abs(o.rollRateRadPerSec) <= 0.05;

            case 13:
                if (desiredLaw() ==
                    game::navigation::LocalFlightControlLaw::Newtonian)
                {
                    return
                        std::abs(o.gimbalYawRad) >= 0.02 &&
                        std::abs(o.mainRightAccelerationMps2) >= 0.10;
                }

                return
                    o.rightVelocityMps >=
                        m_stepStart.rightVelocityMps + 1.0 &&
                    // The installed Assisted executor writes delivered
                    // vectoring to mainEngineAccelerationMps2 and clears
                    // the pre-execution assistedStabilization channel.
                    // Observe the executed main-right force, not a retired
                    // intermediate request channel.
                    o.mainRightAccelerationMps2 >= 0.10;

            default:
                return true;
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

        m_stream.open(m_reportPath, std::ios::out | std::ios::trunc);
        if (m_stream.is_open())
        {
            m_stream
                << "# Elite in-game six-model ship control oracle\n"
                << "# manual-launch mode; no command-duration pass/fail timer\n"
                << "# command remains active until EXPECTED becomes true\n"
                << "# step 13 safety: stop on unresolved probe at 100 m/s\n"
                << "# format: MODEL / COMMAND / EXPECTED / ACTUAL / RESULT\n";
            m_stream.flush();
        }
    }

    void writeActual(const Observation& o)
    {
        m_stream
            << std::fixed << std::setprecision(6)
            << " speed=" << o.speedMps
            << " v_forward=" << o.forwardVelocityMps
            << " v_right=" << o.rightVelocityMps
            << " v_up=" << o.upVelocityMps
            << " rates=("
            << o.pitchRateRadPerSec << ','
            << o.yawRateRadPerSec << ','
            << o.rollRateRadPerSec << ')'
            << " main_a=("
            << o.mainForwardAccelerationMps2 << ','
            << o.mainRightAccelerationMps2 << ','
            << o.mainUpAccelerationMps2 << ')'
            << " rcs_a=("
            << o.manoeuvreRightAccelerationMps2 << ','
            << o.manoeuvreUpAccelerationMps2 << ')'
            << " assisted_a=("
            << o.assistedRightAccelerationMps2 << ','
            << o.assistedUpAccelerationMps2 << ')'
            << " gimbal=("
            << o.gimbalYawRad << ','
            << o.gimbalPitchRad << ')';
    }

    void logModelRequest(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "MODEL_REQUEST from=" << o.physicalModelLabel
            << " to="
            << game::ship::plant::physicalControlModelName(desiredModel())
            << "\n";
        m_stream.flush();
    }

    void logModelStart(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "\nMODEL_START index=" << (m_modelIndex + 1)
            << "/6 model=" << o.physicalModelLabel
            << " law="
            << game::navigation::localFlightControlLawName(o.law)
            << "\n"
            << "PREPARE expected=\"speed<0.10 and angular rates<0.02; BrakeToStop remains active until true\"\n";
        m_stream.flush();
    }

    void logCommand(const Observation&)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "COMMAND step=" << m_step
            << " name=" << commandName(m_step)
            << "\n"
            << "EXPECTED step=" << m_step
            << " \"" << expectedText() << "\"\n";
        m_stream.flush();
    }

    void logActualAndPass(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "ACTUAL step=" << m_step
            << " name=" << commandName(m_step);
        writeActual(o);
        m_stream << "\n"
            << "RESULT step=" << m_step
            << " name=" << commandName(m_step)
            << " status=PASS\n";
        m_stream.flush();
    }

    void logRunawayFailure(const Observation& o)
    {
        if (!m_stream.is_open())
            return;
        m_stream << "ACTUAL step=" << m_step
                 << " name=" << commandName(m_step);
        writeActual(o);
        m_stream << "\nRESULT step=" << m_step
                 << " name=" << commandName(m_step)
                 << " status=FAIL reason=unresolved_probe_runaway_speed"
                 << " limit_mps=100\n"
                 << "SUMMARY status=FAILED model=" << o.physicalModelLabel
                 << " step=" << m_step << "\n";
        m_stream.flush();
    }

    void logModelComplete(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "MODEL_SUMMARY model=" << o.physicalModelLabel
            << " status=COMPLETE\n";
        m_stream.flush();
    }

    void heartbeat(const Observation& o, const char* state)
    {
        if (!m_stream.is_open())
            return;

        if (o.universeSeconds - m_lastHeartbeatAt < 1.0)
            return;

        m_lastHeartbeatAt = o.universeSeconds;
        m_stream
            << state
            << " model=" << o.physicalModelLabel
            << " step=" << m_step;
        writeActual(o);
        m_stream << "\n";
        m_stream.flush();
    }

private:
    bool m_enabled = false;
    bool m_complete = false;
    bool m_failed = false;
    bool m_waitingForModel = true;
    bool m_modelRequestPending = true;
    bool m_modelStarted = false;
    bool m_preparing = false;

    int m_modelIndex = 0;
    int m_step = 0;
    double m_lastHeartbeatAt = -1.0e30;
    Observation m_stepStart {};

    std::string m_reportPath =
        "D:/tmp/elite/ship_control_oracle_matrix.log";
    std::ofstream m_stream;
};

} // namespace game::debug
