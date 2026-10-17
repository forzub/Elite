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

namespace game::debug
{

class ShipRuntimeDamageOracle final
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

    ShipRuntimeDamageOracle()
    {
        const char* enabled = std::getenv("ELITE_SHIP_DAMAGE_ORACLE");
        m_enabled =
            enabled != nullptr &&
            std::string(enabled) != "" &&
            std::string(enabled) != "0";

        if (!m_enabled)
        {
            std::error_code ec;
            m_enabled = std::filesystem::exists(
                "D:/tmp/elite/ship_damage_oracle.enable",
                ec
            );
        }

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

        // Keep the same ordinary pilot command active until the damaged
        // physical signature is actually observed. No duration-based verdict.
        c.targetSpeedRate = 1.0f;
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

    bool takeDamageRequest(std::size_t& slotIndex) noexcept
    {
        if (!m_damageRequestPending)
            return false;

        // Slot zero is intentionally deterministic:
        // Assisted-Y = aft_top, Newtonian-Y = main_top.
        slotIndex = 0;
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
        ++m_modelIndex;
        m_modelStarted = false;
        m_preparing = false;
        m_damageIssued = false;

        if (m_modelIndex >= static_cast<int>(Models.size()))
        {
            m_complete = true;
            if (m_stream.is_open())
            {
                m_stream << "SUMMARY status=COMPLETE models=2 damaged_slot=0\n";
                m_stream.flush();
            }

            std::error_code ec;
            std::filesystem::remove(
                "D:/tmp/elite/ship_damage_oracle.enable",
                ec
            );
            return;
        }

        m_waitingForModel = true;
        m_modelRequestPending = true;
        logModelRequest(o);
    }

private:
    static constexpr std::array<Model, 2> Models = {
        Model::AssistedY,
        Model::NewtonianY
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

        if (model == Model::AssistedY)
        {
            // The raw asymmetric bank creates a pitch bias, but Assisted
            // should spend trim authority before that avoidable torque reaches
            // the hull. The trim diagnostics are the authoritative evidence
            // that the failure was applied and compensation is active.
            return
                o.forwardVelocityMps >= 3.0 &&
                o.trimLoad01 > 1.0e-4 &&
                o.trimUsableScale01 < 0.9999 &&
                std::abs(o.mainAlphaPitchRadPerSec2) < 1.0e-4 &&
                std::abs(o.mainAlphaYawRadPerSec2) < 1.0e-4;
        }

        // Newtonian-Y has the same failed top main but does not receive
        // Assisted hidden trim. Its mains sit close to the axis, so the
        // residual reaction must be real but moderate rather than explosive.
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
                << "# manual launch; condition-driven, no physics timeout\n"
                << "# Assisted-Y and Newtonian-Y; slot 0 main is failed\n"
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
            << "\nMODEL_START model=" << o.physicalModelLabel
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
            << "DAMAGE slot=0\n"
            << "COMMAND forward_burn\n";

        if (desiredModel() == Model::AssistedY)
        {
            m_stream
                << "EXPECTED \"trim_load>0; usable_scale<1; forward thrust remains; avoidable pitch/yaw reaction suppressed\"\n";
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
        m_stream << state << " model=" << o.physicalModelLabel;
        writeActual(o);
        m_stream << "\n";
        m_stream.flush();
    }

private:
    bool m_enabled = false;
    bool m_complete = false;
    bool m_waitingForModel = true;
    bool m_modelRequestPending = true;
    bool m_damageRequestPending = false;
    bool m_modelStarted = false;
    bool m_preparing = false;
    bool m_damageIssued = false;

    int m_modelIndex = 0;
    double m_lastHeartbeatAt = -1.0e30;

    std::string m_reportPath =
        "D:/tmp/elite/ship_damage_oracle.log";
    std::ofstream m_stream;
};

} // namespace game::debug
