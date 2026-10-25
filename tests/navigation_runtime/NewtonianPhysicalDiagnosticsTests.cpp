// Automated numerical diagnostics of the REAL fixed-nozzle allocator and
// I/Y/X installed main-drive matrix. Outputs to D:/tmp on Windows.
// This is an isolated deterministic actuator test, NOT a ShipCore/runtime
// flight acceptance. The separate server flight trace covers that boundary.
#include "src/game/ship/plant/NewtonianRcsAllocator.h"
#include "src/game/ship/plant/ShipEngineControlMatrix.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"
#include "src/game/equipment/engine/ManoeuvreEnginePreset.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using game::equipment::EquipmentDamageLayer;
using game::equipment::engine::Engine;
using game::equipment::engine::EngineOperatingState;
using game::equipment::engine::EngineUnitDescriptor;
using game::equipment::engine::MountedEngine;
using game::ship::plant::EngineMatrixDemand;
using game::ship::plant::NewtonianRcsDemand;
using game::ship::plant::NewtonianRcsResult;
using Model=game::ship::plant::PhysicalControlModelId;

constexpr double Dt=0.02;
constexpr double MassKg=260000.0;
const glm::dvec3 HalfExtents {12.0,2.5,10.0};
const glm::dvec3 Inertias {11219866.667,25324866.667,15188333.333};

void require(bool ok,const char* why)
{
    if(!ok) throw std::runtime_error(why);
}
void recordVec(std::ostream& out,const glm::dvec3& v)
{
    out<<','<<v.x<<','<<v.y<<','<<v.z;
}
bool near(const glm::dvec3& a,const glm::dvec3& b,double tol=1.e-6)
{
    return glm::length(a-b)<=tol;
}

struct NozzleFixture
{
    std::array<EngineUnitDescriptor,24> passports {};
    std::array<EquipmentDamageLayer,24> damage {};
    std::array<EngineOperatingState,24> operating {};
    std::array<MountedEngine,24> mounted {};

    NozzleFixture()
    {
        const auto& catalog=game::ship::presets::cobra::newtonianRcsNozzles();
        for(std::size_t i=0;i<24;++i)
        {
            passports[i]=game::equipment::engine::cobraManoeuvreNozzlePreset();
            damage[i].failure.failureAtDamage01=0.95;
            operating[i].temperatureK=passports[i].nominalOperatingTemperatureK;
            mounted[i]={
                &passports[i],&damage[i],&operating[i],
                catalog[i].nozzleId,true
            };
        }
    }
};

struct MainFixture
{
    std::vector<EngineUnitDescriptor> passports;
    std::vector<EquipmentDamageLayer> damage;
    std::vector<EngineOperatingState> operating;
    std::vector<MountedEngine> mounted;
    std::size_t mainCount=0;
    explicit MainFixture(Model model)
    {
        const auto& slots=game::ship::plant::physicalControlSlots(model);
        passports.resize(slots.size());
        damage.resize(slots.size());
        operating.resize(slots.size());
        mounted.resize(slots.size());
        for(std::size_t i=0;i<slots.size();++i)
        {
            if(slots[i].role!=game::ship::plant::EngineSlotRole::AftMain)
                continue;
            ++mainCount;
            passports[i]=game::equipment::engine::presets::
                CobrettyCR1VectorMainDrive();
            operating[i].temperatureK=passports[i].nominalOperatingTemperatureK;
            mounted[i]={
                &passports[i],&damage[i],&operating[i],slots[i].id,true
            };
        }
    }
};

const char* label(Model m)
{
    return game::ship::plant::physicalControlModelName(m);
}

