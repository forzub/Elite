#pragma once

// Private helpers shared by the SystemMapRenderer translation units
// (SystemMapRenderer*.cpp). Not a public interface.

#include "src/game/system_map/SystemMapRenderer.h"
#include "src/input/Input.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <limits>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

#include "render/HUD/TextRenderer.h"
#include "src/game/navigation/NavigationAddressFormatter.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/game/navigation/DockingCompatibility.h"
#include "src/game/navigation/HubCoMovingFrame.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/debug/DebugSettings.h"

namespace system_map_renderer_detail
{
    inline constexpr double AU_KM = 149597870.7;
























    inline constexpr double AU_PER_LIGHT_YEAR = 63241.077084266;

    inline std::string fmtDistanceLy(double ly)
    {
        std::ostringstream ss;

        if (ly < 0.01)
        {
            ss << std::fixed << std::setprecision(4) << ly << " ly";
        }
        else if (ly < 10.0)
        {
            ss << std::fixed << std::setprecision(2) << ly << " ly";
        }
        else
        {
            ss << std::fixed << std::setprecision(1) << ly << " ly";
        }

        return ss.str();
    }





    inline bool lineTriangleIntersection(
        const glm::dvec3& lineOrigin,
        const glm::dvec3& lineDirection,
        const glm::dvec3& a,
        const glm::dvec3& b,
        const glm::dvec3& c,
        double& outT
    )
    {
        constexpr double epsilon = 1.0e-9;
        const glm::dvec3 edge1 = b - a;
        const glm::dvec3 edge2 = c - a;
        const glm::dvec3 p = glm::cross(lineDirection, edge2);
        const double det = glm::dot(edge1, p);
        if (std::abs(det) <= epsilon)
            return false;

        const double invDet = 1.0 / det;
        const glm::dvec3 tvec = lineOrigin - a;
        const double u = glm::dot(tvec, p) * invDet;
        if (u < -epsilon || u > 1.0 + epsilon)
            return false;

        const glm::dvec3 q = glm::cross(tvec, edge1);
        const double v = glm::dot(lineDirection, q) * invDet;
        if (v < -epsilon || u + v > 1.0 + epsilon)
            return false;

        outT = glm::dot(edge2, q) * invDet;
        return true;
    }

    inline glm::dvec3 objectMeshPointToHub(
        const world::celestial::LocalSceneObject& object,
        const glm::mat4& meshToObject,
        const glm::vec3& meshPoint
    )
    {
        const glm::vec4 objectPoint4 =
            meshToObject * glm::vec4(meshPoint, 1.0f);
        const glm::dvec3 objectPoint(objectPoint4);
        return object.positionMeters +
            object.axes.x * objectPoint.x +
            object.axes.y * objectPoint.y +
            object.axes.z * objectPoint.z;
    }

    inline bool hubMeshBodyHitDepth(
        const game::system_map::HubMapCameraSnapshot& camera,
        const world::celestial::LocalSceneObject& object,
        const game::ship::geometry::MeshData& mesh,
        const glm::mat4& meshToObject,
        const glm::dvec3& lineOrigin,
        const glm::dvec3& lineDirection,
        double& ioNearestCameraDepth
    )
    {
        bool hit = false;
        for (const auto& triangle : mesh.triangles)
        {
            if (triangle.v0 < 0 || triangle.v1 < 0 || triangle.v2 < 0 ||
                triangle.v0 >= static_cast<int>(mesh.vertices.size()) ||
                triangle.v1 >= static_cast<int>(mesh.vertices.size()) ||
                triangle.v2 >= static_cast<int>(mesh.vertices.size()))
            {
                continue;
            }

            const glm::dvec3 a = objectMeshPointToHub(
                object,
                meshToObject,
                mesh.vertices[triangle.v0].position
            );
            const glm::dvec3 b = objectMeshPointToHub(
                object,
                meshToObject,
                mesh.vertices[triangle.v1].position
            );
            const glm::dvec3 c = objectMeshPointToHub(
                object,
                meshToObject,
                mesh.vertices[triangle.v2].position
            );

            double t = 0.0;
            if (!lineTriangleIntersection(
                    lineOrigin,
                    lineDirection,
                    a,
                    b,
                    c,
                    t))
            {
                continue;
            }

            const glm::dvec3 hitPoint = lineOrigin + lineDirection * t;
            const double cameraDepth = camera.pointToCamera(hitPoint).z;
            ioNearestCameraDepth = std::max(
                ioNearestCameraDepth,
                cameraDepth
            );
            hit = true;
        }
        return hit;
    }

