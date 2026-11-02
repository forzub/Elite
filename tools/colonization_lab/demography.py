"""Causal demography of the colonies, the hyper-era frontier and Earth (2026-3126).

Replaces the old "fit one constant growth rate to an authored pop_3126".
Nothing here targets a population number. Every year, for every unit
(colony / polity, hyper-era outpost or frontier pool, Earth region):

  economy of children  ->  economic desired fertility (TFR_econ)
      living space: affordability of a NEW household place = income / marginal
                    habitat cost x crowding (space is built volume, not surface:
                    domes and lava tubes -> burrowed moons/asteroids -> rotating
                    habitats; marginal cost rises tier by tier, falls with
                    automation and construction technology)
      labour value:  (1 - automation) x labour shortage (expansion = shortage)
      old age:       1 - institutional pension coverage
      cost:          length of education (complexity of the technology base)
      child mortality (replacement births)
  cultural norm      follows TFR_econ with a lag of ~1.5 generations (both ways)
  religion           a high-fertility subculture: own births, apostasy, revival
  state policy       pronatal / antinatal by regime and situation, small effects
  births licences    hard brake when habitats are over capacity (ship tradition)
  mortality          life table from medicine/biotech capability and living
                     standard; seeded epidemics/habitat accidents; anchored wars
  capacity           habitat places built by the unit's own workers (endogenous)
  technology         per-domain capability: sustainable level needs a minimum
                     specialist base; carried machinery is cannibalised; lost /
                     restored / imported / stolen / state secret
  migration          only after the hyper drive, over the beacon graph, limited
                     by seats in ships and by regime barriers; driven by income,
                     space, war, crowding, religion/culture affinity.

Historical regularities and their sources: D:/tmp/DEMOGRAPHY_RESEARCH.md.
Deterministic: no clock, RNG seeded per unit, sorted iteration.
"""
import math
import random
import zlib

import endowment as endow

YEAR0 = 2026
YEAR_END = 3126
NB = 21                      # 5-year age bins, last one = 100+
WORK = range(3, 13)          # 15..64
FERT_W = {3: 0.07, 4: 0.22, 5: 0.27, 6: 0.21, 7: 0.14, 8: 0.07, 9: 0.02}
_MIG = [0.07, 0.04, 0.04, 0.12, 0.21, 0.18, 0.12, 0.07, 0.05, 0.035, 0.025, 0.015, 0.01, 0.007, 0.004, 0.002, 0.001, 0, 0, 0, 0]
MIG_W = [x / sum(_MIG) for x in _MIG]          # Rogers-Castro-like: migrants are 15-35 (research §8)
DOMAINS = ["energy", "extraction", "metallurgy", "manufacturing", "electronics", "bio", "shipbuilding",
           "military", "research", "automation", "construction", "jump"]
# skilled people (25 % of workers) needed to sustain the full capability of a domain; below that the
# domain falls to a craft level (research §10/§11 + futurological assumption F4)
CRIT = {"energy": 3e3, "extraction": 2e3, "metallurgy": 1.2e4, "manufacturing": 2.5e4, "electronics": 1.5e5,
        "bio": 2e4, "shipbuilding": 6e4, "military": 1e4, "research": 6e4, "automation": 9e4, "construction": 4e3,
        "jump": 2e5}
CRAFT = {"electronics": 0.15, "automation": 0.12, "research": 0.2, "shipbuilding": 0.2, "jump": 0.0}

# Habitat tiers (assumption F2): (capacity rule, labour-years per new place at reference technology)
C_TIER = (5.0, 8.0, 15.0)    # domes / lava tubes on good bodies, burrowed moons & asteroids, rotating habitats
E_OPEN_AIR = 5.0             # Earth surface: no life support, but land rent of a crowded planet
# Scenarios (futurological switches, DEMOGRAPHY_RESEARCH.md part B):
#   F9  heritable pronatal disposition: fertility preferences are transmitted (parent-child
#       correlation 0.1-0.3); after the transition the high-fertility lines dominate within a few
#       generations (Kolk et al. 2014). Modelled as a disposition D -> D_MAX with tau 150 yr,
#       suppressed while birth licences equalise family size.
#   F10 automated care: AI tutoring / robotic childcare remove up to half of the parental time
#       cost of a long education in highly automated societies.
SCENARIOS = {
    "baseline": dict(herit=True, care=True),
    "transition_persists": dict(herit=False, care=False),
}
SCEN = dict(SCENARIOS["baseline"])
D_MAX, D_TAU = 0.5, 150.0
TAU_NORM = 45.0              # years: family norm lags economics ~1.5 generations (research §6/§7)
SHORT_RESP = 0.35            # immediate response of actual TFR to the economic signal
AFFORD_K = 0.5               # calibration: Earth rich belt 2026 -> space index ~0.25 (assumption F3)


def clamp(x, lo=0.0, hi=1.0):
    return lo if x < lo else hi if x > hi else x


# ---------------------------------------------------------------- life tables
def _rates(s):
    out = []
    for i in range(NB):
        x = i * 5 + 2.5
        if i == 0:
            y = 0.060
        elif i == 1:
            y = 0.010
        elif i == 2:
            y = 0.006
        elif i < 9:
            y = 0.008
        else:
            y = 0.010
        g = 0.00003 * math.exp(0.095 * x)
        m = s * y + g * (0.35 + 0.65 * min(1.0, s) ** 0.5)
        out.append(min(0.6, m if i < NB - 1 else max(0.45, m)))
    return out


def _e0(rates):
    l, e = 1.0, 0.0
    for i, m in enumerate(rates):
        if i == NB - 1:
            e += l / m
            break
        p = math.exp(-5 * m)
        e += 5 * l * (1 + p) / 2
        l *= p
    return e


