#pragma once

// MotherComputer: client-local ship systems computer.
//
// Only immutable, already-replicated OBSERVATIONS enter this object. There is
// one synchronous flight-critical "fast lane" for the autopilot, and one
// asynchronous, latest-wins advisory analysis lane. Neither may directly
// command an engine, mutate a ShipCore, or query the authoritative server.
// Never treat the worker's previous ship/tick as current control authority.
//
// SERVER BOUNDARY (intentional no-extra-RPC policy):
//   ServerRuntime owns canonical fixed-tick kinematics, forces/torques,
//   equipment health/damage, energy and heat simulation, collisions, docking
//   validation, persistence, ship identity and world time.
//   Client MotherComputer owns read-only BITE aggregation, feasibility
//   certificates, HUD, route feasibility analysis and pilot planning.
//   The only inputs here are fields ALREADY copied into ClientShipState.
//   Neither MotherComputer nor its worker may call a transport/session or
//   request complete NPC engineering graphs just to plan the player ship.
//   If an existing payload lacks a required source, mark its proof UNVERIFIED;
//   extend the server's minimal factual snapshot only after profiling and
//   documenting the missing authoritative datum, never transplant logic.

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <limits>
#include <string>
#include <vector>
#include <thread>
#include <utility>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/presentation/DockingStatusTerminal.h"
#include "src/game/bite/BiteTypes.h"
#include "src/game/simulation/ShipCoreStatus.h"
#include "src/game/ship/telemetry/EngineLiveTelemetry.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/capability/ShipMobilityAssessment.h"
#include "src/game/ship/computer/FlightControlProfile.h"

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
    // Existing engine BITE becomes an immutable motherboard channel.
    // Renderers no longer have to rebuild equipment faults independently.
    game::bite::State installedEquipmentBite {};
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

// Not an airworthiness certificate. This fast status derives useful
// information from existing signed motor-basis samples and systems counters
// while the full power/thermal and combined 6DOF proof is still missing.
struct MotherComputerAdvisory
{
    bool installedMotorBasisKnown = false;
    bool oneAxisForward = false;
    bool oneAxisReverse = false;
    bool yawBidirectional = false;
    bool pitchBidirectional = false;
    bool rollBidirectional = false;
    double optimisticHalfTurnSeconds =
        std::numeric_limits<double>::infinity();
    double observedBusHeadroomMW = 0.0;
    double observedThermalHeadroomK = 0.0;
    double lastMotorElectricalDemandMW = 0.0;
    double lastMotorWasteHeatMW = 0.0;
    bool powerBusOverloaded = false;
    bool reactorShutdown = false;
    bool motorPowerAllocationProofMissing = true;
    bool thermalEnduranceProofMissing = true;
};

// Pure factual cockpit snapshot, refreshed on CLIENT presentation time.
// Owns no client-world, descriptor, server or texture pointers; renderers
// consume only this node. Separate from slower diagnostic worker revisions.
struct MotherComputerEngineHudSlot
{
    std::string id;
    double groupX = 0.0;
    double groupY = 0.0;
    bool fore = false;
    float available01 = 0.0f; // damaged real capacity (yellow)
    float delivered01 = 0.0f; // actually fired motor output (red)
    bool fault = false;       // same canonical BITE state as terminal
    bool hasEngineTemperature = false;
    float engineTemperatureK = 0.0f;
    float engineSafeTemperatureK = 0.0f;
    bool hasEmitterIntegrity = false;
    float emitterIntegrity01 = 0.0f;
    bool hasNozzleAngle = false;
    float nozzleAngleDeg = 0.0f;
};

struct MotherComputerDriveHudGroup
{
    std::string id;
    std::vector<std::string> poweredEngineSlots;
    bool installed = false;
    bool failed = false;
    float integrity01 = 0.0f;
    bool simulatedPower = false;
    bool ignitionOn = false;
    bool ignitionStarting = false;
    bool hasCoreTemperature = false;
    float coreTemperatureK = 0.0f;
    float safeCoreTemperatureK = 0.0f;
    float generatedHeatMW = 0.0f;
    float padHeatMW = 0.0f;
    bool hasPadIntegrity = false;
    float padIntegrity01 = 0.0f;
    float coolantHeatMW = 0.0f;
    float storedHeatMW = 0.0f;
};

struct MotherComputerRcsHudSlot
{
    std::string id;
    bool known = false;
    float integrity01 = 0.0f;
    bool firingKnown = false;
    float firing01 = 0.0f;
};

struct MotherComputerCockpitView
{
    bool valid = false;
    std::uint64_t shipInstanceId = 0;
    std::uint64_t serverTick = 0;
    ShipTransform renderTransform {};
    // Emergency direction projected from installed equipment at the same
    // server tick as the cockpit; deliberately independent from smoothed pose.
    bool emergencyWorkingThrustAxis = false;
    glm::dvec3 workingThrustAxisBody {0.0,0.0,-1.0};
    double nominalSpeedLimitMps = 0.0;
    std::string physicalModelLabel;
    std::string engineHudStyleId = "human_amber";
    std::vector<MotherComputerEngineHudSlot> engineSlots {};
    std::vector<MotherComputerDriveHudGroup> drives {};
    std::vector<MotherComputerRcsHudSlot> rcsSlots {};
    game::bite::State bite {};
    game::ShipCoreStatus systems {};
};

