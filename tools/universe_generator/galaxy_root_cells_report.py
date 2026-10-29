"""Audit of the Galaxy navigation root cells against the physical catalog.

usage: python galaxy_root_cells_report.py REPO_ROOT OUT_TXT [LEGACY_NAVIGATION_GRID_JSON]

(the legacy config with rootNavigation.allowedCells is in git history before
the connected-envelope policy commit)

Reproduces the LEGACY root-domain rule of GalaxyNavigationGrid (configured cells of
navigation_grid.json plus every root cell holding a catalog system, as
synchronizeCatalogPositions() does) and reports, per active root cell, its
index, ICRS bounds, systems and origin, plus connectivity and the empty cells
between inhabited ones (the audit that motivated the connected-envelope policy;
the current domain is reported by `EliteUniverseGenerator domain`). Root cells are centred: index i spans
[(i - 0.5) e, (i + 0.5) e] with e = terminalCellEdgeKm * subdivision^depth.
"""
import glob
import itertools
import json
import math
import os
import sys

KM_PER_LY = 9.4607304725808e12


def load(repo, cfg_path=None):
    cfg_path = cfg_path or os.path.join(repo, "src/assets/data/navigation/navigation_grid.json")
    cfg = json.load(open(cfg_path, encoding="utf-8"))
    edge = cfg["terminalCellEdgeKm"] * cfg["subdivisionPerAxis"] ** cfg["maximumDepth"] / KM_PER_LY
    configured = [tuple(c) for c in cfg["rootNavigation"].get("allowedCells", [])]
    systems = []
    root = os.path.join(repo, "src/assets/data/galaxy_details")
    for sub in ("systems", "distant_systems"):
        for p in sorted(glob.glob(os.path.join(root, sub, "*", "system.json"))):
            d = json.load(open(p, encoding="utf-8"))
            q = d["position_ly"]
            systems.append((d["id"], d["name"], (q["x"], q["y"], q["z"]), d.get("catalog_scope", "local")))
    return edge, configured, systems


def root_of(pos, edge):
    # llround: half away from zero
    return tuple(int(math.copysign(math.floor(abs(c / edge) + 0.5), c)) for c in pos)


def components(cells, neighbours):
    cells = set(cells)
    out = []
    while cells:
        stack = [cells.pop()]
        comp = set(stack)
        while stack:
            c = stack.pop()
            for d in neighbours:
                n = (c[0] + d[0], c[1] + d[1], c[2] + d[2])
                if n in cells:
                    cells.remove(n)
                    comp.add(n)
                    stack.append(n)
        out.append(sorted(comp))
    return sorted(out, key=lambda c: (-len(c), c))


def line_cells(a, b):
    n = max(abs(a[i] - b[i]) for i in range(3))
    return {tuple(round(a[i] + (b[i] - a[i]) * k / n) for i in range(3)) for k in range(n + 1)} if n else {a}


def main(repo, out, cfg_path=None):
    edge, configured, systems = load(repo, cfg_path)
    by_cell = {}
    for sid, name, pos, scope in systems:
        by_cell.setdefault(root_of(pos, edge), []).append((sid, name, scope, pos))
    active = sorted(set(configured) | set(by_cell))
    face = [d for d in itertools.product((-1, 0, 1), repeat=3) if sum(map(abs, d)) == 1]
    full = [d for d in itertools.product((-1, 0, 1), repeat=3) if any(d)]
    L = [f"Galaxy navigation root cells (edge {edge:.4f} ly, centred index cells, ICRS heliocentric ly)", ""]
    for c in active:
        lo = [(i - 0.5) * edge for i in c]
        hi = [(i + 0.5) * edge for i in c]
        origin = ("config + catalog" if c in configured and c in by_cell
                  else "config (no catalog system)" if c in configured else "added dynamically by catalog")
        names = by_cell.get(c, [])
        L.append(f"cell {c}  x[{lo[0]:.1f},{hi[0]:.1f}] y[{lo[1]:.1f},{hi[1]:.1f}] z[{lo[2]:.1f},{hi[2]:.1f}]  "
                 f"systems={len(names)}  origin={origin}")
        for sid, name, scope, pos in names:
            L.append(f"    {sid:>6} {name}  ({scope}, d={math.sqrt(sum(v * v for v in pos)):.1f} ly)")
    L.append("")
    L.append(f"total active root cells: {len(active)} (configured {len(configured)}, "
             f"added dynamically {len(set(by_cell) - set(configured))}, configured but empty "
             f"{len(set(configured) - set(by_cell))})")
    for label, nb in (("face-connected (6)", face), ("26-connected", full)):
        comps = components(active, nb)
        L.append(f"connected components, {label}: {len(comps)}")
        for comp in comps:
            sysn = sum(len(by_cell.get(c, [])) for c in comp)
            L.append(f"    {len(comp)} cells, {sysn} systems: {comp}")
    inhabited = sorted(by_cell)
    gaps = set()
    for a, b in itertools.combinations(inhabited, 2):
        if max(abs(a[i] - b[i]) for i in range(3)) <= 3:
            gaps |= {c for c in line_cells(a, b) if c not in active}
    L.append(f"blocked cells on straight lines between inhabited cells at most 3 roots apart: {len(gaps)}")
    L.append(f"    {sorted(gaps)}")
    far = [(a, b) for a, b in itertools.combinations(inhabited, 2) if max(abs(a[i] - b[i]) for i in range(3)) > 3]
    if far:
        L.append(f"inhabited cell pairs more than 3 roots apart (distant systems): {len(far)} pairs, e.g. {far[:3]}")
    local = [pos for _, _, pos, scope in systems if scope == "local"]
    for label, pts in (("local catalog", local), ("all systems", [s[2] for s in systems])):
        mn = [min(p[i] for p in pts) for i in range(3)]
        mx = [max(p[i] for p in pts) for i in range(3)]
        L.append(f"{label} bounds ly: x[{mn[0]:.1f},{mx[0]:.1f}] y[{mn[1]:.1f},{mx[1]:.1f}] z[{mn[2]:.1f},{mx[2]:.1f}], "
                 f"max distance {max(math.sqrt(sum(v * v for v in p)) for p in pts):.1f} ly")
    open(out, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("\n".join(L[-12:]))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