def _build_table():
    tab = []
    for k in range(0, 71):
        e0 = 25.0 + k
        lo, hi = 0.0, 6.0
        for _ in range(50):
            mid = (lo + hi) / 2
            if _e0(_rates(mid)) > e0:
                lo = mid
            else:
                hi = mid
        r = _rates((lo + hi) / 2)
        tab.append((e0, r, 1 - math.exp(-5 * r[0])))
    return tab


LIFE = _build_table()


def life(e0):
    e0 = clamp(e0, 25.0, 95.0)
    k = min(69, int(e0 - 25.0))
    f = (e0 - 25.0) - k
    a, b = LIFE[k], LIFE[k + 1]
    return [x * (1 - f) + y * f for x, y in zip(a[1], b[1])], a[2] * (1 - f) + b[2] * f


def age_struct(kind, total):
    if kind == "ship":        # adult-heavy crew of an ark, few old
        w = [6, 5, 5, 7, 11, 12, 11, 10, 9, 7, 6, 4, 3, 2, 1, 0.5, 0.3, 0.1, 0, 0, 0]
    elif kind == "young":     # high-fertility population
        w = [14, 13, 12, 11, 10, 8.5, 7, 6, 5, 4, 3.3, 2.6, 2, 1.4, 0.9, 0.5, 0.2, 0.1, 0, 0, 0]
    elif kind == "old":       # rich low-fertility population
        w = [4.5, 4.8, 5, 5.3, 5.8, 6.3, 6.5, 6.6, 6.6, 6.6, 6.8, 7, 6.6, 6, 5.2, 4.2, 3, 1.8, 0.9, 0.3, 0.1]
    else:                     # outpost: working adults
        w = [3, 2, 2, 8, 20, 22, 16, 10, 7, 5, 3, 1.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0]
    t = sum(w)
    return [total * x / t for x in w]


# ---------------------------------------------------------------- regimes and culture
REGIME = {  # pension state capacity, policy style, emigration barrier, immigration barrier, construction push, gini
    "technocratic_meritocracy": dict(state=1.0, policy="neutral", exitb=0.0, entryb=0.45, build=0.02, gini=0.40),
    "security_oligarchy": dict(state=0.8, policy="manpower", exitb=0.7, entryb=0.8, build=0.0, gini=0.45),
    "party_technocracy": dict(state=1.0, policy="planner", exitb=0.6, entryb=0.6, build=0.04, gini=0.33),
    "sage_council": dict(state=1.0, policy="planner", exitb=0.4, entryb=0.4, build=0.04, gini=0.34),
    "shareholder_agora": dict(state=0.8, policy="neutral", exitb=0.0, entryb=0.1, build=0.01, gini=0.44),
    "scholar_theocracy": dict(state=0.8, policy="faith", exitb=0.2, entryb=0.6, build=0.01, gini=0.36),
    "parliamentary_bureaucracy": dict(state=1.1, policy="family", exitb=0.0, entryb=0.3, build=0.0, gini=0.28),
    "corporate_democracy": dict(state=0.8, policy="neutral", exitb=0.0, entryb=0.25, build=0.01, gini=0.42),
    "merit_caste_republic": dict(state=0.8, policy="family", exitb=0.1, entryb=0.4, build=0.01, gini=0.40),
    "council_of_chiefs": dict(state=0.7, policy="family", exitb=0.1, entryb=0.3, build=0.02, gini=0.38),
    "grandees_and_church": dict(state=0.6, policy="faith", exitb=0.1, entryb=0.3, build=0.0, gini=0.52),
    "caste_theocracy": dict(state=0.6, policy="faith", exitb=0.8, entryb=0.95, build=0.0, gini=0.50),
    "exile_republic": dict(state=0.7, policy="manpower", exitb=0.1, entryb=0.2, build=0.02, gini=0.35),
    "machine_oracle": dict(state=0.9, policy="antinatal", exitb=0.2, entryb=0.85, build=0.0, gini=0.45),
    "corporate_board": dict(state=0.7, policy="company", exitb=0.0, entryb=0.2, build=0.02, gini=0.50),
    "clan_contracts": dict(state=0.3, policy="neutral", exitb=0.0, entryb=0.0, build=0.0, gini=0.48),
    "litigating_city_states": dict(state=0.8, policy="neutral", exitb=0.0, entryb=0.3, build=0.0, gini=0.42),
    "synod_theocracy": dict(state=0.6, policy="faith", exitb=0.3, entryb=0.9, build=0.0, gini=0.38),
    "none_quiet": dict(state=0.5, policy="neutral", exitb=1.0, entryb=1.0, build=0.0, gini=0.25),
    "sol_authority": dict(state=1.0, policy="family", exitb=0.0, entryb=0.6, build=0.0, gini=0.40),
}


def policy_effect(style, crowd, threat, y):
    """State policy: historical long-run effects are small (+0.1..0.3 TFR); coercive antinatal
    policy (one-child style) bites harder under crowding (research §5)."""
    if style == "manpower":
        return 0.25 + 0.25 * threat
    if style == "planner":
        return -0.7 * clamp((crowd - 1.0) / 0.15) + 0.15 * threat
    if style == "faith":
        return 0.15
    if style == "family":
        return 0.15
    if style == "company":
        return -0.4 * clamp((crowd - 0.95) / 0.15)
    if style == "antinatal":
        return -0.6
    return 0.0


