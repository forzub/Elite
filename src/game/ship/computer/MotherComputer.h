#pragma once

// MotherComputer: client-local ship systems computer.
//
// Only immutable, already-replicated OBSERVATIONS enter this object. There is
// one synchronous flight-critical "fast lane" for the autopilot, and one
// asynchronous, latest-wins advisory analysis lane. Neither may directly
// command an engine, mutate a ShipCore, or query the authoritative server.
// Never treat the worker's previous ship/tick as current control authority.

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/simulation/ShipCoreStatus.h"
#include "src/game/ship/telemetry/EngineLiveTelemetry.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/capability/ShipMobilityAssessment.h"

namespace game::ship::computer
{

struct MotherComputerInput
{
    std::uint64_t shipInstanceId = 0;
    std::uint64_t serverTick = 0;
    int physicalControlModel = -1;

    // A coherent copy from replicated physics. The ONLY path the autopilot
    // may use to obtain an Assisted installed-motor envelope in this client.
    bool physicalEnvelopeValid = false;
    ShipParams physicalEnvelope {};
    game::ship::telemetry::ShipControlAuthorityTelemetry
        installedMotorAuthority {};

    // Factual copied detailed diagnostics: per-engine thrust/vector
    // and electrical/thermal motor consumption, BITE conditions, and motion
    // feedback. The asynchronous worker never touches the live source.
    game::ship::telemetry::EngineLiveTelemetry engines {};
    game::ShipCoreStatus biteAndSystems {};
    glm::dvec3 velocityLocalMps {0.0};
    glm::dvec3 deliveredAngularAccelerationBodyRadPerSec2 {0.0};
    glm::dvec3 requestedAngularAccelerationBodyRadPerSec2 {0.0};
    glm::dvec3 workingThrustAxisBody {0.0,0.0,-1.0};
    bool emergencyWorkingThrustAxis = false;

    // Read-only BITE / life-support / resource evidence. These are observed
    // powers and temperatures, NOT proof of forward maneuver endurance.
    std::uint32_t biteAlerts = 0;
    std::uint32_t biteCriticalSystems = 0;
    std::uint32_t biteWarningSystems = 0;
    bool reactorShutdown = false;
    bool busOverloaded = false;
    double reactorOutputMW = 0.0;
    double busRequestedMW = 0.0;
    double measuredEngineElectricalMW = 0.0;
    double thermalTemperatureK = 0.0;
    double thermalCriticalK = 0.0;
    double radiatedPowerMW = 0.0;
    double measuredEngineWasteHeatMW = 0.0;

    // A signed 6D reachable-maneuver proof is NOT the same as the present
    // independent motor basis. Power/thermal certificates are likewise not
    // available until engine demand is included in the power bus. These
    // providers must populate their proofs explicitly, never synthesize
    // verification from healthy-looking counters.
    game::ship::capability::ShipMobilityEvidence mobilityEvidence {};
};

struct MotherComputerSnapshot
{
    bool valid = false;
    MotherComputerInput input {};

    // This flag is true only for a result calculated on the EXACT source
    // ship and server tick in input, never an older worker generation.
    bool analysisCurrent = false;
    game::ship::capability::ShipMobilityAssessment mobility {};

    // Fast lane can be current even when the asynchronous computer is busy.
    bool autopilotAuthorityCurrent = false;
};

class MotherComputer final
{
public:
    MotherComputer() = default;
    ~MotherComputer()
    {
        stop();
    }

    MotherComputer(const MotherComputer&) = delete;
    MotherComputer& operator=(const MotherComputer&) = delete;
    MotherComputer(MotherComputer&&) = delete;
    MotherComputer& operator=(MotherComputer&&) = delete;

    // Call ONLY from the owning client frame after consuming a copied server
    // snapshot. No pointer/reference into ShipCore is retained.
    void observe(MotherComputerInput input)
    {
        if (input.shipInstanceId == 0 || input.serverTick == 0)
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
                return;
            if (m_latest.valid &&
                m_latest.input.shipInstanceId == input.shipInstanceId &&
                input.serverTick < m_latest.input.serverTick)
                return;
            // Only one pending sample. If ten frames arrive while a report
            // is running, diagnostics analyze the newest, not a queue of
            // obsolete data. Flight-critical controls never wait for it.
            m_latest.valid = true;
            m_latest.input = std::move(input);
            m_latest.analysisCurrent = false;
            m_latest.mobility = {};
            m_latest.autopilotAuthorityCurrent =
                m_latest.input.physicalEnvelopeValid;
            m_pending = m_latest.input;
            m_pendingGeneration = ++m_latestGeneration;
            m_pendingAvailable = true;
            if (!m_worker.joinable())
                m_worker = std::thread([this]() { workerLoop(); });
        }
        m_cv.notify_one();
    }

    // Read the currently published *synchronous* flight authority. No stale
    // asynchronous analysis may override control.
    [[nodiscard]] bool assistedFlightEnvelope(
        std::uint64_t shipId,
        std::uint64_t serverTick,
        ShipParams& out,
        game::ship::telemetry::ShipControlAuthorityTelemetry&
            liveAuthority
    ) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_latest.valid ||
            m_latest.input.shipInstanceId != shipId ||
            m_latest.input.serverTick != serverTick ||
            !m_latest.input.physicalEnvelopeValid)
            return false;
        out = m_latest.input.physicalEnvelope;
        liveAuthority = m_latest.input.installedMotorAuthority;
        if (!liveAuthority.valid ||
            liveAuthority.model !=
                static_cast<game::ship::plant::PhysicalControlModelId>(
                    m_latest.input.physicalControlModel
                ))
            return false;
        out = game::ship::telemetry::applyAuthorityToControlParams(
            out, liveAuthority
        );
        return true;
    }

    [[nodiscard]] MotherComputerSnapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_latest;
    }

    // Terminate and join worker deterministically before its owning
    // SpaceState, game client or GL context is destroyed.
    void stop() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
            m_pendingAvailable = false;
        }
        m_cv.notify_all();
        if (m_worker.joinable())
            m_worker.join();
    }

private:
    void workerLoop() noexcept
    {
        for (;;)
        {
            MotherComputerInput input;
            std::uint64_t generation = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    return m_stopping || m_pendingAvailable;
                });
                if (m_stopping)
                    return;
                input = std::move(m_pending);
                generation = m_pendingGeneration;
                m_pendingAvailable = false;
            }

            // This proof evaluator is pure/read-only and may later perform
            // more expensive scenario analyses. It can never stall motors.
            const auto result =
                game::ship::capability::assessShipMobility(
                    input.mobilityEvidence
                );

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stopping)
                    return;
                if (m_latest.valid &&
                    m_latestGeneration == generation &&
                    m_latest.input.shipInstanceId == input.shipInstanceId &&
                    m_latest.input.serverTick == input.serverTick)
                {
                    m_latest.mobility = result;
                    m_latest.analysisCurrent = true;
                }
            }
        }
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    bool m_stopping = false;
    bool m_pendingAvailable = false;
    std::uint64_t m_latestGeneration = 0;
    std::uint64_t m_pendingGeneration = 0;
    MotherComputerInput m_pending {};
    MotherComputerSnapshot m_latest {};
};

} // namespace game::ship::computer