    inline bool hubInfrastructureBodyHitDepth(
        const game::system_map::HubMapCameraSnapshot& camera,
        const world::celestial::LocalSceneObject& object,
        const glm::dvec2& mousePx,
        double& outNearestCameraDepth
    )
    {
        using game::ship::geometry::AssemblyMeshLibrary;

        if (!object.valid ||
            object.objectClass != world::celestial::DetailObjectClass::Hub ||
            object.typeId == ObjectType::None ||
            !AssemblyMeshLibrary::has(object.typeId))
        {
            return false;
        }

        // Cheap broad phase only. It never authorizes selection; the final
        // answer always comes from triangle intersections with the same CPU
        // mesh definition used by the Hub geometry pass.
        const double finalScale = camera.scale * camera.state.zoom;
        const double broadRadiusPx =
            glm::length(object.sizeMeters) * 0.5 * finalScale + 4.0;
        if (glm::length(mousePx - camera.project(object.positionMeters)) >
            broadRadiusPx)
        {
            return false;
        }

        glm::dvec3 lineDirection =
            camera.vectorFromCamera(glm::dvec3(0.0, 0.0, 1.0));
        const double directionLength = glm::length(lineDirection);
        if (directionLength <= 1.0e-12)
            return false;
        lineDirection /= directionLength;
        const glm::dvec3 lineOrigin = camera.unprojectPlane(mousePx);

        const auto& assembly = AssemblyMeshLibrary::get(object.typeId);
        double nearestDepth = -std::numeric_limits<double>::infinity();
        bool hit = false;

        if (assembly.hasWholeShipProxy &&
            !assembly.wholeShipProxyMesh.triangles.empty())
        {
            hit = hubMeshBodyHitDepth(
                camera,
                object,
                assembly.wholeShipProxyMesh,
                glm::mat4(1.0f),
                lineOrigin,
                lineDirection,
                nearestDepth
            );
        }
        else
        {
            for (const auto& module : assembly.modules)
            {
                const glm::mat4 moduleToObject =
                    world::modules::
                        buildAssemblyModuleStaticHierarchicalLocalModel(
                            assembly,
                            module.id
                        );

                for (const auto& part : module.meshes)
                {
                    const auto& mesh = !part.lod1Mesh.triangles.empty()
                        ? part.lod1Mesh
                        : part.lod0Mesh;
                    if (mesh.triangles.empty())
                        continue;

                    const glm::mat4 partToModule =
                        glm::translate(
                            glm::mat4(1.0f),
                            part.localOffset
                        );

                    hit = hubMeshBodyHitDepth(
                        camera,
                        object,
                        mesh,
                        moduleToObject * partToModule,
                        lineOrigin,
                        lineDirection,
                        nearestDepth
                    ) || hit;
                }
            }
        }

        if (hit)
            outNearestCameraDepth = nearestDepth;
        return hit;
    }

    inline const world::celestial::LocalSceneObject* pickHubInfrastructureBody(
        const game::system_map::HubMapCameraSnapshot& camera,
        const world::celestial::HubMapSnapshot& hub,
        const glm::dvec2& mousePx
    )
    {
        const world::celestial::LocalSceneObject* best = nullptr;
        double bestDepth = -std::numeric_limits<double>::infinity();

        for (const auto& object : hub.scene.objects)
        {
            double depth = -std::numeric_limits<double>::infinity();
            if (!hubInfrastructureBodyHitDepth(
                    camera,
                    object,
                    mousePx,
                    depth))
            {
                continue;
            }

            if (!best || depth > bestDepth + 1.0e-6 ||
                (std::abs(depth - bestDepth) <= 1.0e-6 &&
                 object.stableId < best->stableId))
            {
                best = &object;
                bestDepth = depth;
            }
        }

        return best;
    }

    inline std::string fmt2(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }

    inline std::string fmt4(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << v;
        return ss.str();
    }




































    inline glm::vec4 alphaScaled(
        glm::vec4 color,
        float intensity
    )
    {
        color.a *=
            std::max(
                0.0f,
                intensity
            );

        return color;
    }



























    inline double degToRadD(
        double deg
    )
    {
        return deg *
            glm::pi<double>() /
            180.0;
    }

    inline glm::dvec3 safeNormalizeD(
        const glm::dvec3& v,
        const glm::dvec3& fallback
    )
    {
        const double len =
            glm::length(v);

        if (len <= 1e-12)
            return fallback;

        return v / len;
    }