# ---------------------------------------------------------------- unit
class Unit:
    def __init__(self, uid, name, kind, system, E, regime, culture, founded, religion, owner=None, earth=False, site_share=1.0):
        self.id, self.name, self.kind, self.system, self.owner = uid, name, kind, system, owner
        self.E = E[system]
        self.regime = regime
        self.cult = culture                     # 24 axes 0..100
        self.founded = founded
        self.earth = earth
        self.pop = [0.0] * NB
        self.r = religion                       # share of a high-fertility religious / traditional subculture
        self.norm = 4.0
        self.pension = 0.5
        self.tech = {d: 0.0 for d in DOMAINS}
        self.stock = {d: 0.0 for d in DOMAINS}  # carried / imported machinery that props capability up
        self.L0 = {d: 0.0 for d in DOMAINS}
        self.frontier = 1.0                     # local technological frontier (1 = Earth 2100)
        self.archive = 1.0
        self.H = 0.0
        self.active = False
        self.quarantined = False
        self.hist = []
        self.events = []
        self.tech_log = {d: [] for d in DOMAINS}
        self.net_mig = 0.0
        self.imm_y = 0.0
        self.emi_y = 0.0
        self.mig_in_tot = 0.0
        self.mig_out_tot = 0.0
        self.tfr_mult = 1.0
        self.extra_mort = 0.0
        self.threat = 0.0
        self.connected = False
        self.import_access = 0.0
        self.rng = None
        self.deaths_events = 0.0
        e = self.E
        # habitat tiers from the physical catalog (assumption F2)
        if earth:
            self.K = (11.0e9, 4e8, None)  # Earth surface + Sol habitats (moons, Mars, belt)
            self.c = (E_OPEN_AIR, C_TIER[1], C_TIER[2])
        else:
            k1 = 1.6e6 * e["n_sealed"] + 4e6 * e["n_open"]
            k2 = 2.5e6 * e["n_minor"] + 2.0e7 * math.log10(1.0 + 1e4 * e["belt_me"]) + 0.6e7 * math.log10(1.0 + 1e3 * e["comet_me"])
            self.K = (k1 * site_share, k2 * site_share, None)
            self.c = C_TIER
        self.cal_tfr = None
        self.lic = 1.0
        self.refuses_network = False
        self.D = 0.0
        self.y = 1.0
        self.S = 0.5
        self.A = 0.5
        self.Lv = 0.3
        self.Ed = 0.5
        self.q5 = 0.1
        self.e0 = 50.0
        self.tfr = 4.0
        self.tfr_econ = 4.0
        self.crowd = 0.8
        self.births = 0.0
        self.deaths = 0.0
        self.urban = 0.0
        self.tier = 0
        self.workers = 0.0

    # --- helpers
    def P(self):
        return sum(self.pop)

    def add_people(self, n, kind="ship", tech=None, archive=None):
        st = age_struct(kind, n)
        for i in range(NB):
            self.pop[i] += st[i]
        if tech:
            for d in DOMAINS:
                if tech[d] > self.tech[d]:
                    self.tech[d] = tech[d]
                self.stock[d] = max(self.stock[d], tech[d])
                self.L0[d] = max(self.L0[d], tech[d])
        if archive is not None:
            self.archive = max(self.archive if self.active else 0.0, archive)
        self.H += n * (1.35 if kind == "ship" else 1.1)

    def remove_people(self, n, weights=None):
        P = self.P()
        if P <= 0 or n <= 0:
            return 0.0
        n = min(n, P * 0.95)
        if weights is None:
            f = 1 - n / P
            self.pop = [x * f for x in self.pop]
            return n
        taken = 0.0
        tot = sum(w * self.pop[i] for i, w in enumerate(weights)) or 1.0
        for i, w in enumerate(weights):
            q = min(self.pop[i] * 0.9, n * w * self.pop[i] / tot)
            self.pop[i] -= q
            taken += q
        return taken

    def marginal_cost(self):
        """Labour-years per new habitat place (assumption F2)."""
        H = self.H
        k1, k2, _ = self.K
        # Ricardian rent: the best sites of a tier are used first, so the marginal place gets dearer
        # as the tier fills; the next tier takes over when it becomes the cheaper option
        c1 = self.c[0] * (1 + 2.0 * (H / k1) ** 2) if k1 > 0 else 1e9
        c2 = self.c[1] * (1 + 1.5 * (max(0.0, H - k1) / k2) ** 2) if k2 > 0 else 1e9
        c3 = self.c[2]
        base = min(c1, c2, c3)
        self.tier = 1 if base == c1 else 2 if base == c2 else 3
        e = self.E["idx"]
        materials = 1.0
        if not self.earth:
            water_poor = (1 - e["water"]) * (1 - 0.6 * self.import_access)
            materials = (1 + 0.8 * water_poor) * (1 + 0.5 * (1 - e["metals"]))
        prod = (0.35 + 0.65 * min(1.3, self.tech["construction"])) * (1 + 1.5 * self.A)
        return base * materials / prod

    # --- yearly step
    def step(self, y, rng):
        P = self.P()
        if P < 1:
            self.active = False
            return
        T = self.tech
        workers = sum(self.pop[i] for i in WORK) * 0.85
        self.workers = workers
        # technology -------------------------------------------------------
        skilled = workers * 0.25
        for d in DOMAINS:
            if d == "jump":
                continue
            sig = 1.0 / (1.0 + math.exp(-math.log(max(skilled, 1.0) / CRIT[d])))
            craft = CRAFT.get(d, 0.35)
            sus = self.archive * self.frontier * (craft + (1 - craft) * sig)
            self.stock[d] *= 0.975                           # cannibalisation of carried machinery
            floor = max(sus, self.stock[d])
            if T[d] > floor:
                T[d] -= (T[d] - floor) * 0.02                # specialists retire, spares run out
            else:
                T[d] += (floor - T[d]) * 0.04                # rebuilt by the local base
        rs = 1.0 / (1.0 + math.exp(-math.log(max(skilled, 1.0) / 1e5)))
        self.frontier += 0.0012 * rs * (0.5 + T["research"]) - (0.0004 if rs < 0.1 else 0.0)
        self.archive = min(1.0, self.archive + 0.002 * rs)
        A = clamp(0.1 + 0.75 * (T["automation"] + T["electronics"] + T["manufacturing"]) / 3.0 / 1.3, 0.05, 0.95)
        self.A = A
        # income per capita (Earth 2100 = ~1)
        e = self.E["idx"]
        endow_f = 0.75 + 0.5 * (e["volatiles"] + e["metals"] + max(e["he3"], 0.3)) / 3.0 + 0.25 * self.import_access
        prodw = (0.25 + 1.1 * A) * (T["energy"] + T["manufacturing"] + T["extraction"] + T["metallurgy"]) / 4.0 * endow_f
        self.y = max(0.02, workers * prodw / P * 1.55)
        # habitat construction -------------------------------------------------
        c = self.marginal_cost()
        self.crowd = P / max(self.H, 1.0)
        reg = REGIME.get(self.regime, REGIME["shareholder_agora"])
        # construction follows demand: almost only maintenance while habitats stand half empty
        s = 0.012 + 0.17 * clamp((self.crowd - 0.75) / 0.35) + reg["build"] * clamp((self.crowd - 0.7) / 0.3)
        if self.quarantined:
            s = 0.02
        self.H += s * workers / c - 0.004 * self.H
        self.crowd = P / max(self.H, 1.0)
        # economics of children ------------------------------------------------
        # the household standard (volume per person) rises with income, so technology makes a place
        # cheaper but does not make space free (research §4: housing cost, Easterlin relative income)
        self.c_eff = c * max(self.y, 0.05) ** 0.8
        afford = AFFORD_K / (self.c_eff * self.crowd ** 2)
        self.S = afford / (1 + afford)
        shortage = clamp(0.35 + 1.3 * (1 - self.crowd) + 0.3 * clamp((s - 0.05) / 0.13))
        self.Lv = (1 - A) ** 1.3 * shortage
        target_p = clamp(0.2 + 0.8 * clamp((self.y - 0.3) / 0.9)) * reg["state"]
        self.pension += (clamp(target_p) - self.pension) * 0.03
        self.Ed = clamp(12 * clamp((T["electronics"] + T["research"]) / 2 / 1.1) / 14)
        # mortality ------------------------------------------------------------
        hlt = clamp(0.3 + 0.5 * min(1.25, T["bio"]) + 0.2 * clamp(self.y / 1.2)) - 0.15 * clamp(self.crowd - 1.0)
        self.e0 = 30 + 64 * hlt
        rates, q5 = life(self.e0)
        self.q5 = q5
        ed_cost = self.Ed * ((1 - 0.5 * clamp((A - 0.6) / 0.35)) if SCEN["care"] else 1.0)
        tfr_econ = 1.3 + 2.8 * self.S + 1.6 * self.Lv + 1.1 * (1 - self.pension) + 2.5 * q5 - 1.0 * ed_cost
        self.tfr_econ = clamp(tfr_econ, 0.9, 8.0)
        self.norm += (self.tfr_econ - self.norm) / TAU_NORM
        if SCEN["herit"]:
            self.D += ((D_MAX if self.crowd < 1.05 else 0.0) - self.D) / D_TAU
        pol = policy_effect(reg["policy"], self.crowd, self.threat, y)
        if self.cal_tfr is not None:      # observed starting TFR (Earth 2026): solve the norm once
            r_ = self.r
            prem = 2.6 * (1 - 0.35 * self.Ed) * (1 - r_) ** 1.5
            self.norm = (self.cal_tfr - r_ * prem - pol - self.D - SHORT_RESP * self.tfr_econ) / (1 - SHORT_RESP)
            self.cal_tfr = None
        tfr_s = self.norm + SHORT_RESP * (self.tfr_econ - self.norm) + pol + self.D
        # the religious premium is a minority phenomenon: it shrinks as the subculture becomes the
        # mainstream and pays the mainstream's costs (research §3, Hutterite / Haredi convergence)
        tfr_r = tfr_s + 2.6 * (1 - 0.35 * self.Ed) * (1 - self.r) ** 1.5
        tfr_s, tfr_r = clamp(tfr_s, 0.7, 8.5), clamp(tfr_r, 1.5, 9.5)
        lic = 1.0 if self.crowd < 1.08 else (1.08 / self.crowd) ** 6     # birth licences when habitats are full
        self.lic = lic
        tfr_s *= lic * self.tfr_mult
        tfr_r *= lic * self.tfr_mult
        self.tfr = (1 - self.r) * tfr_s + self.r * tfr_r
        # births ---------------------------------------------------------------
        women = {i: self.pop[i] * 0.5 for i in FERT_W}
        b_s = sum(women[i] * (1 - self.r) * tfr_s * w / 5 for i, w in FERT_W.items())
        b_r = sum(women[i] * self.r * tfr_r * w / 5 for i, w in FERT_W.items())
        births = b_s + b_r
        # religion: own births, apostasy (prosperity, openness), revival (hardship) (research §3)
        apost = (0.003 + 0.010 * clamp(self.y / 1.5) * (0.5 + 0.5 * self.import_access)) * (0.35 if reg["policy"] == "faith" else 1.0)
        revival = 0.0015 * clamp(self.crowd - 0.95) * 5 + 0.001 * self.threat + 0.002 * clamp(0.6 - self.y)
        newP = P + births
        self.r = clamp((self.r * P + b_r) / newP * (1 - apost) + revival * (1 - self.r), 0.0, 0.95)
        # deaths ---------------------------------------------------------------
        extra = self.extra_mort + 0.02 * clamp(self.crowd - 1.15)
        deaths = 0.0
        newpop = [0.0] * NB
        for i in range(NB):
            m = rates[i] + extra
            surv = self.pop[i] * math.exp(-m)
            deaths += self.pop[i] - surv
            if i < NB - 1:
                newpop[i] += surv * 0.8
                newpop[i + 1] += surv * 0.2
            else:
                newpop[i] += surv
        newpop[0] += births * math.exp(-rates[0] * 0.5)
        deaths += births * (1 - math.exp(-rates[0] * 0.5))
        # seeded epidemics / habitat accidents (expected frequency, not history)
        p_ep = 0.006 * (1 + clamp(self.crowd - 0.7)) * (1.6 - hlt)
        if rng.random() < p_ep:
            sev = rng.uniform(0.004, 0.03) * (1.5 - hlt)
            lost = sum(newpop) * sev
            newpop = [x * (1 - sev) for x in newpop]
            deaths += lost
            self.events.append(dict(year=y, type="epidemic", loss=int(lost), source="simulated"))
        self.pop = newpop
        self.births, self.deaths = births, deaths
        # urbanisation: Zipf over habitats (habitat size grows with tier)
        P2 = self.P()
        hsize = {1: 2.0e4, 2: 6.0e4, 3: 4.0e5}[self.tier] if not self.earth else 1.0e5
        n_set = max(1, int(P2 / hsize) + 1)
        hn = sum(1.0 / k for k in range(1, min(n_set, 5000) + 1)) + (math.log(n_set / 5000) if n_set > 5000 else 0)
        kmax = int(P2 / (1e5 * hn))
        hk = sum(1.0 / k for k in range(1, min(kmax, 5000) + 1)) + (math.log(kmax / 5000) if kmax > 5000 else 0)
        self.urban = clamp(hk / hn) if kmax >= 1 else 0.0
        self.n_settlements = n_set
        self.largest = P2 / hn

    def phase(self, growth, mig_rate):
        natural = growth - mig_rate
        if growth < -0.002:
            return "decline"
        if mig_rate > 0.004 and growth > 0.01:
            return "boom"
        if natural > 0.01:
            return "expansion"
        if natural > 0.003:
            return "growth"
        if self.crowd >= 0.95 or self.lic < 1.0:
            return "saturation"
        return "stagnation"

    def record(self, y):
        P = self.P()
        prev = self.hist[-1] if self.hist else None
        growth = 0.0 if not prev or prev["pop"] <= 0 else (P / prev["pop"]) ** (1.0 / max(1, y - prev["year"])) - 1
        mig_rate = self.net_mig / max(P, 1.0)
        self.hist.append(dict(
            year=y, pop=P, tfr=self.tfr, tfr_econ=self.tfr_econ, norm=self.norm, religious=self.r,
            e0=self.e0, q5=self.q5, cbr=self.births / max(P, 1) * 1000, cdr=self.deaths / max(P, 1) * 1000,
            net_mig=self.net_mig, crowd=self.crowd, space=self.S, labour_value=self.Lv, pension=self.pension,
            education=self.Ed, automation=self.A, income=self.y, urban=self.urban, tier=self.tier,
            capacity=self.H, growth=growth, phase=self.phase(growth, mig_rate),
            tech={d: round(v, 3) for d, v in self.tech.items()}))


