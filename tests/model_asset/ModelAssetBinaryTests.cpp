#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/ModelAssetBinary.h"

using namespace elite::model_asset;

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(float a, float b, float eps = 1.0e-6f)
{
    return std::abs(a - b) <= eps;
}
}

int main()
{
    try
    {
        ModelAsset asset;
        asset.assetId = "station_test";
        asset.displayName = "Station Test";
        asset.sourceObjectType = 2;
        asset.sourceBasis.preset = "blender_model";
        asset.sourceBasis.up = AxisDirection::PositiveZ;
        asset.sourceBasis.forward = AxisDirection::NegativeY;
        asset.minBounds = {-10.0f, -20.0f, -30.0f};
        asset.maxBounds = {10.0f, 20.0f, 30.0f};

        MaterialDefinition material;
        material.id = "emit_nav_red";
        material.sourceName = "Emit.Nav.Red";
        material.emissiveColor = {1.0f, 0.1f, 0.05f};
        material.emissiveStrength = 2.0f;
        asset.materials.push_back(material);

        GeometryDefinition geometry;
        geometry.id = "habitat_segment";
        geometry.sourceLod0 = "segment.obj";
        geometry.surfaceMode = SurfaceMode::ThinTwoSided;
        MeshLod lod;
        lod.vertices = {
            {{0,0,0},{0,1,0},{0,0}},
            {{1,0,0},{0,1,0},{1,0}},
            {{0,0,1},{0,1,0},{0,1}}
        };
        lod.triangles.push_back({0,1,2,7,0,42});
        lod.edges.push_back({0,1,0,-1,EdgeBoundary,EdgeRenderElite});
        geometry.lods.push_back(lod);
        asset.geometries.push_back(geometry);

        Node a;
        a.id = "segment_a";
        a.geometryIndex = 0;
        a.joint.type = JointType::Revolute;
        a.joint.axis = {0.0f, 1.0f, 0.0f};
        a.joint.defaultRateDegPerSec = 2.0f;
        a.joint.breakable = true;
        a.joint.breakForceN = 250000.0f;
        a.physics.mode = MassPropertyMode::Manual;
        a.physics.massKg = 1200.0f;
        a.physics.centerOfMass = {0.2f, 0.0f, -0.1f};
        a.physics.inertiaDiagonal = {4.0f, 5.0f, 6.0f};
        a.physics.inertiaProducts = {0.1f, 0.2f, 0.3f};
        asset.nodes.push_back(a);
        Node b;
        b.id = "segment_b";
        b.geometryIndex = 0; // explicit instance of the same geometry definition
        b.localRotationDeg = {0.0f, 120.0f, 0.0f};
        asset.nodes.push_back(b);

        CollisionVolume box;
        box.id = "hit.segment.box";
        box.halfSize = {2.0f, 3.0f, 4.0f};
        asset.collisionVolumes.push_back(box);
        CollisionVolume capsule;
        capsule.id = "hit.segment.capsule";
        capsule.shape = CollisionShape::Capsule;
        capsule.radius = 1.25f;
        capsule.halfHeight = 3.5f;
        capsule.localRotationDeg = {0.0f, 0.0f, 45.0f};
        asset.collisionVolumes.push_back(capsule);

        Socket socket;
        socket.id = "light.nav.red";
        socket.kind = "light_point";
        socket.parentNodeIndex = 0;
        socket.localPosition = {0.0f, 1.0f, 2.0f};
        socket.light.type = LightType::Point;
        socket.light.color = {1.0f, 0.0f, 0.0f};
        socket.light.intensity = 12.0f;
        socket.light.rangeMeters = 40.0f;
        asset.sockets.push_back(socket);

        const auto path = std::filesystem::temp_directory_path() / "elite_model_asset_roundtrip.elmodel";
        std::string error;
        require(ModelAssetBinary::save(path.string(), asset, &error), error.c_str());

        ModelAsset loaded;
        require(ModelAssetBinary::load(path.string(), loaded, &error), error.c_str());
        std::filesystem::remove(path);

        require(loaded.assetId == asset.assetId, "asset id lost");
        require(loaded.sourceBasis.preset == "blender_model" && loaded.sourceBasis.up == AxisDirection::PositiveZ,
            "source basis metadata lost");
        require(loaded.materials.size() == 1 && loaded.materials[0].id == "emit_nav_red" && near(loaded.materials[0].emissiveStrength, 2.0f),
            "material semantic id/emissive data lost");
        require(loaded.geometries.size() == 1, "geometry count changed");
        require(loaded.geometries[0].lods[0].triangles[0].materialIndex == 0 && loaded.geometries[0].lods[0].triangles[0].smoothingGroupId == 42,
            "triangle material/smoothing data lost");
        require(loaded.nodes.size() == 2, "node count changed");
        require(loaded.nodes[0].geometryIndex == loaded.nodes[1].geometryIndex,
            "geometry instancing was expanded or lost");
        require(loaded.nodes[0].joint.type == JointType::Revolute && loaded.nodes[0].joint.breakable,
            "joint/break metadata lost");
        require(loaded.nodes[0].physics.mode == MassPropertyMode::Manual && near(loaded.nodes[0].physics.massKg, 1200.0f) && near(loaded.nodes[0].physics.inertiaProducts.z, 0.3f),
            "rigid mass properties lost");
        require(loaded.geometries[0].surfaceMode == SurfaceMode::ThinTwoSided,
            "thin-surface mode lost");
        require(loaded.geometries[0].lods[0].edges[0].renderMask == EdgeRenderElite,
            "edge render mask lost");
        require(loaded.collisionVolumes.size() == 2 && loaded.collisionVolumes[1].shape == CollisionShape::Capsule && near(loaded.collisionVolumes[1].halfHeight, 3.5f),
            "compound collision primitives lost");
        require(loaded.sockets.size() == 1 && loaded.sockets[0].light.type == LightType::Point && near(loaded.sockets[0].light.intensity, 12.0f),
            "typed light anchor lost");

        std::cout << "[PASS] model asset v2 roundtrip preserves materials/instances/collision/joints/mass/lights\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }
}
