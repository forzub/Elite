#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace game::physics::materials
{

// Stable physical identity. Renderer and trading systems are consumers, not
// owners of the material passport. Values are design prototypes for this
// fictional universe, NOT certified engineering data.
struct MaterialPassport
{
    std::string_view materialId;
    std::string_view displayName;
    std::string_view meshMaterialId;
    std::string_view tradeCommodityId;
    double densityKgM3;
    double specificHeatJPerKgK;
    double thermalConductivityWPerMK;
    double emissivity01;
    double laserAbsorptivity01;
    double nominalOperatingTemperatureK;
    double damageOnsetTemperatureK;
    double destructionTemperatureK;
    double laserFailureFluenceMJPerM2;
    double kineticFailureEnergyMJPerM2;
    double thermalShockLimitKPerSec;
    double fragmentationAreaFactor;
};

// Fictional titanium-aluminide-based engine superalloy: one material for
// BOTH thrust housing and TVC/nozzle hinge. Its 1200 K continuous service
// temperature is a gameplay rating, NOT a claim about ordinary Ti alloys.
// Carbon/SiC laminate: thin radiator skins, tolerant of 1050 K in vacuum.
// HfC/graphitic toughened ceramic: much hotter but brittle engine annulus.
// Failure fluences include geometry/thickness abstractions for the *surface*
// only; they are not universal projectile penetration laws for an entire hull.
inline constexpr std::array<MaterialPassport, 3> UniverseMaterialCatalog {{
    {
        "elite.material.radiator.c-sic.v1",
        "Aster C/SiC radiator laminate",
        "mat_radiator_aster_c_sic",
        "commodity_c_sic_radiator_laminate",
        2650.0, 1000.0, 25.0,
        0.90, 0.85,
        1050.0, 1300.0, 1900.0,
        4.0, 2.5, 350.0, 1.8
    },
    {
        "elite.material.engine.hfc-c.v1",
        "Vanta HfC/C reinforced emitter ceramic",
        "mat_engine_vanta_hfc_c",
        "commodity_hfc_c_emitter_ceramic",
        10500.0, 550.0, 35.0,
        0.90, 0.82,
        2500.0, 2850.0, 3800.0,
        12.0, 6.0, 500.0, 1.5
    },
    {
        "elite.material.engine.tial-ht.v1",
        "Talon Ti-Al HT engine alloy",
        "mat_engine_talon_tial_ht",
        "commodity_tial_ht_engine_alloy",
        4550.0, 720.0, 17.0,
        0.68, 0.43,
        1200.0, 1450.0, 1850.0,
        9.0, 9.0, 280.0, 1.2
    }
}};

[[nodiscard]] inline const MaterialPassport* findMaterial(
    std::string_view id
) noexcept
{
    for (const auto& item : UniverseMaterialCatalog)
        if (item.materialId == id)
            return &item;
    return nullptr;
}

enum class SurfaceImpactKind { Laser, Kinetic };

// All energies are MEGAJOULES physically incident upon THIS surface.
// Callers must supply a converted energy and actual illuminated/contact
// footprint, not raw DamageEvent::energy (whose units are unspecified).
struct SurfaceExposure
{
    SurfaceImpactKind kind = SurfaceImpactKind::Laser;
    double incidentEnergyMJ = 0.0;
    double footprintAreaM2 = 0.0;
    double surfaceTemperatureK = 0.0;
    double temperatureRateKPerSec = 0.0;
};

// Independently persistent area damage. Microcracks reduce subsequent
// resilience but do not magically erase undamaged distant surface.
struct SurfaceIntegrity
{
    double intactArea01 = 1.0;
    double microcrack01 = 0.0;
};

// A deterministic bounded local fracture/ablation model. A single small hit
// cannot destroy an entire 11 m2 ring regardless of projectile energy.
// At/above destruction temperature the surface is structurally unusable.
// Thermal shock damages the impact footprint; only surface-wide exposure
// can destroy the whole surface.
[[nodiscard]] inline SurfaceIntegrity exposeSurface(
    const MaterialPassport& material,
    double totalSurfaceAreaM2,
    SurfaceIntegrity previous,
    const SurfaceExposure& hit
) noexcept
{
    if (!std::isfinite(totalSurfaceAreaM2) || totalSurfaceAreaM2 <= 0.0)
        return {0.0, 1.0};

    previous.intactArea01 = std::isfinite(previous.intactArea01)
        ? std::clamp(previous.intactArea01, 0.0, 1.0) : 0.0;
    previous.microcrack01 = std::isfinite(previous.microcrack01)
        ? std::clamp(previous.microcrack01, 0.0, 1.0) : 1.0;

    if (std::isfinite(hit.surfaceTemperatureK) &&
        hit.surfaceTemperatureK >= material.destructionTemperatureK)
        return {0.0, 1.0};

    if (!std::isfinite(hit.incidentEnergyMJ) ||
        !std::isfinite(hit.footprintAreaM2) ||
        hit.incidentEnergyMJ < 0.0 || hit.footprintAreaM2 <= 0.0)
        return previous;

    const double area = std::min(totalSurfaceAreaM2, hit.footprintAreaM2);
    const double exposedFraction = area / totalSurfaceAreaM2;
    const double softened01 =
        std::isfinite(hit.surfaceTemperatureK) &&
        hit.surfaceTemperatureK > material.damageOnsetTemperatureK
        ? std::clamp(
            (hit.surfaceTemperatureK - material.damageOnsetTemperatureK) /
                std::max(1.0, material.destructionTemperatureK -
                                  material.damageOnsetTemperatureK),
            0.0, 1.0)
        : 0.0;
    const double strength = std::max(
        0.08, (1.0 - 0.8 * softened01) *
              (1.0 - 0.5 * previous.microcrack01)
    );
    const double absorbedMJ = hit.incidentEnergyMJ *
        (hit.kind == SurfaceImpactKind::Laser
            ? std::clamp(material.laserAbsorptivity01, 0.0, 1.0)
            : 1.0);
    const double failureFluence = hit.kind == SurfaceImpactKind::Laser
        ? material.laserFailureFluenceMJPerM2
        : material.kineticFailureEnergyMJPerM2;
    const double localFailureRatio = absorbedMJ /
        std::max(1.0e-9, area * failureFluence * strength);
    const double shockRatio =
        std::isfinite(hit.temperatureRateKPerSec) &&
        material.thermalShockLimitKPerSec > 0.0
            ? std::max(0.0, std::abs(hit.temperatureRateKPerSec) /
                                  material.thermalShockLimitKPerSec - 1.0)
            : 0.0;
    const double localDamage = std::clamp(
        localFailureRatio + 0.25 * shockRatio, 0.0, 1.0);
    const double fracturedArea01 = std::min(
        previous.intactArea01,
        localDamage * exposedFraction *
            std::max(1.0, material.fragmentationAreaFactor)
    );
    previous.intactArea01 -= fracturedArea01;
    previous.microcrack01 = std::clamp(
        previous.microcrack01 + exposedFraction *
            std::min(1.0, 0.35 * localFailureRatio + 0.1 * shockRatio),
        0.0, 1.0);
    return previous;
}

} // namespace game::physics::materials