    inline glm::dvec3 planetNorthAxisWorld(
        const world::celestial::DetailMapSnapshot& planet
    )
    {
        const double tilt =
            degToRadD(
                planet.planetAxialTiltDeg
            );

        const double node =
            degToRadD(
                planet.planetAxisNodeDeg
            );

        // axialTiltDeg = 0 means north is +Y.
        // axisNodeDeg chooses the tilt direction in XZ plane.
        return safeNormalizeD(
            glm::dvec3(
                std::sin(tilt) * std::cos(node),
                std::cos(tilt),
                std::sin(tilt) * std::sin(node)
            ),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    inline glm::dvec3 planetPrimeAxisWorld(
        const glm::dvec3& north
    )
    {
        glm::dvec3 ref(1.0, 0.0, 0.0);

        if (std::abs(glm::dot(ref, north)) > 0.92)
            ref = glm::dvec3(0.0, 0.0, 1.0);

        return safeNormalizeD(
            ref - north * glm::dot(ref, north),
            glm::dvec3(1.0, 0.0, 0.0)
        );
    }

    inline glm::dvec3 planetEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return safeNormalizeD(
            glm::cross(north, prime),
            glm::dvec3(0.0, 0.0, 1.0)
        );
    }

    inline glm::dvec3 systemBodyPrimeAxisWorld(
        const glm::dvec3& north
    )
    {
        return planetPrimeAxisWorld(
            north
        );
    }

    inline glm::dvec3 systemBodyEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return planetEastAxisWorld(
            north,
            prime
        );
    }

    inline glm::dvec3 systemBodyRingAxisYWorld(
        const world::celestial::SystemMapBody& body,
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        const glm::dvec3 east =
            systemBodyEastAxisWorld(
                north,
                prime
            );

        const double inclination =
            degToRadD(
                body.ringPlaneInclinationOffsetDeg
            );

        return safeNormalizeD(
            east * std::cos(inclination) +
            north * std::sin(inclination),
            east
        );
    }

    inline glm::dvec3 planetSurfacePointMeters(
        const world::celestial::DetailMapSnapshot& planet,
        double latitudeRad,
        double textureLongitudeRad,
        double radiusScale = 1.0
    )
    {
        const double radius =
            planet.planetRadiusMeters *
            radiusScale;

        const glm::dvec3 north =
            planetNorthAxisWorld(planet);

        const glm::dvec3 prime0 =
            planetPrimeAxisWorld(north);

        const glm::dvec3 east0 =
            planetEastAxisWorld(
                north,
                prime0
            );

        const double textureOffset =
            degToRadD(
                planet.planetTextureLongitudeOffsetDeg
            );

        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            planet.planetRotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 localWorld =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return
            planet.planetCenterMeters +
            localWorld;
    }








    inline glm::dvec3 systemBodyNorthAxisWorld(
        const world::celestial::SystemMapBody& body
    )
    {
        const double tilt =
            degToRadD(
                body.axialTiltDeg
            );

        const double node =
            degToRadD(
                body.axisNodeDeg
            );

        return safeNormalizeD(
            glm::dvec3(
                std::sin(tilt) * std::cos(node),
                std::cos(tilt),
                std::sin(tilt) * std::sin(node)
            ),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    inline glm::vec3 systemBodySurfacePoint(
        const world::celestial::SystemMapBody& body,
        const glm::vec3& center,
        float radius,
        double latitudeRad,
        double textureLongitudeRad
    )
    {
        const glm::dvec3 north =
            systemBodyNorthAxisWorld(
                body
            );

        const glm::dvec3 prime0 =
            planetPrimeAxisWorld(
                north
            );

        const glm::dvec3 east0 =
            planetEastAxisWorld(
                north,
                prime0
            );

        const double textureOffset =
            degToRadD(
                body.textureLongitudeOffsetDeg
            );

        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            body.rotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 local =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return center +
            glm::vec3(
                static_cast<float>(local.x),
                static_cast<float>(local.y),
                static_cast<float>(local.z)
            );
    }


























    inline double niceSystemMapScaleNumber(
        double value
    )
    {
        if (value <= 0.0 ||
            !std::isfinite(value))
        {
            return 1.0;
        }

        const double exponent =
            std::floor(
                std::log10(value)
            );

        const double base =
            std::pow(
                10.0,
                exponent
            );

        const double normalized =
            value / base;

        double nice =
            1.0;

        if (normalized <= 1.0)
            nice = 1.0;
        else if (normalized <= 2.0)
            nice = 2.0;
        else if (normalized <= 5.0)
            nice = 5.0;
        else
            nice = 10.0;

        return
            nice * base;
    }

    inline std::string fmtSystemMapScaleDistance(
        double km
    )
    {
        std::ostringstream ss;

        if (km >= AU_KM * 0.1)
        {
            ss
                << std::fixed
                << std::setprecision(3)
                << (km / AU_KM)
                << " AU";
        }
        else if (km >= 1000000.0)
        {
            ss
                << std::fixed
                << std::setprecision(2)
                << (km / 1000000.0)
                << " M km";
        }
        else if (km >= 1000.0)
        {
            ss
                << std::fixed
                << std::setprecision(0)
                << km
                << " km";
        }
        else
        {
            ss
                << std::fixed
                << std::setprecision(1)
                << km
                << " km";
        }

        return ss.str();
    }









    inline float wrapAngleRadF(
        float a
    )
    {
        const float twoPi =
            glm::two_pi<float>();

        while (a > glm::pi<float>())
            a -= twoPi;

        while (a < -glm::pi<float>())
            a += twoPi;

        return a;
    }

    inline double wrapAngleRadD(
        double a
    )
    {
        const double twoPi =
            glm::two_pi<double>();

        while (a > glm::pi<double>())
            a -= twoPi;

        while (a < -glm::pi<double>())
            a += twoPi;

        return a;
    }

    inline float galaxyStarTypeVisualScale(
        const std::string& starType
    )
    {
        if (starType.empty())
            return 1.0f;

        const char spectralClass =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        starType.front()
                    )
                )
            );

