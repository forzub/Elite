#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "src/assets/data/galaxy"
SKY = DATA / "sky_cultures"
LOC_SKY = ROOT / "src/assets/localization/sky"


def fail(message: str) -> None:
    print(f"[FAIL] sky-culture contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def segments(culture: dict) -> int:
    return sum(
        max(0, len(polyline) - 1)
        for constellation in culture.get("constellations", [])
        for polyline in constellation.get("polylines", [])
    )


def main() -> int:
    manifest = json.loads(read("src/assets/data/galaxy/sky_cultures/manifest.json"))
    expected_order = ["iau-western", "chinese-28-mansions", "hawaiian-starlines"]
    entries = manifest.get("cultures", [])
    require([entry.get("id") for entry in entries] == expected_order,
            "approved culture order changed")
    require(manifest.get("default_culture") == "iau-western",
            "western culture must remain default")

    cultures: dict[str, dict] = {}
    for entry in entries:
        path = SKY / entry["file"]
        require(path.exists(), f"missing sky-culture file {path.name}")
        culture = json.loads(path.read_text(encoding="utf-8"))
        require(culture.get("culture_id") == entry["id"],
                f"culture/file id mismatch for {entry['id']}")
        require(culture.get("star_identifier") in ("hr", "hip"),
                f"invalid star identifier for {entry['id']}")
        require("culture_names" not in culture,
                f"{entry['id']} topology file still owns localized culture names")
        for c in culture.get("constellations", []):
            require("names" not in c and "name" not in c,
                    f"{entry['id']}/{c.get('id')} topology still owns display text")
        require(bool(culture.get("source", {}).get("license")),
                f"{entry['id']} lost source/license provenance")
        cultures[entry["id"]] = culture

    western = cultures["iau-western"]
    chinese = cultures["chinese-28-mansions"]
    hawaiian = cultures["hawaiian-starlines"]

    require(western["star_identifier"] == "hr", "western must use HR topology")
    require(len(western["constellations"]) == 88, "western 88-constellation set changed")
    require(segments(western) == 750, "western 750-segment topology changed")

    legacy = json.loads(read("src/assets/data/galaxy/constellation_lines.json"))
    legacy_by_id = {c["id"]: c["polylines_hr"] for c in legacy["constellations"]}
    western_by_id = {c["id"]: c["polylines"] for c in western["constellations"]}
    require(western_by_id == legacy_by_id,
            "versioned western culture drifted from approved legacy topology")

    require(chinese["star_identifier"] == "hip" and len(chinese["constellations"]) == 42,
            "Chinese culture must remain curated 28-mansion + 14-asterism set")
    curated_chinese_extras = [
        "extra-northern-dipper", "extra-northern-pole", "extra-north-river",
        "extra-five-chariots", "extra-celestial-boat", "extra-celestial-ford",
        "extra-purple-forbidden-left-wall", "extra-celestial-meadows",
        "extra-celestial-orchard", "extra-bow-and-arrow", "extra-southern-boat",
        "extra-cross", "extra-southern-gate", "extra-peafowl",
    ]
    require([c["id"] for c in chinese["constellations"][-14:]] == curated_chinese_extras,
            "curated Chinese surrounding-asterism selection changed")
    require(hawaiian["star_identifier"] == "hip" and len(hawaiian["constellations"]) == 13,
            "Hawaiian culture must remain approved 13-group set")

    # Localized names are a separate asset domain, split by culture.
    folder_by_id = {
        "iau-western": "western",
        "chinese-28-mansions": "chinese",
        "hawaiian-starlines": "hawaiian",
    }
    loc_names: dict[str, dict] = {}
    for cid, folder in folder_by_id.items():
        culture_file = json.loads((LOC_SKY / folder / "culture.json").read_text(encoding="utf-8"))
        constellation_file = json.loads((LOC_SKY / folder / "constellations.json").read_text(encoding="utf-8"))
        require(culture_file.get("kind") == "sky_culture_names" and culture_file.get("culture_id") == cid,
                f"invalid culture localization file for {cid}")
        require(constellation_file.get("kind") == "sky_constellation_names" and constellation_file.get("culture_id") == cid,
                f"invalid constellation localization file for {cid}")
        require(culture_file.get("names", {}).get("en"), f"{cid} culture lost English fallback")
        entries_by_id = constellation_file.get("entries", {})
        require(set(entries_by_id) == {c["id"] for c in cultures[cid]["constellations"]},
                f"{cid} localized-name IDs drifted from topology IDs")
        for object_id, names in entries_by_id.items():
            require(names.get("en"), f"{cid}/{object_id} lost English fallback")
        loc_names[cid] = entries_by_id

    for c in western["constellations"]:
        require({"en", "ru"}.issubset(loc_names["iau-western"][c["id"]]),
                f"western {c['id']} lost en/ru names")
    for c in chinese["constellations"]:
        require({"en", "zh-Hans", "ja", "native", "pronounce"}.issubset(
            loc_names["chinese-28-mansions"][c["id"]]),
            f"Chinese {c['id']} lost multilingual names")
    for c in hawaiian["constellations"]:
        require({"en", "zh-Hans", "ja", "native"}.issubset(
            loc_names["hawaiian-starlines"][c["id"]]),
            f"Hawaiian {c['id']} lost multilingual names")

    wings = next((c for c in chinese["constellations"] if c["id"].endswith("-wings")), None)
    require(wings is not None and wings.get("omitted_source_star_ids") == [53975],
            "independently-unverified Chinese HIP omission changed")
    require(not any(53975 in p for p in wings["polylines"]),
            "HIP 53975 was reintroduced without verified support coordinates")

    real = json.loads(read("src/assets/data/galaxy/real_star_catalog.json"))
    real_stars = real.get("stars", [])
    visible = {
        "hr": {s["hr"] for s in real_stars if isinstance(s.get("hr"), int) and s["hr"] > 0},
        "hip": {s["hip"] for s in real_stars if isinstance(s.get("hip"), int) and s["hip"] > 0},
    }
    support = json.loads(read("src/assets/data/galaxy/constellation_support_stars.json"))
    support_ids = {"hr": set(), "hip": set()}
    for star in support.get("stars", []):
        catalog = star.get("catalog")
        ident = star.get("id")
        require(catalog in support_ids and isinstance(ident, int) and ident > 0,
                "invalid support-star entry")
        require(ident not in support_ids[catalog], f"duplicate support {catalog.upper()} {ident}")
        require(ident not in visible[catalog], f"support {catalog.upper()} {ident} duplicates visible catalog")
        support_ids[catalog].add(ident)

    referenced = {"hr": set(), "hip": set()}
    for culture in cultures.values():
        identifier = culture["star_identifier"]
        for constellation in culture["constellations"]:
            for polyline in constellation["polylines"]:
                referenced[identifier].update(polyline)
    for identifier in ("hr", "hip"):
        unresolved = referenced[identifier] - visible[identifier] - support_ids[identifier]
        require(not unresolved, f"unresolved {identifier.upper()} topology IDs: {sorted(unresolved)}")

    catalog_h = read("src/render/starfield/SkyCultureCatalog.h")
    catalog_cpp = read("src/render/starfield/SkyCultureCatalog.cpp")
    star_cpp = read("src/render/starfield/GalaxyStarfieldRenderer.cpp")
    scene_cpp = read("src/scene/SceneRenderer.cpp")
    require("loadLocalizationDirectory" in catalog_h + catalog_cpp,
            "sky display names are not loaded from separate localization domain")
    require("recursive_directory_iterator" in catalog_cpp,
            "sky-localization directory is not recursively discoverable")
    require("localizedFallback" in catalog_cpp and 'findName("en")' in catalog_cpp,
            "sky names lost exact/base/English fallback")
    require('"assets/localization/sky"' in star_cpp,
            "starfield does not load unified sky localization root")
    require("displayName(\n                m_uiLocale" in scene_cpp or "displayName(m_uiLocale)" in scene_cpp,
            "constellation labels are no longer driven by global UI locale")

    print("[PASS] topology-only sky cultures + separated recursive localized names")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
