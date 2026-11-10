// Author 2026-11-10: the Cobra drive bay shrinks to dia 3.5 m; the CR-1 pad becomes a convex cone 1.3 m high on the
// 3.5 m base and the drive's efficiency rises to 0.991, so the heat margin and the pad-damage tolerance stay those of
// the former flat 5 m pad (x1.65 vs x1.68; full thrust down to ~61 % pad integrity).
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/equipment/engine/presets/KobraN1AxialMainDrive.h"

using namespace game::equipment::engine;
namespace th = game::physics::thermal;

static void require(bool ok, const char* what)
{
    if (!ok)
    {
        std::printf("COBRA COMPACT DRIVE THERMAL: FAIL: %s\n", what);
        std::exit(1);
    }
}

static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) <= eps * std::max(1.0, std::fabs(b)); }

// steady thermal ceiling: evaluated at the safe housing temperature, where the protection band applies the
// pad-limited steady fraction (a cool engine may exceed it for a while on its thermal capacity)
static double safeFraction(const EngineUnitDescriptor& d, double intact01)
{
    EngineOperatingState op;
    op.temperatureK = d.safeOperatingTemperatureK;
    op.emitter.surface.intactArea01 = intact01;
    game::equipment::EquipmentDamageLayer damage;
    const auto eff = evaluateEngineUnit(d, damage, op);
    return safeEmitterThrustFraction01(d, eff, op);
}

int main()
{
    // geometry law: a flat pad is unchanged, a cone adds lateral area, a frustum keeps the opening at the tip
    require(near(th::conicalEmitterAreaM2(5.0, 0.0, 0.0), th::annulusAreaM2(5.0, 0.0)), "flat pad area changed");
    require(near(th::conicalEmitterAreaM2(3.5, 0.0, 1.3), 11.9853), "cone 3.5 x 1.3 m lateral area");
    require(th::conicalEmitterAreaM2(3.5, 1.0, 1.3) > th::annulusAreaM2(3.5, 1.0), "frustum must exceed its annulus");
    require(th::conicalEmitterAreaM2(3.5, 0.0, -1.0) == th::annulusAreaM2(3.5, 0.0), "negative height is a flat pad");

    const auto cr1 = presets::CobrettyCR1VectorMainDrive();
    require(near(cr1.thermalEmitter.outerDiameterM, 3.5) && near(cr1.thermalEmitter.coneHeightM, 1.3), "CR-1 pad geometry");
    const double area = th::conicalEmitterAreaM2(3.5, 0.0, 1.3);
    const double capacity = th::propulsionPadCoolingMW(area, 1.0, 1.0, 0.0, 1.0);
    const double waste = cr1.nominalElectricalPowerMW * (1.0 - cr1.nominalEfficiency01);
    require(near(capacity, 71.912, 1e-3), "CR-1 pad capacity 71.9 MW");
    require(near(waste, 43.65, 1e-3), "CR-1 full-thrust waste 43.65 MW");
    require(capacity / waste > 1.6 && capacity / waste < 1.7, "CR-1 heat margin ~x1.65 (former flat 5 m pad x1.68)");

    require(near(safeFraction(cr1, 1.0), 1.0), "healthy CR-1 must hold full thrust");
    require(safeFraction(cr1, 0.62) > 0.999, "CR-1 holds full thrust down to ~61 % pad integrity");
    require(near(safeFraction(cr1, 0.5), 0.5 * capacity / waste, 1e-2), "half pad -> sustained thrust ~82 %");

    // the N1 axial drive inherits the CR-1 thermal passport (same Cobra bay)
    const auto n1 = presets::KobraN1AxialMainDrive();
    require(near(n1.thermalEmitter.outerDiameterM, 3.5) && near(n1.thermalEmitter.coneHeightM, 1.3) &&
                near(n1.nominalEfficiency01, 0.991), "N1 must inherit the compact CR-1 thermal passport");

    std::printf("COBRA COMPACT DRIVE THERMAL: PASS (area %.2f m2, capacity %.1f MW, waste %.2f MW, margin x%.2f, "
                "sustained at 50%% pad %.0f%%)\n", area, capacity, waste, capacity / waste, 100.0 * safeFraction(cr1, 0.5));
    return 0;
}
