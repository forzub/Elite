#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/debug/RuntimeScenarioSelection.h"

namespace game::debug
{

class ShipRuntimeDamageOracle final
{
public:
    using Model = game::ship::plant::PhysicalControlModelId;

    struct ScenarioCase
    {
        Model model = Model::AssistedI;
        std::size_t failedSlot = 0;
    };

    enum class Phase
    {
        ModelSelect = 0,
        Prepare,
        DamageSettle,
        Burn,
        Observe
    };

    struct Capture
    {
        double peakSpeedMps = 0.0;
        double peakForwardVelocityMps = 0.0;
        double peakMainAccelerationMps2 = 0.0;
        double peakAngularRateRadPerSec = 0.0;
        double peakResidualMainAlphaRadPerSec2 = 0.0;
        double peakTrimLoad01 = 0.0;
        double minimumUsableScale01 = 1.0;
    };

    struct Observation
    {
        bool worldReady = false;
        double universeSeconds = 0.0;
        std::string physicalModelLabel;
        game::navigation::LocalFlightControlLaw law =
            game::navigation::defaultLocalFlightControlLaw();

        double speedMps = 0.0;
        double forwardVelocityMps = 0.0;
        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        double mainForwardAccelerationMps2 = 0.0;
        double mainRightAccelerationMps2 = 0.0;
        double mainUpAccelerationMps2 = 0.0;

        double mainAlphaPitchRadPerSec2 = 0.0;
        double mainAlphaYawRadPerSec2 = 0.0;
        double mainAlphaRollRadPerSec2 = 0.0;

        double trimLoad01 = 0.0;
        double trimUsableScale01 = 1.0;
    };

    ShipRuntimeDamageOracle() = default;

