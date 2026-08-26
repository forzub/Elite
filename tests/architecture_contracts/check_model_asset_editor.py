from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")

def require(path: str, *tokens: str) -> None:
    data = text(path)
    for token in tokens:
        if token not in data:
            raise AssertionError(f"{path} missing {token!r}")

# The compiled model contract is shared between editor and future runtime. It
# may contain geometry/physics semantics but must stay independent of OpenGL,
# SceneRenderer and game presentation state.
model = text("src/model_asset/ModelAsset.h")
for forbidden in ("glad/", "GLFW", "MeshGPU", "SceneRenderer", "SpaceState"):
    if forbidden in model:
        raise AssertionError(f"shared ModelAsset leaked runtime/render dependency {forbidden!r}")

require(
    "src/model_asset/ModelAsset.h",
    "ModelAssetFormatVersion = 2",
    "struct SourceBasis",
    "struct MaterialDefinition",
    "struct GeometryDefinition",
    "struct NodeJoint",
    "struct RigidBodyProperties",
    "inertiaProducts",
    "struct CollisionVolume",
    "Capsule",
    "struct Socket",
    "LightProperties",
    "ThinTwoSided",
    "EdgeTriangulationInternal",
    "EdgeRenderTechnical",
    "EdgeRenderElite",
)
require(
    "src/model_asset/ModelAssetBinary.cpp",
    "{'M','A','T','L'}",
    "{'G','E','O','M'}",
    "{'N','O','D','E'}",
    "{'C','O','L','L'}",
    "{'S','O','C','K'}",
)

# Stage 2 mesh compilation must bypass the old runtime ObjLoader so source
# polygon ids, authored corner normals, UVs and material ids survive.
require(
    "tools/model_asset_editor/NativeObjImporter.cpp",
    "tinyobj::LoadObj",
    "polygonId",
    "materialIndexFor",
    "EdgeTriangulationInternal",
    "EdgeMaterialSeam",
    "EdgeNormalSeam",
)
importer = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
for forbidden in ("AssemblyMeshLibrary", "ObjLoader", "MeshData"):
    if forbidden in importer:
        raise AssertionError(f"editor importer still depends on old runtime mesh processing {forbidden!r}")
require(
    "tools/model_asset_editor/RuntimeAssemblyImporter.cpp",
    "ObjectAssemblyRegistry::get",
    "importObjNative",
    "geometryBySource",
    "moduleNodeById",
    "ShipAttachmentPoint",
    "JointType::Revolute",
)
require(
    "src/assets/webui/model_asset_editor.html",
    "Raycaster",
    "Edit edges",
    "Isolate",
    "Hit volumes",
    "Capsule",
    "Duplicate instance",
    "Break instance",
    "Radial array",
    "Joint / break attachment",
    "Center of mass",
    "Materials",
    "Light payload",
    "ioStatusBar",
    "Blender axes → Game",
    "ioActivity",
    "set_node_geometry",
)
require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    '"reading"',
    '"writing"',
    "generate_radial_capsules",
    "create_radial_instances",
    "estimatePhysicsFromCollision",
    "convertAssetBasisToCanonical",
    "delete_unused_geometries",
)

cmake = text("CMakeLists.txt")
if "ELITE_BUILD_ASSET_EDITOR" not in cmake or "EliteModelAsset" not in cmake or "NativeObjImporter.cpp" not in cmake:
    raise AssertionError("standalone asset-editor/native compiler targets are not wired")

# Editor-first gate remains intact: no runtime model migration in this patch.
for game_path in (
    "src/scene/SceneRenderer.cpp",
    "src/render/geometry/AssemblyGpuLibrary.cpp",
    "src/game/geometry/AssemblyMeshLibrary.cpp",
):
    if "ModelAssetBinary" in text(game_path):
        raise AssertionError(f"{game_path} migrated runtime model loading before the editor gate")

print("[PASS] model asset editor stage2 native/topology/physics/instancing boundary")
