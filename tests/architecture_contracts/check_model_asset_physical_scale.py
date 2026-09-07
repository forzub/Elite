#!/usr/bin/env python3
"""v0.10.62: raw authoring WORKING + asset-wide sourceToMeters + metric BUILD copy."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


model = text("src/model_asset/ModelAsset.h")
binary = text("src/model_asset/ModelAssetBinary.cpp")
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
importer = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
web = text("src/assets/webui/model_asset_editor.html")
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")
pipeline = text("src/model_asset/MODEL_ASSET_PIPELINE.md")
model_tests = text("tests/model_asset/ModelAssetBinaryTests.cpp")
version = text("tools/model_asset_editor/EditorVersion.h")

for token in (
    "enum class PhysicalGeometrySpace",
    "Authoring = 0",
    "Meters = 1",
    "LegacyUnknown = 2",
    "float sourceExtent = 0.0f;",
    "float sourceToMeters = 1.0f;",
    "bool gameLinked = false;",
    "glm::vec3 gameDimensionsMeters",
    "PhysicalGeometrySpace geometrySpace = PhysicalGeometrySpace::Authoring;",
):
    if token not in model:
        raise AssertionError(f"physical scale data contract missing {token!r}")

for token in (
    "w.pod(a.physicalSize.sourceExtent);",
    "w.pod(a.physicalSize.sourceToMeters);",
    "w.vec3(a.physicalSize.gameDimensionsMeters);",
    "w.pod(static_cast<std::uint8_t>(a.physicalSize.geometrySpace));",
    "PhysicalGeometrySpace::LegacyUnknown",
):
    if token not in binary:
        raise AssertionError(f"SIZE persistence/legacy fence missing {token!r}")

calibrate = between(
    session,
    'if (command == "set_physical_size_profile" || command == "apply_physical_size" ||',
    'if (command == "add_socket")',
)
for token in (
    "const float sourceExtent = authoringAxisExtent(m_asset, profile.axis);",
    "profile.sourceExtent = sourceExtent;",
    "profile.sourceToMeters = profile.targetMeters / sourceExtent;",
    "profile.geometrySpace = PhysicalGeometrySpace::Authoring;",
    "invalidateWizardFrom(\"physics\")",
    "WORKING geometry was not resized",
):
    if token not in calibrate:
        raise AssertionError(f"manual scale calibration missing {token!r}")
for forbidden in (
    "scaleAuthoringDistancesUniform(m_asset",
    "scaleModelAssetUniform",
    "sendAsset(",
):
    if forbidden in calibrate:
        raise AssertionError(f"calibration still mutates/reloads WORKING geometry: {forbidden!r}")

build = between(
    session,
    "bool ModelAssetEditorSession::buildProductionAsset()",
    "std::string ModelAssetEditorSession::maintenanceComponentId(",
)
for token in (
    "ModelAsset productionAsset = m_asset;",
    "scaleAuthoringDistancesUniform(productionAsset, productionAsset.physicalSize.sourceToMeters);",
    "productionAsset.physicalSize.geometrySpace = PhysicalGeometrySpace::Meters;",
    "ModelAssetBinary::save(path.string(), productionAsset, &error)",
    "saved WORKING remains unchanged authoring space",
):
    if token not in build:
        raise AssertionError(f"metric BUILD-copy boundary missing {token!r}")
if "scaleAuthoringDistancesUniform(m_asset" in build:
    raise AssertionError("BUILD mutates the editor WORKING asset")

# The only m_asset inverse conversion is adoption/migration of a metric package;
# no SOURCE maintenance path may scale freshly imported mesh bytes.
scan = between(
    session,
    "void ModelAssetEditorSession::sendSourceChangeScan()",
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
)
reload = between(
    session,
    "bool ModelAssetEditorSession::reloadMeshFromSource(",
    "bool ModelAssetEditorSession::replaceSourcePart(",
)
for label, body in (("scan", scan), ("per-mesh reload", reload)):
    if "scaleAuthoringDistancesUniform" in body or "scaleModelAssetUniform" in body:
        raise AssertionError(f"{label} applies physical scaling to SOURCE geometry")

for token in (
    "asset.physicalSize.gameLinked = true;",
    "asset.physicalSize.gameDimensionsMeters",
    "asset.physicalSize.enabled = false;",
    "asset.physicalSize.sourceToMeters = 1.0f;",
):
    if token not in importer:
        raise AssertionError(f"runtime dimensions are not read-only game context: {token!r}")
if "asset.physicalSize.autoApplyOnSourceImport = true" in importer:
    raise AssertionError("runtime bootstrap still enables destructive auto-scale")

for token in (
    "PHYSICAL SCALE · AUTHORING → METERS",
    "AUTHORING SIZE · RAW SOURCE SPACE",
    "GAME LINK · NOT LINKED",
    "SOURCE → METERS",
    "SET SCALE CONTRACT",
    "calibrate_physical_scale",
):
    if token not in web:
        raise AssertionError(f"physical-scale UI missing {token!r}")
for forbidden in ("PHYSICAL SIZE · UNIFORM ASSET SCALE", "APPLY AFTER SOURCE REIMPORT"):
    if forbidden in web:
        raise AssertionError(f"retired destructive-scale UI survived: {forbidden!r}")

for token in (
    'state["schemaVersion"] = 15',
    'state["physicalScaleGraph"]',
    '{"sourceToMeters", m_asset.physicalSize.sourceToMeters}',
    '{"geometrySpace", physicalGeometrySpaceName(m_asset.physicalSize.geometrySpace)}',
):
    if token not in session:
        raise AssertionError(f"schema-15 physical scale graph missing {token!r}")

for token in (
    "PrimitiveMass primitiveMass(const CollisionVolume& c, float density, float sourceToMeters)",
    "c.localPosition * scale",
    "c.radius * scale",
    "c.halfSize * scale",
):
    if token not in session:
        raise AssertionError(f"SI physics conversion missing {token!r}")

for token in (
    "physical-scale graph lost in v4 SIZE chunk round trip",
    "PhysicalGeometrySpace::Authoring",
):
    if token not in model_tests:
        raise AssertionError(f"binary scale regression anchor missing {token!r}")

for token in (
    "Physical scale boundary — authoring space in WORKING, meters only at BUILD",
    "SOURCE OBJ coordinates are imported exactly as authored",
    "sourceToMeters",
    "temporary production copy",
    "Do not reintroduce `scaleModelAssetUniform(m_asset, ...)`",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT scale boundary missing {token!r}")


for token in (
    "WORKING remains in raw authoring coordinates",
    "sourceToMeters = targetMeters / sourceExtent",
    "temporary production copy",
    "must not apply a second scale",
):
    if token not in pipeline:
        raise AssertionError(f"MODEL_ASSET_PIPELINE physical boundary missing {token!r}")

if 'ModelAssetEditorVersion = "0.10.63"' not in version:
    raise AssertionError("editor version is not 0.10.63")

print("[PASS] model asset editor v0.10.63 authoring-space scale / metric BUILD copy / SI physics")
