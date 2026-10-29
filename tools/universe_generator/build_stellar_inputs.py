"""Build the generator INPUT directories of every catalog star system.

usage: python build_stellar_inputs.py LEGACY_ROOT OUT_ROOT --positions real|legacy

LEGACY_ROOT  legacy galaxy_details (systems_details/, distant_systems_details/)
OUT_ROOT     galaxy_details; writes systems/<NNN>_<slug>/ and
             distant_systems/<id>_<slug>/ with system.json, stars/, observed/

What is preserved from the legacy catalog: system id, name, alternative
names, catalog scope / visibility / quest flags, the stellar components and
their identity. Legacy planets, moons, rings and belts are NOT inputs.

Stellar values, by priority: NASA Exoplanet Archive host parameters (pinned
snapshot) > the literature table below > legacy values (marked unverified).
Legacy radii are not used: they are derived from L and Teff (many legacy
diameters contradict the luminosity).

Observational anchors: only confirmed, non-controversial planets of the
pinned archive snapshot, matched to systems by real sky position. Sol is an
authored observed system (J2000 elements, Standish 1992 / JPL; physical
values: NASA planetary fact sheets) with its legacy names, so body ids such as
"system_0.Sol.Земля" and the Sol visuals stay valid.
"""
import argparse
import csv
import glob
import json
import math
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import resolve_stellar_positions as rsp  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ARCHIVE = os.path.join(HERE, "reference", "nasa_exoplanet_archive_pscomppars_2026-10-29.csv")
HYG = os.path.join(HERE, "reference", "hyg_v41_catalog_stars_subset.csv")
ARCHIVE_SRC = "NASA Exoplanet Archive pscomppars, retrieved 2026-10-29"
LIT = "literature compilation 2026-10-29"
EARTH_KG = 5.9722e24
EARTH_R_KM = 6378.1  # IAU nominal equatorial, the generator unit
LY_PER_PC = 3.261563777

