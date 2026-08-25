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

namespace
{
    constexpr double AU_KM = 149597870.7;
























    constexpr double AU_PER_LIGHT_YEAR = 63241.077084266;

    std::string fmtDistanceLy(double ly)
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





    bool lineTriangleIntersection(
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

    glm::dvec3 objectMeshPointToHub(
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

    bool hubMeshBodyHitDepth(
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

    bool hubInfrastructureBodyHitDepth(
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

    const world::celestial::LocalSceneObject* pickHubInfrastructureBody(
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

    std::string fmt2(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }

    std::string fmt4(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << v;
        return ss.str();
    }




































    glm::vec4 alphaScaled(
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



























    double degToRadD(
        double deg
    )
    {
        return deg *
            glm::pi<double>() /
            180.0;
    }

    glm::dvec3 safeNormalizeD(
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

    glm::dvec3 planetNorthAxisWorld(
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

    glm::dvec3 planetPrimeAxisWorld(
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

    glm::dvec3 planetEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return safeNormalizeD(
            glm::cross(north, prime),
            glm::dvec3(0.0, 0.0, 1.0)
        );
    }

    glm::dvec3 systemBodyPrimeAxisWorld(
        const glm::dvec3& north
    )
    {
        return planetPrimeAxisWorld(
            north
        );
    }

    glm::dvec3 systemBodyEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return planetEastAxisWorld(
            north,
            prime
        );
    }

    glm::dvec3 systemBodyRingAxisYWorld(
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

    glm::dvec3 planetSurfacePointMeters(
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








    glm::dvec3 systemBodyNorthAxisWorld(
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

    glm::vec3 systemBodySurfacePoint(
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


























    double niceSystemMapScaleNumber(
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

    std::string fmtSystemMapScaleDistance(
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









    float wrapAngleRadF(
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

    double wrapAngleRadD(
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

    float galaxyStarTypeVisualScale(
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



}





















double SystemMapRenderer::currentTimeSeconds() const
{
    return glfwGetTime();
}


void SystemMapRenderer::drawMapStarfield(
    const Viewport& viewport,
    const glm::dvec3& observerPositionLy,
    const glm::mat4& cameraView,
    float fieldOfViewDeg,
    float sizeScale,
    bool distantGalaxyBackdrop,
    float starBrightnessScale,
    float milkyWayIntensityScale,
    const glm::vec3& milkyWayColorTint
)
{
    m_mapResources.drawStarfield(
        viewport,
        observerPositionLy,
        cameraView,
        fieldOfViewDeg,
        sizeScale,
        distantGalaxyBackdrop,
        starBrightnessScale,
        milkyWayIntensityScale,
        milkyWayColorTint
    );
}


void SystemMapRenderer::beginTextFrame(
    int viewportWidth,
    int viewportHeight
)
{
    TextRenderer::instance().beginFrameForViewport(
        viewportWidth,
        viewportHeight
    );
}


void SystemMapRenderer::drawTextPx(
    const std::string& text,
    float x,
    float y,
    int pixelHeight,
    const glm::vec4& color
)
{
    TextRenderer::instance().textDrawPx(
        text,
        x,
        y,
        pixelHeight,
        color
    );
}


void SystemMapRenderer::endTextFrame()
{
    TextRenderer::instance().endFrame();
}


SystemMapRenderer::SystemMapRenderer(
    game::navigation::ClientNavigationWorkspace& navigationWorkspace
)
    : m_navigationWorkspace(navigationWorkspace),
      m_detailBackend(m_mapResources),
      m_hubBackend(m_mapResources)
{
}


void SystemMapRenderer::init()
{
    ensureGlObjects();
    ensureShader();

    ensureTexturedGlObjects();
    ensureTexturedShader();

    ensureBackground();

    m_mapResources.init(
        m_galaxyView.visuals().starfieldMinimumDistanceLy
    );

    game::navigation::CoordinateDisplayService::instance().setFormat(
        game::navigation::navigationCoordinateFormatFromString(
            m_galaxyView.state().navigationGrid
                .config()
                .defaultCoordinateFormat
        )
    );

    if (!m_navigationRegionCatalog.loaded())
    {
        const bool namesLoaded =
            m_navigationRegionCatalog.loadFromRuntimeOrSource(
                "assets/localization/world/navigation_regions",
                "src/assets/localization/world/navigation_regions"
            );

        if (!namesLoaded)
        {
            std::cerr
                << "[SystemMapRenderer] navigation region names not loaded\n";
        }
    }

    m_initialized = true;
}























void SystemMapRenderer::ensureGlObjects()
{
    if (m_vao && m_vbo)
        return;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, pos))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}







void SystemMapRenderer::ensureTexturedGlObjects()
{
    if (m_texturedVao && m_texturedVbo)
        return;

    glGenVertexArrays(1, &m_texturedVao);
    glGenBuffers(1, &m_texturedVbo);

    glBindVertexArray(m_texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texturedVbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, pos))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, uv))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
















void SystemMapRenderer::ensureShader()
{
    if (m_shader)
        return;

    m_shader = ShaderLibrary::instance().get("system_map_lines");

    if (!m_shader)
    {

        return;
    }



    m_mvpLoc = glGetUniformLocation(m_shader, "uMVP");
}






void SystemMapRenderer::ensureTexturedShader()
{
    if (m_texturedShader)
        return;

    m_texturedShader =
        ShaderLibrary::instance().get("system_map_body_preview");

    if (!m_texturedShader)
    {
        static bool warned = false;

        if (!warned)
        {
            warned = true;

            std::cerr
                << "[SystemMapRenderer] shader system_map_body_preview not available; "
                << "map body previews disabled.\n";
        }

        return;
    }

    m_texturedMvpLoc =
        glGetUniformLocation(m_texturedShader, "uMVP");

    m_texturedSamplerLoc =
        glGetUniformLocation(m_texturedShader, "uTexture");
}



























void SystemMapRenderer::resetView()
{
    m_mode = Mode::Galaxy;
    m_galaxyView.reset();
    m_systemView.reset();

    m_navigationLevelAnnouncement.text.clear();
    m_navigationLevelAnnouncement.startedAtSeconds = -1.0;

    m_detailView.reset();
    m_hubView.reset();
    m_detailPresentation =
        game::system_map::DetailMapPresentation{};
    m_hubPresentation =
        game::system_map::HubMapPresentation{};
    m_pendingScrollY = 0.0;
    m_systemPresentation =
        game::system_map::SystemMapPresentation{};
    m_systemSceneFrame =
        game::system_map::SystemMapSceneFrame{};
    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;


    m_systemView.state().lastScale = 1.0f;


    m_mapResources.resetPresentationTime();

    m_navigationLevelZeroButtonHovered = false;
    m_navigationTrackButtonHovered = false;
    m_navigationOverlayLeftWasDown = false;
    m_objectOverlayState.clearTransientDrag();
    m_routeOverlayState.clearTransientDrag();

}





















void SystemMapRenderer::onGalaxyMapEntered(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation
)
{
    m_galaxyView.onEntered(
        galaxy,
        navigation
    );
}


void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    m_galaxyView.focusSystem(
        systemId,
        galaxy,
        m_mode == Mode::Galaxy,
        glfwGetTime()
    );
}


int SystemMapRenderer::selectedSystemId() const
{
    return m_galaxyView.state().selectedSystemId;
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::selectedGalaxyEntryIntent(
    const world::celestial::GalaxyMapSnapshot& galaxy) const
{
    if (m_mode != Mode::Galaxy)
        return std::nullopt;

    const auto& state = m_galaxyView.state();

    // A named system selected from the native list can be opened at any
    // Galaxy grid level, matching the old STAR ATLAS dropdown semantics.
    if (state.selectedSystemId >= 0)
    {
        const auto system = std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& candidate)
            {
                return candidate.id == state.selectedSystemId;
            });
        if (system != galaxy.systems.end())
        {
            return game::system_map::MapIntent::enterKnownSystem(
                system->id,
                system->positionLy);
        }
    }

    // Empty-space entry is meaningful only at the terminal Galaxy cube.
    if (!state.navigationFocusValid ||
        state.navigationGrid.level() != state.navigationGrid.maximumLevel())
    {
        return std::nullopt;
    }

    const auto intent = m_galaxyView.entryIntentForPosition(
        galaxy,
        state.navigationFocusLy);
    return intent.valid() ? std::optional<game::system_map::MapIntent>(intent)
                          : std::nullopt;
}

int SystemMapRenderer::focusedSystemId() const
{
    return m_galaxyView.state().focusedSystemId;
}























void SystemMapRenderer::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    const Mode previousMode = m_mode;

    if (previousMode == Mode::Detail)
    {
        auto& systemState = m_systemView.state();
        const auto& detailState = m_detailView.state();

        systemState.selectedHubId =
            detailState.selectedHubId;
        systemState.selectedHubParentBodyId =
            detailState.selectedHubParentBodyId;

        if (!systemState.selectedHubId.empty())
            systemState.selectedBodyId.clear();
    }

    m_mode = mode;

    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;
    m_objectOverlayState.clearTransientDrag();
    m_routeOverlayState.clearTransientDrag();

    /*
        Если пользователь открыл другую карту во время перелёта,
        сохраняем конечную позицию Galaxy-камеры.
    */
    if (m_mode != Mode::Galaxy)
    {
        m_galaxyView.cancelCameraFlight(
            true
        );
    }

    if (m_mode == Mode::Detail)
    {
        m_detailView.reset();
        m_detailView.selectHub(
            m_systemView.state().selectedHubId,
            m_systemView.state().selectedHubParentBodyId
        );
        m_detailPresentation =
            game::system_map::DetailMapPresentation{};
    }

    if (m_mode == Mode::Hub)
    {
        if (previousMode != Mode::Detail)
        {
            m_detailView.selectHub(
                m_systemView.state().selectedHubId,
                m_systemView.state().selectedHubParentBodyId
            );
        }

        m_hubView.beginScene();
        m_hubPresentation =
            game::system_map::HubMapPresentation{};
    }
}



SystemMapRenderer::Mode SystemMapRenderer::mode() const
{
    return m_mode;
}









void SystemMapRenderer::beginLines()
{
    m_vertices.clear();
}

void SystemMapRenderer::addLine(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec4& color
)
{
    m_vertices.push_back({ a, color });
    m_vertices.push_back({ b, color });
}

void SystemMapRenderer::addCircleXZ(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments = std::max(12, segments);

    for (int i = 0; i < segments; ++i)
    {
        const float a0 = float(i) / float(segments) * glm::two_pi<float>();
        const float a1 = float(i + 1) / float(segments) * glm::two_pi<float>();

        glm::vec3 p0 =
            center + glm::vec3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);

        glm::vec3 p1 =
            center + glm::vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);

        addLine(p0, p1, color);
    }
}




void SystemMapRenderer::addCircleXY(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments = std::max(12, segments);

    for (int i = 0; i < segments; ++i)
    {
        const float a0 = float(i) / float(segments) * glm::two_pi<float>();
        const float a1 = float(i + 1) / float(segments) * glm::two_pi<float>();

        glm::vec3 p0 = center + glm::vec3(std::cos(a0) * radius, std::sin(a0) * radius, 0.0f);
        glm::vec3 p1 = center + glm::vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f);

        addLine(p0, p1, color);
    }
}



void SystemMapRenderer::addOrbitCircle3D(
    const glm::vec3& center,
    float radius,
    double inclinationDeg,
    double longitudeOfAscendingNodeDeg,
    double argumentOfPeriapsisDeg,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glm::dmat4 rot(
        1.0
    );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                longitudeOfAscendingNodeDeg
            ),
            glm::dvec3(
                0.0,
                1.0,
                0.0
            )
        );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                inclinationDeg
            ),
            glm::dvec3(
                1.0,
                0.0,
                0.0
            )
        );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                argumentOfPeriapsisDeg
            ),
            glm::dvec3(
                0.0,
                1.0,
                0.0
            )
        );