# ---------------------------------------------------------------- world simulation
def _tech_at_launch(earth_hist, year, knowledge):
    """Ships carry Earth's technology of their launch year, degraded by voyage knowledge loss."""
    best = None
    for h in earth_hist:
        if h["year"] <= year:
            best = h
    base = best["tech"] if best else {d: 0.8 for d in DOMAINS}
    out = {d: base[d] * (0.55 + 0.45 * knowledge) for d in DOMAINS}
    out["jump"] = 0.0
    return out


def _culture_vec(c):
    return c


def build_units(world, missions, voyages, E, culture_of, regime_of):
    units = {}
    # Earth: two regions (rich North, developing South) of one Sol Authority
    eh = world["earth_demography"]
    for reg in eh["regions"]:
        u = Unit(reg["id"], reg["name"], "earth", 0, E, "sol_authority", culture_of("sol_authority"), YEAR0, reg["religious_share"], earth=True)
        u.pop = age_struct(reg["age"], reg["pop_2026"])
        u.H = reg["pop_2026"] * 1.12
        u.norm = reg["tfr_2026"]
        u.cal_tfr = reg["tfr_2026"]
        u.pension = reg["pension_2026"]
        u.frontier = 1.0
        u.archive = 1.0
        for d in DOMAINS:
            u.tech[d] = reg["tech_2026"] if d != "jump" else 0.0
            u.L0[d] = u.tech[d]
        u.share_K = reg["capacity_share"]
        u.K = tuple(None if k is None else k * reg["capacity_share"] for k in u.K)
        u.active = True
        units[u.id] = u
    for c in world["colonies"]:
        u = Unit(c["id"], c["name"], "colony", c["system"], E, c["regime_3126"], culture_of(c["id"]), c["founded"],
                 c["demography"]["religious_share"], site_share=c["demography"].get("site_share", 1.0))
        u.refuses_network = bool(c["demography"].get("refuses_network", False))
        u.norm = c["demography"].get("norm_at_founding", 4.0)
        units[u.id] = u
    for o in world["outposts"] + world.get("frontier_pools", []):
        owner = o["owners"][0]
        u = Unit(o["id"], o["name"], "outpost" if o in world["outposts"] else "frontier", o["system"], E,
                 regime_of(owner), culture_of(owner), o["founded"], 0.05, owner=owner, site_share=o.get("site_share", 1.0))
        u.norm = 2.5
        units[u.id] = u
    return units