# --------------------------------------------------------------------------
# Stellar literature table: (value, reference). Only where the legacy value is
# wrong or missing and the archive has no host entry.
STAR_LITERATURE = {
    "sol": dict(mass_solar=(1.0, "IAU 2015 nominal"), radius_solar=(1.0, "IAU 2015 nominal"),
                teff_k=(5772, "IAU 2015 nominal"), luminosity_solar=(1.0, "IAU 2015 nominal"),
                metallicity_feh=(0.0, "definition"), age_gyr=(4.57, "Bouvier & Wadhwa 2010"),
                rotation_period_days=(25.38, "Carrington")),
    "alpha_centauri_a": dict(mass_solar=(1.0788, "Akeson et al. 2021"), radius_solar=(1.2175, "Akeson et al. 2021"),
                             teff_k=(5790, "Morel 2018"), luminosity_solar=(1.5059, "Akeson et al. 2021"),
                             metallicity_feh=(0.20, "Morel 2018"), age_gyr=(5.3, "Joyce & Chaboyer 2018")),
    "alpha_centauri_b": dict(mass_solar=(0.9092, "Akeson et al. 2021"), radius_solar=(0.8591, "Akeson et al. 2021"),
                             teff_k=(5260, "Morel 2018"), luminosity_solar=(0.4981, "Akeson et al. 2021"),
                             metallicity_feh=(0.23, "Morel 2018"), age_gyr=(5.3, "Joyce & Chaboyer 2018")),
    "proxima_centauri": dict(age_gyr=(4.85, "coeval with alpha Cen AB")),
    "barnards_star": dict(age_gyr=(10.0, "Ribas et al. 2018")),
    "wolf_359": dict(mass_solar=(0.110, "Pineda et al. 2021"), radius_solar=(0.144, "Pineda et al. 2021"),
                     teff_k=(2749, "Pineda et al. 2021"), luminosity_solar=(0.00106, "Pineda et al. 2021"),
                     metallicity_feh=(0.25, "Mann et al. 2015"), age_gyr=(0.5, "young flare star, 0.1-1.5 Gyr")),
    "lalande_21185": dict(mass_solar=(0.389, "Mann et al. 2015"), radius_solar=(0.392, "Mann et al. 2015"),
                          teff_k=(3547, "Mann et al. 2015"), luminosity_solar=(0.0195, "Mann et al. 2015"),
                          metallicity_feh=(-0.20, "Mann et al. 2015"), age_gyr=(8.0, "kinematic estimate")),
    "sirius_a": dict(mass_solar=(2.063, "Bond et al. 2017"), radius_solar=(1.711, "Davis et al. 2011"),
                     teff_k=(9940, "Adelman 2004"), luminosity_solar=(25.4, "Liebert et al. 2005"),
                     metallicity_feh=(0.0, "bulk (surface Am overabundance not bulk)"),
                     age_gyr=(0.242, "Bond et al. 2017")),
    "sirius_b": dict(mass_solar=(1.018, "Bond et al. 2017"), radius_solar=(0.0084, "Barstow et al. 2005"),
                     teff_k=(25000, "Barstow et al. 2005"), age_gyr=(0.242, "Bond et al. 2017")),
    "kapteyns_star": dict(mass_solar=(0.281, "Guinan et al. 2016"), radius_solar=(0.291, "Segransan 2003"),
                           teff_k=(3550, "Guinan et al. 2016"), luminosity_solar=(0.012, "Guinan et al. 2016"),
                           metallicity_feh=(-0.86, "Woolf & Wallerstein 2005"), age_gyr=(11.5, "Wylie-de Boer 2010 (halo)")),
    "procyon_a": dict(mass_solar=(1.478, "Bond et al. 2015"), radius_solar=(2.048, "Kervella et al. 2004"),
                      teff_k=(6530, "Fuhrmann 1997"), luminosity_solar=(6.93, "Liebert et al. 2013"),
                      metallicity_feh=(-0.02, "Allende Prieto 2002"), age_gyr=(1.87, "Liebert et al. 2013")),
    "procyon_b": dict(mass_solar=(0.592, "Bond et al. 2015"), radius_solar=(0.01234, "Provencal 2002"),
                      teff_k=(7740, "Provencal 2002"), luminosity_solar=(0.00049, "Provencal 2002"),
                      age_gyr=(1.87, "Liebert et al. 2013")),
    "61_cygni_a": dict(mass_solar=(0.70, "Kervella et al. 2008"), radius_solar=(0.665, "Kervella et al. 2008"),
                       teff_k=(4526, "Kervella et al. 2008"), luminosity_solar=(0.153, "Kervella et al. 2008"),
                       metallicity_feh=(-0.20, "Luck 2017"), age_gyr=(6.1, "Kervella et al. 2008")),
    "61_cygni_b": dict(mass_solar=(0.63, "Kervella et al. 2008"), radius_solar=(0.595, "Kervella et al. 2008"),
                       teff_k=(4077, "Kervella et al. 2008"), luminosity_solar=(0.085, "Kervella et al. 2008"),
                       metallicity_feh=(-0.27, "Luck 2017"), age_gyr=(6.1, "Kervella et al. 2008")),
    "gliese_229_a": dict(age_gyr=(3.0, "Brandt et al. 2021 (2-6 Gyr)")),
    "gliese_229_b": dict(mass_solar=(0.0692, "Xuan et al. 2024 (Ba 38.1 + Bb 34.4 M_J, one component here)"),
                         teff_k=(900, "T7 spectral type"), age_gyr=(3.0, "Brandt et al. 2021")),
    "epsilon_indi_ba": dict(mass_solar=(0.0639, "Chen et al. 2022 (66.9 M_J)"), teff_k=(1300, "T1"),
                            age_gyr=(3.5, "Chen et al. 2022")),
    "epsilon_indi_bb": dict(mass_solar=(0.0508, "Chen et al. 2022 (53.2 M_J)"), teff_k=(900, "T6"),
                            age_gyr=(3.5, "Chen et al. 2022")),
    "delta_pavonis": dict(mass_solar=(0.99, "Bruntt et al. 2010"), radius_solar=(1.22, "Bruntt et al. 2010"),
                          teff_k=(5604, "Bruntt et al. 2010"), luminosity_solar=(1.22, "Bruntt et al. 2010"),
                          metallicity_feh=(0.33, "Bruntt et al. 2010"), age_gyr=(6.6, "Mamajek & Hillenbrand 2008")),
    "9_ceti": dict(mass_solar=(1.04, "Valenti & Fischer 2005"), teff_k=(5837, "Valenti & Fischer 2005"),
                   luminosity_solar=(1.06, "Valenti & Fischer 2005"), metallicity_feh=(0.21, "Valenti & Fischer 2005"),
                   age_gyr=(0.6, "BE Ceti, young solar analogue")),
    "hamal": dict(age_gyr=(3.9, "giant of 1.5 M_sun past its main sequence")),
    "hd_102194": dict(mass_solar=(1.3, "assumed for a K2 III giant (no measurement)"),
                      teff_k=(4500, "K2 III spectral type"), luminosity_solar=(60.0, "HYG absolute magnitude, K2 III"),
                      age_gyr=(4.0, "giant of ~1.3 M_sun")),
}