    for (int i = 0; i < segments; ++i)
    {
        const double a0 =
            static_cast<double>(i) /
            static_cast<double>(segments) *
            glm::two_pi<double>();

        const double a1 =
            static_cast<double>(i + 1) /
            static_cast<double>(segments) *
            glm::two_pi<double>();

        const glm::dvec3 local0(
            std::cos(a0) *
                static_cast<double>(radius),
            0.0,
            std::sin(a0) *
                static_cast<double>(radius)
        );

        const glm::dvec3 local1(
            std::cos(a1) *
                static_cast<double>(radius),
            0.0,
            std::sin(a1) *
                static_cast<double>(radius)
        );

        const glm::dvec3 rotated0 =
            glm::dvec3(
                rot *
                glm::dvec4(
                    local0,
                    0.0
                )
            );

        const glm::dvec3 rotated1 =
            glm::dvec3(
                rot *
                glm::dvec4(
                    local1,
                    0.0
                )
            );

        addLine(
            center +
                glm::vec3(
                    static_cast<float>(rotated0.x),
                    static_cast<float>(rotated0.y),
                    static_cast<float>(rotated0.z)
                ),
            center +
                glm::vec3(
                    static_cast<float>(rotated1.x),
                    static_cast<float>(rotated1.y),
                    static_cast<float>(rotated1.z)
                ),
            color
        );
    }
}














void SystemMapRenderer::beginSolids()
{
    m_solidVertices.clear();
}

void SystemMapRenderer::addBillboardBall(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view,
    int segments
)
{
    if (radius <= 0.0f || segments < 8)
        return;

    const glm::vec3 right {
        view[0][0],
        view[1][0],
        view[2][0]
    };

    const glm::vec3 up {
        view[0][1],
        view[1][1],
        view[2][1]
    };

    const glm::vec4 coreColor {
        color.r,
        color.g,
        color.b,
        std::min(color.a, 0.92f)
    };

    const glm::vec4 edgeColor {
        color.r,
        color.g,
        color.b,
        std::min(color.a * 0.55f, 0.55f)
    };

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);

        const float a1 =
            6.28318530718f * static_cast<float>(i + 1) / static_cast<float>(segments);

        const glm::vec3 p0 =
            center + (std::cos(a0) * right + std::sin(a0) * up) * radius;

        const glm::vec3 p1 =
            center + (std::cos(a1) * right + std::sin(a1) * up) * radius;

        m_solidVertices.push_back({ center, coreColor });
        m_solidVertices.push_back({ p0, edgeColor });
        m_solidVertices.push_back({ p1, edgeColor });
    }
}























void SystemMapRenderer::flushSolids(const glm::mat4& mvp)
{
    if (!m_shader || !m_vao || !m_vbo || m_solidVertices.empty())
        return;

    GLboolean depthWasEnabled =
    glIsEnabled(GL_DEPTH_TEST);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    GLboolean depthMaskWasEnabled =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldDepthFunc =
        GL_LESS;

    glGetIntegerv(
        GL_DEPTH_FUNC,
        &oldDepthFunc
    );

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_solidVertices.size() * sizeof(Vertex)),
        m_solidVertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_solidVertices.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthFunc(
        oldDepthFunc
    );

    glDepthMask(
        depthMaskWasEnabled
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




void SystemMapRenderer::beginTexturedBodies()
{
    for (auto& batch : m_texturedBatches)
    {
        batch.vertices.clear();
    }
}





void SystemMapRenderer::flushTexturedBodies(
    const glm::mat4& mvp
)
{
    if (!m_texturedShader ||
        !m_texturedVao ||
        !m_texturedVbo ||
        m_texturedBatches.empty())
    {
        return;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWasEnabled = GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldDepthFunc = GL_LESS;

    glGetIntegerv(
        GL_DEPTH_FUNC,
        &oldDepthFunc
    );

    GLint oldCullFaceMode = GL_BACK;

    glGetIntegerv(
        GL_CULL_FACE_MODE,
        &oldCullFaceMode
    );

    GLint oldFrontFaceMode = GL_CCW;

    glGetIntegerv(
        GL_FRONT_FACE,
        &oldFrontFaceMode
    );




    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Textured system bodies are real 3D spheres.
    // At strong orthographic zoom their front/back depth difference is tiny
    // relative to the system-map far plane. Rendering both sides causes
    // z-fighting stripes. Cull backfaces and draw only the visible shell.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Планеты и луны рисуем как opaque geometry.
    // Alpha-канал generated texture не должен резать сферу полосами.
    glDisable(GL_BLEND);

    glUseProgram(m_texturedShader);

    glUniformMatrix4fv(
        m_texturedMvpLoc,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );

    glUniform1i(m_texturedSamplerLoc, 0);

    glBindVertexArray(m_texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texturedVbo);

    glActiveTexture(GL_TEXTURE0);

    for (const TexturedBatch& batch : m_texturedBatches)
    {
        if (batch.texture == 0 ||
            batch.vertices.empty())
        {
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, batch.texture);

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                batch.vertices.size() * sizeof(TexturedVertex)
            ),
            batch.vertices.data(),
            GL_DYNAMIC_DRAW
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            static_cast<GLsizei>(batch.vertices.size())
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthFunc(
        oldDepthFunc
    );

    glDepthMask(
        depthMaskWasEnabled
    );




    glCullFace(oldCullFaceMode);
    glFrontFace(oldFrontFaceMode);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);




    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}



void SystemMapRenderer::addCross(
    const glm::vec3& center,
    float size,
    const glm::vec4& color
)
{
    addLine(
        center + glm::vec3(-size, 0.0f, 0.0f),
        center + glm::vec3( size, 0.0f, 0.0f),
        color
    );

    addLine(
        center + glm::vec3(0.0f, -size, 0.0f),
        center + glm::vec3(0.0f,  size, 0.0f),
        color
    );

    addLine(
        center + glm::vec3(0.0f, 0.0f, -size),
        center + glm::vec3(0.0f, 0.0f,  size),
        color
    );
}

void SystemMapRenderer::flushLines(const glm::mat4& mvp)
{
    if (!m_shader || !m_vao || !m_vbo || m_vertices.empty())
        return;

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    GLfloat oldLineWidth = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &oldLineWidth);

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
        m_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.5f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

    glLineWidth(oldLineWidth);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}










