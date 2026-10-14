#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Model = game::ship::plant::PhysicalControlModelId;
using Family = game::ship::plant::PhysicalControlFamily;
using Role = game::ship::plant::EngineSlotRole;
using Regime = game::ship::plant::RuntimeFlightRegime;
using SlotState = game::ship::plant::EngineSlotState;
using SlotDefinition = game::ship::plant::EngineSlotDefinition;
using Snapshot = game::ship::plant::PhysicalControlSnapshot;

constexpr double kDt = 0.02;
constexpr int kSampleEveryTicks = 5;
constexpr double kScenarioDurationSeconds = 2.0;
constexpr double kInitialForwardSpeedMps = 50.0;

#ifndef ELITE_SHIP_PHYSICAL_DAMAGE_DEFAULT_LOG_DIR
#define ELITE_SHIP_PHYSICAL_DAMAGE_DEFAULT_LOG_DIR "ship_physical_damage_logs"
#endif

struct DamageCase
{
    std::string name;
    std::vector<SlotState> slots;
};

struct Scenario
{
    const char* name;
    double durationSeconds;
    double initialForwardSpeedMps;
    ShipControlState control;
};

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

const char* modelName(Model model)
{
    switch (model)
    {
        case Model::AssistedI: return "Assisted-I";
        case Model::AssistedY: return "Assisted-Y";
        case Model::AssistedX: return "Assisted-X";
        case Model::NewtonianI: return "Newtonian-I";
        case Model::NewtonianY: return "Newtonian-Y";
        case Model::NewtonianX: return "Newtonian-X";
    }
    return "unknown";
}

const char* familyName(Family family)
{
    return family == Family::Assisted ? "Assisted" : "Newtonian";
}

const char* roleName(Role role)
{
    switch (role)
    {
        case Role::AftMain: return "aft_main";
        case Role::ForeMain: return "fore_main";
        case Role::ManoeuvreCluster: return "manoeuvre";
    }
    return "unknown";
}

const char* regimeName(Regime regime)
{
    switch (regime)
    {
        case Regime::FullAssisted: return "FullAssisted";
        case Regime::DegradedAssisted: return "DegradedAssisted";
        case Regime::ThrustAxisFallback: return "ThrustAxisFallback";
        case Regime::FullNewtonian: return "FullNewtonian";
        case Regime::DegradedNewtonian: return "DegradedNewtonian";
        case Regime::SurvivalControl: return "SurvivalControl";
        case Regime::Uncontrollable: return "Uncontrollable";
    }
    return "unknown";
}

ShipParams nominalPhysics()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 60.0f;
    p.reverseMainEngineAccelerationMps2 = 60.0f;

    p.mainGimbalPitchLimitRad = 0.12f;
    p.mainGimbalYawLimitRad = 0.12f;
    p.mainGimbalRateRadPerSec = 0.14f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 40.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 4.0f;

    // Keep the damage probe focused on actuator authority rather than
    // accumulator endurance. Resource-depletion tests live elsewhere.
    p.manoeuvreGasUsePerSecond = 0.0f;
    p.manoeuvreGasRechargePerSecond = 0.0f;
    p.manoeuvreGasRestartFraction = 0.20f;

    p.maxGs = 20.0f;
    p.maxLinearGs = 20.0f;
    p.turnRadius = 20.0f;
    p.stopSpeedEpsilonMps = 0.05f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

std::vector<std::size_t> slotIndices(
    const std::vector<SlotDefinition>& definitions,
    Role role
)
{
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < definitions.size(); ++i)
    {
        if (definitions[i].role == role)
            out.push_back(i);
    }
    return out;
}

void failSlot(SlotState& slot)
{
    slot.operational = false;
    slot.thrust01 = 0.0;
    slot.vectoring01 = 0.0;
}