# Stellar hierarchy per multiple system: (node tree, orbit table).
# Projected separations are from the real angular separation x distance.
HIERARCHY = {
    1: {"id": "ab", "children": ["alpha_centauri_a", "alpha_centauri_b"],
        "orbit": {"semi_major_axis_au": 23.3, "eccentricity": 0.5208, "period_years": 79.91,
                  "inclination_deg": 79.24, "source": "Akeson et al. 2021 / Pourbaix 2016"}},
    5: {"id": "ab", "children": ["sirius_a", "sirius_b"],
        "orbit": {"semi_major_axis_au": 19.8, "eccentricity": 0.5914, "period_years": 50.13,
                  "inclination_deg": 136.3, "source": "Bond et al. 2017"}},
    52: {"id": "ab", "children": ["procyon_a", "procyon_b"],
         "orbit": {"semi_major_axis_au": 15.0, "eccentricity": 0.407, "period_years": 40.84,
                   "inclination_deg": 31.1, "source": "Bond et al. 2015"}},
    31: {"id": "ab", "children": ["61_cygni_a", "61_cygni_b"],
         "orbit": {"semi_major_axis_au": 85.6, "eccentricity": 0.40, "period_years": 678.0,
                   "inclination_deg": 51.0, "source": "Kervella et al. 2008"}},
    11: {"id": "system_barycenter",
         "children": [{"id": "ab", "children": ["gliese_667_a", "gliese_667_b"],
                       "orbit": {"semi_major_axis_au": 12.6, "eccentricity": 0.58, "period_years": 42.15,
                                 "source": "Soderhjelm 1999"}},
                      "gliese_667_c"],
         "orbit": {"projected_separation_au": 235.0, "source": "32.4 arcsec x 7.24 pc"}},
    29: {"id": "ab", "children": ["gliese_229_a", "gliese_229_b"],
         "orbit": {"semi_major_axis_au": 33.3, "eccentricity": 0.851, "period_years": 237.9,
                   "source": "Brandt et al. 2021"}},
    30: {"id": "system_barycenter",
         "children": ["epsilon_indi_a",
                      {"id": "bab", "children": ["epsilon_indi_ba", "epsilon_indi_bb"],
                       "orbit": {"semi_major_axis_au": 2.405, "eccentricity": 0.54, "period_years": 11.0,
                                 "source": "Chen et al. 2022"}}],
         "orbit": {"projected_separation_au": 1459.0, "source": "402 arcsec x 3.63 pc"}},
    35: {"id": "ab", "children": ["tau_cygni_a", "tau_cygni_b"],
         "orbit": {"period_years": 49.6, "source": "visual orbit (ORB6), unverified here"}},
    38: {"id": "system_barycenter",
         "children": ["sigma2_uma_a", {"id": "bc", "children": ["sigma2_uma_b", "sigma2_uma_c"],
                                       "orbit": {"projected_separation_au": 1.5, "source": "legacy, unverified"}}],
         "orbit": {"projected_separation_au": 48.0, "source": "legacy, unverified"}},
    42: {"id": "ab", "children": ["17_cygni_a", "17_cygni_b"],
         "orbit": {"projected_separation_au": 550.0, "source": "26 arcsec x 21.2 pc, unverified"}},
    43: {"id": "ab", "children": ["theta_draconis_a", "theta_draconis_b"],
         "orbit": {"period_years": 3.0708 / 365.25, "eccentricity": 0.0,
                   "source": "single-lined spectroscopic binary, P = 3.07 d"}},
    44: {"id": "ab", "children": ["hd_33564_a", "hd_33564_b"],
         "orbit": {"projected_separation_au": 85.0, "source": "legacy component, unverified"}},
    45: {"id": "ab", "children": ["83_leonis_a", "83_leonis_b"],
         "orbit": {"projected_separation_au": 507.0, "source": "28.3 arcsec x 17.9 pc (HYG)"}},
    46: {"id": "ab", "children": ["gj_3021_a", "gj_3021_b"],
         "orbit": {"projected_separation_au": 67.0, "source": "3.8 arcsec x 17.6 pc"}},
    48: {"id": "ab", "children": ["hd_102365_a", "hd_102365_b"],
         "orbit": {"projected_separation_au": 211.0, "source": "23 arcsec x 9.2 pc"}},
    57: {"id": "ab", "children": ["hd_10360", "p_eridani_a"],
         "orbit": {"projected_separation_au": 63.0, "source": "8.1 arcsec x 7.8 pc (HYG)"}},
}

# Real companions missing from the legacy catalog.
EXTRA_STARS = {
    57: [dict(name="p Eridani A", alternative_names=["HD 10361", "Gl 66 A"], type="K2V",
              mass_solar=(0.82, "Mamajek 2013 K2V"), teff_k=(5040, "K2V"), luminosity_solar=(0.30, "K2V"))],
}

# Archive host -> component key for multiple systems.
HOST_COMPONENT = {
    "GJ 667 C": "gliese_667_c", "eps Ind A": "epsilon_indi_a", "GJ 229": "gliese_229_a",
    "HD 99492": "83_leonis_b", "HD 1237": "gj_3021_a", "HD 102365": "hd_102365_a",
}

# Transit surveys: (radius limit R_earth, period baseline d, mission).
TRANSIT_SURVEY = {
    100062: (1.0, 1400.0, "Kepler prime mission"),
    17: (0.6, 80.0, "Spitzer / K2 / JWST photometry"),
}

# Survey completeness of the system's real stars (radial velocities).
def survey(sid, stars):
    stages = {s.get("stage_hint") for s in stars}
    if sid == 0:
        return None
    if stages & {"giant"}:
        return {"rv_detection_limit_ms": 20.0, "rv_baseline_days": 3650.0,
                "source": "giant-star RV jitter limits surveys to ~20 m/s"}
    if sid == 100062:
        return {"rv_detection_limit_ms": 5.0, "rv_baseline_days": 1500.0,
                "source": "Kepler transit host, sparse RV follow-up"}
    return None  # filled from distance below


