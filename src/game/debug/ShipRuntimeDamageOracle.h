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

    void configure(const RuntimeScenarioSelection& selection)
    {
        m_enabled = false;
        m_cases.clear();

        if (selection.kind != RuntimeScenarioKind::ShipDamage)
            return;

        m_enabled = parseScenario(selection.payload);
        if (!m_enabled)
            return;

        const char* report =
            std::getenv("ELITE_SHIP_DAMAGE_ORACLE_REPORT");
        if (report && *report)
            m_reportPath = report;
    }

    bool enabled() const noexcept { return m_enabled; }
    bool complete() const noexcept { return m_complete; }
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

        if (desiredModel() == Model::NewtonianI)
        {
            // With its only main failed, Newtonian-I should not invent
            // longitudinal authority. Keep requesting forward thrust so the
            // test proves the physical absence of response.
            c.targetSpeedRate = 1.0f;
            return c;
        }

        c.targetSpeedRate = 1.0f;
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
            m_preparing = true;
            logModelStart(o);
            return;
        }

        if (!m_modelStarted)
        {
            m_modelStarted = true;
            m_preparing = true;
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
            m_damageRequestPending = true;
            m_damageIssued = true;
            logDamageCommand(o);
            return;
        }

        if (!m_damageIssued)
            return;

        if (!damageExpectationSatisfied(o))
        {
            heartbeat(o, "WAIT_DAMAGE_RESPONSE");
            return;
        }

        logActualAndPass(o);
        advanceCase(o);
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

    bool damageExpectationSatisfied(const Observation& o) const noexcept
    {
        const Model model = desiredModel();
        const auto family = game::ship::plant::familyOf(model);

        if (family == game::ship::plant::PhysicalControlFamily::Assisted)
        {
            const double residualAlpha =
                std::max({
                    std::abs(o.mainAlphaPitchRadPerSec2),
                    std::abs(o.mainAlphaYawRadPerSec2),
                    std::abs(o.mainAlphaRollRadPerSec2)
                });

            return
                o.forwardVelocityMps >= 3.0 &&
                o.trimLoad01 > 1.0e-4 &&
                o.trimUsableScale01 < 0.9999 &&
                residualAlpha < 1.0e-4;
        }

        if (model == Model::NewtonianI)
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

            return
                o.speedMps < 0.20 &&
                mainAcceleration < 0.01 &&
                o.trimLoad01 < 1.0e-9 &&
                o.trimUsableScale01 > 0.999999;
        }

        const double residualAlpha =
            std::max(
                std::abs(o.mainAlphaPitchRadPerSec2),
                std::abs(o.mainAlphaYawRadPerSec2)
            );

        return
            o.forwardVelocityMps >= 3.0 &&
            o.trimLoad01 < 1.0e-9 &&
            o.trimUsableScale01 > 0.999999 &&
            residualAlpha > 1.0e-4 &&
            residualAlpha < 0.25;
    }

    void advanceCase(const Observation& o)
    {
        ++m_caseIndex;
        m_modelStarted = false;
        m_preparing = false;
        m_damageIssued = false;
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
                << "# condition-driven; no elapsed-time physics verdict\n"
                << "# format: MODEL / DAMAGE / EXPECTED / ACTUAL / RESULT\n";
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
            << "COMMAND forward_burn\n";

        if (game::ship::plant::familyOf(desiredModel()) ==
            game::ship::plant::PhysicalControlFamily::Assisted)
        {
            m_stream
                << "EXPECTED \"trim_load>0; usable_scale<1; forward thrust remains; residual main-reaction alpha approximately zero\"\n";
        }
        else if (desiredModel() == Model::NewtonianI)
        {
            m_stream
                << "EXPECTED \"only main failed; no invented main acceleration; no Assisted trim\"\n";
        }
        else
        {
            m_stream
                << "EXPECTED \"no hidden trim; forward thrust remains; short-lever asymmetric main produces moderate residual pitch/yaw reaction\"\n";
        }
        m_stream.flush();
    }

    void logActualAndPass(const Observation& o)
    {
        if (!m_stream.is_open())
            return;

        m_stream << "ACTUAL";
        writeActual(o);
        m_stream << "\nRESULT model=" << o.physicalModelLabel
                 << " failed_slot=" << currentCase().failedSlot
                 << " status=PASS\n";
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
    bool m_preparing = false;
    bool m_damageIssued = false;

    int m_caseIndex = 0;
    double m_lastHeartbeatAt = -1.0e30;
    std::vector<ScenarioCase> m_cases;

    std::string m_reportPath =
        "D:/tmp/elite/ship_damage_oracle.log";
    std::ofstream m_stream;
};

} // namespace game::debug