// Terminal composition from docking/navigation is published through the
// motherboard channel. UI's typewriter is still an independent renderer,
// but never reads ship core diagnostics or motor states directly.
struct MotherComputerTerminalView
{
    bool valid = false;
    std::uint64_t shipInstanceId = 0;
    std::uint64_t serverTick = 0;
    game::presentation::DockingTerminalSnapshot presentation {};
};

struct MotherComputerSnapshot
{
    bool valid = false;
    MotherComputerInput input {};

    // This flag is true only for a result calculated on the EXACT source
    // ship and server tick in input, never an older worker generation.
    bool analysisCurrent = false;
    game::ship::capability::ShipMobilityAssessment mobility {};
    MotherComputerAdvisory advisory {};
    // The MotherComputer's current physical DOCTRINE, independent of the
    // asynchronous mobility certificate. Source is one matched ship/tick.
    FlightControlProfile flightProfile {};

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
            if (m_cockpit.valid &&
                (m_cockpit.shipInstanceId != input.shipInstanceId ||
                 m_cockpit.serverTick != input.serverTick))
                m_cockpit = {};
            if (m_terminal.valid &&
                (m_terminal.shipInstanceId != input.shipInstanceId ||
                 m_terminal.serverTick != input.serverTick))
                m_terminal = {};
            m_latest.valid = true;
            m_latest.input = std::move(input);
            m_latest.flightProfile = selectFlightDoctrine(
                m_latest.input.physicalEnvelope,
                static_cast<game::ship::plant::PhysicalControlModelId>(
                    m_latest.input.physicalControlModel),
                m_latest.input.installedMotorAuthority,
                m_latest.input.physicalEnvelopeValid
            );
            // The client-side analysis may not know a changed live CoM
            // before replication. The server-executed axis is the final
            // authority; NEVER hand the autopilot a contrary body reference.
            const bool clientOffset =
                m_latest.flightProfile.doctrine ==
                    PropulsionDoctrine::DamageOffsetAxis;
            if (m_latest.input.physicalEnvelopeValid &&
                clientOffset !=
                    m_latest.input.emergencyWorkingThrustAxis)
                m_latest.flightProfile = {};
            if (clientOffset &&
                m_latest.input.emergencyWorkingThrustAxis)
            {
                const auto& serverAxis =
                    m_latest.input.workingThrustAxisBody;
                const auto& clientAxis =
                    m_latest.flightProfile.workingForwardBody;
                if (!std::isfinite(glm::length(serverAxis)) ||
                    glm::length(serverAxis)<1.0e-8 ||
                    glm::dot(glm::normalize(serverAxis),
                             glm::normalize(clientAxis)) < 0.999)
                    m_latest.flightProfile = {};
            }
            m_latest.analysisCurrent = false;
            m_latest.mobility = {};
            m_latest.autopilotAuthorityCurrent =
                m_latest.input.physicalEnvelopeValid &&
                m_latest.input.installedMotorAuthority.valid;
            m_pending = m_latest.input;
            m_pendingGeneration = ++m_latestGeneration;
            m_pendingAvailable = true;
            if (!m_worker.joinable())
                m_worker = std::thread([this]() { workerLoop(); });
        }
        m_cv.notify_one();
    }

    // All control-law pilots read their *observed physical envelope* here.
    // For Newtonian this is still the legacy nominal/projection envelope:
    // a future installed 6DOF torque certificate must extend it; it is NOT
    // evidence that every commanded force/torque is reachable.
    [[nodiscard]] bool physicalFlightEnvelope(
        std::uint64_t shipId,
        std::uint64_t serverTick,
        ShipParams& out
    ) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_latest.valid ||
            m_latest.input.shipInstanceId != shipId ||
            m_latest.input.serverTick != serverTick ||
            !m_latest.input.physicalEnvelopeValid)
            return false;
        out = m_latest.input.physicalEnvelope;
        // Newtonian control limits come from the server-sampled installed
        // RCS authority, bounded exactly as the server executes them.
        const auto& authority = m_latest.input.installedMotorAuthority;
        const auto model =
            static_cast<game::ship::plant::PhysicalControlModelId>(
                m_latest.input.physicalControlModel);
        if (authority.valid && authority.model == model &&
            game::ship::plant::familyOf(model) ==
                game::ship::plant::PhysicalControlFamily::Newtonian)
            out = game::ship::telemetry::applyAuthorityToControlParams(
                out, authority);
        return true;
    }

    // Server-published installed control authority for this ship/tick.
    [[nodiscard]] bool installedControlAuthority(
        std::uint64_t shipId,
        std::uint64_t serverTick,
        game::ship::telemetry::ShipControlAuthorityTelemetry& out
    ) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_latest.valid ||
            m_latest.input.shipInstanceId != shipId ||
            m_latest.input.serverTick != serverTick)
            return false;
        out = m_latest.input.installedMotorAuthority;
        return out.valid &&
            out.model ==
                static_cast<game::ship::plant::PhysicalControlModelId>(
                    m_latest.input.physicalControlModel);
    }

    // Assisted additionally requires measured INSTALLED motor authority;
    // a bare nominal descriptor is forbidden as an autopilot substitute.
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
                ) ||
            !m_latest.flightProfile.valid)
            return false;
        out = game::ship::telemetry::applyAuthorityToControlParams(
            out, liveAuthority
        );
        // A physical plant proposes a feasible vector; the ship computer
        // publishes the actual chosen reference to the autopilot.
        out.assistedWorkingThrustAxisEmergency =
            m_latest.flightProfile.doctrine ==
                PropulsionDoctrine::DamageOffsetAxis;
        out.assistedWorkingThrustAxisBody =
            m_latest.flightProfile.workingForwardBody;
        out.assistedWorkingThrustMotorIndex =
            m_latest.flightProfile.preferredDriveMotorIndex;
        return true;
    }

    // The caller is a presentation-thread adapter, never a server or
    // renderer. This supports higher-frequency client interpolated attitude
    // and velocity while BITE and motor health remain server-tick based.
    void publishCockpit(MotherComputerCockpitView view)
    {
        if (!view.valid || view.shipInstanceId==0)
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
            return;
        if (m_latest.valid &&
            (m_latest.input.shipInstanceId != view.shipInstanceId ||
             view.serverTick != m_latest.input.serverTick))
            return; // don't splice different time/ship facts
        m_cockpit = std::move(view);
    }

    [[nodiscard]] MotherComputerCockpitView cockpitSnapshot(
        std::uint64_t shipId,
        std::uint64_t serverTick
    ) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_cockpit.valid ||
            m_cockpit.shipInstanceId != shipId ||
            m_cockpit.serverTick != serverTick)
            return {};
        return m_cockpit;
    }

    // Temporary presentation-adapter seam: docking/navigation produces
    // events locally; the terminal receives them only from the computer.
    // Future navigator integration can publish structured event codes here.
    void publishTerminal(MotherComputerTerminalView view)
    {
        if(!view.valid || view.shipInstanceId==0)
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_stopping || !m_latest.valid ||
           m_latest.input.shipInstanceId!=view.shipInstanceId ||
           m_latest.input.serverTick!=view.serverTick)
            return;
        m_terminal=std::move(view);
    }

    [[nodiscard]] MotherComputerTerminalView terminalSnapshot(
        std::uint64_t shipId,std::uint64_t serverTick
    ) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_terminal.valid ||
           m_terminal.shipInstanceId!=shipId ||
           m_terminal.serverTick!=serverTick)
            return {};
        return m_terminal;
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

            MotherComputerAdvisory advisory;
            const auto& a = input.installedMotorAuthority;
            advisory.installedMotorBasisKnown = a.valid;
            if (a.valid)
            {
                constexpr double eps=1.0e-6;
                advisory.oneAxisForward =
                    a.negative[2]>eps ||
                    a.workingForwardAccelerationMps2>eps;
                advisory.oneAxisReverse = a.positive[2]>eps;
                advisory.pitchBidirectional =
                    a.positive[3]>eps && a.negative[3]>eps;
                advisory.yawBidirectional =
                    a.positive[4]>eps && a.negative[4]>eps;
                advisory.rollBidirectional =
                    a.positive[5]>eps && a.negative[5]>eps;
                if (advisory.yawBidirectional)
                {
                    // Lower bound only: assumes instantaneous maximum
                    // symmetric +/- yaw acceleration. Does not account for
                    // coupling, preload, lag, energy or thermal capacity.
                    const double yawA =
                        std::min(a.positive[4],a.negative[4]);
                    advisory.optimisticHalfTurnSeconds =
                        2.0 * std::sqrt(
                            3.14159265358979323846 / yawA
                        );
                }
            }
            advisory.observedBusHeadroomMW =
                input.reactorOutputMW-input.busRequestedMW;
            advisory.observedThermalHeadroomK =
                input.thermalCriticalK-input.thermalTemperatureK;
            advisory.lastMotorElectricalDemandMW =
                input.measuredEngineElectricalMW;
            advisory.lastMotorWasteHeatMW =
                input.measuredEngineWasteHeatMW;
            advisory.powerBusOverloaded = input.busOverloaded;
            advisory.reactorShutdown = input.reactorShutdown;

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
                    m_latest.advisory = advisory;
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
    MotherComputerCockpitView m_cockpit {};
    MotherComputerTerminalView m_terminal {};
};

} // namespace game::ship::computer