SOL_BODIES = [
    # designation, alt, a AU, e, i, Omega, varpi, L, mass Earth, radius km, rot h, obliquity, albedo, P Pa, T K,
    # water, iron, envelope, atmosphere
    ("Меркурий", ["Mercury"], 0.38709927, 0.20563593, 7.00497902, 48.33076593, 77.45779628, 252.25032350,
     0.0553, 2439.7, 1407.6, 0.034, 0.088, 0.0, 440.0, 0.0, 0.70, None, {}),
    ("Венера", ["Venus"], 0.72333566, 0.00677672, 3.39467605, 76.67984255, 131.60246718, 181.97909950,
     0.815, 6051.8, 5832.5, 177.36, 0.76, 9.2e6, 737.0, 0.0, 0.30, None, {"CO2": 0.965, "N2": 0.035}),
    ("Земля", ["Earth"], 1.00000261, 0.01671123, 0.0, 0.0, 102.93768193, 100.46457166,
     1.0, 6371.0, 23.9345, 23.44, 0.306, 101325.0, 288.0, 0.00023, 0.325, None,
     {"N2": 0.7808, "O2": 0.2095, "Ar": 0.0093, "CO2": 0.0004}),
    ("Марс", ["Mars"], 1.52371034, 0.09339410, 1.84969142, 49.55953891, 336.05637041, 355.44656795,
     0.107, 3389.5, 24.6229, 25.19, 0.25, 610.0, 210.0, 1.0e-5, 0.24, None,
     {"CO2": 0.9532, "N2": 0.027, "Ar": 0.016, "O2": 0.0013, "CO": 0.0008}),
    ("Юпитер", ["Jupiter"], 5.20288700, 0.04838624, 1.30439695, 100.47390909, 14.72847983, 34.39644051,
     317.83, 69911.0, 9.925, 3.13, 0.343, 1.0e5, 165.0, 0.0, None, 0.90, {"H2": 0.898, "He": 0.102, "CH4": 0.003}),
    ("Сатурн", ["Saturn"], 9.53667594, 0.05386179, 2.48599187, 113.66242448, 92.59887831, 49.95424423,
     95.16, 58232.0, 10.656, 26.73, 0.342, 1.0e5, 134.0, 0.0, None, 0.75, {"H2": 0.963, "He": 0.0325, "CH4": 0.0045}),
    ("Уран", ["Uranus"], 19.18916464, 0.04725744, 0.77263783, 74.01692503, 170.95427630, 313.23810451,
     14.54, 25362.0, 17.24, 97.77, 0.30, 1.0e5, 76.0, 0.6, None, 0.15, {"H2": 0.825, "He": 0.152, "CH4": 0.023}),
    ("Нептун", ["Neptune"], 30.06992276, 0.00859048, 1.77004347, 131.78422574, 44.96476227, 304.87997031,
     17.15, 24622.0, 16.11, 28.32, 0.29, 1.0e5, 72.0, 0.6, None, 0.10, {"H2": 0.80, "He": 0.19, "CH4": 0.015}),
]

# moon designation, parent, a km, e, i deg (to the planet equator / Laplace plane), mass kg, radius km,
# water fraction, retrograde, pressure Pa, T K, atmosphere
SOL_MOONS = [
    ("Луна", "Земля", 384399, 0.0549, 5.145, 7.342e22, 1737.4, 0.0, False, 0.0, 250.0, {}),
    ("Фобос", "Марс", 9376, 0.0151, 1.093, 1.0659e16, 11.27, 0.0, False, 0.0, 233.0, {}),
    ("Деймос", "Марс", 23463, 0.00033, 0.93, 1.4762e15, 6.2, 0.0, False, 0.0, 233.0, {}),
    ("Амальтея", "Юпитер", 181366, 0.0032, 0.374, 2.08e18, 83.5, 0.05, False, 0.0, 120.0, {}),
    ("Ио", "Юпитер", 421700, 0.0041, 0.05, 8.931938e22, 1821.6, 0.0, False, 0.0, 110.0, {}),
    ("Европа", "Юпитер", 671034, 0.009, 0.47, 4.799844e22, 1560.8, 0.08, False, 0.0, 102.0, {}),
    ("Ганимед", "Юпитер", 1070412, 0.0013, 0.2, 1.4819e23, 2634.1, 0.45, False, 0.0, 110.0, {}),
    ("Каллисто", "Юпитер", 1882709, 0.0074, 0.192, 1.075938e23, 2410.3, 0.5, False, 0.0, 134.0, {}),
    ("Гималия", "Юпитер", 11461000, 0.16, 27.5, 4.2e18, 69.8, 0.1, False, 0.0, 124.0, {}),
    ("Элара", "Юпитер", 11741000, 0.217, 26.6, 8.7e17, 43.0, 0.1, False, 0.0, 124.0, {}),
    ("Пасифе", "Юпитер", 23624000, 0.409, 151.4, 3.0e17, 30.0, 0.1, True, 0.0, 124.0, {}),
    ("Мимас", "Сатурн", 185539, 0.0196, 1.574, 3.7493e19, 198.2, 0.7, False, 0.0, 64.0, {}),
    ("Энцелад", "Сатурн", 237948, 0.0047, 0.009, 1.08022e20, 252.1, 0.4, False, 0.0, 75.0, {}),
    ("Тетис", "Сатурн", 294619, 0.0001, 1.12, 6.17449e20, 531.1, 0.9, False, 0.0, 86.0, {}),
    ("Диона", "Сатурн", 377396, 0.0022, 0.019, 1.095452e21, 561.4, 0.5, False, 0.0, 87.0, {}),
    ("Рея", "Сатурн", 527108, 0.001258, 0.345, 2.306518e21, 763.8, 0.75, False, 0.0, 76.0, {}),
    ("Титан", "Сатурн", 1221870, 0.0288, 0.34854, 1.3452e23, 2574.73, 0.45, False, 146700.0, 94.0,
     {"N2": 0.984, "CH4": 0.014, "H2": 0.002}),
    ("Япет", "Сатурн", 3560820, 0.0286, 15.47, 1.805635e21, 734.5, 0.75, False, 0.0, 110.0, {}),
    ("Феба", "Сатурн", 12947780, 0.1562, 175.3, 8.292e18, 106.5, 0.3, True, 0.0, 73.0, {}),
    ("Миранда", "Уран", 129390, 0.0013, 4.232, 6.59e19, 235.8, 0.5, False, 0.0, 59.0, {}),
    ("Ариэль", "Уран", 191020, 0.0012, 0.26, 1.251e21, 578.9, 0.4, False, 0.0, 60.0, {}),
    ("Умбриэль", "Уран", 266000, 0.0039, 0.128, 1.275e21, 584.7, 0.4, False, 0.0, 75.0, {}),
    ("Титания", "Уран", 435910, 0.0011, 0.34, 3.4e21, 788.4, 0.45, False, 0.0, 70.0, {}),
    ("Оберон", "Уран", 583520, 0.0014, 0.058, 3.076e21, 761.4, 0.45, False, 0.0, 75.0, {}),
    ("Ларисса", "Нептун", 73548, 0.0014, 0.205, 4.2e18, 97.0, 0.3, False, 0.0, 51.0, {}),
    ("Протей", "Нептун", 117647, 0.0005, 0.075, 4.4e19, 210.0, 0.3, False, 0.0, 51.0, {}),
    ("Тритон", "Нептун", 354759, 0.000016, 156.885, 2.139e22, 1353.4, 0.25, True, 1.4, 38.0,
     {"N2": 0.9998, "CH4": 0.0002}),
    ("Нереида", "Нептун", 5513818, 0.7507, 7.23, 3.1e19, 170.0, 0.3, False, 0.0, 50.0, {}),
]