struct Recorders
{
    std::ofstream matrix;
    std::ofstream nozzles;
    std::ofstream summary;
    unsigned rows=0;
    unsigned physicalAssertions=0;
    explicit Recorders(const std::filesystem::path& dir)
    {
        std::error_code error;
        std::filesystem::create_directories(dir,error);
        if(error)
            throw std::runtime_error("Cannot create diagnostic directory");
        matrix.open(dir/"elite_newtonian_matrix.csv",std::ios::trunc);
        nozzles.open(dir/"elite_newtonian_nozzles.csv",std::ios::trunc);
        summary.open(dir/"elite_newtonian_summary.txt",std::ios::trunc);
        if(!matrix || !nozzles || !summary)
            throw std::runtime_error("Cannot open Newtonian D:/tmp files");
        matrix<<std::fixed<<std::setprecision(9);
        nozzles<<std::fixed<<std::setprecision(9);
        summary<<std::fixed<<std::setprecision(6);
        matrix<<"model,scenario,tick,dt,installed_main_count,gas01,gas_depleted,"
                 "gas_scale,power_budget_MW,requested_force_x_N,requested_force_y_N,"
                 "requested_force_z_N,requested_moment_x_Nm,requested_moment_y_Nm,"
                 "requested_moment_z_Nm,delivered_force_x_N,delivered_force_y_N,"
                 "delivered_force_z_N,delivered_moment_x_Nm,delivered_moment_y_Nm,"
                 "delivered_moment_z_Nm,delta_v_x_mps,delta_v_y_mps,"
                 "delta_v_z_mps,delta_pitch_radps,delta_yaw_radps,"
                 "delta_roll_radps,electrical_MW,heat_MW,active_nozzles,"
                 "force_residual_N,moment_residual_Nm,main_force_x_N,"
                 "main_force_y_N,main_force_z_N,main_torque_x_Nm,"
                 "main_torque_y_Nm,main_torque_z_Nm\n";
        nozzles<<"model,scenario,tick,nozzle_index,module_id,attached,"
                 "available_N,throttle01,electrical_MW,heat_MW,"
                 "force_x_N,force_y_N,force_z_N,moment_x_Nm,"
                 "moment_y_Nm,moment_z_Nm\n";
        summary<<"SOURCE: isolated installed actuator solver, not a full ShipCore gameplay run\n";
        summary<<"Models: Newtonian-I/Y/X, main force matrix and the same physical 24 RCS nozzle bank\n";
        summary<<"CSV fields are delivered solver forces, not values inferred from HUD or movement\n";
    }
};