void SystemMapRenderer::render(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& planet,
    const world::celestial::HubMapSnapshot& hub,
    const world::celestial::PlayerNavigationState& nav
)
{


    if (!m_initialized)
        init();

    const double nowSeconds =
        glfwGetTime();

    m_galaxyView.updateCameraFlight(
        nowSeconds
    );


    m_mapTransition.update(
        nowSeconds
    );



    /*
        Начало нового кадра процедурных облаков.

        Это должно выполняться ровно один раз до любого
        вызова textureForStyle(), независимо от режима карты
        и наличия старой solid geometry.
    */
    m_mapResources.beginFrame();



    const Viewport& vp = viewport;

    glViewport(vp.x, vp.y, vp.width, vp.height);
    glScissor(vp.x, vp.y, vp.width, vp.height);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);

    drawBackground();

    // System map рисует свою 3D-сцену поверх игрового кадра.
    // Поэтому depth buffer надо очистить, иначе карта может
    // наследовать глубину от предыдущего рендера.
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);



    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);




    if (m_mode == Mode::Detail)
    {
        if (!m_detailFramePrepared || m_detailFrameDirty)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    viewport,
                    planet
                );

            m_detailFrameDirty = false;
        }

        m_detailSceneRenderer.render(
            m_detailPresentation,
            m_detailBackend,
            viewport,
            planet
        );

        m_detailFramePrepared = false;
        m_detailFrameDirty = true;
    }
    else if (m_mode == Mode::Hub)
    {
        if (!m_hubFramePrepared || m_hubFrameDirty)
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    viewport,
                    hub
                );
            decorateHubDockingOverlay(hub);

            m_hubFrameDirty = false;
        }

        m_hubSceneRenderer.render(
            m_hubPresentation,
            m_hubBackend,
            viewport,
            hub
        );

        m_hubFramePrepared = false;
        m_hubFrameDirty = true;
    }
    else if (m_mode == Mode::Galaxy)
    {
        m_galaxyRenderer.render(
            m_galaxyView,
            *this,
            vp,
            galaxy,
            nav
        );
        refreshGalaxyWaypointCandidate(vp, galaxy);
    }
    else if (m_mode == Mode::System)
    {
        renderSystem(
            vp,
            system,
            nav
        );
        refreshSystemWaypointCandidate(vp, system);
    }

    // The planner owns world-space guidance samples; each map owns only their
    // projection. This makes the same active docking task visible as a smooth
    // planned trajectory on System/Detail/Hub without teaching the planner
    // anything about cameras or render scale.
    decorateActiveGuidanceTrajectory(vp, system, planet, hub);

    if (m_mode == Mode::Galaxy)
    {
        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
        m_objectOverlayRenderer.render(
            vp,
            m_galaxyInfoOverlayFrame,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }
    else if (m_mode == Mode::System)
    {
        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_systemSceneFrame.interaction.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }
    else if (m_mode == Mode::Detail)
    {
        synchronizeNavigationTracking(
            m_detailPresentation.frame.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_detailPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }
    else if (m_mode == Mode::Hub)
    {
        synchronizeNavigationTracking(
            m_hubPresentation.frame.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_hubPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }

    m_routeOverlayRenderer.render(
        vp,
        m_navigationWorkspace.routePlan(),
        m_routeOverlayState,
        m_navigationMapTextProfile
    );

    drawNavigationCoordinateOverlay(
        vp,
        galaxy,
        system,
        nav
    );

    /*
        Настоящий crossfade.

        Если переход только начался, framebuffer ещё содержит
        старое состояние. Сохраняем его в текстуру и только
        после этого выполняем смену камеры или режима.

        На следующих кадрах уже рисуется новое состояние,
        а старый снимок постепенно растворяется поверх него.
    */
    glViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glScissor(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    if (m_mapTransition.needsOutgoingCapture())
    {
        captureMapTransitionSnapshot(
            viewport
        );

        m_mapTransition.outgoingCaptured(
            glfwGetTime()
        );
    }
    else if (m_mapTransition.needsIncomingWarmup())
    {
        // The new mode has now rendered one complete frame, but it remains
        // fully covered by the outgoing snapshot. Lazy presentation builders,
        // geometry and shader state therefore cannot flash half-prepared.
        drawMapTransitionSnapshot(
            viewport,
            1.0f
        );
        m_mapTransition.incomingFrameRendered(
            glfwGetTime()
        );
    }
    else if (m_mapTransition.active())
    {
        drawMapTransitionSnapshot(
            viewport,
            m_mapTransition.outgoingAlpha()
        );
    }


    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}




#include "src/game/system_map/SystemMapRendererSystem.inl"





glm::vec2 SystemMapRenderer::projectToScreen(
    const glm::vec3& world,
    const glm::mat4& mvp,
    const Viewport& vp,
    bool& visible,
    float& depth
) const
{
    const glm::vec4 clip = mvp * glm::vec4(world, 1.0f);

    visible = false;
    depth = 2.0f;

    if (clip.w <= 0.00001f)
        return {0.0f, 0.0f};

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    visible =
        ndc.x >= -1.0f && ndc.x <= 1.0f &&
        ndc.y >= -1.0f && ndc.y <= 1.0f &&
        ndc.z >= -1.0f && ndc.z <= 1.0f;

    depth = ndc.z;

    return {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(vp.width),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(vp.height)
    };
}













namespace
{
game::navigation::RouteTargetRef routeTargetRefForOverlayItem(
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
}

namespace
{
constexpr const char* HubModuleOverlayPrefix = "hub-module:";
constexpr const char* HubDockOverlayPrefix = "hub-dock:";

std::string hubDockOverlayId(
    const std::string& moduleId,
    const std::string& anchorId
)
{
    return std::string(HubDockOverlayPrefix) + moduleId + "/" + anchorId;
}

bool parseHubDockOverlayId(
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

std::string formatDockPair(double a, double b)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(0) << a << " × " << b;
    return out.str();
}

std::string formatDockScalar(double value, int precision = 0)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

glm::dvec3 hubLocalToWorldMetersForOverlay(
    const world::celestial::HubMapSnapshot& hub,
    const glm::dvec3& localMeters
)
{
    return hub.hubWorldPositionMeters +
        hub.hubWorldAxes.x * localMeters.x +
        hub.hubWorldAxes.y * localMeters.y +
        hub.hubWorldAxes.z * localMeters.z;
}

} // namespace

void SystemMapRenderer::decorateHubDockingOverlay(
    const world::celestial::HubMapSnapshot& hub
)
{
    if (!hub.valid || !m_hubSemanticAnchors)
        return;

    auto& frame = m_hubPresentation.frame.objectOverlay;

    // Module cards expose how many authored docking ports they own. The module
    // itself remains selectable even when it has zero ports.
    for (auto& item : frame.items)
    {
        if (item.infoKind != game::system_map::MapObjectInfoKind::Infrastructure ||
            item.semanticTargetId.empty())
        {
            continue;
        }

        int dockingPortCount = 0;
        for (const auto& anchor :
             m_hubSemanticAnchors->anchorsForModule(item.semanticTargetId))
        {
            if (anchor.enabled &&
                anchor.kind == game::navigation::HubSemanticAnchorKind::DockingPort)
            {
                ++dockingPortCount;
            }
        }

        game::system_map::MapObjectInfoField docks;
        docks.labelKey = "docking_ports";
        docks.value = std::to_string(dockingPortCount);
        item.extraFields.push_back(std::move(docks));
    }

    // Resolve the local player's logical docking envelope once per ship type.
    // LogicalDimensions use the canonical ship basis:
    // width=left/right, height=belly/top, length=nose/tail.
    for (const auto& object : hub.scene.objects)
    {
        if (!object.valid ||
            object.objectClass != world::celestial::DetailObjectClass::Ship ||
            !object.player)
        {
            continue;
        }

        if (object.typeId != m_cachedDockingEnvelopeShipType)
        {
            m_cachedDockingEnvelopeShipType = object.typeId;
            m_cachedDockingEnvelope = {};
            try
            {
                const auto& descriptor = ShipDescriptorRegistry::get(object.typeId);
                const auto& dimensions = descriptor.logicalDimensions();
                if (dimensions.enabled)
                {
                    m_cachedDockingEnvelope.lengthMeters = dimensions.length;
                    m_cachedDockingEnvelope.widthMeters = dimensions.width;
                    m_cachedDockingEnvelope.heightMeters = dimensions.height;
                    m_cachedDockingEnvelope.valid =
                        dimensions.length > 0.0f &&
                        dimensions.width > 0.0f &&
                        dimensions.height > 0.0f;
                }
            }
            catch (...)
            {
                m_cachedDockingEnvelope = {};
            }
        }
        break;
    }

    const double finalScale =
        m_hubPresentation.scale * m_hubPresentation.camera.state.zoom;

    const glm::vec4 good(0.42f, 1.00f, 0.58f, 1.00f);
    const glm::vec4 warn(1.00f, 0.78f, 0.30f, 1.00f);
    const glm::vec4 bad(1.00f, 0.34f, 0.30f, 1.00f);

    const auto runtimeText = [&](game::navigation::DockingOperationalState state)
    {
        switch (state)
        {
            case game::navigation::DockingOperationalState::Online:
                return m_navigationMapTextProfile.statusOnline;
            case game::navigation::DockingOperationalState::Offline:
                return m_navigationMapTextProfile.statusOffline;
            case game::navigation::DockingOperationalState::Damaged:
                return m_navigationMapTextProfile.statusDamaged;
            case game::navigation::DockingOperationalState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };
    const auto occupancyText = [&](game::navigation::DockingOccupancyState state)
    {
        switch (state)
        {
            case game::navigation::DockingOccupancyState::Free:
                return m_navigationMapTextProfile.statusFree;
            case game::navigation::DockingOccupancyState::Occupied:
                return m_navigationMapTextProfile.statusOccupied;
            case game::navigation::DockingOccupancyState::Reserved:
                return m_navigationMapTextProfile.statusReserved;
            case game::navigation::DockingOccupancyState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };
    const auto accessText = [&](game::navigation::DockingAccessState state)
    {
        switch (state)
        {
            case game::navigation::DockingAccessState::Allowed:
                return m_navigationMapTextProfile.statusAllowed;
            case game::navigation::DockingAccessState::ClearanceRequired:
                return m_navigationMapTextProfile.statusClearanceRequired;
            case game::navigation::DockingAccessState::Denied:
                return m_navigationMapTextProfile.statusDenied;
            case game::navigation::DockingAccessState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };

    // Docking ports are presentation/interaction objects in their own right.
    // Do not make the player select the much larger parent module first: every
    // authored enabled port is projected each Hub frame and receives first
    // refusal inside the exact projected opening rectangle.
    for (const auto& module : hub.scene.objects)
    {
        if (!module.valid ||
            module.objectClass != world::celestial::DetailObjectClass::Hub ||
            module.stableId.empty())
        {
            continue;
        }

        const auto& anchors =
            m_hubSemanticAnchors->anchorsForModule(module.stableId);
        if (anchors.empty())
            continue;

        const glm::dmat3 moduleBasis(
            module.axes.x,
            module.axes.y,
            module.axes.z
        );

        for (const auto& anchor : anchors)
        {
            if (!anchor.enabled ||
                anchor.kind != game::navigation::HubSemanticAnchorKind::DockingPort)
            {
                continue;
            }

            game::navigation::DockingPortRuntimeState unavailableState;
            unavailableState.hubModuleId = module.stableId;
            unavailableState.anchorId = anchor.id;
            const auto* runtime = m_dockingPortRuntimeStates
                ? m_dockingPortRuntimeStates->find(module.stableId, anchor.id)
                : nullptr;
            const auto& effectiveRuntime = runtime ? *runtime : unavailableState;

            const auto compatibility = game::navigation::evaluateDockingCompatibility(
                m_cachedDockingEnvelope,
                anchor,
                effectiveRuntime
            );

            const glm::dvec3 localPosition =
                module.positionMeters + moduleBasis * anchor.localPositionMeters;

            game::system_map::MapObjectOverlayItem item;
            item.objectId = hubDockOverlayId(module.stableId, anchor.id);
            item.semanticTargetId = module.stableId;
            item.semanticAnchorId = anchor.id;
            item.trackingSystemId = hub.systemId;
            item.name = anchor.displayName.empty() ? anchor.id : anchor.displayName;
            item.typeName = "Docking port";
            item.kind = game::system_map::MapObjectGlyphKind::DockingPort;
            item.infoKind = game::system_map::MapObjectInfoKind::DockingPort;
            item.navigationHubId = hub.hubId;
            item.navigationHubParentBodyId = hub.parentBodyId;
            item.trackingWorldPosition = world::coordinates::makeWorldPositionFromMeters(
                hubLocalToWorldMetersForOverlay(hub, localPosition)
            );
            item.hasTrackingWorldPosition = true;
            item.factionColor = compatibility.routeAvailable
                ? glm::vec4(0.42f, 0.96f, 0.72f, 0.94f)
                : glm::vec4(0.96f, 0.68f, 0.30f, 0.92f);
            item.screenPx = m_hubPresentation.camera.project(localPosition);
            const glm::dvec2 hubViewportSize =
                m_hubPresentation.centerPx * 2.0;
            item.visible =
                item.screenPx.x >= -20.0 &&
                item.screenPx.y >= -20.0 &&
                item.screenPx.x <= hubViewportSize.x + 20.0 &&
                item.screenPx.y <= hubViewportSize.y + 20.0;
            item.physicalSizeMeters = std::max(
                1.0,
                std::max(anchor.extentMeters.x, anchor.extentMeters.y)
            );
            item.glyphScale = game::system_map::mapObjectGlyphScale(
                item.physicalSizeMeters,
                finalScale
            );
            item.hitRadiusPx = 15.0;
            item.pickPriority = 200;
            item.pointerInteractive = true;
            item.drawGlyph = true;

            const glm::dvec3 forwardModule = glm::normalize(
                moduleBasis * anchor.localForward
            );
            const glm::dvec3 upModule = glm::normalize(
                moduleBasis * anchor.localUp
            );
            glm::dvec3 rightModule = glm::cross(forwardModule, upModule);
            if (glm::length(rightModule) > 1.0e-9)
            {
                rightModule = glm::normalize(rightModule);
                const double halfW = std::max(0.5, anchor.extentMeters.x * 0.5);
                const double halfH = std::max(0.5, anchor.extentMeters.y * 0.5);
                const glm::dvec3 corners[4] = {
                    localPosition - rightModule * halfW - upModule * halfH,
                    localPosition + rightModule * halfW - upModule * halfH,
                    localPosition + rightModule * halfW + upModule * halfH,
                    localPosition - rightModule * halfW + upModule * halfH
                };
                item.hitPolygonPx.reserve(4);
                for (const auto& corner : corners)
                    item.hitPolygonPx.push_back(m_hubPresentation.camera.project(corner));
            }

            auto addField = [&](
                const std::string& key,
                std::string value,
                const glm::vec4* color = nullptr,
                std::string unit = {})
            {
                game::system_map::MapObjectInfoField field;
                field.labelKey = key;
                field.value = std::move(value);
                field.unit = std::move(unit);
                if (color)
                {
                    field.valueColor = *color;
                    field.hasValueColor = true;
                }
                item.extraFields.push_back(std::move(field));
            };

            addField(
                "dock_opening",
                formatDockPair(
                    compatibility.openingWidthMeters,
                    compatibility.openingHeightMeters
                ),
                nullptr,
                "m"
            );
            addField(
                "ship_envelope",
                m_cachedDockingEnvelope.valid
                    ? formatDockPair(
                        m_cachedDockingEnvelope.widthMeters,
                        m_cachedDockingEnvelope.heightMeters
                      )
                    : "—",
                nullptr,
                m_cachedDockingEnvelope.valid ? "m" : ""
            );
            addField(
                "dock_clearance",
                formatDockScalar(compatibility.requiredClearanceMeters),
                nullptr,
                "m"
            );
            addField(
                "dock_fit",
                compatibility.geometryFits
                    ? m_navigationMapTextProfile.statusAvailable
                    : m_navigationMapTextProfile.statusUnavailable,
                compatibility.geometryFits ? &good : &bad
            );
            addField(
                "dock_operational",
                runtimeText(effectiveRuntime.operational),
                compatibility.operational ? &good : &bad
            );
            addField(
                "dock_status",
                occupancyText(effectiveRuntime.occupancy),
                compatibility.free
                    ? &good
                    : effectiveRuntime.occupancy ==
                        game::navigation::DockingOccupancyState::Reserved
                        ? &warn
                        : &bad
            );
            addField(
                "dock_access",
                accessText(effectiveRuntime.access),
                compatibility.accessAllowed
                    ? &good
                    : effectiveRuntime.access ==
                        game::navigation::DockingAccessState::ClearanceRequired
                        ? &warn
                        : &bad
            );
            addField(
                "dock_max_entry_speed",
                formatDockScalar(anchor.maxEntrySpeedMps),
                nullptr,
                "m/s"
            );

            game::system_map::MapObjectPanelAction calculate;
            calculate.key = "calculate_docking_route";
            calculate.labelKey = "calculate_route";
            calculate.enabled = compatibility.routeAvailable;
            const auto& pending =
                m_navigationWorkspace.dockingRouteRequests().pending();
            const auto target = routeTargetRefForOverlayItem(item);
            calculate.active = pending.valid() &&
                game::navigation::sameRouteTarget(pending.target, target);
            item.panelActions.push_back(std::move(calculate));

            frame.items.push_back(std::move(item));
        }
    }
}

void SystemMapRenderer::applyDockingAction(
    const std::string& objectId,
    const std::string& actionKey
)
{
    if (actionKey != "calculate_docking_route")
        return;

    const auto* item = currentOverlayItem(objectId);
    if (!item ||
        item->infoKind != game::system_map::MapObjectInfoKind::DockingPort)
    {
        return;
    }

    const auto target = routeTargetRefForOverlayItem(*item);
    if (!target.valid() ||
        target.kind != game::navigation::NavigationRouteAnchorKind::SemanticAnchor)
    {
        return;
    }

    const auto serial =
        m_navigationWorkspace.dockingRouteRequests().request(target);
    if (serial != 0)
    {
        // CALCULATE ROUTE is an explicit request to see the advisory tunnel.
        // The pilot may hide the HUD layer afterwards without disabling the
        // planner/safety modules.
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::HudGuidanceCorridor,
            true
        );
    }
}


void SystemMapRenderer::cancelDockingTaskForClosedCard(
    const std::string& objectId
)
{
    std::string moduleId;
    std::string anchorId;
    if (!parseHubDockOverlayId(objectId, moduleId, anchorId))
        return;

    const auto& pending =
        m_navigationWorkspace.dockingRouteRequests().pending();
    if (!pending.valid())
        return;

    if (pending.target.stableObjectId == moduleId &&
        pending.target.semanticAnchorId == anchorId)
    {
        // Docking guidance is intentionally card-scoped. Closing the dock
        // information card means the pilot has cancelled that advisory task;
        // SpaceState drops the corridor on the next update and tracking is
        // reconciled from the remaining open cards immediately.
        m_navigationWorkspace.dockingRouteRequests().clear();
    }
}

void SystemMapRenderer::decorateActiveGuidanceTrajectory(
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& detail,
    const world::celestial::HubMapSnapshot& hub
)
{
    game::system_map::MapObjectOverlayFrame* overlay = nullptr;
    int systemId = -1;
    double universeTimeSeconds = 0.0;

    switch (m_mode)
    {
        case Mode::System:
            overlay = &m_systemSceneFrame.interaction.objectOverlay;
            systemId = system.systemId;
            universeTimeSeconds = system.universeTimeSeconds;
            break;
        case Mode::Detail:
            overlay = &m_detailPresentation.frame.objectOverlay;
            systemId = detail.systemId;
            universeTimeSeconds = detail.universeTimeSeconds;
            break;
        case Mode::Hub:
            overlay = &m_hubPresentation.frame.objectOverlay;
            systemId = hub.systemId;
            universeTimeSeconds = hub.universeTimeSeconds;
            break;
        case Mode::Galaxy:
        default:
            return;
    }

    if (!overlay)
        return;

    // Rebuild only this local docking producer's rows. Other history,
    // prediction or long-range producers own their own trajectory entries and
    // must survive this presentation pass.
    overlay->trajectories.erase(
        std::remove_if(
            overlay->trajectories.begin(),
            overlay->trajectories.end(),
            [](const game::system_map::MapObjectTrajectory& trajectory)
            {
                return trajectory.objectId.rfind("dock:", 0) == 0;
            }
        ),
        overlay->trajectories.end()
    );

    const auto* corridor = m_navigationWorkspace.guidance().activePredictive(
        systemId,
        universeTimeSeconds,
        &m_navigationWorkspace.modules()
    );
    if (!corridor || corridor->frames.size() < 2)
        return;

    game::system_map::MapObjectTrajectory trajectory;
    trajectory.objectId = corridor->id;
    trajectory.kind = game::system_map::MapTrajectoryKind::Planned;
    trajectory.noSafePrimarySolution = corridor->noSafePrimarySolution;
    trajectory.points.reserve(corridor->frames.size());

    const glm::dvec3 systemOriginMeters =
        system.systemPositionLy * world::coordinates::MetersPerLightYear;

    game::navigation::HubCoMovingFrameSeed hubFrameSeed;
    if (m_mode == Mode::Hub)
    {
        hubFrameSeed = game::navigation::makeHubCoMovingFrameSeed(
            hub.systemId,
            hub.hubId,
            hub.universeTimeSeconds,
            hub.hubWorldPositionMeters,
            hub.hubWorldVelocityMps,
            hub.parentPlanetWorldPositionMeters,
            hub.parentPlanetWorldVelocityMps,
            hub.hubWorldAxes.x,
            hub.hubWorldAxes.y,
            hub.hubWorldAxes.z
        );
    }

    const auto projectWorldPoint = [&]
    (
        const glm::dvec3& worldMeters,
        double sampleUniverseTimeSeconds,
        glm::dvec2& screenPx
    ) -> bool
    {
        if (m_mode == Mode::System)
        {
            const glm::dvec3 systemRelativeAu =
                (worldMeters - systemOriginMeters) /
                world::celestial::MetersPerAu;
            const glm::dvec3 absoluteMapUnits =
                systemRelativeAu *
                static_cast<double>(m_systemSceneFrame.systemScale);
            const glm::vec3 renderPosition = glm::vec3(
                absoluteMapUnits - m_systemSceneFrame.cameraOrigin
            );
            const glm::vec4 clip =
                m_systemSceneFrame.mvp * glm::vec4(renderPosition, 1.0f);
            if (clip.w <= 1.0e-6f)
                return false;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) ||
                !std::isfinite(ndc.z) || ndc.z < -1.2f || ndc.z > 1.2f)
            {
                return false;
            }

            screenPx = glm::dvec2(
                (ndc.x * 0.5 + 0.5) * viewport.width,
                (1.0 - (ndc.y * 0.5 + 0.5)) * viewport.height
            );
            return true;
        }

        if (m_mode == Mode::Detail)
        {
            glm::dvec3 detailPoint = worldMeters;
            if (detail.scene.coordinateSpace ==
                world::celestial::LocalSceneCoordinateSpace::AnchorLocalMeters)
            {
                detailPoint -= detail.scene.originWorldMeters;
            }
            screenPx = m_detailPresentation.camera.project(detailPoint);
            return std::isfinite(screenPx.x) && std::isfinite(screenPx.y);
        }

        if (m_mode == Mode::Hub)
        {
            if (!hubFrameSeed.valid)
                return false;

            // The Hub map is a co-moving rotating coordinate domain. Project
            // each future sample against the Hub frame at that same future
            // time. Subtracting hub NOW from ship FUTURE is what produced the
            // previous hundreds-of-kilometres tangent/"laser".
            const auto futureFrame =
                game::navigation::predictHubCoMovingFrameAt(
                    hubFrameSeed,
                    sampleUniverseTimeSeconds
                );
            if (!futureFrame.valid)
                return false;

            const glm::dvec3 local =
                futureFrame.worldToLocalPosition(worldMeters);
            screenPx = m_hubPresentation.camera.project(local);
            return std::isfinite(screenPx.x) && std::isfinite(screenPx.y);
        }

        return false;
    };

    // Map presentation consumes only the accepted predictive trajectory.  A
    // separate spatialManualTunnel may be regenerated from the live ship/dock
    // pose for the cockpit, but it must never bend the map trajectory.  Keep
    // every supplied physical sample here; projection may move samples into the
    // current map view but must not silently trim the route start.
    for (const auto& guidanceFrame : corridor->frames)
    {
        game::system_map::MapTrajectoryPoint point;
        point.universeTimeSeconds = guidanceFrame.universeTimeSeconds;
        point.position = guidanceFrame.centerMeters;
        point.screenProjected = projectWorldPoint(
            point.position,
            point.universeTimeSeconds,
            point.screenPx
        );

        if (point.screenProjected)
            trajectory.points.push_back(std::move(point));
    }

    trajectory.terminalPositionErrorMeters =
        corridor->terminalPositionErrorMeters;
    if (corridor->hasTerminalTarget)
    {
        const double terminalTime = corridor->frames.empty()
            ? universeTimeSeconds
            : corridor->frames.back().universeTimeSeconds;
        trajectory.terminalTargetProjected = projectWorldPoint(
            corridor->terminalTargetMeters,
            terminalTime,
            trajectory.terminalTargetScreenPx
        );
    }

    if (trajectory.points.size() >= 2)
        overlay->trajectories.push_back(std::move(trajectory));
}


void SystemMapRenderer::synchronizeNavigationTracking(
    game::system_map::MapObjectOverlayFrame& frame
)
{
    auto& targets = m_navigationWorkspace.targets();
    auto& routePlan = m_navigationWorkspace.routePlan();
    const auto openIds = m_objectOverlayState.openObjectIds();
    targets.reconcileOpenCards(openIds);
    routePlan.pruneTransientCandidates(openIds);

    const auto routeActionsFor =
        [&](const game::system_map::MapObjectOverlayItem& item)
        {
            std::vector<game::system_map::MapObjectPanelAction> actions;
            if (item.objectId == "player" || !item.hasTrackingWorldPosition)
                return actions;

            const auto target = routeTargetRefForOverlayItem(item);
            if (!target.valid())
                return actions;

            const auto* route = routePlan.findByTarget(target);
            const bool isWaypoint =
                route && route->role ==
                    game::navigation::NavigationWaypointRole::Intermediate;
            const bool isFinish =
                route && route->role ==
                    game::navigation::NavigationWaypointRole::Finish;

            if (!isFinish)
            {
                game::system_map::MapObjectPanelAction waypointAction;
                waypointAction.key = "toggle_intermediate";
                waypointAction.labelKey =
                    isWaypoint
                        ? "cancel_waypoint"
                        : item.kind == game::system_map::MapObjectGlyphKind::Ship
                            ? "set_rendezvous"
                            : "set_waypoint";
                waypointAction.active = isWaypoint;
                actions.push_back(std::move(waypointAction));
            }

            if (isFinish || !routePlan.hasFinishWaypoint())
            {
                game::system_map::MapObjectPanelAction finishAction;
                finishAction.key = "toggle_finish";
                finishAction.labelKey =
                    isFinish ? "cancel_finish" : "set_finish";
                finishAction.active = isFinish;
                actions.push_back(std::move(finishAction));
            }
            return actions;
        };

    for (auto& item : frame.items)
    {
        if (item.hasTrackingWorldPosition)
        {
            const auto target = routeTargetRefForOverlayItem(item);
            if (target.valid())
            {
                if (auto* route = routePlan.findByTarget(target);
                    route && route->role !=
                        game::navigation::NavigationWaypointRole::None)
                {
                    routePlan.bindPresentationSource(route->id, item.objectId);
                    route->worldPosition = item.trackingWorldPosition;
                    if (!item.name.empty())
                        route->displayName = item.name;

                    item.routeDisplayIndex =
                        route->role == game::navigation::NavigationWaypointRole::Finish
                            ? static_cast<int>(routePlan.routeSize())
                            : route->sequence;
                }
            }
        }

        if (item.infoKind == game::system_map::MapObjectInfoKind::Tactical ||
            item.infoKind == game::system_map::MapObjectInfoKind::Celestial)
        {
            item.panelActions = routeActionsFor(item);
        }

        if (!m_objectOverlayState.isOpen(item.objectId))
            continue;

        if (item.infoKind == game::system_map::MapObjectInfoKind::Tactical)
        {
            const bool numberedShipTarget =
                item.kind == game::system_map::MapObjectGlyphKind::Ship &&
                item.objectId != "player";
            targets.rememberTacticalObject(
                item.objectId,
                item.typeName,
                item.name,
                item.factionColor,
                numberedShipTarget,
                numberedShipTarget
                    ? m_objectOverlayState.shipTargetNumberFor(item.objectId)
                    : 0
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::Celestial &&
            item.hasTrackingWorldPosition)
        {
            targets.rememberCelestialBody(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.typeName,
                item.name,
                item.trackingWorldPosition,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::Infrastructure &&
            !item.semanticTargetId.empty())
        {
            targets.rememberInfrastructure(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.typeName,
                item.name,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::DockingPort &&
            !item.semanticTargetId.empty() &&
            !item.semanticAnchorId.empty())
        {
            targets.rememberSemanticAnchor(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.semanticAnchorId,
                item.typeName,
                item.name,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::WaypointCandidate &&
            item.hasTrackingWorldPosition)
        {
            std::string address;
            for (const auto& field : item.extraFields)
            {
                if (field.labelKey == "address")
                {
                    address = field.value;
                    break;
                }
            }
            const auto target = routeTargetRefForOverlayItem(item);
            routePlan.rememberCandidate(
                target,
                item.objectId,
                item.trackingWorldPosition,
                std::move(address),
                item.name.empty() ? m_navigationMapTextProfile.spaceTarget : item.name
            );
        }
    }
}

namespace
{
std::string navigationCellAddress(
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

std::string waypointCandidateId(
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

bool waypointHasPrefix(
    const game::navigation::NavigationWaypoint& waypoint,
    char prefix
)
{
    const std::string expected = std::string("waypoint_candidate:") + prefix + ":";
    return waypoint.sourceObjectId.rfind(expected, 0) == 0;
}

glm::vec4 waypointRoleColor(game::navigation::NavigationWaypointRole role)
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

const std::string& waypointRoleTypeName(
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

std::vector<game::system_map::MapObjectPanelAction> waypointPanelActions(
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
}

void SystemMapRenderer::refreshGalaxyWaypointCandidate(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    (void)galaxy;
    m_galaxyInfoOverlayFrame.items.clear();
    m_galaxyInfoOverlayFrame.trajectories.clear();

    const glm::mat4 mvp =
        m_galaxyView.projectionMatrix(viewport) *
        m_galaxyView.viewMatrix();

    for (const auto& waypoint : m_navigationWorkspace.routePlan().waypoints())
    {
        if (waypoint.role == game::navigation::NavigationWaypointRole::None ||
            !waypointHasPrefix(waypoint, 'G'))
            continue;

        game::system_map::MapObjectOverlayItem item;
        item.objectId = waypoint.sourceObjectId;
        item.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
        item.typeName = waypointRoleTypeName(waypoint.role, m_navigationMapTextProfile);
        item.name = waypoint.address.empty() ? waypoint.displayName : waypoint.address;
        item.drawGlyph = true;
        item.pointerInteractive = true;
        item.screenAffordance = true;
        item.glyphScale = 0.78;
        item.hitRadiusPx = 12.0;
        item.facingScreenDirection = glm::dvec2(0.0, -1.0);
        item.factionColor = waypointRoleColor(waypoint.role);
        item.routeDisplayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint.sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), waypoint);
        item.extraFields.push_back({"address", waypoint.address, ""});
        item.trackingWorldPosition = waypoint.worldPosition;
        item.hasTrackingWorldPosition = true;

        const glm::vec3 renderPosition =
            m_galaxyView.positionLyToRender(
                world::coordinates::toGalacticLy(waypoint.worldPosition)
            );
        bool visible = false;
        float depth = 1.0f;
        item.screenPx = glm::dvec2(
            projectToScreen(renderPosition, mvp, viewport, visible, depth)
        ) + glm::dvec2(20.0, -2.0);
        item.visible = visible;
        m_galaxyInfoOverlayFrame.items.push_back(std::move(item));
    }

    if (!m_galaxyWaypointCandidate.has_value() ||
        m_galaxyView.state().selectedSystemId >= 0)
    {
        return;
    }

    auto item = *m_galaxyWaypointCandidate;
    const auto& grid = m_galaxyView.state().navigationGrid;
    if (!grid.hasSelectedCell())
        return;

    const auto& cell = grid.selectedCell();
    if (item.objectId != waypointCandidateId('G', cell))
        return;

    const glm::vec3 renderPosition =
        m_galaxyView.positionLyToRender(cell.center);
    bool visible = false;
    float depth = 1.0f;
    item.screenPx = glm::dvec2(
        projectToScreen(renderPosition, mvp, viewport, visible, depth)
    ) + glm::dvec2(20.0, -2.0);
    item.visible = visible;
    item.drawGlyph = true;
    item.pointerInteractive = true;
    item.screenAffordance = true;
    item.facingScreenDirection = glm::dvec2(0.0, -1.0);
    item.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    const auto* waypoint = m_navigationWorkspace.routePlan().findByTarget(
        routeTargetRefForOverlayItem(item)
    );
    if (waypoint)
    {
        item.typeName = waypointRoleTypeName(waypoint->role, m_navigationMapTextProfile);
        item.name = waypoint->address.empty() ? waypoint->displayName : waypoint->address;
        item.factionColor = waypointRoleColor(waypoint->role);
        item.routeDisplayIndex =
            waypoint->role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint->sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), *waypoint);
        item.extraFields.clear();
        item.extraFields.push_back({"address", waypoint->address, ""});
    }
    m_galaxyWaypointCandidate = item;

    const auto duplicate = std::find_if(
        m_galaxyInfoOverlayFrame.items.begin(),
        m_galaxyInfoOverlayFrame.items.end(),
        [&](const game::system_map::MapObjectOverlayItem& existing)
        {
            return existing.objectId == item.objectId;
        }
    );
    if (duplicate == m_galaxyInfoOverlayFrame.items.end())
        m_galaxyInfoOverlayFrame.items.push_back(std::move(item));
}

void SystemMapRenderer::refreshSystemWaypointCandidate(
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system
)
{
    auto& items = m_systemSceneFrame.interaction.objectOverlay.items;
    items.erase(
        std::remove_if(
            items.begin(),
            items.end(),
            [](const game::system_map::MapObjectOverlayItem& candidate)
            {
                return candidate.infoKind ==
                    game::system_map::MapObjectInfoKind::WaypointCandidate;
            }
        ),
        items.end()
    );

    for (const auto& waypoint : m_navigationWorkspace.routePlan().waypoints())
    {
        if (waypoint.role == game::navigation::NavigationWaypointRole::None ||
            !waypointHasPrefix(waypoint, 'S'))
            continue;

        game::system_map::MapObjectOverlayItem item;
        item.objectId = waypoint.sourceObjectId;
        item.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
        item.typeName = waypointRoleTypeName(waypoint.role, m_navigationMapTextProfile);
        item.name = waypoint.address.empty() ? waypoint.displayName : waypoint.address;
        item.drawGlyph = true;
        item.pointerInteractive = true;
        item.screenAffordance = true;
        item.glyphScale = 0.78;
        item.hitRadiusPx = 12.0;
        item.facingScreenDirection = glm::dvec2(0.0, -1.0);
        item.factionColor = waypointRoleColor(waypoint.role);
        item.routeDisplayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint.sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), waypoint);
        item.extraFields.push_back({"address", waypoint.address, ""});
        item.trackingWorldPosition = waypoint.worldPosition;
        item.hasTrackingWorldPosition = true;

        const glm::dvec3 relativeMeters =
            world::coordinates::fullMeters(waypoint.worldPosition) -
            (system.systemPositionLy * world::coordinates::MetersPerLightYear);
        const glm::dvec3 relativeAu =
            relativeMeters / world::celestial::MetersPerAu;
        const glm::dvec3 absoluteMap =
            relativeAu * static_cast<double>(m_systemSceneFrame.systemScale);
        const glm::vec3 renderPosition =
            m_systemSceneFrame.camera.relativePosition(absoluteMap);

        bool visible = false;
        float depth = 1.0f;
        item.screenPx = glm::dvec2(
            projectToScreen(
                renderPosition,
                m_systemSceneFrame.mvp,
                viewport,
                visible,
                depth
            )
        ) + glm::dvec2(20.0, -2.0);
        item.visible = visible;
        items.push_back(std::move(item));
    }

    if (!m_systemWaypointCandidate.has_value() ||
        !m_systemView.state().navigationGrid.hasSelectedCell())
    {
        return;
    }

    const auto& cell = m_systemView.state().navigationGrid.selectedCell();
    if (m_systemWaypointCandidate->objectId != waypointCandidateId('S', cell))
        return;

    auto item = *m_systemWaypointCandidate;
    const glm::dvec3 absoluteMap =
        cell.center * static_cast<double>(m_systemSceneFrame.systemScale);
    const glm::vec3 renderPosition =
        m_systemSceneFrame.camera.relativePosition(absoluteMap);

    bool visible = false;
    float depth = 1.0f;
    item.screenPx = glm::dvec2(
        projectToScreen(
            renderPosition,
            m_systemSceneFrame.mvp,
            viewport,
            visible,
            depth
        )
    ) + glm::dvec2(20.0, -2.0);
    item.visible = visible;
    item.drawGlyph = true;
    item.pointerInteractive = true;
    item.screenAffordance = true;
    item.facingScreenDirection = glm::dvec2(0.0, -1.0);
    item.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    const auto* waypoint = m_navigationWorkspace.routePlan().findByTarget(
        routeTargetRefForOverlayItem(item)
    );
    if (waypoint)
    {
        item.typeName = waypointRoleTypeName(waypoint->role, m_navigationMapTextProfile);
        item.name = waypoint->address.empty() ? waypoint->displayName : waypoint->address;
        item.factionColor = waypointRoleColor(waypoint->role);
        item.routeDisplayIndex =
            waypoint->role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint->sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), *waypoint);
        item.extraFields.clear();
        item.extraFields.push_back({"address", waypoint->address, ""});
    }
    m_systemWaypointCandidate = item;

    const auto duplicate = std::find_if(
        items.begin(),
        items.end(),
        [&](const game::system_map::MapObjectOverlayItem& existing)
        {
            return existing.objectId == item.objectId;
        }
    );
    if (duplicate == items.end())
        items.push_back(std::move(item));
}

const game::system_map::MapObjectOverlayItem*
SystemMapRenderer::currentOverlayItem(const std::string& objectId) const
{
    const game::system_map::MapObjectOverlayFrame* frame = nullptr;
    if (m_mode == Mode::Galaxy)
        frame = &m_galaxyInfoOverlayFrame;
    else if (m_mode == Mode::System)
        frame = &m_systemSceneFrame.interaction.objectOverlay;
    else if (m_mode == Mode::Detail)
        frame = &m_detailPresentation.frame.objectOverlay;
    else if (m_mode == Mode::Hub)
        frame = &m_hubPresentation.frame.objectOverlay;

    if (!frame)
        return nullptr;
    const auto found = std::find_if(
        frame->items.begin(),
        frame->items.end(),
        [&](const game::system_map::MapObjectOverlayItem& item)
        {
            return item.objectId == objectId;
        }
    );
    return found == frame->items.end() ? nullptr : &(*found);
}

void SystemMapRenderer::applyWaypointAction(
    const std::string& objectId,
    const std::string& actionKey
)
{
    auto& routePlan = m_navigationWorkspace.routePlan();
    const auto* item = currentOverlayItem(objectId);
    game::navigation::NavigationWaypoint* waypoint = nullptr;

    if (item && item->hasTrackingWorldPosition)
    {
        const auto target = routeTargetRefForOverlayItem(*item);
        if (!target.valid())
            return;

        std::string address;
        for (const auto& field : item->extraFields)
        {
            if (field.labelKey == "address")
            {
                address = field.value;
                break;
            }
        }

        waypoint = &routePlan.rememberCandidate(
            target,
            item->objectId,
            item->trackingWorldPosition,
            std::move(address),
            item->name.empty() ? item->typeName : item->name
        );

        using Context = game::navigation::NavigationRouteMapKind;
        Context context = Context::System;
        int systemId = item->trackingSystemId;
        std::string bodyId;
        std::string hubId = item->navigationHubId;
        if (m_mode == Mode::Galaxy)
        {
            context = Context::Galaxy;
        }
        else if (m_mode == Mode::System)
        {
            context = Context::System;
            systemId = m_systemPresentation.systemId;
            bodyId = m_systemView.state().selectedBodyId;
        }
        else if (m_mode == Mode::Detail)
        {
            context = Context::Detail;
            systemId = m_detailPresentation.systemId;
            bodyId = m_systemView.state().selectedBodyId;
        }
        else if (m_mode == Mode::Hub)
        {
            context = Context::Hub;
            systemId = m_hubPresentation.systemId;
            bodyId = m_systemView.state().selectedHubParentBodyId;
            if (hubId.empty())
                hubId = m_hubPresentation.hubId;
        }

        waypoint->authoredMap = context;
        waypoint->authoredSystemId = systemId;
        waypoint->authoredBodyId = std::move(bodyId);
        waypoint->authoredHubId = std::move(hubId);
        waypoint->dynamicTarget =
            target.kind == game::navigation::NavigationRouteAnchorKind::Ship ||
            target.kind == game::navigation::NavigationRouteAnchorKind::Hub;
        waypoint->transitKind =
            target.kind == game::navigation::NavigationRouteAnchorKind::Ship
                ? game::navigation::NavigationWaypointTransitKind::Rendezvous
                : game::navigation::NavigationWaypointTransitKind::PassThrough;
    }
    else
    {
        waypoint = routePlan.findBySourceObjectId(objectId);
    }

    if (!waypoint)
        return;

    if (actionKey == "toggle_finish")
    {
        routePlan.toggleRole(
            waypoint->id,
            game::navigation::NavigationWaypointRole::Finish
        );
    }
    else if (actionKey == "toggle_intermediate")
    {
        routePlan.toggleRole(
            waypoint->id,
            game::navigation::NavigationWaypointRole::Intermediate
        );
    }
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::focusRouteWaypoint(
    std::uint64_t routeNodeId,
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    double nowSeconds
)
{
    const auto* waypoint =
        m_navigationWorkspace.routePlan().findById(routeNodeId);
    if (!waypoint ||
        waypoint->role == game::navigation::NavigationWaypointRole::None)
    {
        return std::nullopt;
    }

    m_pendingRouteFocusNodeId = routeNodeId;
    m_pendingRouteFocusContextApplied = false;
    return advancePendingRouteFocus(
        viewport,
        galaxy,
        system,
        nowSeconds
    );
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::advancePendingRouteFocus(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    double nowSeconds
)
{
    if (m_pendingRouteFocusNodeId == 0)
        return std::nullopt;

    const auto* waypoint = m_navigationWorkspace.routePlan().findById(
        m_pendingRouteFocusNodeId
    );
    if (!waypoint ||
        waypoint->role == game::navigation::NavigationWaypointRole::None)
    {
        m_pendingRouteFocusNodeId = 0;
        m_pendingRouteFocusContextApplied = false;
        return std::nullopt;
    }

    if (m_pendingRouteFocusContextApplied)
        return std::nullopt;

    using Context = game::navigation::NavigationRouteMapKind;
    using game::system_map::MapIntent;

    // The route remembers where the node was authored.  The renderer may
    // prepare selection/camera state, but changing map mode stays a SpaceState
    // responsibility and therefore leaves this class as a MapIntent.
    if (waypoint->authoredMap == Context::Galaxy)
    {
        if (m_mode != Mode::Galaxy)
            return MapIntent::recallRouteMap(Mode::Galaxy);

        const glm::dvec3 targetLy =
            world::coordinates::toGalacticLy(waypoint->worldPosition);
        m_galaxyView.state().navigationGrid.setAnchorFromPositionLy(targetLy);
        m_galaxyView.state().navigationFocusLy = targetLy;
        m_galaxyView.state().navigationFocusValid = true;
        m_galaxyView.beginCameraFlight(
            m_galaxyView.positionLyToRender(targetLy),
            m_galaxyView.state().camera.distance,
            nowSeconds
        );
        m_pendingRouteFocusContextApplied = true;
        (void)viewport;
        return std::nullopt;
    }

    // Known systems retain stable ids.  Empty-space System maps use ephemeral
    // negative ids, so recall them by their spatial anchor instead.
    if (waypoint->authoredSystemId >= 0 &&
        system.systemId != waypoint->authoredSystemId)
    {
        const auto found = std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& candidate)
            {
                return candidate.id == waypoint->authoredSystemId;
            }
        );
        if (found != galaxy.systems.end())
        {
            return MapIntent::enterKnownSystem(
                found->id,
                found->positionLy
            );
        }
        return std::nullopt;
    }

    if (waypoint->authoredSystemId < 0)
    {
        const glm::dvec3 targetLy =
            world::coordinates::toGalacticLy(waypoint->worldPosition);
        const bool wrongEmptyContext =
            system.systemId >= 0 ||
            glm::length(system.systemPositionLy - targetLy) > 1.0e-9;
        if (wrongEmptyContext)
            return MapIntent::enterEmptySector(targetLy);
    }

    if (waypoint->authoredMap == Context::System)
    {
        if (m_mode != Mode::System)
            return MapIntent::recallRouteMap(Mode::System);

        const glm::dvec3 relativeMeters =
            world::coordinates::fullMeters(waypoint->worldPosition) -
            system.systemPositionLy * world::coordinates::MetersPerLightYear;
        const glm::dvec3 relativeAu =
            relativeMeters / world::celestial::MetersPerAu;
        m_systemView.state().navigationGrid.setAnchorFromPosition(relativeAu);
        const double scale = std::max(
            1.0e-12,
            static_cast<double>(m_systemView.state().lastScale)
        );
        m_systemView.beginCameraFlight(
            relativeAu * scale,
            m_systemView.state().camera.distance,
            nowSeconds
        );
        m_pendingRouteFocusContextApplied = true;
        return std::nullopt;
    }

    if (waypoint->authoredMap == Context::Detail)
    {
        if (m_mode == Mode::Detail)
        {
            m_pendingRouteFocusContextApplied = true;
            return std::nullopt;
        }

        if (m_mode != Mode::System)
            return MapIntent::recallRouteMap(Mode::System);

        auto& state = m_systemView.state();
        if (!waypoint->authoredHubId.empty())
        {
            state.selectedBodyId.clear();
            state.selectedHubId = waypoint->authoredHubId;
            state.selectedHubParentBodyId = waypoint->authoredBodyId;
        }
        else if (!waypoint->authoredBodyId.empty())
        {
            state.selectedBodyId = waypoint->authoredBodyId;
            state.selectedHubId.clear();
            state.selectedHubParentBodyId.clear();
        }
        else if (state.navigationGrid.enabled())
        {
            const glm::dvec3 relativeMeters =
                world::coordinates::fullMeters(waypoint->worldPosition) -
                system.systemPositionLy * world::coordinates::MetersPerLightYear;
            const glm::dvec3 relativeAu =
                relativeMeters / world::celestial::MetersPerAu;
            const int maximumLevel = state.navigationGrid.definition().maximumLevel;
            const auto index = state.navigationGrid.nearestIndexForPosition(
                relativeAu,
                maximumLevel
            );
            state.selectedBodyId.clear();
            state.selectedHubId.clear();
            state.selectedHubParentBodyId.clear();
            state.navigationGrid.selectCell(
                state.navigationGrid.cell(index, maximumLevel)
            );
            state.navigationCellExplicitlySelected = true;
        }

        return MapIntent::openBody(waypoint->authoredBodyId);
    }

    if (waypoint->authoredMap == Context::Hub)
    {
        if (m_mode == Mode::Hub)
        {
            m_pendingRouteFocusContextApplied = true;
            return std::nullopt;
        }

        if (m_mode != Mode::System && m_mode != Mode::Detail)
            return MapIntent::recallRouteMap(Mode::System);

        if (waypoint->authoredHubId.empty())
            return std::nullopt;

        auto& state = m_systemView.state();
        state.selectedBodyId.clear();
        state.selectedHubId = waypoint->authoredHubId;
        state.selectedHubParentBodyId = waypoint->authoredBodyId;
        if (m_mode == Mode::Detail)
        {
            m_detailView.selectHub(
                waypoint->authoredHubId,
                waypoint->authoredBodyId
            );
        }

        return MapIntent::openHub(
            waypoint->authoredHubId,
            waypoint->authoredBodyId
        );
    }

    (void)viewport;
    return std::nullopt;
}

void SystemMapRenderer::revealPendingRouteFocus(
    const game::system_map::MapObjectOverlayFrame& frame,
    const Viewport& viewport
)
{
    if (m_pendingRouteFocusNodeId == 0)
        return;

    const auto* routeNode =
        m_navigationWorkspace.routePlan().findById(m_pendingRouteFocusNodeId);
    if (!routeNode || routeNode->sourceObjectId.empty())
        return;

    const auto found = std::find_if(
        frame.items.begin(),
        frame.items.end(),
        [&](const auto& item)
        {
            return item.objectId == routeNode->sourceObjectId;
        }
    );
    if (found == frame.items.end())
        return;

    if (!found->visible && (m_mode == Mode::Detail || m_mode == Mode::Hub))
    {
        // Local maps pan in screen space. Use the already-projected target as
        // a one-frame correction, then let the normal builder reproject it at
        // the center on the next frame.
        auto& camera =
            m_mode == Mode::Hub ? m_hubView.camera() : m_detailView.camera();
        camera.pan +=
            glm::dvec2(viewport.width * 0.5, viewport.height * 0.5) -
            found->screenPx;
        if (m_mode == Mode::Hub)
            m_hubFrameDirty = true;
        else
            m_detailFrameDirty = true;
        return;
    }

    if (!found->visible)
        return;

    m_objectOverlayState.activate(found->objectId);
    m_objectOverlayState.ensureOpen(
        *found,
        glm::dvec2(viewport.width, viewport.height)
    );
    m_pendingRouteFocusNodeId = 0;
    m_pendingRouteFocusContextApplied = false;
}

void SystemMapRenderer::updateActiveTacticalLocalContext(
    const game::system_map::MapObjectOverlayItem& item
)
{
    m_activeTacticalLocalTargetObjectId = item.objectId;
    m_activeTacticalDetailCell.reset();

    // A real Hub binding wins. The System/Detail semantic selection carries
    // that identity; no synthetic cubic address is needed.
    if (!item.navigationHubId.empty() ||
        !item.hasNavigationSystemPositionAu ||
        !m_systemView.state().navigationGrid.enabled())
    {
        return;
    }

    const auto& grid = m_systemView.state().navigationGrid;
    const int maximumLevel = grid.definition().maximumLevel;
    const auto index = grid.nearestIndexForPosition(
        item.navigationSystemPositionAu,
        maximumLevel
    );
    const auto cell = grid.cell(index, maximumLevel);

    world::celestial::DetailSpatialCell detailCell;
    detailCell.level = cell.level;
    detailCell.maximumLevel = maximumLevel;
    detailCell.x = cell.index.x;
    detailCell.y = cell.index.y;
    detailCell.z = cell.index.z;
    detailCell.centerAu = cell.center;
    detailCell.edgeAu = cell.size;
    m_activeTacticalDetailCell = detailCell;
}


std::optional<game::system_map::MapIntent>
SystemMapRenderer::handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& detail,
    const world::celestial::HubMapSnapshot& hub
)
{
    if (m_mode != Mode::Galaxy &&
        m_mode != Mode::System &&
        m_mode != Mode::Detail &&
        m_mode != Mode::Hub)
    {
        return std::nullopt;
    }

    const double inputNowSeconds =
        glfwGetTime();

    if (m_mode == Mode::System)
    {
        m_systemView.updateCameraFlight(
            inputNowSeconds
        );
    }

    /*
        Scroll input belongs to the application-wide Input service.
        SystemMapRenderer must not replace GLFW callbacks or keep raw
        pointers in a global table. The renderer only consumes the
        frame-local wheel delta while the map owns input focus.
    */
    m_pendingScrollY +=
        Input::instance().consumeScrollY();


    /*
        Во время crossfade нельзя вращать или перемещать
        старую либо новую сцену.

        AwaitingCapture тоже считается активной фазой.
    */
    if (m_mapTransition.blocksInput())
    {
        if (m_mode == Mode::System)
        {
            m_systemView.constrainCameraToNavigationBoundary(
                vp
            );
        }

        m_pendingScrollY = 0.0;
        m_navigationLevelZeroButtonHovered = false;
        m_navigationTrackButtonHovered = false;
        m_navigationOverlayLeftWasDown = false;
        return std::nullopt;
    }

    // Cross-map route recall is a small intent-driven state machine.  A prior
    // frame may have asked SpaceState to switch map/system; once that canonical
    // transition commits, continue toward the authored layer here.
    if (m_pendingRouteFocusNodeId != 0)
    {
        if (const auto intent = advancePendingRouteFocus(
                vp, galaxy, system, inputNowSeconds);
            intent.has_value())
        {
            return intent;
        }
    }


    GLFWwindow* window =
        glfwGetCurrentContext();

    if (!window)
        return std::nullopt;

    double mx = 0.0;
    double my = 0.0;

    glfwGetCursorPos(
        window,
        &mx,
        &my
    );

    const double localMx =
        mx - static_cast<double>(vp.x);

    const double localMy =
        my - static_cast<double>(vp.y);

    const bool inside =
        mx >= static_cast<double>(vp.x) &&
        my >= static_cast<double>(vp.y) &&
        mx <= static_cast<double>(vp.x + vp.width) &&
        my <= static_cast<double>(vp.y + vp.height);

    const bool leftDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_LEFT
        ) == GLFW_PRESS;

    const bool rightDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_RIGHT
        ) == GLFW_PRESS;

    const auto routePointer =
        m_routeOverlayState.handlePointer(
            m_navigationWorkspace.routePlan(),
            glm::dvec2(vp.width, vp.height),
            glm::dvec2(localMx, localMy),
            inside,
            leftDown
        );
    if (routePointer.selectedRouteNodeId != 0)
    {
        const auto* selectedNode =
            m_navigationWorkspace.routePlan().findById(
                routePointer.selectedRouteNodeId
            );
        if (selectedNode && !selectedNode->sourceObjectId.empty())
            m_objectOverlayState.activate(selectedNode->sourceObjectId);
    }
    if (routePointer.consumed)
    {
        std::optional<game::system_map::MapIntent> routeFocusIntent;
        if (routePointer.focusRouteNodeId != 0)
        {
            routeFocusIntent = focusRouteWaypoint(
                routePointer.focusRouteNodeId,
                vp,
                galaxy,
                system,
                inputNowSeconds
            );
        }


        m_pendingScrollY = 0.0;
        m_galaxyView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );
        m_systemView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );
        if (routeFocusIntent.has_value())
            return routeFocusIntent;
        return std::nullopt;
    }

    const bool showLevelZeroButton =
        m_mode == Mode::Galaxy ||
        m_mode == Mode::System;

    m_navigationLevelZeroButtonHovered =
        showLevelZeroButton &&
        inside &&
        render::navigation::
            NavigationCoordinateOverlay::
                levelZeroButtonBounds(
                    vp
                )
                .contains(
                    localMx,
                    localMy
                );

    const bool showTrackButton =
        m_mode == Mode::System;

    m_navigationTrackButtonHovered =
        showTrackButton &&
        inside &&
        render::navigation::
            NavigationCoordinateOverlay::
                trackButtonBounds(
                    vp
                )
                .contains(
                    localMx,
                    localMy
                );

    const bool levelZeroPressed =
        m_navigationLevelZeroButtonHovered &&
        leftDown &&
        !m_navigationOverlayLeftWasDown;

    const bool trackPressed =
        m_navigationTrackButtonHovered &&
        leftDown &&
        !m_navigationOverlayLeftWasDown;

    m_navigationOverlayLeftWasDown =
        leftDown;

    if (m_navigationLevelZeroButtonHovered ||
        m_navigationTrackButtonHovered)
    {
        m_pendingScrollY = 0.0;

        if (levelZeroPressed)
        {
            resetNavigationViewToLevelZero(
                vp
            );
        }

        if (trackPressed)
            toggleSelectedBodyTracking();

        /*
            The button is part of the map viewport, so explicitly prevent
            the scene behind it from receiving the same mouse gesture.
        */
        m_galaxyView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );

        m_systemView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );

        return std::nullopt;
    }

    if (m_mode == Mode::System)
    {
        m_systemPresentation =
            m_systemPresentationBuilder.build(
                m_systemView,
                vp,
                system,
                inputNowSeconds,
                false
            );

        updateSelectedBodyTracking(
            m_systemPresentation
        );

        m_systemSceneFrame =
            m_systemSceneFrameBuilder.build(
                m_systemView,
                *this,
                vp,
                system,
                m_systemPresentation
            );

        m_systemFramePrepared = true;
        m_systemSceneFrameDirty = false;

        const auto cameraBefore =
            m_systemView.state().camera;

        game::system_map::SystemMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.nowSeconds = inputNowSeconds;

        refreshSystemWaypointCandidate(vp, system);
        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );
        revealPendingRouteFocus(
            m_systemSceneFrame.interaction.objectOverlay,
            vp
        );

        const game::system_map::SystemMapFrameInteractionContext
            interactionContext(
                m_systemSceneFrame.interaction,
                m_systemView.controls()
            );

        const auto overlayPointer =
            m_objectOverlayState.handlePointer(
                m_systemSceneFrame.interaction.objectOverlay,
                glm::dvec2(
                    static_cast<double>(vp.width),
                    static_cast<double>(vp.height)
                ),
                glm::dvec2(localMx, localMy),
                inside,
                leftDown,
                interactionContext
                    .largestDirectBodyPhysicalSizeMetersAt(
                        localMx,
                        localMy
                    )
            );

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.actionObjectId.empty())
                applyWaypointAction(overlayPointer.actionObjectId, overlayPointer.actionKey);

            if (!overlayPointer.activatedObjectId.empty())
            {
                if (overlayPointer.activatedInfoKind ==
                        game::system_map::MapObjectInfoKind::Celestial &&
                    !overlayPointer.activatedSemanticTargetId.empty())
                {
                    m_systemInteraction.focusBodySelection(
                        m_systemView,
                        interactionContext,
                        overlayPointer.activatedSemanticTargetId,
                        inputNowSeconds
                    );
                }
                else if (overlayPointer.activatedInfoKind ==
                         game::system_map::MapObjectInfoKind::Tactical)
                {
                    const auto overlayItem = std::find_if(
                        m_systemSceneFrame.interaction.objectOverlay.items.begin(),
                        m_systemSceneFrame.interaction.objectOverlay.items.end(),
                        [&](const auto& item)
                        {
                            return item.objectId ==
                                overlayPointer.activatedObjectId;
                        }
                    );

                    const auto hubPoint = std::find_if(
                        m_systemSceneFrame.interaction.hubScreenPoints.begin(),
                        m_systemSceneFrame.interaction.hubScreenPoints.end(),
                        [&](const auto& point)
                        {
                            return point.hubId == overlayPointer.activatedObjectId;
                        }
                    );

                    if (hubPoint !=
                        m_systemSceneFrame.interaction.hubScreenPoints.end())
                    {
                        game::system_map::SystemMapHubSelection hubSelection;
                        hubSelection.hubId = hubPoint->hubId;
                        hubSelection.parentBodyId = hubPoint->parentBodyId;
                        m_systemInteraction.focusHubSelection(
                            m_systemView,
                            interactionContext,
                            hubSelection,
                            inputNowSeconds
                        );
                        if (overlayItem !=
                            m_systemSceneFrame.interaction.objectOverlay.items.end())
                        {
                            updateActiveTacticalLocalContext(*overlayItem);
                        }
                    }
                    else if (overlayItem !=
                             m_systemSceneFrame.interaction.objectOverlay.items.end())
                    {
                        m_systemInteraction.focusTacticalObjectSelection(
                            m_systemView,
                            overlayItem->navigationHubId,
                            overlayItem->navigationHubParentBodyId
                        );
                        updateActiveTacticalLocalContext(*overlayItem);
                    }
                    else
                    {
                        m_systemInteraction.focusTacticalObjectSelection(
                            m_systemView
                        );
                        m_activeTacticalLocalTargetObjectId.clear();
                        m_activeTacticalDetailCell.reset();
                    }
                }
            }

            synchronizeNavigationTracking(
                m_systemSceneFrame.interaction.objectOverlay
            );

            m_systemView.suppressCameraGesture(
                leftDown,
                rightDown,
                mx,
                my
            );
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        const auto result =
            m_systemInteraction.handleInput(
                m_systemView,
                interactionContext,
                frame,
                m_pendingScrollY
            );

        if (result.clickedBodyId.has_value())
        {
            const std::string cardId =
                "body:" + std::to_string(system.systemId) + ":" +
                *result.clickedBodyId;
            const auto bodyInfo = std::find_if(
                m_systemSceneFrame.interaction.objectOverlay.items.begin(),
                m_systemSceneFrame.interaction.objectOverlay.items.end(),
                [&](const game::system_map::MapObjectOverlayItem& item)
                {
                    return item.objectId == cardId;
                }
            );
            if (bodyInfo !=
                m_systemSceneFrame.interaction.objectOverlay.items.end())
            {
                m_objectOverlayState.activate(bodyInfo->objectId);
                m_objectOverlayState.ensureOpen(
                    *bodyInfo,
                    glm::dvec2(vp.width, vp.height)
                );
            }
        }

        if (result.clickedNavigationCell.has_value())
        {
            const auto& cell = *result.clickedNavigationCell;

            game::system_map::MapObjectOverlayItem candidate;
            candidate.objectId = waypointCandidateId('S', cell);
            candidate.infoKind =
                game::system_map::MapObjectInfoKind::WaypointCandidate;
            candidate.typeName = m_navigationMapTextProfile.navigationPoint;
            candidate.name = m_navigationMapTextProfile.spaceTarget;
            candidate.drawGlyph = true;
            candidate.pointerInteractive = true;
            candidate.screenAffordance = true;
            candidate.glyphScale = 0.78;
            candidate.hitRadiusPx = 12.0;
            candidate.facingScreenDirection = glm::dvec2(0.0, -1.0);
            candidate.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
            candidate.extraFields.push_back({
                "address",
                navigationCellAddress('S', cell),
                ""
            });
            candidate.trackingWorldPosition =
                world::coordinates::makeWorldPositionFromMeters(
                    system.systemPositionLy *
                        world::coordinates::MetersPerLightYear +
                    cell.center * world::celestial::MetersPerAu
                );
            candidate.hasTrackingWorldPosition = true;
            candidate.screenPx = glm::dvec2(localMx, localMy);
            candidate.visible = true;
            m_systemWaypointCandidate = candidate;
            refreshSystemWaypointCandidate(vp, system);
        }

        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );

        const auto& semanticSelection = m_systemView.state();
        if (!semanticSelection.selectedHubId.empty())
        {
            // A tactical ship may intentionally retain its parent Hub only as
            // the local-neighborhood drill target. Do not replace the active
            // ship with that Hub on the following frame.
            const bool tacticalObjectOwnsHubContext =
                !m_activeTacticalLocalTargetObjectId.empty() &&
                m_objectOverlayState.activeObjectId() ==
                    m_activeTacticalLocalTargetObjectId;
            if (!tacticalObjectOwnsHubContext)
            {
                m_objectOverlayState.activate(
                    semanticSelection.selectedHubId
                );
            }
        }
        else if (!semanticSelection.selectedBodyId.empty() ||
                 semanticSelection.navigationCellExplicitlySelected)
        {
            m_objectOverlayState.clearActive();
        }

        if (m_systemView.state().navigationGrid.enabled() &&
            m_systemPresentation.systemScale > 0.0f)
        {
            m_systemView.updateNavigationHoverPresentation(
                vp,
                inputNowSeconds
            );
        }

        const auto& cameraAfter =
            m_systemView.state().camera;

        m_systemSceneFrameDirty =
            glm::length(
                cameraBefore.target - cameraAfter.target
            ) > 0.0 ||
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.distance != cameraAfter.distance;

        if (result.systemLevelChanged.has_value())
        {
            announceNavigationLevel(
                'S',
                result.systemLevelChanged.value()
            );
        }

        return std::nullopt;
    }

    if (m_mode == Mode::Detail ||
        m_mode == Mode::Hub)
    {
        if (m_mode == Mode::Detail)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    vp,
                    detail
                );
            m_detailFramePrepared = true;
            m_detailFrameDirty = false;
        }
        else
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    vp,
                    hub
                );
            decorateHubDockingOverlay(hub);
            m_hubFramePrepared = true;
            m_hubFrameDirty = false;
        }

        const auto cameraBefore =
            m_mode == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        auto& objectOverlay =
            m_mode == Mode::Hub
                ? m_hubPresentation.frame.objectOverlay
                : m_detailPresentation.frame.objectOverlay;
        synchronizeNavigationTracking(objectOverlay);
        revealPendingRouteFocus(objectOverlay, vp);

        const auto overlayPointer =
            m_objectOverlayState.handlePointer(
                objectOverlay,
                glm::dvec2(
                    static_cast<double>(vp.width),
                    static_cast<double>(vp.height)
                ),
                glm::dvec2(localMx, localMy),
                inside,
                leftDown
            );

        // Hub infrastructure is not selected through a screen-space radius or
        // convex proxy.  Dock markers/cards get first refusal above; only an
        // otherwise-unconsumed press is tested against actual assembly
        // triangles.  Empty space remains available to the orbit gesture.
        if (m_mode == Mode::Hub &&
            overlayPointer.primaryPressStarted &&
            !overlayPointer.consumed)
        {
            const glm::dvec2 mousePx(localMx, localMy);
            const auto* body = pickHubInfrastructureBody(
                m_hubPresentation.camera,
                hub,
                mousePx
            );

            if (body && !body->stableId.empty())
            {
                const std::string objectId =
                    std::string(HubModuleOverlayPrefix) + body->stableId;
                const auto item = std::find_if(
                    objectOverlay.items.begin(),
                    objectOverlay.items.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.objectId == objectId;
                    }
                );

                if (item != objectOverlay.items.end())
                {
                    m_objectOverlayState.activate(objectId);
                    m_objectOverlayState.ensureOpen(
                        *item,
                        glm::dvec2(
                            static_cast<double>(vp.width),
                            static_cast<double>(vp.height)
                        )
                    );

                    m_hubFrameDirty = true;
                    auto& camera = m_hubView.camera();
                    camera.rotating = false;
                    camera.panning = false;
                    camera.lastMouseX = mx;
                    camera.lastMouseY = my;
                    m_pendingScrollY = 0.0;
                    return std::nullopt;
                }
            }
            else if (!m_objectOverlayState.activeObjectId().empty())
            {
                // Empty Hub-map press only deselects. Information-card
                // lifetime is independent from selection and is controlled
                // exclusively by the card X button. The same press remains
                // available to start orbit/pan.
                m_objectOverlayState.clearActive();
                m_hubFrameDirty = true;
            }
        }

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.closedObjectId.empty())
                cancelDockingTaskForClosedCard(overlayPointer.closedObjectId);

            if (!overlayPointer.actionObjectId.empty())
            {
                if (overlayPointer.actionKey == "calculate_docking_route")
                {
                    applyDockingAction(
                        overlayPointer.actionObjectId,
                        overlayPointer.actionKey
                    );
                }
                else
                {
                    applyWaypointAction(
                        overlayPointer.actionObjectId,
                        overlayPointer.actionKey
                    );
                }
            }

            if (m_mode == Mode::Detail &&
                !overlayPointer.activatedObjectId.empty())
            {
                const auto item = std::find_if(
                    objectOverlay.items.begin(),
                    objectOverlay.items.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.objectId ==
                            overlayPointer.activatedObjectId;
                    }
                );

                if (item != objectOverlay.items.end() &&
                    !item->navigationHubId.empty())
                {
                    m_detailView.selectHub(
                        item->navigationHubId,
                        item->navigationHubParentBodyId
                    );
                }
                else
                {
                    m_detailView.clearHubSelection();
                }
            }

            if (m_mode == Mode::Hub &&
                !overlayPointer.closedObjectId.empty())
            {
                m_hubFrameDirty = true;
            }

            auto& camera =
                m_mode == Mode::Hub
                    ? m_hubView.camera()
                    : m_detailView.camera();
            camera.rotating = false;
            camera.panning = false;
            camera.lastMouseX = mx;
            camera.lastMouseY = my;
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        handleDetailAndHubInput(
            vp,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown
        );

        const auto& cameraAfter =
            m_mode == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        const bool projectionChanged =
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.zoom != cameraAfter.zoom ||
            glm::length(
                cameraBefore.pan - cameraAfter.pan
            ) > 0.0;

        if (m_mode == Mode::Detail)
            m_detailFrameDirty = projectionChanged;
        else
            m_hubFrameDirty = projectionChanged;

        return std::nullopt;
    }

    if (m_mode == Mode::Galaxy)
    {
        refreshGalaxyWaypointCandidate(vp, galaxy);
        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
        revealPendingRouteFocus(m_galaxyInfoOverlayFrame, vp);

        const auto overlayPointer =
            m_objectOverlayState.handlePointer(
                m_galaxyInfoOverlayFrame,
                glm::dvec2(vp.width, vp.height),
                glm::dvec2(localMx, localMy),
                inside,
                leftDown
            );

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.actionObjectId.empty())
                applyWaypointAction(overlayPointer.actionObjectId, overlayPointer.actionKey);

            synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
            m_galaxyView.suppressCameraGesture(
                leftDown,
                rightDown,
                mx,
                my
            );
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        game::system_map::GalaxyMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.transitionActive = m_mapTransition.active();
        frame.nowSeconds = glfwGetTime();

        const auto result =
            m_galaxyInteraction.handleInput(
                m_galaxyView,
                galaxy,
                frame,
                m_pendingScrollY
            );

        if (result.clickedNavigationCell.has_value() &&
            m_galaxyView.state().selectedSystemId < 0)
        {
            const auto& cell = *result.clickedNavigationCell;

            game::system_map::MapObjectOverlayItem candidate;
            candidate.objectId = waypointCandidateId('G', cell);
            candidate.infoKind =
                game::system_map::MapObjectInfoKind::WaypointCandidate;
            candidate.typeName = m_navigationMapTextProfile.navigationPoint;
            candidate.name = m_navigationMapTextProfile.spaceTarget;
            candidate.drawGlyph = true;
            candidate.pointerInteractive = true;
            candidate.screenAffordance = true;
            candidate.glyphScale = 0.78;
            candidate.hitRadiusPx = 12.0;
            candidate.facingScreenDirection = glm::dvec2(0.0, -1.0);
            candidate.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
            candidate.extraFields.push_back({
                "address",
                navigationCellAddress('G', cell),
                ""
            });
            candidate.trackingWorldPosition =
                world::coordinates::makeWorldPositionFromMeters(
                    cell.center * world::coordinates::MetersPerLightYear
                );
            candidate.hasTrackingWorldPosition = true;
            candidate.screenPx = glm::dvec2(localMx, localMy);
            candidate.visible = true;
            m_galaxyWaypointCandidate = candidate;
            refreshGalaxyWaypointCandidate(vp, galaxy);
        }

        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);

        if (result.requestWindowFocus)
            glfwFocusWindow(window);

        if (result.galaxyLevelChanged.has_value())
        {
            announceNavigationLevel(
                'G',
                result.galaxyLevelChanged.value()
            );
        }

        if (result.mapIntent.has_value())
        {
            const auto type =
                result.mapIntent->type;

            if (type ==
                    game::system_map::MapIntentType::
                        EnterKnownSystem ||
                type ==
                    game::system_map::MapIntentType::
                        EnterEmptySector)
            {
                announceNavigationLevel(
                    'S',
                    m_systemView.state().navigationGrid
                        .definition()
                        .minimumLevel
                );
            }
        }

        return result.mapIntent;
    }

    return std::nullopt;

}







