SOL_MOON_ENGLISH = {
    "Луна": "Moon", "Фобос": "Phobos", "Деймос": "Deimos", "Амальтея": "Amalthea", "Ио": "Io",
    "Европа": "Europa", "Ганимед": "Ganymede", "Каллисто": "Callisto", "Гималия": "Himalia",
    "Элара": "Elara", "Пасифе": "Pasiphae", "Мимас": "Mimas", "Энцелад": "Enceladus", "Тетис": "Tethys",
    "Диона": "Dione", "Рея": "Rhea", "Титан": "Titan", "Япет": "Iapetus", "Феба": "Phoebe",
    "Миранда": "Miranda", "Ариэль": "Ariel", "Умбриэль": "Umbriel", "Титания": "Titania",
    "Оберон": "Oberon", "Ларисса": "Larissa", "Протей": "Proteus", "Тритон": "Triton", "Нереида": "Nereid",
}

# J2000.0 (2000-01-01 12:00 TT) on the UniverseClock (real epoch 1998-12-21 00:00).
J2000_UNIVERSE_SECONDS = 376.5 * 86400.0

LEGACY_VISUAL_KEYS = ("environment_preset_id", "axis_node_deg", "rotation_offset_deg",
                      "texture_longitude_offset_deg", "orbit_phase_offset_deg", "ring_system",
                      "rotation_direction", "orbit_direction")


def slug(s):
    s = s.replace("²", "2").replace("'", "")
    out = re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")
    return out


def value(v, src):
    return {"value": v, "source": src}


def stage_hint(sp):
    if not sp:
        return ""
    if sp.startswith("D"):
        return "white_dwarf"
    if sp[0] in "LTY":
        return "brown_dwarf"
    if "III" in sp or ("II" in sp and "IV" not in sp):
        return "giant"
    if "IV" in sp and "IV-V" not in sp and "IV/V" not in sp:
        return "subgiant"
    return "main_sequence"


def archive_hosts():
    rows = list(csv.DictReader(open(ARCHIVE, encoding="utf-8")))
    hosts = {}
    for r in rows:
        hosts.setdefault(r["hostname"], []).append(r)
    return hosts


def fnum(r, k):
    try:
        return float(r[k]) if r.get(k, "") != "" else None
    except ValueError:
        return None


def limit_flag(r, k):
    """Archive *lim columns: 0 = measurement, +-1 = limit (numbers formatted freely)."""
    try:
        return float(r.get(k) or 0.0) != 0.0
    except ValueError:
        return False


def host_for_position(hosts, pos):
    u = pos
    d = math.sqrt(sum(c * c for c in u))
    found = []
    for h, rows in hosts.items():
        r = rows[0]
        if not (r["ra"] and r["dec"] and r["sy_dist"]):
            continue
        hv = rsp.icrs_ly(float(r["ra"]), float(r["dec"]), float(r["sy_dist"]) * LY_PER_PC)
        hd = math.sqrt(sum(c * c for c in hv))
        cosang = sum(a * b for a, b in zip(u, hv)) / (d * hd)
        ang = math.degrees(math.acos(max(-1.0, min(1.0, cosang))))
        if ang < 0.2 and abs(hd - d) / d < 0.1:
            found.append(h)
    return found