        float scale = 1.0f;

        switch (spectralClass)
        {
            case 'O': scale = 1.65f; break;
            case 'B': scale = 1.45f; break;
            case 'A': scale = 1.25f; break;
            case 'F': scale = 1.12f; break;
            case 'G': scale = 1.00f; break;
            case 'K': scale = 0.90f; break;
            case 'M': scale = 0.78f; break;

            // Белые и коричневые карлики.
            case 'D': scale = 0.68f; break;
            case 'L': scale = 0.66f; break;
            case 'T': scale = 0.62f; break;

            default: scale = 1.0f; break;
        }

        /*
            Это не физический радиус, а визуальная поправка
            по классу светимости.
        */
        if (starType.find("III") != std::string::npos)
        {
            scale *= 1.50f;
        }
        else if (starType.find("IV") != std::string::npos)
        {
            scale *= 1.20f;
        }

        return std::clamp(
            scale,
            0.58f,
            1.85f
        );
    }



inline game::navigation::RouteTargetRef routeTargetRefForOverlayItem(
    const game::system_map::MapObjectOverlayItem& item
)
{
    using Anchor = game::navigation::NavigationRouteAnchorKind;
    game::navigation::RouteTargetRef target;
    target.systemId = item.trackingSystemId;
    target.spatialWorldPosition = item.trackingWorldPosition;

    if (item.infoKind == game::system_map::MapObjectInfoKind::WaypointCandidate)
    {
        target.kind = Anchor::FreeSpace;
        return target;
    }

    if (item.infoKind == game::system_map::MapObjectInfoKind::Celestial)
    {
        target.kind = Anchor::CelestialBody;
        target.stableObjectId = item.semanticTargetId;
        return target;
    }

    if (item.infoKind == game::system_map::MapObjectInfoKind::DockingPort)
    {
        target.kind = Anchor::SemanticAnchor;
        target.stableObjectId = item.semanticTargetId;
        target.semanticAnchorId = item.semanticAnchorId;
        return target;
    }

    if (item.kind == game::system_map::MapObjectGlyphKind::Ship)
    {
        target.kind = Anchor::Ship;
        target.shipInstanceId = item.shipInstanceId;
        return target;
    }

    if (item.kind == game::system_map::MapObjectGlyphKind::Hub)
    {
        target.kind = Anchor::Hub;
        target.stableObjectId = !item.navigationHubId.empty()
            ? item.navigationHubId
            : item.objectId;
        return target;
    }

    target.kind = Anchor::Infrastructure;
    // Hub-map infrastructure uses a namespaced presentation id while
    // semanticTargetId carries the durable authored module identity.
    if (!item.semanticTargetId.empty())
        target.stableObjectId = item.semanticTargetId;
    else if (item.objectId.rfind("entity:", 0) != 0)
        target.stableObjectId = item.objectId;
    return target;
}
constexpr const char* HubModuleOverlayPrefix = "hub-module:";
constexpr const char* HubDockOverlayPrefix = "hub-dock:";