SHORTEST = [None]


def simulate(world, missions, voyages, E, culture_of, regime_of, beacon_graph, shortest, seed):
    SHORTEST[0] = shortest
    units = build_units(world, missions, voyages, E, culture_of, regime_of)
    earth_ids = [r["id"] for r in world["earth_demography"]["regions"]]
    earth_hist = []
    rngs = {uid: random.Random(seed * 7919 + zlib.crc32(uid.encode("utf-8"))) for uid in sorted(units)}
    # founding schedule: arrivals of founder ships (anchors), splits, outposts
    arrivals = []
    for c in world["colonies"]:
        if c.get("founded_by_split"):
            continue
        for f in c["founders"]:
            v = voyages.get(f)
            if v is None or not v.arrival:
                continue
            if f == "HOMEWARD-1":
                continue
            arrivals.append((v.arrival["year"], c["id"], f, v.arrival["pop_after"], v.knowledge))
    arrivals.sort()
    dev = sorted(world.get("demography_events", []), key=lambda d: (d["year"], d["unit"], d["type"]))
    flows_log = []
    graph_cache = {}
    mig_sched = world["migration"]
    for y in range(YEAR0, YEAR_END + 1):
        # arrivals
        for (ay, cid, f, n, know) in arrivals:
            if ay == y:
                u = units[cid]
                tech = _tech_at_launch(earth_hist, missions[f]["launch_year"], know)
                u.add_people(n, "ship", tech, archive=0.6 + 0.4 * know)
                if not u.active:
                    u.active = True
                    u.pension = 0.5
                u.events.append(dict(year=y, type="arrival", ship=f, people=n, source="anchor"))
        # outposts / frontier pools: founded by their owner when the beacon / expedition exists
        for uid in sorted(units):
            u = units[uid]
            if u.kind in ("outpost", "frontier") and not u.active and y == u.founded:
                own = units.get(u.owner)
                if own and own.active:
                    n = min(u_seed(world, uid), own.P() * 0.01)
                    own.remove_people(n, MIG_W)
                    u.add_people(n, "outpost", dict(own.tech), archive=own.archive)
                    u.frontier = own.frontier
                    u.active = True
                    u.connected = True
                    u.events.append(dict(year=y, type="founded", by=own.id, people=int(n), source="model"))
        # authored demographic events (wars, exoduses, symbiont, Earth collapse...)
        for u in units.values():
            u.tfr_mult = 1.0
            u.extra_mort = 0.0
        for d in dev:
            if d["year"] <= y <= d.get("until", d["year"]):
                apply_event(units, d, y)
        # Earth aggregate history for launch technology
        for uid in sorted(units):
            u = units[uid]
            if u.active:
                u.step(y, rngs[uid])
        # beacon network & connectivity
        if y >= 2750:
            key = max(k for k in [2750, 2755, 2760, 2765, 2775, 2778, 2785, 2790, 2795, 2800, 2805, 2810, 2840, 2850, 2855,
                                  2860, 2862, 2870, 2880, 2905, 2910, 2950, 3040, 3045, 3050, 3112] if k <= y)
            if key not in graph_cache:
                graph_cache[key] = beacon_graph(key)
            g = graph_cache[key]
        else:
            g = None
        update_connectivity(units, g, y, world)
        diffuse_tech(units, g, y, world)
        for u in units.values():
            u.net_mig = 0.0
        if g is not None:
            flows_log.extend(migrate(units, g, y, world, mig_sched))
        earth_migration(units, earth_ids, y, world, flows_log)
        # records
        if y % 5 == 1 or y == YEAR_END:
            pass
        for uid in sorted(units):
            u = units[uid]
            if u.active and (y % 5 == 0 or y == YEAR_END or y == u.founded):
                u.record(y)
        tot = sum(units[e].P() for e in earth_ids)
        if tot > 0 and (y % 5 == 0 or y == YEAR0):
            earth_hist.append(dict(year=y, pop=tot, tech={d: sum(units[e].tech[d] * units[e].P() for e in earth_ids) / tot for d in DOMAINS}))
    return units, flows_log, earth_hist