void recordRcs(
    Recorders& logs,Model model,const std::string& name,
    NozzleFixture& f,NewtonianRcsDemand command,
    std::size_t mainCount,
    const glm::dvec3& mainForce=glm::dvec3(0.0),
    const glm::dvec3& mainMoment=glm::dvec3(0.0),
    double gas=1.0,bool depleted=false,double gasScale=1.0,
    bool includeNozzles=true
)
{
    const auto out=game::ship::plant::allocateNewtonianRcs(f.mounted,command);
    require(out.valid,"fixed 24-nozzle allocator returned invalid");
    glm::dvec3 reconstructedForce {0.0},reconstructedMoment {0.0};
    double electricity=0.0,heat=0.0;
    unsigned active=0;
    const auto& catalog=game::ship::presets::cobra::newtonianRcsNozzles();
    for(std::size_t i=0;i<24;++i)
    {
        Engine motor(f.mounted[i]);
        const auto sample=motor.execute({
            out.throttle01[i],catalog[i].forceDirectionBody,command.tick
        });
        const glm::dvec3 moment=glm::cross(
            catalog[i].positionBody*command.mountHalfExtentsM-
                command.centerOfMassLocalMeters,
            sample.forceBodyNewtons);
        reconstructedForce+=sample.forceBodyNewtons;
        reconstructedMoment+=moment;
        electricity+=sample.consumedElectricalMW;
        heat+=sample.generatedWasteHeatMW;
        if(out.throttle01[i]>1.e-6) ++active;
        require(out.throttle01[i]>=0.0 && out.throttle01[i]<=1.0,
            "Nozzle throttle out of normalized range");
        require(std::abs(sample.consumedElectricalMW-out.electricalMW[i])<1.e-8,
            "Executed RCS electricity differs from nozzle output");
        if(includeNozzles)
        {
            logs.nozzles<<label(model)<<','<<name<<','<<command.tick
                 <<','<<i<<','<<catalog[i].nozzleId<<','
                 <<(f.mounted[i].attached?1:0)<<','<<out.availabilityN[i]
                 <<','<<out.throttle01[i]<<','<<out.electricalMW[i]
                 <<','<<out.wasteHeatMW[i];
            recordVec(logs.nozzles,sample.forceBodyNewtons);
            recordVec(logs.nozzles,moment);
            logs.nozzles<<'\n';
        }
    }
    require(near(out.deliveredForceN,reconstructedForce),
        "Total RCS force violates sum of real nozzles");
    require(near(out.deliveredMomentNm,reconstructedMoment),
        "Total RCS torque violates sum of CoM-relative nozzle moments");
    // A healthy bank with the specified 8x3 geometry must provide SOME
    // signed authority for each isolated translation and rotation axis.
    // Do not require an arbitrary tracking percentage or assume perfect
    // axis decoupling; the CSV reports the full residual and cross-coupling.
    const bool pureAxis =
        name=="right" || name=="left" || name=="up" ||
        name=="down" || name=="forward" || name=="reverse" ||
        name=="pitch_pos" || name=="pitch_neg" ||
        name=="yaw_pos" || name=="yaw_neg" ||
        name=="roll_pos" || name=="roll_neg";
    if(pureAxis)
    {
        const double requestedForce2=
            glm::dot(command.forceBodyN,command.forceBodyN);
        const double requestedMoment2=
            glm::dot(command.momentBodyNm,command.momentBodyNm);
        if(requestedForce2>1.e-9)
            require(glm::dot(out.deliveredForceN,command.forceBodyN)>
                    0.01*requestedForce2,
                "Healthy RCS cannot generate a requested signed XYZ force");
        else if(requestedMoment2>1.e-9)
            require(glm::dot(out.deliveredMomentNm,command.momentBodyNm)>
                    0.01*requestedMoment2,
                "Healthy RCS cannot generate a requested signed body torque");
    }
    require(std::abs(electricity-
        [&](){double e=0;for(double v:out.electricalMW)e+=v;return e;}())<1.e-8,
        "Power meter inconsistent with individual nozzle power");
    if(std::isfinite(command.availableElectricalMW))
        require(electricity<=command.availableElectricalMW+1.e-8,
            "Physical RCS force exceeded available service MW");
    const glm::dvec3 dv=out.deliveredForceN*(Dt/MassKg);
    const glm::dvec3 dw={
        out.deliveredMomentNm.x*Dt/Inertias.x,
        out.deliveredMomentNm.y*Dt/Inertias.y,
        -out.deliveredMomentNm.z*Dt/Inertias.z
    };
    logs.matrix<<label(model)<<','<<name<<','<<command.tick
               <<','<<Dt<<','<<mainCount<<','<<gas<<','
               <<(depleted?1:0)<<','<<gasScale<<','
               <<command.availableElectricalMW;
    recordVec(logs.matrix,command.forceBodyN);
    recordVec(logs.matrix,command.momentBodyNm);
    recordVec(logs.matrix,out.deliveredForceN);
    recordVec(logs.matrix,out.deliveredMomentNm);
    recordVec(logs.matrix,dv);
    recordVec(logs.matrix,dw);
    logs.matrix<<','<<electricity<<','<<heat<<','<<active
               <<','<<glm::length(out.deliveredForceN-command.forceBodyN)
               <<','<<glm::length(out.deliveredMomentNm-command.momentBodyNm);
    recordVec(logs.matrix,mainForce);
    recordVec(logs.matrix,mainMoment);
    logs.matrix<<'\n';
    ++logs.rows;
    ++logs.physicalAssertions;
    if(name=="power_off")
        require(near(out.deliveredForceN,glm::dvec3(0.0)) &&
                near(out.deliveredMomentNm,glm::dvec3(0.0)) &&
                electricity==0.0 && heat==0.0,
            "Unpowered RCS delivered force, moment or heat");
    if(name=="missing_nozzle")
        require(out.throttle01[0]==0.0 && out.availabilityN[0]==0.0 &&
                out.electricalMW[0]==0.0,
            "A missing physical nozzle generated real force");
}