def build_star(legacy, sid, archive_row):
    name = legacy["name"]
    key = slug(name)
    sp = legacy.get("type", "").split(" — ")[0].strip()
    st = {"schema": "elite.star.v2", "key": key, "name": name,
          "alternative_names": legacy.get("alternative_names", []), "spectral_type": sp,
          "stage_hint": stage_hint(sp)}
    lit = STAR_LITERATURE.get(key, {})
    if archive_row is not None:
        a = archive_row
        for field, col, conv in (("mass_solar", "st_mass", None), ("radius_solar", "st_rad", None),
                                 ("teff_k", "st_teff", None), ("luminosity_solar", "st_lum", lambda x: 10 ** x),
                                 ("metallicity_feh", "st_met", None), ("age_gyr", "st_age", None)):
            v = fnum(a, col)
            if v is not None and field not in lit:
                st[field] = value(conv(v) if conv else v, ARCHIVE_SRC + " (host " + a["hostname"] + ")")
        if a.get("st_spectype"):
            st["spectral_type"] = a["st_spectype"]
    for field, (v, ref) in lit.items():
        st[field] = value(v, LIT + ": " + ref)
    # Legacy values only where nothing better exists (never the radius).
    for field, lk in (("mass_solar", "mass_solar"), ("luminosity_solar", "luminosity_solar"),
                      ("teff_k", "temperature_k"), ("age_gyr", "age_gyr")):
        if field not in st and legacy.get(lk) is not None:
            st[field] = value(legacy[lk], "legacy catalog, unverified")
    st["stage_hint"] = stage_hint(st["spectral_type"]) or st["stage_hint"]
    lore = {k: legacy[k] for k in ("type",) if k in legacy}
    if lore:
        st["extra"] = {"legacy_type_label": legacy["type"]}
    return st


def node_json(spec):
    if isinstance(spec, str):
        return {"star": spec}
    out = {"id": spec["id"], "children": [node_json(c) for c in spec["children"]]}
    if "orbit" in spec:
        o = {}
        for k, v in spec["orbit"].items():
            o[k] = v if k == "source" else value(v, spec["orbit"]["source"])
        out["orbit"] = o
    return out


def observed_from_archive(rows, host_key, sid):
    out = []
    for r in rows:
        if r.get("pl_controv_flag") == "1":
            continue
        name = r["pl_name"]
        o = {"schema": "elite.observed_body.v2", "designation": name, "kind": "planet", "host": host_key,
             "source": f"{ARCHIVE_SRC}; discovery {r['discoverymethod']} {r['disc_year']}",
             "orbit": {}, "physical": {}}
        for k, col in (("period_days", "pl_orbper"), ("semi_major_axis_au", "pl_orbsmax"),
                       ("eccentricity", "pl_orbeccen")):
            v = fnum(r, col)
            if k == "eccentricity" and limit_flag(r, "pl_orbeccenlim"):
                continue  # an upper limit is not a measured eccentricity
            if v is not None:
                o["orbit"][k] = value(v, ARCHIVE_SRC)
        m = fnum(r, "pl_bmasse")
        if m is not None and limit_flag(r, "pl_bmasselim"):
            o["extra"] = {"mass_limit_earth": m, "mass_limit_flag": r.get("pl_bmasselim")}
            m = None  # a limit, not a mass: the generator derives the mass from the radius
        if m is None:
            ms = fnum(r, "pl_msinie")
            if ms is not None and not limit_flag(r, "pl_msinielim") and r["discoverymethod"] != "Transit":
                o["physical"]["minimum_mass_earth"] = value(ms, ARCHIVE_SRC + " (pl_msinie, m sin i)")
        if m is not None:
            if r.get("pl_bmassprov") == "Msini":
                o["physical"]["minimum_mass_earth"] = value(m, ARCHIVE_SRC + " (m sin i)")
            else:
                o["physical"]["mass_earth"] = value(m, ARCHIVE_SRC + f" ({r.get('pl_bmassprov')})")
        rad = fnum(r, "pl_rade")
        if rad is not None and r["discoverymethod"] == "Transit" and not limit_flag(r, "pl_radelim"):
            o["physical"]["radius_earth"] = value(rad, ARCHIVE_SRC + " (transit radius)")
        out.append(o)
    return out