    bool configure(const RuntimeScenarioSelection& selection)
    {
        m_enabled = false;
        m_cases.clear();

        if (selection.kind != RuntimeScenarioKind::ShipDamage)
            return true;

        m_enabled = parseScenario(selection.payload);
        if (!m_enabled)
            return false;

        const char* report =
            std::getenv("ELITE_SHIP_DAMAGE_ORACLE_REPORT");
        if (report && *report)
            m_reportPath = report;

        return true;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool complete() const noexcept { return m_complete; }
    const std::string& reportPath() const noexcept { return m_reportPath; }

    ShipControlState control() const noexcept
    {
        ShipControlState c;
        if (!m_enabled || m_complete || m_waitingForModel)
            return c;

        switch (m_phase)
        {
            case Phase::Prepare:
                c.velocityAlignmentCommand =
                    game::navigation::VelocityAlignmentMode::BrakeToStop;
                break;

            case Phase::Burn:
                // Bounded ordinary pilot longitudinal command. The probe never
                // holds this until a physics expectation becomes true.
                c.targetSpeedRate = 1.0f;
                break;

            case Phase::DamageSettle:
            case Phase::Observe:
            case Phase::ModelSelect:
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
        m_modelRequestIssued = true;
        return true;
    }

    bool takeDamageRequest(std::size_t& slotIndex) noexcept
    {
        if (!m_damageRequestPending)
            return false;

        slotIndex = currentCase().failedSlot;
        m_damageRequestPending = false;
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

        // Matching the model label is not sufficient: every case explicitly
        // selects the diagnostic construction so server-side actuator state is
        // guaranteed to be DiagnosticOverride before slot damage is requested.
        if (!m_modelRequestIssued)
        {
            m_waitingForModel = true;
            m_modelRequestPending = true;
            logModelRequest(o);
            return;
        }

        if (actual != desiredModel())
            return;

        // Require one replicated snapshot after the model request. Damage is
        // deliberately never sent in the same client frame as model selection.
        if (m_waitingForModel)
        {
            if (!m_seenRequestedModelSnapshot)
            {
                m_seenRequestedModelSnapshot = true;
                return;
            }

            m_waitingForModel = false;
            m_modelStarted = true;
            m_phase = Phase::Prepare;
            m_phaseStartedAt = o.universeSeconds;
            logModelStart(o);
            return;
        }

        if (!m_modelStarted)
        {
            m_modelStarted = true;
            m_phase = Phase::Prepare;
            m_phaseStartedAt = o.universeSeconds;
            logModelStart(o);
            return;
        }

        switch (m_phase)
        {
            case Phase::Prepare:
                if (!settled(o))
                {
                    heartbeat(o, "PREPARE");
                    return;
                }

                m_damageRequestPending = true;
                m_damageIssued = true;
                m_phase = Phase::DamageSettle;
                m_phaseStartedAt = o.universeSeconds;
                m_capture = {};
                logDamageCommand(o);
                return;

            case Phase::DamageSettle:
                capture(o);
                if (o.universeSeconds - m_phaseStartedAt < 0.25)
                {
                    heartbeat(o, "DAMAGE_SETTLE");
                    return;
                }

                m_phase = Phase::Burn;
                m_phaseStartedAt = o.universeSeconds;
                logPhase(o, "BURN_START");
                return;

            case Phase::Burn:
                capture(o);
                if (o.universeSeconds - m_phaseStartedAt < 0.50)
                {
                    heartbeat(o, "BURN");
                    return;
                }

                m_phase = Phase::Observe;
                m_phaseStartedAt = o.universeSeconds;
                logPhase(o, "BURN_COMPLETE");
                return;

            case Phase::Observe:
                capture(o);
                if (o.universeSeconds - m_phaseStartedAt < 1.50)
                {
                    heartbeat(o, "OBSERVE");
                    return;
                }

                logCaptureComplete(o);
                advanceCase(o);
                return;

            case Phase::ModelSelect:
            default:
                return;
        }
    }

private:
    bool parseScenario(const std::string& casesText)
    {
        std::stringstream stream(casesText);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            if (token.empty())
                return false;

            const auto at = token.find('@');
            if (at == std::string::npos || at == 0 || at + 1 >= token.size())
                return false;

            Model model {};
            if (!game::ship::plant::parsePhysicalControlModel(
                    token.substr(0, at),
                    model
                ))
            {
                return false;
            }

            std::size_t consumed = 0;
            unsigned long slot = 0;
            try
            {
                slot = std::stoul(token.substr(at + 1), &consumed);
            }
            catch (...)
            {
                return false;
            }

            if (consumed != token.size() - at - 1)
                return false;

            const auto& slots =
                game::ship::plant::physicalControlSlots(model);
            if (slot >= slots.size())
                return false;

            m_cases.push_back({
                model,
                static_cast<std::size_t>(slot)
            });
        }

        return !m_cases.empty();
    }

    const ScenarioCase& currentCase() const noexcept
    {
        return m_cases[
            std::clamp(
                m_caseIndex,
                0,
                static_cast<int>(m_cases.size()) - 1
            )
        ];
    }

    Model desiredModel() const noexcept
    {
        return currentCase().model;
    }

    static bool settled(const Observation& o) noexcept
    {
        return
            o.speedMps < 0.10 &&
            std::abs(o.pitchRateRadPerSec) < 0.02 &&
            std::abs(o.yawRateRadPerSec) < 0.02 &&
            std::abs(o.rollRateRadPerSec) < 0.02;
    }

    void advanceCase(const Observation& o)
    {
        ++m_caseIndex;
        m_modelStarted = false;
        m_damageIssued = false;
        m_phase = Phase::ModelSelect;
        m_phaseStartedAt = -1.0e30;
        m_capture = {};
        m_modelRequestIssued = false;
        m_seenRequestedModelSnapshot = false;

        if (m_caseIndex >= static_cast<int>(m_cases.size()))
        {
            m_complete = true;
            if (m_stream.is_open())
            {
                m_stream
                    << "SUMMARY status=COMPLETE cases="
                    << m_cases.size()
                    << "\n";
                m_stream.flush();
            }
            return;
        }

        m_waitingForModel = true;
        m_modelRequestPending = true;
        logModelRequest(o);
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
                << "# Elite in-game propulsion damage oracle\n"
                << "# scenario comes from --diagnostic-scenario ship-damage:<Model@slot,...>\n"
                << "# bounded measurement probe; command durations are phase-local\n"
                << "# no physics PASS condition can extend the burn indefinitely\n"
                << "# format: MODEL / DAMAGE / PHASE / CAPTURE\n";
            m_stream.flush();
        }
    }

    void writeActual(const Observation& o)
    {
        m_stream
            << std::fixed << std::setprecision(6)
            << " speed=" << o.speedMps
            << " v_forward=" << o.forwardVelocityMps
            << " rates=("
            << o.pitchRateRadPerSec << ','
            << o.yawRateRadPerSec << ','
            << o.rollRateRadPerSec << ')'
            << " main_a=("
            << o.mainForwardAccelerationMps2 << ','
            << o.mainRightAccelerationMps2 << ','
            << o.mainUpAccelerationMps2 << ')'
            << " main_alpha=("
            << o.mainAlphaPitchRadPerSec2 << ','
            << o.mainAlphaYawRadPerSec2 << ','
            << o.mainAlphaRollRadPerSec2 << ')'
            << " trim_load=" << o.trimLoad01
            << " usable_scale=" << o.trimUsableScale01;
    }

    void logModelRequest(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "MODEL_REQUEST case=" << (m_caseIndex + 1)
            << '/' << m_cases.size()
            << " from=" << o.physicalModelLabel
            << " to="
            << game::ship::plant::physicalControlModelName(desiredModel())
            << " fail_slot=" << currentCase().failedSlot
            << "\n";
        m_stream.flush();
    }