def u_seed(world, uid):
    for o in world["outposts"] + world.get("frontier_pools", []):
        if o["id"] == uid:
            return o.get("seed_people", 300)
    return 300


def apply_event(units, d, y):
    u = units.get(d["unit"])
    if u is None:
        return
    t = d["type"]
    first = (y == d["year"])
    if t == "tfr_mult":
        u.tfr_mult *= d["value"]
    elif t == "mortality":
        u.extra_mort += d["value"]
    elif t == "deaths" and first:
        lost = u.remove_people(d["value"], [0.2, 0.2, 0.3, 0.8, 1.0, 1.0, 1.0, 0.9, 0.7, 0.5] + [0.3] * 11)
        u.events.append(dict(year=y, type=d.get("label", "deaths"), loss=int(lost), source=d["status"].lower()))
    elif t == "transfer" and first:
        dst = units.get(d["to"])
        P = u.P()
        n = d["value"] * P if d["value"] < 1 else d["value"]
        moved = u.remove_people(n)
        if dst is not None:
            if not dst.active:
                dst.active = True
                dst.archive = u.archive
                dst.frontier = u.frontier
                dst.pension = u.pension * 0.5
                dst.norm = u.norm
                for dd in DOMAINS:
                    dst.tech[dd] = u.tech[dd] * 0.85
                    dst.L0[dd] = u.L0[dd]
            dst.add_people(moved, "young")
            dst.H += moved * 0.2
        u.H *= max(0.5, 1 - moved / max(P, 1) * 0.5)
        u.events.append(dict(year=y, type=d.get("label", "exodus"), people=int(moved), to=d.get("to"), source=d["status"].lower()))
    elif t == "capacity_loss":
        u.H *= (1 - d["value"])
    elif t == "tech_loss":
        for dd in d.get("domains", DOMAINS):
            u.tech[dd] *= (1 - d["value"])
            u.stock[dd] *= (1 - d["value"])
        u.frontier *= (1 - d["value"] * 0.3)
    elif t == "threat":
        u.threat = d["value"]
    elif t == "quarantine" and first:
        u.quarantined = True
    elif t == "tech_gift" and first:
        for dd in d.get("domains", []):
            u.tech[dd] = max(u.tech[dd], d["value"])
            u.stock[dd] = max(u.stock[dd], d["value"])
        u.events.append(dict(year=y, type=d.get("label", "tech"), source=d["status"].lower()))