void SystemMapRenderer::ensureBackground()
{
    if (m_bgVao && m_bgVbo && m_bgShader)
        return;

    m_bgShader = ShaderLibrary::instance().get("system_map_background");

    if (!m_bgShader)
    {

        return;
    }

    const float verts[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_bgVao);
    glGenBuffers(1, &m_bgVbo);

    glBindVertexArray(m_bgVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_bgVbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 2,
        reinterpret_cast<void*>(0)
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}










void SystemMapRenderer::drawBackground()
{
    if (!m_bgShader || !m_bgVao)
        return;

    const GLboolean depthWasEnabled =
        glIsEnabled(GL_DEPTH_TEST);

    const GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bgShader);

    /*
        Проход 0 — обычный фон карты.
        Проход 1 используется только для transition snapshot.
    */
    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glBindVertexArray(m_bgVao);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




















void SystemMapRenderer::drawMapAtmosphereVeil(
    float centerAlpha,
    float edgeAlpha,
    float aquaStrength
)
{
    if (!m_bgShader || !m_bgVao)
        return;

    /*
        Теперь это не "alpha тёмной пелены", а сила затемнения
        уже нарисованного starfield.

        0.0 = не затемнять
        1.0 = полностью убить яркость
    */
    centerAlpha =
        std::clamp(
            centerAlpha,
            0.0f,
            0.95f
        );

    edgeAlpha =
        std::clamp(
            edgeAlpha,
            centerAlpha,
            0.95f
        );

    aquaStrength =
        std::clamp(
            aquaStrength,
            0.0f,
            1.0f
        );

    if (edgeAlpha <= 0.0f)
        return;

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean previousDepthWriteMask =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &previousDepthWriteMask
    );

    GLint previousBlendEquationRgb =
        GL_FUNC_ADD;

    GLint previousBlendEquationAlpha =
        GL_FUNC_ADD;

    GLint previousBlendSourceRgb =
        GL_ONE;

    GLint previousBlendDestinationRgb =
        GL_ZERO;

    GLint previousBlendSourceAlpha =
        GL_ONE;

    GLint previousBlendDestinationAlpha =
        GL_ZERO;

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &previousBlendDestinationAlpha
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_FALSE
    );

    glEnable(
        GL_BLEND
    );

    /*
        Самая важная часть.

        Мы не добавляем поверх тёмный цвет.
        Мы умножаем уже нарисованный starfield
        на вычисленный RGB-множитель из шейдера.
    */
    glBlendEquationSeparate(
        GL_FUNC_ADD,
        GL_FUNC_ADD
    );

    glBlendFuncSeparate(
        GL_ZERO,
        GL_SRC_COLOR,
        GL_ZERO,
        GL_ONE
    );

    glUseProgram(
        m_bgShader
    );

    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    const GLint centerAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilCenterAlpha"
        );

    const GLint edgeAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilEdgeAlpha"
        );

    const GLint aquaStrengthLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilAquaStrength"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            2
        );
    }

    if (centerAlphaLoc >= 0)
    {
        glUniform1f(
            centerAlphaLoc,
            centerAlpha
        );
    }

    if (edgeAlphaLoc >= 0)
    {
        glUniform1f(
            edgeAlphaLoc,
            edgeAlpha
        );
    }

    if (aquaStrengthLoc >= 0)
    {
        glUniform1f(
            aquaStrengthLoc,
            aquaStrength
        );
    }

    glBindVertexArray(
        m_bgVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(
        0
    );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glUseProgram(
        0
    );

    glBlendEquationSeparate(
        static_cast<GLenum>(
            previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            previousBlendEquationAlpha
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            previousBlendDestinationAlpha
        )
    );

    glDepthMask(
        previousDepthWriteMask
    );

    if (depthWasEnabled)
    {
        glEnable(
            GL_DEPTH_TEST
        );
    }
    else
    {
        glDisable(
            GL_DEPTH_TEST
        );
    }

    if (blendWasEnabled)
    {
        glEnable(
            GL_BLEND
        );
    }
    else
    {
        glDisable(
            GL_BLEND
        );
    }
}














void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}







#include "src/game/system_map/SystemMapRendererCommon.inl"