std::vector<DamageCase> buildDamageCases(Model model)
{
    const auto& definitions =
        game::ship::plant::physicalControlSlots(model);

    const auto healthy =
        game::ship::plant::healthySlotState(definitions);

    const auto aft = slotIndices(definitions, Role::AftMain);
    const auto fore = slotIndices(definitions, Role::ForeMain);
    const auto manoeuvre = slotIndices(definitions, Role::ManoeuvreCluster);

    require(!aft.empty(), std::string(modelName(model)) + " has no aft main slot");

    std::vector<DamageCase> cases;
    cases.push_back({"D0_healthy", healthy});

    {
        auto state = healthy;
        state[aft.front()].thrust01 = 0.75;
        cases.push_back({"D1_primary_main_thrust_75", state});
    }
    {
        auto state = healthy;
        state[aft.front()].thrust01 = 0.50;
        cases.push_back({"D2_primary_main_thrust_50", state});
    }
    {
        auto state = healthy;
        state[aft.front()].vectoring01 = 0.50;
        cases.push_back({"D3_primary_main_vectoring_50", state});
    }
    {
        auto state = healthy;
        failSlot(state[aft.front()]);
        cases.push_back({"D4_primary_main_failed", state});
    }

    if (game::ship::plant::familyOf(model) == Family::Assisted)
    {
        if (!fore.empty())
        {
            auto state = healthy;
            failSlot(state[aft.front()]);
            failSlot(state[fore.front()]);
            cases.push_back({"D5_one_aft_one_fore_failed", state});
        }

        if (!fore.empty())
        {
            auto state = healthy;
            for (const auto index : fore)
                failSlot(state[index]);
            cases.push_back({"D6_fore_bank_failed", state});
        }

        {
            auto state = healthy;
            for (std::size_t i = 0; i < definitions.size(); ++i)
            {
                if (definitions[i].role == Role::AftMain ||
                    definitions[i].role == Role::ForeMain)
                {
                    failSlot(state[i]);
                }
            }
            cases.push_back({"D7_all_main_failed", state});
        }
    }
    else
    {
        if (!manoeuvre.empty())
        {
            auto state = healthy;
            failSlot(state[manoeuvre.front()]);
            cases.push_back({"D5_one_rcs_failed", state});
        }

        if (!manoeuvre.empty())
        {
            auto state = healthy;
            for (std::size_t i = 0; i < manoeuvre.size(); i += 2)
                failSlot(state[manoeuvre[i]]);
            cases.push_back({"D6_half_rcs_failed", state});
        }

        if (!manoeuvre.empty())
        {
            auto state = healthy;
            state[aft.front()].thrust01 = 0.50;
            for (std::size_t i = 0; i < manoeuvre.size(); i += 2)
                failSlot(state[manoeuvre[i]]);
            cases.push_back({"D7_main50_half_rcs_failed", state});
        }

        if (aft.size() > 1)
        {
            auto state = healthy;
            for (const auto index : aft)
                failSlot(state[index]);
            cases.push_back({"D8_all_main_failed_rcs_healthy", state});
        }

        if (!manoeuvre.empty())
        {
            auto state = healthy;
            for (const auto index : manoeuvre)
                failSlot(state[index]);
            cases.push_back({"D9_all_rcs_failed_main_healthy", state});
        }

        {
            auto state = healthy;
            for (auto& slot : state)
                failSlot(slot);
            cases.push_back({"D10_all_propulsion_failed", state});
        }
    }

    return cases;
}

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> out;

    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        out.push_back({"forward_burn", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        out.push_back({"coast", kScenarioDurationSeconds, kInitialForwardSpeedMps, c});
    }
    {
        ShipControlState c;
        c.targetSpeedRate = -1.0f;
        out.push_back({"reverse_request", kScenarioDurationSeconds, kInitialForwardSpeedMps, c});
    }
    {
        ShipControlState c;
        c.forwardInput = 1.0f;
        out.push_back({"precision_forward", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.strafeInput = 1.0f;
        out.push_back({"strafe_right", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.liftInput = 1.0f;
        out.push_back({"lift_up", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.pitchInput = 1.0f;
        out.push_back({"pitch_positive", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.yawInput = 1.0f;
        out.push_back({"yaw_positive", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.rollInput = 1.0f;
        out.push_back({"roll_positive", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        c.mainGimbalRightInput = 1.0f;
        out.push_back({"tvc_right_with_main", kScenarioDurationSeconds, 0.0, c});
    }
    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        c.mainGimbalUpInput = 1.0f;
        out.push_back({"tvc_up_with_main", kScenarioDurationSeconds, 0.0, c});
    }

    return out;
}

game::navigation::KinematicFrame testFrame()
{
    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "ship-physical-damage-matrix";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;
    return frame;
}

ShipTransform makeTransform(
    game::navigation::LocalFlightControlLaw law,
    double initialForwardSpeedMps
)
{
    ShipTransform transform {};
    transform.orientation = glm::mat4(1.0f);
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.travelFrame = testFrame();
    transform.motion.localControlLaw = law;
    transform.motion.localPositionMeters = {0.0, 0.0, 0.0};

    const glm::dvec3 forward(transform.forward());
    transform.motion.localVelocityMps =
        forward * initialForwardSpeedMps;
    transform.motion.targetForwardSpeedMps =
        initialForwardSpeedMps;
    transform.setWorldPositionMeters({0.0, 0.0, 0.0});
    return transform;
}

bool finiteVec(const glm::dvec3& v)
{
    return
        std::isfinite(v.x) &&
        std::isfinite(v.y) &&
        std::isfinite(v.z);
}

std::string slotStateText(
    const std::vector<SlotDefinition>& definitions,
    const std::vector<SlotState>& state
)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < definitions.size(); ++i)
    {
        if (i > 0)
            out << '|';

        out
            << definitions[i].id << ':'
            << roleName(definitions[i].role) << ':'
            << (state[i].operational ? 1 : 0) << ':'
            << state[i].thrust01 << ':'
            << state[i].vectoring01;
    }
    return out.str();
}

std::filesystem::path logDirectory()
{
    const char* configured =
        std::getenv("ELITE_SHIP_PHYSICAL_DAMAGE_LOG_DIR");

    const std::filesystem::path path =
        configured && *configured
            ? std::filesystem::path(configured)
            : std::filesystem::path(
                  ELITE_SHIP_PHYSICAL_DAMAGE_DEFAULT_LOG_DIR
              );

    std::filesystem::create_directories(path);
    return path;
}

void writeCsvHeader(std::ofstream& csv)
{
    csv
        << "model,family,damage_case,scenario,time_s,regime,"
        << "main_slots,operational_main_slots,manoeuvre_slots,operational_manoeuvre_slots,"
        << "authority_forward01,authority_reverse01,authority_strafe01,authority_lift01,"
        << "authority_pitch01,authority_yaw01,authority_roll01,"
        << "powered_vector_strafe01,powered_vector_lift01,powered_vector_pitch01,"
        << "powered_vector_yaw01,powered_vector_roll01,"
        << "forward_cap_mps2,reverse_cap_mps2,strafe_cap_mps2,rcs_cap_mps2,angular_cap_radps2,"
        << "gimbal_pitch_limit_rad,gimbal_yaw_limit_rad,gimbal_rate_radps,"
        << "cmd_target_speed_rate,cmd_forward,cmd_strafe,cmd_lift,cmd_pitch,cmd_yaw,cmd_roll,"
        << "cmd_gimbal_right,cmd_gimbal_up,"
        << "pos_x_m,pos_y_m,pos_z_m,vel_x_mps,vel_y_mps,vel_z_mps,"
        << "main_ax_mps2,main_ay_mps2,main_az_mps2,"
        << "rcs_ax_mps2,rcs_ay_mps2,rcs_az_mps2,"
        << "assist_ax_mps2,assist_ay_mps2,assist_az_mps2,"
        << "engine_ax_mps2,engine_ay_mps2,engine_az_mps2,"
        << "pitch_rate_radps,yaw_rate_radps,roll_rate_radps,"
        << "gimbal_yaw_rad,gimbal_pitch_rad,slot_state\n";
}

void writeCsvRow(
    std::ofstream& csv,
    Model model,
    const DamageCase& damage,
    const Scenario& scenario,
    double timeSeconds,
    const Snapshot& snapshot,
    const ShipTransform& transform,
    const std::string& slotState
)
{
    const auto& p = snapshot.effectivePhysics;
    const auto& m = transform.motion;
    const auto& c = scenario.control;

    csv << std::fixed << std::setprecision(9)
        << modelName(model) << ','
        << familyName(game::ship::plant::familyOf(model)) << ','
        << damage.name << ','
        << scenario.name << ','
        << timeSeconds << ','
        << regimeName(snapshot.regime) << ','
        << snapshot.mainSlots << ','
        << snapshot.operationalMainSlots << ','
        << snapshot.manoeuvreSlots << ','
        << snapshot.operationalManoeuvreSlots << ','
        << snapshot.authority.longitudinal.positive << ','
        << snapshot.authority.longitudinal.negative << ','
        << snapshot.authority.strafe.positive << ','
        << snapshot.authority.lift.positive << ','
        << snapshot.authority.pitch.positive << ','
        << snapshot.authority.yaw.positive << ','
        << snapshot.authority.roll.positive << ','
        << snapshot.poweredMainVectoring.trajectoryStrafe01 << ','
        << snapshot.poweredMainVectoring.trajectoryLift01 << ','
        << snapshot.poweredMainVectoring.pitch01 << ','
        << snapshot.poweredMainVectoring.yaw01 << ','
        << snapshot.poweredMainVectoring.roll01 << ','
        << p.forwardMainEngineAccelerationMps2 << ','
        << p.reverseMainEngineAccelerationMps2 << ','
        << p.strafeAccel << ','
        << p.manoeuvreThrusterAccel << ','
        << p.angularAccel << ','
        << p.mainGimbalPitchLimitRad << ','
        << p.mainGimbalYawLimitRad << ','
        << p.mainGimbalRateRadPerSec << ','
        << c.targetSpeedRate << ','
        << c.forwardInput << ','
        << c.strafeInput << ','
        << c.liftInput << ','
        << c.pitchInput << ','
        << c.yawInput << ','
        << c.rollInput << ','
        << c.mainGimbalRightInput << ','
        << c.mainGimbalUpInput << ','
        << m.localPositionMeters.x << ','
        << m.localPositionMeters.y << ','
        << m.localPositionMeters.z << ','
        << m.localVelocityMps.x << ','
        << m.localVelocityMps.y << ','
        << m.localVelocityMps.z << ','
        << m.mainEngineAccelerationMps2.x << ','
        << m.mainEngineAccelerationMps2.y << ','
        << m.mainEngineAccelerationMps2.z << ','
        << m.manoeuvreAccelerationMps2.x << ','
        << m.manoeuvreAccelerationMps2.y << ','
        << m.manoeuvreAccelerationMps2.z << ','
        << m.assistedStabilizationAccelerationMps2.x << ','
        << m.assistedStabilizationAccelerationMps2.y << ','
        << m.assistedStabilizationAccelerationMps2.z << ','
        << m.engineAccelerationMps2.x << ','
        << m.engineAccelerationMps2.y << ','
        << m.engineAccelerationMps2.z << ','
        << transform.pitchRate << ','
        << transform.yawRate << ','
        << transform.rollRate << ','
        << m.mainGimbalYawRad << ','
        << m.mainGimbalPitchRad << ','
        << slotState
        << '\n';
}

void runScenario(
    Model model,
    const DamageCase& damage,
    const Scenario& scenario,
    const Snapshot& snapshot,
    std::ofstream& csv,
    std::ofstream& summary
)
{
    const auto law = game::ship::plant::nativeFlightLaw(model);
    const auto& definitions =
        game::ship::plant::physicalControlSlots(model);

    ShipTransform transform =
        makeTransform(law, scenario.initialForwardSpeedMps);
    WorldParams world {};
    const std::string slots =
        slotStateText(definitions, damage.slots);

    const glm::dvec3 startPosition =
        transform.motion.localPositionMeters;
    const glm::dvec3 startVelocity =
        transform.motion.localVelocityMps;

    const int ticks = static_cast<int>(
        std::ceil(scenario.durationSeconds / kDt)
    );

    for (int tick = 0; tick <= ticks; ++tick)
    {
        const double timeSeconds =
            std::min(
                scenario.durationSeconds,
                static_cast<double>(tick) * kDt
            );

        if (tick % kSampleEveryTicks == 0 || tick == ticks)
        {
            writeCsvRow(
                csv,
                model,
                damage,
                scenario,
                timeSeconds,
                snapshot,
                transform,
                slots
            );
        }

        if (tick == ticks)
            break;

        SharedShipPhysics::integrate(
            transform,
            snapshot.effectivePhysics,
            scenario.control,
            world,
            static_cast<float>(kDt)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            transform.motion.travelFrame,
            snapshot.effectivePhysics,
            static_cast<float>(kDt),
            scenario.control.targetSpeedRate,
            scenario.control.cruiseActive,
            scenario.control.forwardInput,
            scenario.control.liftInput,
            scenario.control.strafeInput,
            transform.forward(),
            transform.right(),
            transform.up()
        );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            transform.motion,
            transform.worldPosition,
            transform.motion.travelFrame,
            snapshot.effectivePhysics,
            kDt
        );

        require(
            finiteVec(transform.motion.localPositionMeters) &&
            finiteVec(transform.motion.localVelocityMps) &&
            finiteVec(transform.motion.engineAccelerationMps2) &&
            std::isfinite(transform.pitchRate) &&
            std::isfinite(transform.yawRate) &&
            std::isfinite(transform.rollRate),
            std::string(modelName(model)) + "/" +
                damage.name + "/" + scenario.name +
                " produced non-finite physical state"
        );
    }

    const glm::dvec3 deltaPosition =
        transform.motion.localPositionMeters - startPosition;
    const glm::dvec3 deltaVelocity =
        transform.motion.localVelocityMps - startVelocity;

    summary << std::fixed << std::setprecision(6)
        << "[RESULT]"
        << " model=" << modelName(model)
        << " family=" << familyName(game::ship::plant::familyOf(model))
        << " damage=" << damage.name
        << " regime=" << regimeName(snapshot.regime)
        << " scenario=" << scenario.name
        << " delta_pos_m=("
        << deltaPosition.x << ';'
        << deltaPosition.y << ';'
        << deltaPosition.z << ')'
        << " delta_v_mps=("
        << deltaVelocity.x << ';'
        << deltaVelocity.y << ';'
        << deltaVelocity.z << ')'
        << " final_speed_mps="
        << glm::length(transform.motion.localVelocityMps)
        << " final_rates_radps=("
        << transform.pitchRate << ';'
        << transform.yawRate << ';'
        << transform.rollRate << ')'
        << " final_gimbal_rad=("
        << transform.motion.mainGimbalYawRad << ';'
        << transform.motion.mainGimbalPitchRad << ')'
        << '\n';
}

void writeCaseHeader(
    std::ofstream& summary,
    Model model,
    const DamageCase& damage,
    const Snapshot& snapshot
)
{
    const auto& definitions =
        game::ship::plant::physicalControlSlots(model);

    summary << std::fixed << std::setprecision(6)
        << "\n[CASE]"
        << " model=" << modelName(model)
        << " family=" << familyName(game::ship::plant::familyOf(model))
        << " damage=" << damage.name
        << " regime=" << regimeName(snapshot.regime)
        << " operational_main="
        << snapshot.operationalMainSlots << '/' << snapshot.mainSlots
        << " operational_manoeuvre="
        << snapshot.operationalManoeuvreSlots << '/' << snapshot.manoeuvreSlots
        << " forward_cap_mps2="
        << snapshot.effectivePhysics.forwardMainEngineAccelerationMps2
        << " reverse_cap_mps2="
        << snapshot.effectivePhysics.reverseMainEngineAccelerationMps2
        << " strafe_cap_mps2="
        << snapshot.effectivePhysics.strafeAccel
        << " rcs_cap_mps2="
        << snapshot.effectivePhysics.manoeuvreThrusterAccel
        << " angular_cap_radps2="
        << snapshot.effectivePhysics.angularAccel
        << " slots="
        << slotStateText(definitions, damage.slots)
        << '\n';
}

void runMatrix(
    const std::filesystem::path& csvPath,
    const std::filesystem::path& summaryPath
)
{
    std::ofstream csv(csvPath, std::ios::out | std::ios::trunc);
    std::ofstream summary(summaryPath, std::ios::out | std::ios::trunc);

    require(csv.is_open(), "failed to open CSV log: " + csvPath.string());
    require(summary.is_open(), "failed to open summary log: " + summaryPath.string());

    writeCsvHeader(csv);

    summary
        << "SHIP PHYSICAL DAMAGE MATRIX\n"
        << "dt_s=" << kDt << '\n'
        << "scenario_duration_s=" << kScenarioDurationSeconds << '\n'
        << "note=diagnostic matrix records current production physics; it does not impose desired autopilot behavior\n"
        << "note=per-slot damage state is logged, but the current accepted motion backend consumes the aggregate ShipParams projection\n";

    constexpr std::array<Model, 6> models = {
        Model::AssistedI,
        Model::AssistedY,
        Model::AssistedX,
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    const auto allScenarios = scenarios();
    const ShipParams nominal = nominalPhysics();

    for (const auto model : models)
    {
        for (const auto& damage : buildDamageCases(model))
        {
            const Snapshot snapshot =
                game::ship::plant::evaluatePhysicalControlModel(
                    model,
                    nominal,
                    damage.slots
                );

            require(
                snapshot.valid,
                std::string(modelName(model)) + "/" +
                    damage.name + " produced invalid physical snapshot"
            );

            writeCaseHeader(summary, model, damage, snapshot);

            for (const auto& scenario : allScenarios)
            {
                runScenario(
                    model,
                    damage,
                    scenario,
                    snapshot,
                    csv,
                    summary
                );
            }
        }
    }

    csv.flush();
    summary.flush();
    require(csv.good(), "CSV log write failed");
    require(summary.good(), "summary log write failed");
}

} // namespace

int main()
{
    try
    {
        const auto directory = logDirectory();
        const auto csvPath =
            directory / "ship_physical_damage_matrix.csv";
        const auto summaryPath =
            directory / "ship_physical_damage_summary.log";

        runMatrix(csvPath, summaryPath);

        std::cout
            << "SHIP PHYSICAL DAMAGE MATRIX: PASS\n"
            << " - six Assisted/Newtonian I/Y/X constructions\n"
            << " - healthy, partial-thrust, vectoring-loss, slot-loss and survival/uncontrollable cases\n"
            << " - common direct-control scenarios sampled through the accepted motion backend\n"
            << " - csv=" << csvPath.string() << "\n"
            << " - summary=" << summaryPath.string() << "\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP PHYSICAL DAMAGE MATRIX: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
