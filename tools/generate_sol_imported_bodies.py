from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

BODY_VISUALS_DIR = ROOT / "src/assets/data/celestial/body_visuals/sol"
SOURCE_CATALOG_DIR = ROOT / "src/assets/data/celestial/source_catalog/sol"

BODY_VISUALS_DIR.mkdir(parents=True, exist_ok=True)
SOURCE_CATALOG_DIR.mkdir(parents=True, exist_ok=True)


def body_spec(
    folder,
    display_name,
    preset_id,
    parent=None,
    has_atmosphere=False,
    has_cloud_layer=False,
    has_emission=False,
    shape="sphere",
    optional_assets=None,
    data_confidence="provisional",
):
    return {
        "folder": folder,
        "display_name": display_name,
        "preset_id": preset_id,
        "parent": parent,
        "has_atmosphere": has_atmosphere,
        "has_cloud_layer": has_cloud_layer,
        "has_emission": has_emission,
        "shape": shape,
        "optional_assets": optional_assets or [],
        "data_confidence": data_confidence,
    }


BODIES = [
    # ------------------------------------------------------------------
    # Inner planets
    # ------------------------------------------------------------------
    body_spec(
        folder="mercury",
        display_name="Mercury",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
    body_spec(
        folder="venus",
        display_name="Venus",
        preset_id="venusian",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["height", "clouds"],
    ),
    body_spec(
        folder="earth",
        display_name="Earth",
        preset_id="terrestrial",
        has_atmosphere=True,
        has_cloud_layer=True,
        has_emission=True,
        optional_assets=["height", "clouds", "emission"],
        data_confidence="authoritative",
    ),
    body_spec(
        folder="moon",
        display_name="Moon",
        parent="Earth",
        preset_id="rocky_barren",
        optional_assets=["height"],
        data_confidence="authoritative",
    ),
    body_spec(
        folder="mars",
        display_name="Mars",
        preset_id="desert_world",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["height", "clouds"],
        data_confidence="authoritative",
    ),
    body_spec(
        folder="phobos",
        display_name="Phobos",
        parent="Mars",
        preset_id="rocky_barren",
        shape="irregular_mesh",
        data_confidence="authoritative",
    ),
    body_spec(
        folder="deimos",
        display_name="Deimos",
        parent="Mars",
        preset_id="rocky_barren",
        shape="irregular_mesh",
        data_confidence="authoritative",
    ),

    # ------------------------------------------------------------------
    # Jupiter system
    # ------------------------------------------------------------------
    body_spec(
        folder="jupiter",
        display_name="Jupiter",
        preset_id="gas_giant",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["clouds"],
    ),
    body_spec(
        folder="io",
        display_name="Io",
        parent="Jupiter",
        preset_id="lava_world",
        has_emission=True,
        optional_assets=["height", "emission"],
    ),
    body_spec(
        folder="europa",
        display_name="Europa",
        parent="Jupiter",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="ganymede",
        display_name="Ganymede",
        parent="Jupiter",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="callisto",
        display_name="Callisto",
        parent="Jupiter",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="amalthea",
        display_name="Amalthea",
        parent="Jupiter",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
    body_spec(
        folder="himalia",
        display_name="Himalia",
        parent="Jupiter",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
    body_spec(
        folder="elara",
        display_name="Elara",
        parent="Jupiter",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
    body_spec(
        folder="pasiphae",
        display_name="Pasiphae",
        parent="Jupiter",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),

    # ------------------------------------------------------------------
    # Saturn system
    # ------------------------------------------------------------------
    body_spec(
        folder="saturn",
        display_name="Saturn",
        preset_id="gas_giant",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["clouds"],
    ),
    body_spec(
        folder="titan",
        display_name="Titan",
        parent="Saturn",
        preset_id="rocky_ice",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["height", "clouds"],
    ),
    body_spec(
        folder="rhea",
        display_name="Rhea",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="iapetus",
        display_name="Iapetus",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="dione",
        display_name="Dione",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="tethys",
        display_name="Tethys",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="enceladus",
        display_name="Enceladus",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="mimas",
        display_name="Mimas",
        parent="Saturn",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="phoebe",
        display_name="Phoebe",
        parent="Saturn",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),

    # ------------------------------------------------------------------
    # Uranus system
    # ------------------------------------------------------------------
    body_spec(
        folder="uranus",
        display_name="Uranus",
        preset_id="ice_giant",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["clouds"],
    ),
    body_spec(
        folder="titania",
        display_name="Titania",
        parent="Uranus",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="oberon",
        display_name="Oberon",
        parent="Uranus",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="umbriel",
        display_name="Umbriel",
        parent="Uranus",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="ariel",
        display_name="Ariel",
        parent="Uranus",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="miranda",
        display_name="Miranda",
        parent="Uranus",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),

    # ------------------------------------------------------------------
    # Neptune system
    # ------------------------------------------------------------------
    body_spec(
        folder="neptune",
        display_name="Neptune",
        preset_id="ice_giant",
        has_atmosphere=True,
        has_cloud_layer=True,
        optional_assets=["clouds"],
    ),
    body_spec(
        folder="triton",
        display_name="Triton",
        parent="Neptune",
        preset_id="rocky_ice",
        has_atmosphere=True,
        optional_assets=["height"],
    ),
    body_spec(
        folder="proteus",
        display_name="Proteus",
        parent="Neptune",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
    body_spec(
        folder="nereid",
        display_name="Nereid",
        parent="Neptune",
        preset_id="rocky_ice",
        optional_assets=["height"],
    ),
    body_spec(
        folder="larissa",
        display_name="Larissa",
        parent="Neptune",
        preset_id="rocky_barren",
        optional_assets=["height"],
    ),
]


def source_body_id(spec):
    if spec["parent"]:
        return f"system_0.Sol.{spec['parent']}.{spec['display_name']}"
    return f"system_0.Sol.{spec['display_name']}"


def body_notes(spec):
    if spec["shape"] == "irregular_mesh":
        return (
            f"{spec['display_name']} requires a real irregular shape model and "
            f"matching surface texture. Do not generate it as a sphere."
        )

    if spec["data_confidence"] == "authoritative":
        return (
            f"{spec['display_name']} should be imported from curated source assets "
            f"rather than generated procedurally."
        )

    return (
        f"{spec['display_name']} is imported from source assets. "
        f"Source quality may be upgraded later without changing the pipeline contract."
    )


def make_body_visual(spec):
    return {
        "preset_id": spec["preset_id"],
        "system_folder_name": "sol",
        "body_folder_name": spec["folder"],
        "display_name": spec["display_name"],
        "source_system_id": "0",
        "source_body_id": source_body_id(spec),
        "surface_source_kind": "ImportedRealData",
        "source_catalog_path": (
            f"src/assets/data/celestial/source_catalog/sol/{spec['folder']}.source.json"
        ),
        "data_confidence": spec["data_confidence"],
        "notes": body_notes(spec),
        "has_atmosphere": spec["has_atmosphere"],
        "has_cloud_layer": spec["has_cloud_layer"],
        "has_emission": spec["has_emission"],
    }


def make_source_catalog(spec):
    folder = spec["folder"]
    body_name = spec["display_name"]

    required_assets = {
        "albedo": f"src/assets/source/celestial/sol/{folder}/albedo_source.*"
    }

    if spec["shape"] == "irregular_mesh":
        required_assets = {
            "mesh": f"src/assets/source/celestial/sol/{folder}/shape_model_source.*",
            "albedo": f"src/assets/source/celestial/sol/{folder}/albedo_source.*",
        }

    optional_assets = {}

    if "height" in spec["optional_assets"]:
        optional_assets["height"] = (
            f"src/assets/source/celestial/sol/{folder}/height_source.*"
        )

    if "clouds" in spec["optional_assets"]:
        optional_assets["clouds"] = (
            f"src/assets/source/celestial/sol/{folder}/clouds_source.*"
        )

    if "emission" in spec["optional_assets"]:
        optional_assets["emission"] = (
            f"src/assets/source/celestial/sol/{folder}/emission_source.*"
        )

    derived_outputs = {}

    if spec["shape"] == "irregular_mesh":
        derived_outputs["mesh"] = (
            f"src/assets/generated/celestial/sol/{folder}/shape_model.obj"
        )

    derived_outputs["albedo"] = (
        f"src/assets/generated/celestial/sol/{folder}/albedo.tga"
    )
    derived_outputs["normal"] = (
        f"src/assets/generated/celestial/sol/{folder}/normal.tga"
    )
    derived_outputs["material"] = (
        f"src/assets/generated/celestial/sol/{folder}/material.tga"
    )

    if "clouds" in spec["optional_assets"]:
        derived_outputs["clouds"] = (
            f"src/assets/generated/celestial/sol/{folder}/clouds.tga"
        )

    if "emission" in spec["optional_assets"]:
        derived_outputs["emission"] = (
            f"src/assets/generated/celestial/sol/{folder}/emission.tga"
        )

    derived_outputs["preview"] = (
        f"src/assets/generated/celestial/sol/{folder}/preview.tga"
    )
    derived_outputs["meta"] = (
        f"src/assets/generated/celestial/sol/{folder}/meta.json"
    )

    return {
        "system_id": "sol",
        "body_id": folder,
        "body_name": body_name,
        "source_policy": "authoritative_import_required",
        "body_shape": spec["shape"],
        "projection": "mesh_uv" if spec["shape"] == "irregular_mesh" else "equirectangular",
        "required_assets": required_assets,
        "optional_assets": optional_assets,
        "derived_outputs": derived_outputs,
    }


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
        f.write("\n")


def main():
    for spec in BODIES:
        body_visual_path = BODY_VISUALS_DIR / f"{spec['folder']}.json"
        source_catalog_path = SOURCE_CATALOG_DIR / f"{spec['folder']}.source.json"

        write_json(body_visual_path, make_body_visual(spec))
        write_json(source_catalog_path, make_source_catalog(spec))

        print(f"written: {body_visual_path.relative_to(ROOT)}")
        print(f"written: {source_catalog_path.relative_to(ROOT)}")

    print()
    print(f"done: generated {len(BODIES)} body visuals")
    print(f"done: generated {len(BODIES)} source catalogs")


if __name__ == "__main__":
    main()