def update_connectivity(units, g, y, world):
    for u in units.values():
        if not u.active or u.earth:
            continue
        nodes = g[0] if g else {}
        u.connected = bool(g) and (u.system in nodes) and not u.quarantined and not u.refuses_network
        target = 0.0
        if u.connected:
            target = min(1.0, (y - 2750) / 120.0)
        u.import_access += (target - u.import_access) * 0.05


def diffuse_tech(units, g, y, world):
    """Technology moves with ships: licensed and pirated jump drives, imported machinery,
    specialists. The jump core itself is a state secret of the Beacon Authority."""
    jump = world["jump_technology"]
    for uid in sorted(units):
        u = units[uid]
        if not u.active or u.earth:
            continue
        for rule in jump:
            if rule["unit"] == uid and y >= rule["year"]:
                lvl = rule["level"]
                if u.tech["jump"] < lvl:
                    u.tech["jump"] = lvl
                    u.tech_log["jump"].append((y, rule["how"]))
        if not u.connected:
            continue
        peers = [units[p] for p in sorted(units) if units[p].active and units[p].connected and not units[p].earth and p != uid]
        if not peers:
            continue
        lead_frontier = max(p.frontier for p in peers)
        if lead_frontier > u.frontier:
            u.frontier += (lead_frontier - u.frontier) * 0.01 * u.import_access
        for d in DOMAINS:
            if d == "jump":
                continue
            best = max(p.tech[d] for p in peers)
            imported = best * 0.85 * u.import_access
            if imported > u.stock[d]:
                if u.stock[d] < u.tech[d] and imported > u.tech[d] and not any(t == "imported" for _, t in u.tech_log[d]):
                    u.tech_log[d].append((y, "imported"))
                u.stock[d] = imported


def _utility(u):
    war = u.threat
    return math.log(max(u.y, 0.03)) + 1.2 * u.S - 0.8 * max(0.0, u.crowd - 1.0) - 0.8 * war


def migrate(units, g, y, world, sched):
    nodes, edges, hop = g
    shortest = SHORTEST[0]
    act = [units[k] for k in sorted(units) if units[k].active and units[k].connected and not units[k].earth]
    if len(act) < 2:
        return []
    ramp = clamp((y - 2752) / 110.0)
    seats = sched["seat_share_max"] * ramp * sum(u.P() for u in act)
    flows = []
    U = {u.id: _utility(u) for u in act}
    sp = {}
    rel = {tuple(sorted((r["a"], r["b"]))): r["grievance"] for r in world["relations_anchor"]}
    cand = []
    for a in act:
        if a.system not in sp:
            sp[a.system] = shortest(edges, a.system)
        dist, hops = sp[a.system]
        ra = REGIME.get(a.regime, REGIME["shareholder_agora"])
        for b in act:
            if b is a or b.system not in dist:
                continue
            rb = REGIME.get(b.regime, REGIME["shareholder_agora"])
            gr = rel.get(tuple(sorted((a.id, b.id))), 0)
            if b.owner == a.id or a.owner == b.id:
                gr = 0
            if gr >= 60:
                continue
            h = hops[b.system]
            cost = 0.25 + 0.12 * h + 0.6 * rb["entryb"] + 0.5 * ra["exitb"] + gr / 100.0
            if b.owner == a.id:
                cost -= 0.3                                     # own frontier: company / state recruitment
            if b.kind == "frontier" or b.kind == "outpost":
                cost -= 0.1 * b.S
            gap = U[b.id] - U[a.id] - cost
            if gap <= 0:
                continue
            aff = 1.0 - min(0.6, _cdist(a.cult, b.cult) / 60.0)
            rate = sched["rate_per_gap"] * gap * aff
            cand.append((a.id, b.id, rate, h))
    out_rate = {}
    for a_id, b_id, rate, h in cand:
        out_rate[a_id] = out_rate.get(a_id, 0.0) + rate
    want = []
    for a_id, b_id, rate, h in cand:
        a = units[a_id]
        scale = min(1.0, sched["max_emig_rate"] / out_rate[a_id]) if out_rate[a_id] > 0 else 0
        want.append((a_id, b_id, a.P() * rate * scale, h))
    tot = sum(w for _, _, w, _ in want)
    k = min(1.0, seats / tot) if tot > 0 else 0.0
    inflow = {}
    for a_id, b_id, w, h in sorted(want):
        n = w * k
        b = units[b_id]
        cap_in = sched["max_immig_rate"] * max(b.P(), 2e4) - inflow.get(b_id, 0.0)
        n = max(0.0, min(n, cap_in))
        if n < 1:
            continue
        inflow[b_id] = inflow.get(b_id, 0.0) + n
        moved = units[a_id].remove_people(n, MIG_W)
        st = [moved * w for w in MIG_W]
        for i in range(NB):
            b.pop[i] += st[i]
        units[a_id].net_mig -= moved
        b.net_mig += moved
        units[a_id].mig_out_tot += moved
        b.mig_in_tot += moved
        flows.append((y, a_id, b_id, moved, h))
    return flows


def earth_migration(units, earth_ids, y, world, flows_log):
    """Earth: South->North (wage gap) every year; off-world emigration only through the windows
    the jump chronology allows (2752-2790 open, 2790-3112 Cordon of Sol with selective visas,
    3112+ own beacon)."""
    n_id, s_id = earth_ids
    n, s = units[n_id], units[s_id]
    gap = _utility(n) - _utility(s) - 0.4
    if gap > 0:
        m = min(0.004, 0.01 * gap) * s.P()
        moved = s.remove_people(m, MIG_W)
        for i in range(NB):
            n.pop[i] += moved * MIG_W[i]
        s.net_mig -= moved
        n.net_mig += moved
    w = world["migration"]["earth_windows"]
    quota = 0.0
    for win in w:
        if win["from"] <= y < win["to"]:
            quota = win["per_year"]
    if quota <= 0:
        return
    dest = [units[k] for k in sorted(units) if units[k].active and units[k].connected and not units[k].earth
            and REGIME.get(units[k].regime, {}).get("entryb", 1) < 0.7]
    if not dest:
        return
    wts = [max(0.0, u.tech["research"]) * u.P() ** 0.5 for u in dest]
    tw = sum(wts) or 1.0
    for u, wt in zip(dest, wts):
        q = quota * wt / tw
        src = n if n.P() > 0 else s
        moved = src.remove_people(q, MIG_W)
        for i in range(NB):
            u.pop[i] += moved * MIG_W[i]
        src.net_mig -= moved
        u.net_mig += moved
        src.mig_out_tot += moved
        u.mig_in_tot += moved
        flows_log.append((y, src.id, u.id, moved, 0))


