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


SystemMapRenderer::SystemMapRenderer()
    : m_detailBackend(m_mapResources),
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
    }
    else if (m_mode == Mode::System)
    {
        renderSystem(
            vp,
            system,
            nav
        );
    }

    if (m_mode == Mode::System)
    {
        m_objectOverlayRenderer.render(
            vp,
            m_systemSceneFrame.interaction.objectOverlay,
            m_objectOverlayState,
            m_navigationNamingLocale
        );
    }
    else if (m_mode == Mode::Detail)
    {
        m_objectOverlayRenderer.render(
            vp,
            m_detailPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationNamingLocale
        );
    }
    else if (m_mode == Mode::Hub)
    {
        m_objectOverlayRenderer.render(
            vp,
            m_hubPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationNamingLocale
        );
    }

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
            if (!overlayPointer.toggledObjectId.empty())
            {
                const auto hubPoint = std::find_if(
                    m_systemSceneFrame.interaction.hubScreenPoints.begin(),
                    m_systemSceneFrame.interaction.hubScreenPoints.end(),
                    [&](const auto& point)
                    {
                        return point.hubId == overlayPointer.toggledObjectId;
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
                }
            }

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
            m_hubFramePrepared = true;
            m_hubFrameDirty = false;
        }

        const auto cameraBefore =
            m_mode == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        const auto& objectOverlay =
            m_mode == Mode::Hub
                ? m_hubPresentation.frame.objectOverlay
                : m_detailPresentation.frame.objectOverlay;

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

        if (overlayPointer.consumed)
        {
            if (m_mode == Mode::Detail &&
                !overlayPointer.toggledObjectId.empty())
            {
                const auto hubPoint = std::find_if(
                    m_detailPresentation.frame.hubScreenPoints.begin(),
                    m_detailPresentation.frame.hubScreenPoints.end(),
                    [&](const auto& point)
                    {
                        return point.hubId == overlayPointer.toggledObjectId;
                    }
                );

                if (hubPoint !=
                    m_detailPresentation.frame.hubScreenPoints.end())
                {
                    m_detailView.selectHub(
                        hubPoint->hubId,
                        hubPoint->parentBodyId
                    );
                }
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