inline std::string hubDockOverlayId(
    const std::string& moduleId,
    const std::string& anchorId
)
{
    return std::string(HubDockOverlayPrefix) + moduleId + "/" + anchorId;
}

inline bool parseHubDockOverlayId(
    const std::string& objectId,
    std::string& moduleId,
    std::string& anchorId
)
{
    if (objectId.rfind(HubDockOverlayPrefix, 0) != 0)
        return false;

    const std::string rest = objectId.substr(
        std::char_traits<char>::length(HubDockOverlayPrefix)
    );
    const auto separator = rest.find('/');
    if (separator == std::string::npos ||
        separator == 0 ||
        separator + 1 >= rest.size())
    {
        return false;
    }

    moduleId = rest.substr(0, separator);
    anchorId = rest.substr(separator + 1);
    return true;
}

inline std::string formatDockPair(double a, double b)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(0) << a << " × " << b;
    return out.str();
}

inline std::string formatDockScalar(double value, int precision = 0)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

inline glm::dvec3 hubLocalToWorldMetersForOverlay(
    const world::celestial::HubMapSnapshot& hub,
    const glm::dvec3& localMeters
)
{
    return hub.hubWorldPositionMeters +
        hub.hubWorldAxes.x * localMeters.x +
        hub.hubWorldAxes.y * localMeters.y +
        hub.hubWorldAxes.z * localMeters.z;
}

inline std::string navigationCellAddress(
    char prefix,
    const game::navigation::CubicNavigationCell& cell
)
{
    std::ostringstream out;
    out << prefix << cell.level << "["
        << cell.index.x << ","
        << cell.index.y << ","
        << cell.index.z << "]";
    return out.str();
}

inline std::string waypointCandidateId(
    char prefix,
    const game::navigation::CubicNavigationCell& cell
)
{
    std::ostringstream out;
    out << "waypoint_candidate:" << prefix << ":"
        << cell.level << ":"
        << cell.index.x << ":"
        << cell.index.y << ":"
        << cell.index.z;
    return out.str();
}

inline bool waypointHasPrefix(
    const game::navigation::NavigationWaypoint& waypoint,
    char prefix
)
{
    const std::string expected = std::string("waypoint_candidate:") + prefix + ":";
    return waypoint.sourceObjectId.rfind(expected, 0) == 0;
}

inline glm::vec4 waypointRoleColor(game::navigation::NavigationWaypointRole role)
{
    switch (role)
    {
        case game::navigation::NavigationWaypointRole::Finish:
            return glm::vec4(0.44f, 1.00f, 0.62f, 0.96f);
        case game::navigation::NavigationWaypointRole::Intermediate:
            return glm::vec4(0.40f, 0.92f, 0.60f, 0.94f);
        default:
            return glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    }
}

inline const std::string& waypointRoleTypeName(
    game::navigation::NavigationWaypointRole role,
    const game::system_map::NavigationMapTextProfile& textProfile
)
{
    switch (role)
    {
        case game::navigation::NavigationWaypointRole::Finish:
            return textProfile.finishTarget;
        case game::navigation::NavigationWaypointRole::Intermediate:
            return textProfile.intermediateTarget;
        default:
            return textProfile.spaceTarget;
    }
}

inline std::vector<game::system_map::MapObjectPanelAction> waypointPanelActions(
    const game::navigation::RoutePlan& routePlan,
    const game::navigation::NavigationWaypoint& waypoint
)
{
    std::vector<game::system_map::MapObjectPanelAction> actions;

    const bool isFinish =
        waypoint.role == game::navigation::NavigationWaypointRole::Finish;
    const bool isWaypoint =
        waypoint.role == game::navigation::NavigationWaypointRole::Intermediate;

    if (!isFinish)
    {
        game::system_map::MapObjectPanelAction intermediate;
        intermediate.key = "toggle_intermediate";
        intermediate.labelKey =
            isWaypoint ? "cancel_waypoint" : "set_waypoint";
        intermediate.active = isWaypoint;
        actions.push_back(std::move(intermediate));
    }

    if (isFinish || !routePlan.hasFinishWaypoint())
    {
        game::system_map::MapObjectPanelAction finish;
        finish.key = "toggle_finish";
        finish.labelKey = isFinish ? "cancel_finish" : "set_finish";
        finish.active = isFinish;
        actions.push_back(std::move(finish));
    }

    return actions;
}
} // namespace system_map_renderer_detail

using namespace system_map_renderer_detail;
