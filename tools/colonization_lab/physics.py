"""Order-of-magnitude engineering of a generation ship (deterministic, no RNG).

The point is not precision. The point is to show where an authored ship is
"hard but possible under our assumptions" and where the authored numbers are
off by an order of magnitude. Classical rocket equation is adequate below 0.2c.

Canonical technology assumptions (see Notes/History/COLONIZATION_CANON_3126.md):
  * 2055: compact fusion reactor -> cheap ENERGY, not cheap MASS.
  * propulsion: direct-fusion D-He3 drive with magnetic nozzle; aneutronic
    branch is what keeps the radiator problem bounded (waste fraction ~0.3 %).
  * magnetic-sail braking against the interstellar medium (from the first
    wave): removes most of the braking delta-v, final ~0.015c done by rocket.
  * reaction mass = fusion fuel + inert water; the energy balance fixes the
    fuel fraction of the propellant.
"""
import math

C = 2.998e8
G0 = 9.81
YEAR = 3.156e7
SIGMA = 5.67e-8
DHE3_J_PER_KG = 3.5e14       # D-He3 specific yield
HE3_MASS_SHARE = 0.6         # He3 share of a stoichiometric D-He3 fuel mix
HE3_IN_GIANT_ATMO = 2.3e-5   # accessible He3 mass fraction (generated catalog, GJ 876 b)

ERA = {  # exhaust velocity [c], jet efficiency, waste-heat fraction, magsail
    "I":   dict(ve=0.040, eta=0.80, waste=0.004, magsail_floor=0.015),
    "II":  dict(ve=0.050, eta=0.82, waste=0.003, magsail_floor=0.015),
    "III": dict(ve=0.060, eta=0.85, waste=0.002, magsail_floor=0.012),
}

# bottom-up mass coefficients [t]
T_PER_AWAKE = 250.0      # rotating habitat + GCR shielding (~5 t/m2) + life support + spares
T_PER_CRYO = 5.0         # pod, vault, share of shielding and power
T_PER_COLONIST_KIT = 120.0  # landing craft, seed industry, stores, per colonist
T_FIXED = 1.5e6          # spine, frontal dust shield (tens of m ice), reactors, coast radiators


def era_of(launch_year):
    return "I" if launch_year < 2150 else ("II" if launch_year < 2250 else "III")


def dry_mass_t(pop, cryo_share, awake_override=None):
    awake = awake_override if awake_override is not None else pop * (1.0 - cryo_share)
    cryo = pop - awake
    return T_FIXED + awake * T_PER_AWAKE + cryo * T_PER_CRYO + pop * T_PER_COLONIST_KIT


def design(pop, cryo_share, cruise_c, launch_year, accel_years=12.0, awake_override=None,
           magsail=True, claimed_full_mt=None, claimed_dry_mt=None, dry_override_mt=None):
    e = ERA[era_of(launch_year)]
    dry = dry_override_mt * 1e6 if dry_override_mt else dry_mass_t(pop, cryo_share, awake_override)
    brake = e["magsail_floor"] if magsail else cruise_c
    dv = cruise_c + brake
    ratio = math.exp(dv / e["ve"])
    full = dry * ratio
    prop = full - dry
    # energy balance: specific jet energy / (eta * yield) = fuel fraction of propellant
    ve = e["ve"] * C
    fuel_frac = min(1.0, 0.5 * ve * ve / (e["eta"] * DHE3_J_PER_KG))
    fuel_t = prop * fuel_frac
    he3_t = fuel_t * HE3_MASS_SHARE
    atmo_t = he3_t / HE3_IN_GIANT_ATMO
    # boost phase: mean mass ~ geometric mean, constant thrust phase of accel_years
    a = cruise_c * C / (accel_years * YEAR)
    m_mean = math.sqrt(full * (dry * math.exp(brake / e["ve"]))) * 1e3
    p_jet = 0.5 * m_mean * a * ve
    p_waste = p_jet * e["waste"] / (1.0 - e["waste"])
    rad_km2 = p_waste / (SIGMA * 1800.0 ** 4) / 1e6     # liquid-droplet radiator at 1800 K
    coast_power_w = 1.2e4 * (pop * (1.0 - cryo_share) if awake_override is None else awake_override) + 2e3 * pop + 3e8
    coast_rad_km2 = coast_power_w / (SIGMA * 350.0 ** 4) / 1e6
    out = dict(era=era_of(launch_year), dry_mt=dry / 1e6, full_mt=full / 1e6, mass_ratio=ratio, dv_c=dv,
               he3_mt=he3_t / 1e6, giant_atmo_processed_t=atmo_t, accel_g=a / G0, jet_power_pw=p_jet / 1e15,
               boost_radiator_km2=rad_km2, coast_power_gw=coast_power_w / 1e9, coast_radiator_km2=coast_rad_km2)
    if claimed_full_mt:
        out["claimed_full_mt"] = claimed_full_mt
        out["full_vs_claim"] = out["full_mt"] / claimed_full_mt
    if claimed_dry_mt:
        out["claimed_dry_mt"] = claimed_dry_mt
        out["dry_vs_claim"] = out["dry_mt"] / claimed_dry_mt
    return out


def verdict(d):
    """'possible' if the bottom-up ship fits in the authored envelope within x3,
    'fix' otherwise (with the dominant reason)."""
    reasons = []
    if d.get("full_vs_claim") and not (1 / 3 <= d["full_vs_claim"] <= 3):
        reasons.append(f"полная масса {d['full_mt']:.0f} Мт против заявленных {d['claimed_full_mt']:.0f} Мт")
    if d.get("dry_vs_claim") and not (1 / 3 <= d["dry_vs_claim"] <= 3):
        reasons.append(f"сухая масса {d['dry_mt']:.1f} Мт против заявленных {d['claimed_dry_mt']:.0f} Мт")
    if d["mass_ratio"] > 60:
        reasons.append(f"отношение масс {d['mass_ratio']:.0f} — за пределом одной ступени")
    if d["he3_mt"] > 300:
        reasons.append(f"He-3 {d['he3_mt']:.0f} Мт — больше всей вероятной добычи Сола за десятилетия")
    return ("fix", reasons) if reasons else ("possible", [])