def sol_observed(legacy_sol):
    legacy = {p["name"]: p for p in legacy_sol["details"]["stars"][0]["planets"]}
    legacy_moons = {m["name"]: m for p in legacy.values() for m in p.get("moons", [])}
    out = []
    src = "authored observed Solar System: JPL J2000 mean elements (Standish), NASA planetary fact sheets"
    for (name, alt, a, e, i, node, varpi, lon, mass, rkm, rot, obl, alb, pres, temp, water, iron, env,
         atm) in SOL_BODIES:
        lp = legacy.get(name, {})
        o = {"schema": "elite.observed_body.v2", "designation": name,
             "alternative_names": lp.get("alternative_names", alt), "kind": "planet", "host": "sol",
             "source": src,
             "orbit": {"semi_major_axis_au": a, "eccentricity": e, "inclination_deg": i,
                       "longitude_ascending_node_deg": node,
                       "argument_of_periapsis_deg": (varpi - node) % 360.0,
                       "mean_anomaly_at_epoch_deg": (lon - varpi) % 360.0,
                       "epoch_universe_seconds": J2000_UNIVERSE_SECONDS},
             "physical": {"mass_earth": mass, "radius_earth": rkm / EARTH_R_KM, "rotation_period_hours": rot,
                          "obliquity_deg": obl, "bond_albedo": alb, "surface_pressure_pa": pres,
                          "mean_surface_temperature_k": temp, "water_mass_fraction": water}}
        if iron is not None:
            o["physical"]["iron_core_fraction"] = iron
        if env is not None:
            o["physical"]["envelope_mass_fraction"] = env
        if atm:
            o["atmosphere_composition"] = atm
        rs = lp.get("ring_system")
        if rs and rs.get("bands"):
            o["rings"] = [{"name": b["name"], "inner_radius_km": b["distance_from_planet_km"]["inner"],
                           "outer_radius_km": b["distance_from_planet_km"]["outer"],
                           "optical_depth": b.get("render", {}).get("optical_depth", 0.01),
                           "ice_fraction": 0.95 if name == "Сатурн" else 0.3,
                           "composition": b.get("composition", "")} for b in rs["bands"]]
        extra = {k: lp[k] for k in LEGACY_VISUAL_KEYS if k in lp}
        if extra:
            o["extra"] = extra
        out.append(o)
    for (name, parent, akm, e, i, mkg, rkm, water, retro, pres, temp, atm) in SOL_MOONS:
        lm = legacy_moons.get(name, {})
        alt = lm.get("alternative_names") or []
        if SOL_MOON_ENGLISH[name] not in alt:
            alt = [SOL_MOON_ENGLISH[name]] + alt
        o = {"schema": "elite.observed_body.v2", "designation": name,
             "alternative_names": alt, "kind": "moon", "host": parent,
             "source": src + "; JPL satellite mean elements",
             "orbit": {"semi_major_axis_au": akm * 1000.0 / 149597870700.0, "eccentricity": e,
                       "inclination_deg": i, "retrograde": retro},
             "physical": {"mass_earth": mkg / EARTH_KG, "radius_earth": rkm / EARTH_R_KM,
                          "water_mass_fraction": water, "mean_surface_temperature_k": temp,
                          "surface_pressure_pa": pres, "tidally_locked": True}}
        if atm:
            o["atmosphere_composition"] = atm
        extra = {k: lm[k] for k in LEGACY_VISUAL_KEYS if k in lm}
        if extra:
            o["extra"] = extra
        out.append(o)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("legacy_root")
    ap.add_argument("out_root")
    ap.add_argument("--positions", choices=["real", "legacy"], required=True)
    args = ap.parse_args()

    resolved = {r["id"]: r for r in rsp.resolve(args.legacy_root, HYG, ARCHIVE)}
    hosts = archive_hosts()
    report = []

    for sub, outsub in (("systems_details", "systems"), ("distant_systems_details", "distant_systems")):
        target_root = os.path.join(args.out_root, outsub)
        os.makedirs(target_root, exist_ok=True)
        for path in sorted(glob.glob(os.path.join(args.legacy_root, sub, "*.json"))):
            legacy = json.load(open(path, encoding="utf-8"))
            sid = legacy["id"]
            name = legacy["name"]
            dname = f"{sid:03d}_{slug(name.split(' (')[0])}" if sid < 100000 else f"{sid}_{slug(name)}"
            ddir = os.path.join(target_root, dname)
            if os.path.isdir(ddir):
                for k in ("system.json", "stars", "observed"):
                    p = os.path.join(ddir, k)
                    if os.path.isdir(p):
                        shutil.rmtree(p)
                    elif os.path.exists(p):
                        os.remove(p)
            os.makedirs(os.path.join(ddir, "stars"), exist_ok=True)

            res = resolved.get(sid, {})
            real = (float(res["x"]), float(res["y"]), float(res["z"])) if res.get("x") not in (None, "") else None
            lp = legacy["position_ly"]
            legacy_pos = (lp["x"], lp["y"], lp["z"])
            pos = real if (args.positions == "real" and real is not None) else legacy_pos
            pos_src = (f"{res.get('source')} {res.get('key')} (ICRS J2000)" if pos is real
                       else "legacy catalog position")

            # Archive hosts in this system (matched by the real position).
            sys_hosts = host_for_position(hosts, real) if real is not None and any(real) else []
            legacy_stars = list(legacy["details"]["stars"])
            for extra in EXTRA_STARS.get(sid, []):
                legacy_stars.append(dict(name=extra["name"], alternative_names=extra["alternative_names"],
                                         type=extra["type"]))
            star_docs = []
            for ls in legacy_stars:
                key = slug(ls["name"])
                arow = None
                for h in sys_hosts:
                    comp = HOST_COMPONENT.get(h)
                    if comp == key or (comp is None and ls is legacy_stars[0]):
                        arow = hosts[h][0]
                doc = build_star(ls, sid, arow)
                for extra in EXTRA_STARS.get(sid, []):
                    if extra["name"] == ls["name"]:
                        for f in ("mass_solar", "teff_k", "luminosity_solar"):
                            doc[f] = value(extra[f][0], LIT + ": " + extra[f][1])
                star_docs.append(doc)
            for doc in star_docs:
                json.dump(doc, open(os.path.join(ddir, "stars", doc["key"] + ".json"), "w", encoding="utf-8",
                                    newline="\n"), ensure_ascii=False, indent=2)

            hier = HIERARCHY.get(sid)
            if hier is None:
                if len(star_docs) != 1:
                    raise SystemExit(f"system {sid} {name}: {len(star_docs)} stars without a hierarchy entry")
                hierarchy = {"star": star_docs[0]["key"]}
            else:
                hierarchy = node_json(hier)
                keys = set()
                stack = [hierarchy]
                while stack:
                    n = stack.pop()
                    if "star" in n:
                        keys.add(n["star"])
                    stack.extend(n.get("children", []))
                missing = {d["key"] for d in star_docs} ^ keys
                if missing:
                    raise SystemExit(f"system {sid}: hierarchy/star mismatch {missing}")

            sysdoc = {"schema": "elite.star_system.v2", "id": sid, "name": name, "slug": dname.split("_", 1)[1],
                      "alternative_names": legacy["details"].get("alternative_names", []),
                      "catalog_scope": legacy.get("catalog_scope", "local"),
                      "atlas_visible": legacy.get("atlas_visible", True),
                      "route_target": legacy.get("route_target", True)}
            if legacy.get("quest_role"):
                sysdoc["quest_role"] = legacy["quest_role"]
            sysdoc["position_ly"] = {"x": pos[0], "y": pos[1], "z": pos[2]}
            sysdoc["position_source"] = pos_src
            sysdoc["legacy_position_ly"] = {"x": legacy_pos[0], "y": legacy_pos[1], "z": legacy_pos[2]}
            sysdoc["stellar_hierarchy"] = hierarchy
            if sid == 0:
                sysdoc["observed_system_complete"] = True
                sysdoc["observed_reservoirs"] = {
                    "asteroid_belt": {"name": "Пояс астероидов", "inner_au": 2.06, "outer_au": 3.27,
                                      "total_mass_earth": 4.5e-4,
                                      "source": "Pitjeva & Pitjev 2018"},
                    "kuiper_belt": {"name": "Пояс Койпера", "inner_au": 30.0, "outer_au": 50.0,
                                    "total_mass_earth": 0.02,
                                    "source": "Pitjeva & Pitjev 2018 (classical + resonant belt)"},
                    "comet_cloud": {"name": "Облако Оорта", "inner_au": 2000.0, "outer_au": 100000.0,
                                    "total_mass_earth": 2.0,
                                    "source": "Oort cloud estimate, order of magnitude"},
                }
            sv = survey(sid, star_docs)
            if sv is None and sid != 0:
                d_ly = math.sqrt(sum(c * c for c in (real or legacy_pos)))
                remnant = all(s["stage_hint"] in ("white_dwarf", "brown_dwarf") for s in star_docs)
                if not remnant:
                    sv = ({"rv_detection_limit_ms": 1.5, "rv_baseline_days": 7300.0,
                           "source": "nearby star in long-baseline precision RV surveys"} if d_ly < 50
                          else {"rv_detection_limit_ms": 5.0, "rv_baseline_days": 3650.0,
                                "source": "RV surveys of moderate precision"})
            # Transit-discovered planets mean an edge-on system: coplanar planets
            # above the survey's radius limit would have transited too.
            transit = any(r["discoverymethod"] == "Transit" and r.get("pl_controv_flag") != "1"
                          for h in sys_hosts for r in hosts[h])
            if transit:
                tsv = TRANSIT_SURVEY.get(sid, (1.5, 27.0, "TESS sector photometry"))
                sv = dict(sv or {"source": "transit survey"})
                sv["transit_radius_limit_earth"] = tsv[0]
                sv["transit_baseline_days"] = tsv[1]
                sv["source"] = sv["source"] + "; transits: " + tsv[2]
            if sv:
                sysdoc["survey_completeness"] = {k: (v if k == "source" else value(v, sv["source"]))
                                                 for k, v in sv.items()}
            # Every catalogued designation of the system (also controversial or
            # refuted planets) stays reserved: procedural planets never reuse it.
            reserved = sorted({r["pl_name"] for h in sys_hosts for r in hosts[h]})
            if reserved:
                sysdoc["reserved_designations"] = reserved
            if legacy.get("observational_reference_2026"):
                sysdoc["observational_reference"] = legacy["observational_reference_2026"]
            if legacy.get("stability"):
                sysdoc["lore"] = {"stellar_description": legacy["stability"]}
            json.dump(sysdoc, open(os.path.join(ddir, "system.json"), "w", encoding="utf-8", newline="\n"),
                      ensure_ascii=False, indent=2)

            observed = []
            if sid == 0:
                observed = sol_observed(legacy)
            else:
                for h in sys_hosts:
                    comp = HOST_COMPONENT.get(h, star_docs[0]["key"])
                    observed += observed_from_archive(hosts[h], comp, sid)
            if observed:
                os.makedirs(os.path.join(ddir, "observed"), exist_ok=True)
                used = set()
                for o in observed:
                    base = slug(o["designation"]) or (slug(o["alternative_names"][0]) if o.get("alternative_names") else "")
                    if not base.isascii() or not base:
                        base = slug(o["alternative_names"][0])
                    if not base:
                        raise SystemExit(f"system {sid}: no ASCII file name for {o['designation']}")
                    fn = o["kind"] + "_" + base
                    if fn in used:
                        raise SystemExit(f"system {sid}: duplicate observed file {fn}")
                    used.add(fn)
                    json.dump(o, open(os.path.join(ddir, "observed", fn + ".json"), "w", encoding="utf-8",
                                      newline="\n"), ensure_ascii=False, indent=2)
            report.append((sid, name, len(star_docs), len(observed), sys_hosts, pos_src))

    for r in report:
        print(f"{r[0]:>6} {r[1][:26]:26} stars={r[2]} observed={r[3]:2} hosts={r[4]} pos={r[5]}")


if __name__ == "__main__":
    main()