    void logModelStart(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "\nMODEL_START case=" << (m_caseIndex + 1)
            << '/' << m_cases.size()
            << " model=" << o.physicalModelLabel
            << " law="
            << game::navigation::localFlightControlLawName(o.law)
            << "\n"
            << "PREPARE expected=\"ship stopped before damage probe\"\n";
        m_stream.flush();
    }

    void logDamageCommand(const Observation&)
    {
        if (!m_stream.is_open())
            return;

        m_stream
            << "DAMAGE slot=" << currentCase().failedSlot << "\n"
            << "PHASE damage_settle duration_s=0.25 command=neutral\n"
            << "PHASE burn duration_s=0.50 command=targetSpeedRate:+1\n"
            << "PHASE observe duration_s=1.50 command=neutral\n";
        m_stream.flush();
    }

    void logPhase(const Observation& o, const char* name)
    {
        if (!m_stream.is_open())
            return;

        m_stream << name;
        writeActual(o);
        m_stream << "\n";
        m_stream.flush();
    }

    void capture(const Observation& o)
    {
        const double mainAcceleration =
            std::sqrt(
                o.mainForwardAccelerationMps2 *
                    o.mainForwardAccelerationMps2 +
                o.mainRightAccelerationMps2 *
                    o.mainRightAccelerationMps2 +
                o.mainUpAccelerationMps2 *
                    o.mainUpAccelerationMps2
            );
        const double angularRate =
            std::max({
                std::abs(o.pitchRateRadPerSec),
                std::abs(o.yawRateRadPerSec),
                std::abs(o.rollRateRadPerSec)
            });
        const double residualAlpha =
            std::max({
                std::abs(o.mainAlphaPitchRadPerSec2),
                std::abs(o.mainAlphaYawRadPerSec2),
                std::abs(o.mainAlphaRollRadPerSec2)
            });

        m_capture.peakSpeedMps =
            std::max(m_capture.peakSpeedMps, o.speedMps);
        m_capture.peakForwardVelocityMps =
            std::max(
                m_capture.peakForwardVelocityMps,
                std::abs(o.forwardVelocityMps)
            );
        m_capture.peakMainAccelerationMps2 =
            std::max(
                m_capture.peakMainAccelerationMps2,
                mainAcceleration
            );
        m_capture.peakAngularRateRadPerSec =
            std::max(
                m_capture.peakAngularRateRadPerSec,
                angularRate
            );
        m_capture.peakResidualMainAlphaRadPerSec2 =
            std::max(
                m_capture.peakResidualMainAlphaRadPerSec2,
                residualAlpha
            );
        m_capture.peakTrimLoad01 =
            std::max(m_capture.peakTrimLoad01, o.trimLoad01);
        m_capture.minimumUsableScale01 =
            std::min(
                m_capture.minimumUsableScale01,
                o.trimUsableScale01
            );
    }

    void logCaptureComplete(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream << "FINAL";
        writeActual(o);
        m_stream
            << "\nCAPTURE"
            << " peak_speed=" << m_capture.peakSpeedMps
            << " peak_v_forward=" << m_capture.peakForwardVelocityMps
            << " peak_main_a=" << m_capture.peakMainAccelerationMps2
            << " peak_rate=" << m_capture.peakAngularRateRadPerSec
            << " peak_residual_main_alpha="
            << m_capture.peakResidualMainAlphaRadPerSec2
            << " peak_trim_load=" << m_capture.peakTrimLoad01
            << " min_usable_scale=" << m_capture.minimumUsableScale01
            << "\nRESULT model=" << o.physicalModelLabel
            << " failed_slot=" << currentCase().failedSlot
            << " status=CAPTURED\n";
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
            << " failed_slot=" << currentCase().failedSlot;
        writeActual(o);
        m_stream << "\n";
        m_stream.flush();
    }

private:
    bool m_enabled = false;
    bool m_complete = false;
    bool m_waitingForModel = true;
    bool m_modelRequestPending = true;
    bool m_modelRequestIssued = false;
    bool m_seenRequestedModelSnapshot = false;
    bool m_damageRequestPending = false;
    bool m_modelStarted = false;
    bool m_damageIssued = false;

    Phase m_phase = Phase::ModelSelect;
    Capture m_capture {};
    int m_caseIndex = 0;
    double m_phaseStartedAt = -1.0e30;
    double m_lastHeartbeatAt = -1.0e30;
    std::vector<ScenarioCase> m_cases;

    std::string m_reportPath =
        "D:/tmp/elite/ship_damage_oracle.log";
    std::ofstream m_stream;
};

} // namespace game::debug
