#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"

namespace game::debug
{

class ShipRuntimeControlOracle final
{
public:
    struct Observation
    {
        bool worldReady = false;
        double universeSeconds = 0.0;
        std::string physicalModelLabel;
        game::navigation::LocalFlightControlLaw law =
            game::navigation::defaultLocalFlightControlLaw();

        // Instantaneous velocity resolved in the current hull basis.
        double forwardVelocityMps = 0.0;
        double rightVelocityMps = 0.0;
        double upVelocityMps = 0.0;
        double speedMps = 0.0;

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        // Instantaneous propulsion acceleration resolved in the hull basis.
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

    ShipRuntimeControlOracle()
    {
        const char* enabled = std::getenv("ELITE_SHIP_CONTROL_ORACLE");
        m_enabled =
            enabled != nullptr &&
            std::string(enabled) != "" &&
            std::string(enabled) != "0";

        const char* report =
            std::getenv("ELITE_SHIP_CONTROL_ORACLE_REPORT");
        if (report && *report)
            m_reportPath = report;

        const char* model =
            std::getenv("ELITE_SHIP_PHYSICAL_TEST_MODEL");
        if (model && *model)
            m_requestedModel = model;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool complete() const noexcept { return m_complete; }
    bool failed() const noexcept { return m_failed; }
    const std::string& reportPath() const noexcept { return m_reportPath; }

    ShipControlState control() const noexcept
    {
        ShipControlState c;

        switch (m_step)
        {
            case 0: // neutral baseline
                break;

            case 1: // main forward
                c.targetSpeedRate = 1.0f;
                break;

            case 2: // coast after main
                break;

            case 3: // right translation
                c.strafeInput = 1.0f;
                break;

            case 4: // right translation release / stabilization
                break;

            case 5: // up translation
                c.liftInput = 1.0f;
                break;

            case 6: // up translation release / stabilization
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
                // Direct TVC is an ordinary Newtonian pilot actuator. Assisted
                // obtains translation through its assisted vectoring allocator
                // and intentionally does not expose this manual nozzle handle.
                if (m_observedLaw ==
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

    void update(const Observation& o)
    {
        if (!m_enabled || m_complete)
            return;

        ensureOpen();

        if (!m_started)
        {
            if (!o.worldReady)
                return;

            if (!m_requestedModel.empty() &&
                !o.physicalModelLabel.empty() &&
                o.physicalModelLabel != m_requestedModel)
            {
                finish(
                    o,
                    false,
                    "requested/actual physical model mismatch"
                );
                return;
            }

            m_started = true;
            m_observedLaw = o.law;
            m_stepStartedAt = o.universeSeconds;
            m_stepStart = o;
            logStart(o);
            logCommand(o);
            return;
        }

        const double age = o.universeSeconds - m_stepStartedAt;
        if (age < durationForStep(m_step))
            return;

        const bool pass = evaluateStep(o);
        logResult(o, pass);
        if (!pass)
        {
            finish(o, false, "control-to-motion expectation failed");
            return;
        }

        ++m_step;
        if (m_step >= StepCount)
        {
            finish(o, true, "");
            return;
        }

        m_stepStartedAt = o.universeSeconds;
        m_stepStart = o;
        logCommand(o);
    }

private:
    static constexpr int StepCount = 14;

    static double durationForStep(int step) noexcept
    {
        switch (step)
        {
            case 0: return 0.75;
            case 4:
            case 6:
            case 8:
            case 10:
            case 12:
                return 1.50;
            default:
                return 1.00;
        }
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

    const char* expectedText(int step) const noexcept
    {
        switch (step)
        {
            case 0:
                return "neutral ship remains nearly still";
            case 1:
                return "forward pilot command increases forward speed and main thrust responds";
            case 2:
                return "released longitudinal handle does not create a lateral impulse";
            case 3:
                return "strafe-right command increases body-right velocity";
            case 4:
                return "released strafe command reduces body-right velocity toward zero";
            case 5:
                return "lift-up command increases body-up velocity";
            case 6:
                return "released lift command reduces body-up velocity toward zero";
            case 7:
                return "positive pitch command creates positive pitch rate";
            case 8:
                return "released pitch command damps pitch rate toward zero";
            case 9:
                return "positive yaw command creates positive yaw rate";
            case 10:
                return "released yaw command damps yaw rate toward zero";
            case 11:
                return "positive roll command creates positive roll rate";
            case 12:
                return "released roll command damps roll rate toward zero";
            case 13:
                return m_observedLaw ==
                        game::navigation::LocalFlightControlLaw::Newtonian
                    ? "Newtonian main+gimbal-right creates nonzero yaw gimbal and rightward main-thrust component"
                    : "Assisted strafe request produces rightward translation through the assisted vectoring channel";
            default:
                return "complete";
        }
    }

    bool evaluateStep(const Observation& o) const noexcept
    {
        constexpr double Tiny = 1.0e-3;
        constexpr double SettleRatio = 0.55;

        switch (m_step)
        {
            case 0:
                return
                    o.speedMps < 0.05 &&
                    std::abs(o.pitchRateRadPerSec) < 0.02 &&
                    std::abs(o.yawRateRadPerSec) < 0.02 &&
                    std::abs(o.rollRateRadPerSec) < 0.02;

            case 1:
                return
                    o.forwardVelocityMps >
                        m_stepStart.forwardVelocityMps + Tiny &&
                    std::abs(o.mainForwardAccelerationMps2) > Tiny;

            case 2:
                return
                    std::abs(
                        o.rightVelocityMps -
                        m_stepStart.rightVelocityMps
                    ) < 0.10 &&
                    std::abs(
                        o.upVelocityMps -
                        m_stepStart.upVelocityMps
                    ) < 0.10;

            case 3:
                return
                    o.rightVelocityMps >
                        m_stepStart.rightVelocityMps + Tiny;

            case 4:
                return
                    std::abs(o.rightVelocityMps) <=
                        std::max(
                            0.20,
                            std::abs(m_stepStart.rightVelocityMps) *
                                SettleRatio
                        );

            case 5:
                return
                    o.upVelocityMps >
                        m_stepStart.upVelocityMps + Tiny;

            case 6:
                return
                    std::abs(o.upVelocityMps) <=
                        std::max(
                            0.20,
                            std::abs(m_stepStart.upVelocityMps) *
                                SettleRatio
                        );

            case 7:
                return o.pitchRateRadPerSec >
                    m_stepStart.pitchRateRadPerSec + Tiny;

            case 8:
                return std::abs(o.pitchRateRadPerSec) <=
                    std::max(
                        0.02,
                        std::abs(m_stepStart.pitchRateRadPerSec) *
                            SettleRatio
                    );

            case 9:
                return o.yawRateRadPerSec >
                    m_stepStart.yawRateRadPerSec + Tiny;

            case 10:
                return std::abs(o.yawRateRadPerSec) <=
                    std::max(
                        0.02,
                        std::abs(m_stepStart.yawRateRadPerSec) *
                            SettleRatio
                    );

            case 11:
                return o.rollRateRadPerSec >
                    m_stepStart.rollRateRadPerSec + Tiny;

            case 12:
                return std::abs(o.rollRateRadPerSec) <=
                    std::max(
                        0.02,
                        std::abs(m_stepStart.rollRateRadPerSec) *
                            SettleRatio
                    );

            case 13:
                if (m_observedLaw ==
                    game::navigation::LocalFlightControlLaw::Newtonian)
                {
                    return
                        std::abs(o.gimbalYawRad) > Tiny &&
                        std::abs(o.mainRightAccelerationMps2) > Tiny;
                }

                return
                    o.rightVelocityMps >
                        m_stepStart.rightVelocityMps + Tiny &&
                    std::abs(o.assistedRightAccelerationMps2) > Tiny;

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
                << "# Elite in-game ship control oracle\n"
                << "# requested_model="
                << (m_requestedModel.empty()
                        ? "descriptor-default"
                        : m_requestedModel)
                << "\n"
                << "# format: COMMAND / EXPECTED / ACTUAL / RESULT\n";
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

    void logStart(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "START model=" << o.physicalModelLabel
            << " law="
            << game::navigation::localFlightControlLawName(o.law)
            << "\n";
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
            << " \"" << expectedText(m_step) << "\"\n";
        m_stream.flush();
    }

    void logResult(const Observation& o, bool pass)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "ACTUAL step=" << m_step
            << " name=" << commandName(m_step);
        writeActual(o);
        m_stream << "\n";

        m_stream
            << "RESULT step=" << m_step
            << " name=" << commandName(m_step)
            << " status=" << (pass ? "PASS" : "FAIL")
            << "\n";
        m_stream.flush();
    }

    void finish(
        const Observation& o,
        bool pass,
        const std::string& reason
    )
    {
        m_failed = !pass;
        m_complete = true;

        if (!m_stream.is_open())
            return;

        m_stream
            << "SUMMARY model=" << o.physicalModelLabel
            << " law="
            << game::navigation::localFlightControlLawName(o.law)
            << " status=" << (pass ? "COMPLETE" : "FAILED");

        if (!reason.empty())
            m_stream << " reason=\"" << reason << "\"";

        m_stream << "\n";
        m_stream.flush();
    }

private:
    bool m_enabled = false;
    bool m_started = false;
    bool m_complete = false;
    bool m_failed = false;
    int m_step = 0;
    double m_stepStartedAt = 0.0;
    Observation m_stepStart {};
    game::navigation::LocalFlightControlLaw m_observedLaw =
        game::navigation::defaultLocalFlightControlLaw();

    std::string m_requestedModel;
    std::string m_reportPath =
        "D:/tmp/elite_ship_control_oracle.log";
    std::ofstream m_stream;
};

} // namespace game::debug