void runModel(Recorders& logs,Model model,std::size_t expectedMain)
{
    using namespace game::ship::plant;
    MainFixture main(model);
    require(main.mainCount==expectedMain,
        "Selected Newtonian main installation count incorrect");
    ShipParams ship {};
    ship.massKg=MassKg;
    ship.physicalActuatorHalfExtentsMeters=HalfExtents;
    ship.pitchInertiaKgM2=Inertias.x;
    ship.yawInertiaKgM2=Inertias.y;
    ship.rollInertiaKgM2=Inertias.z;
    ship.mainGimbalPitchLimitRad=0.12f;
    ship.mainGimbalYawLimitRad=0.12f;
    ship.turnRadius=20.0f;
    EngineMatrixDemand mainDemand;
    mainDemand.tick=100;
    mainDemand.accelerationBodyMps2={0.0,0.0,-4.0};
    const auto mainResult=executeEngineControlMatrix(
        model,main.mounted,ship,mainDemand);
    require(mainResult.valid &&
        glm::length(mainResult.forceBodyNewtons)>1000.0,
        "Live I/Y/X main engine matrix did not generate forward force");
    require(mainResult.forceBodyNewtons.z<0.0,
        "Newtonian main engine thrust was not aft-to-fore");
    logs.summary<<label(model)<<": physical main slots="<<main.mainCount
        <<", actual main force N="
        <<mainResult.forceBodyNewtons.x<<","
        <<mainResult.forceBodyNewtons.y<<","
        <<mainResult.forceBodyNewtons.z
        <<", main torque Nm="
        <<mainResult.torqueBodyNm.x<<","
        <<mainResult.torqueBodyNm.y<<","
        <<mainResult.torqueBodyNm.z<<"\n";

    NozzleFixture f;
    struct Demand
    {
        const char* name;
        glm::dvec3 force;
        glm::dvec3 moment;
    };
    const Demand commands[] {
        {"right",{12000,0,0},{0,0,0}},
        {"left",{-12000,0,0},{0,0,0}},
        {"up",{0,12000,0},{0,0,0}},
        {"down",{0,-12000,0},{0,0,0}},
        {"forward",{0,0,-12000},{0,0,0}},
        {"reverse",{0,0,12000},{0,0,0}},
        {"pitch_pos",{0,0,0},{30000,0,0}},
        {"pitch_neg",{0,0,0},{-30000,0,0}},
        {"yaw_pos",{0,0,0},{0,30000,0}},
        {"yaw_neg",{0,0,0},{0,-30000,0}},
        {"roll_pos",{0,0,0},{0,0,30000}},
        {"roll_neg",{0,0,0},{0,0,-30000}},
        {"combined",{9000,5000,-6000},{11000,-17000,22000}}
    };
    for(std::size_t i=0;i<sizeof(commands)/sizeof(commands[0]);++i)
    {
        NewtonianRcsDemand cmd;
        cmd.forceBodyN=commands[i].force;
        cmd.momentBodyNm=commands[i].moment;
        cmd.mountHalfExtentsM=HalfExtents;
        cmd.tick=200+i;
        cmd.availableElectricalMW=5.0;
        recordRcs(logs,model,commands[i].name,f,cmd,main.mainCount,
            mainResult.forceBodyNewtons,mainResult.torqueBodyNm);
    }
    NewtonianRcsDemand affected;
    affected.forceBodyN={-12000,3000,2000};
    affected.momentBodyNm={6000,-8000,2000};
    affected.mountHalfExtentsM=HalfExtents;
    affected.availableElectricalMW=5.0;
    affected.tick=300;
    f.mounted[0].attached=false;
    recordRcs(logs,model,"missing_nozzle",f,affected,main.mainCount);
    f.mounted[0].attached=true;
    f.damage[0].accumulated.impact01=1.0;
    recordRcs(logs,model,"damaged_nozzle",f,affected,main.mainCount);
    f.damage[0].accumulated.impact01=0.0;
    affected.availableElectricalMW=0.0;
    recordRcs(logs,model,"power_off",f,affected,main.mainCount);
    affected.availableElectricalMW=0.30;
    recordRcs(logs,model,"power_limited",f,affected,main.mainCount);
    affected.availableElectricalMW=5.0;
    affected.centerOfMassLocalMeters={1.1,-0.7,0.8};
    recordRcs(logs,model,"cargo_com_shift",f,affected,main.mainCount);

    double gas=1.0;
    bool depleted=false;
    bool everDepleted=false;
    bool everRearmed=false;
    // Same replenish/restart gameplay law as installed ShipCore; the burn
    // remains limited by ACTUAL nozzle solver, never retroactively clipped.
    for(int tick=0;tick<1150;++tick)
    {
        const bool burning=tick<650 || tick>=820;
        const bool wasDepleted=depleted;
        const double scale=advanceNewtonianRcsGas(
            gas,depleted,Dt,burning?1.0:0.0,0.20,0.08,0.20);
        everDepleted=everDepleted || depleted;
        everRearmed=everRearmed || (wasDepleted && !depleted);
        if(tick%20==0 || wasDepleted!=depleted ||
           tick==649 || tick==819 || tick==820 || tick==1149)
        {
            NewtonianRcsDemand command;
            command.forceBodyN=burning
                ? glm::dvec3(12000.0*scale,0.0,0.0)
                : glm::dvec3(0.0);
            command.mountHalfExtentsM=HalfExtents;
            command.availableElectricalMW=5.0;
            command.tick=400+tick;
            recordRcs(logs,model,burning?"gas_burn":"gas_recharge",
                f,command,main.mainCount,glm::dvec3(0.0),glm::dvec3(0.0),
                gas,depleted,scale,false);
        }
    }
    require(everDepleted && everRearmed,
        "RCS gas did not deplete and later recover during scripted demand");
    logs.summary<<label(model)<<": gas deplete/restart seen; "
        <<"isolated allocator scenarios passed\n";
}
} // namespace

int main()
{
#ifdef _WIN32
    const std::filesystem::path output="D:/tmp";
#else
    const std::filesystem::path output=
        std::filesystem::temp_directory_path()/"elite_newtonian";
#endif
    try
    {
        Recorders logs(output);
        runModel(logs,Model::NewtonianI,1);
        runModel(logs,Model::NewtonianY,3);
        runModel(logs,Model::NewtonianX,4);
        logs.summary<<"PASS: "<<logs.rows<<" scenario/time samples, "
                    <<logs.physicalAssertions<<" physical conservation/power checks\n";
        std::cout<<"NEWTONIAN PHYSICS DIAGNOSTICS: PASS; logs in "
                 <<output.string()<<'\n';
        return 0;
    }
    catch(const std::exception& e)
    {
        std::ofstream failed(output/"elite_newtonian_diagnostics_failure.txt",
            std::ios::trunc);
        if(failed) failed<<"FAIL: "<<e.what()<<'\n';
        std::cerr<<"NEWTONIAN PHYSICS DIAGNOSTICS: FAIL: "<<e.what()
                 <<"; inspect "<<output.string()<<'\n';
        return 1;
    }
}