def _cdist(a, b):
    ks = sorted(a)
    return math.sqrt(sum((a[k] - b[k]) ** 2 for k in ks) / len(ks))


# ---------------------------------------------------------------- tech status labels
def tech_status(u):
    """kept / lost / restored / imported / stolen / state_secret / own / never per domain."""
    out = {}
    for d in DOMAINS:
        hs = [h["tech"][d] for h in u.hist]
        L0 = u.L0[d] or (hs[0] if hs else 0.0)
        lab = [t for _, t in u.tech_log[d]]
        if d == "jump":
            out[d] = lab[-1] if lab else "none"
            continue
        if not hs or L0 <= 0:
            out[d] = "n/a"
            continue
        mn = min(hs)
        last = hs[-1]
        lost = mn < 0.6 * L0
        if lost and last >= 0.85 * L0:
            out[d] = "imported" if "imported" in lab else "restored"
        elif lost:
            out[d] = "lost"
        elif "imported" in lab and last > 1.05 * max(hs[:max(1, len(hs) // 2)]):
            out[d] = "imported"
        else:
            out[d] = "kept"
    return out


# ---------------------------------------------------------------- exports
HABITAT = {1: "купола, лавовые трубки и укрытые поселения на твёрдых телах", 2: "выработки в лунах и астероидах",
           3: "вращающиеся хабитаты (цилиндры, кольца)"}
HABITAT_EARTH = {1: "поверхность Земли", 2: "хабитаты системы Сол (Луна, Марс, пояс)", 3: "вращающиеся хабитаты Сола"}


def checkpoints(u, step=25):
    rows = []
    n = len(u.hist)
    for k, h in enumerate(u.hist):
        if not (h["year"] % step == 0 or k == 0 or k == n - 1):
            continue
        P = h["pop"]
        rows.append(dict(
            year=h["year"], pop=int(round(P)), tfr=round(h["tfr"], 2), tfr_econ=round(h["tfr_econ"], 2), family_norm=round(h["norm"], 2),
            religious_share=round(h["religious"], 3), e0=round(h["e0"], 1), q5=round(h["q5"], 4), cbr=round(h["cbr"], 1), cdr=round(h["cdr"], 1),
            net_migration_per_1000=round(h["net_mig"] / max(P, 1) * 1000, 2), phase=h["phase"], urbanisation=round(h["urban"], 3),
            habitat=(HABITAT_EARTH if u.earth else HABITAT)[h["tier"] or 1], crowding=round(h["crowd"], 3), space_index=round(h["space"], 3),
            labour_value=round(h["labour_value"], 3), pension_coverage=round(h["pension"], 3), education_cost=round(h["education"], 3),
            automation=round(h["automation"], 3), income=round(h["income"], 3),
            tech_index=round(sum(v for k2, v in h["tech"].items() if k2 != "jump") / (len(DOMAINS) - 1), 3)))
    return rows


def min_automation(u):
    best = None
    for h in u.hist:
        if best is None or h["automation"] < best[1]:
            best = (h["year"], h["automation"])
    return best


def trajectories(units, cols_by_id):
    """Six contrasting trajectories picked by rule (not by hand). Candidates need >= 100 years of
    history and must not be quarantined; the young frontier comes from units founded after 2500
    (colonies, outposts, frontier pools) with >= 60 years of history."""
    def g(u):
        h0 = u.hist[0]
        return math.log(max(u.P(), 1) / max(h0["pop"], 1)) / max(1, YEAR_END - h0["year"])
    def age(u):
        return YEAR_END - u.hist[0]["year"]
    cand = [u for u in units.values() if u.id in cols_by_id and u.hist and u.P() > 1000 and age(u) >= 100 and not u.quarantined]
    out = []
    used = set()
    def pick(tag, key, pool, why):
        pool = [u for u in pool if u.id not in used]
        if not pool:
            return
        u = sorted(pool, key=lambda x: (key(x), x.id))[0]
        used.add(u.id)
        out.append(dict(kind=tag, unit=u.id, why=why))
    old = [u for u in cand if u.hist[0]["year"] < 2300]
    pick("old_successful_expansive", lambda u: -u.P(), old, "основана до 2300, наибольшее население 3126")
    pick("rich_low_fertility", lambda u: -u.y if u.hist[-1]["tfr"] < 2.0 else 9, cand, "наибольший доход среди колоний с СКР < 2")
    pick("religious_high_fertility", lambda u: -u.r, cand, "наибольшая доля религиозной субкультуры (история >= 100 лет)")
    pick("degraded_labour_intensive", lambda u: min_automation(u)[1], cand, "самая глубокая деградация автоматизации")
    young = [u for u in units.values() if u.hist and not u.earth and u.hist[0]["year"] >= 2750 and age(u) >= 60 and u.P() > 1000]
    pick("young_hyper_frontier", lambda u: -u.mig_in_tot, young, "основана в гипер-эпоху (аванпост или фронтир), больше всего переселенцев")
    pick("stagnant", lambda u: abs(u.hist[-1]["growth"]), cand, "самый малый темп роста к 3126 среди колоний с историей >= 100 лет")
    return out
