#include "src/game/system_map/SystemMapRenderer.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <fstream>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/HUD/TextRenderer.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

namespace
{
    constexpr double AU_KM = 149597870.7;
    
    double g_systemMapScrollY = 0.0;

    void systemMapScrollCallback(GLFWwindow*, double, double yoffset)
    {
        g_systemMapScrollY += yoffset;
    }


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
}

void SystemMapRenderer::init()
{
    ensureGlObjects();
    ensureShader();
    ensureBackground();

    GLFWwindow* window = glfwGetCurrentContext();
    if (window)
        glfwSetScrollCallback(window, systemMapScrollCallback);

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




void SystemMapRenderer::resetView()
{
    m_mode = Mode::Galaxy;
    m_galaxyCamera = GalaxyCamera{};
    m_systemCamera = SystemCamera{};
    m_selectedSystemId = -1;
    m_focusedSystemId = -1;
    m_comboOpen = false;
}




void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    for (const auto& s : galaxy.systems)
    {
        if (s.id != systemId)
            continue;

        m_selectedSystemId = s.id;
        m_focusedSystemId = s.id;
        m_galaxyCamera.target = galaxyStarPosition(s);

        // Не зумим насильно, но если камера слишком далеко —
        // подводим к рабочей дистанции.
        if (m_galaxyCamera.distance > 130.0f)
            m_galaxyCamera.distance = 130.0f;

        return;
    }
}

int SystemMapRenderer::selectedSystemId() const
{
    return m_selectedSystemId;
}


int SystemMapRenderer::focusedSystemId() const
{
    return m_focusedSystemId;
}


void SystemMapRenderer::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (m_mode == Mode::Hub)
    {
        m_planetCamera.yaw = 0.0;
        m_planetCamera.pitch = 0.0;
        m_planetCamera.zoom = 1.0;
        m_planetCamera.pan = glm::dvec2(0.0);

        m_planetCamera.rotating = false;
        m_planetCamera.panning = false;

        m_hubMapOrbitPivotLocalMeters =
            glm::dvec3(0.0, 0.0, 0.0);

        m_hubMapOrbitPivotScreenPx =
            glm::dvec2(0.0, 0.0);
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

void SystemMapRenderer::addCircleYZ(
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

        glm::vec3 p0 = center + glm::vec3(0.0f, std::cos(a0) * radius, std::sin(a0) * radius);
        glm::vec3 p1 = center + glm::vec3(0.0f, std::cos(a1) * radius, std::sin(a1) * radius);

        addLine(p0, p1, color);
    }
}

void SystemMapRenderer::addSphereWire(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color
)
{
    addCircleXZ(center, radius, color, 64);
    addCircleXY(center, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.65f), 64);
    addCircleYZ(center, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.65f), 64);
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

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

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

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_solidVertices.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
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

glm::vec4 SystemMapRenderer::colorForStarType(const std::string& starType) const
{
    if (starType.empty())
        return { 1.0f, 0.86f, 0.65f, 1.0f };

    const char t = static_cast<char>(std::toupper(starType[0]));

    switch (t)
    {
        case 'O': return { 0.61f, 0.69f, 1.00f, 1.0f };
        case 'B': return { 0.66f, 0.75f, 1.00f, 1.0f };
        case 'A': return { 0.86f, 0.91f, 1.00f, 1.0f };
        case 'F': return { 0.97f, 0.97f, 1.00f, 1.0f };
        case 'G': return { 1.00f, 0.92f, 0.62f, 1.0f };
        case 'K': return { 1.00f, 0.73f, 0.45f, 1.0f };
        case 'M': return { 1.00f, 0.43f, 0.31f, 1.0f };
        default:  return { 1.00f, 0.86f, 0.65f, 1.0f };
    }
}

glm::vec4 SystemMapRenderer::colorForBodyType(world::celestial::BodyType type) const
{
    using world::celestial::BodyType;

    switch (type)
    {
        case BodyType::Star:         return { 1.00f, 0.93f, 0.62f, 1.00f };
        case BodyType::Planet:       return { 0.36f, 0.68f, 1.00f, 1.00f };
        case BodyType::Moon:         return { 0.70f, 0.72f, 0.78f, 1.00f };
        case BodyType::AsteroidBelt: return { 0.45f, 0.45f, 0.45f, 0.65f };
        default:                     return { 0.60f, 0.82f, 1.00f, 1.00f };
    }
}

float SystemMapRenderer::bodyVisualRadius(
    const world::celestial::SystemMapBody& body,
    float distanceScale
) const
{
    using world::celestial::BodyType;

    const double radiusKm = body.radiusKm;

    if (radiusKm <= 0.0)
        return 0.045f;

    const double radiusAu = radiusKm / AU_KM;

    const float boost =
        body.type == BodyType::Star ? 7.0f :
        body.type == BodyType::Moon ? 36.0f :
                                      42.0f;

    const float raw =
        static_cast<float>(radiusAu) * distanceScale * boost;

    if (body.type == BodyType::Star)
        return std::clamp(raw, 0.16f, 1.10f);

    if (body.type == BodyType::Moon)
        return std::clamp(raw, 0.045f, 0.22f);

    if (body.type == BodyType::Planet)
    {
        const float earthRadiusKm = 6371.0f;

        const float relative =
            static_cast<float>(body.radiusKm / earthRadiusKm);

        const float compressed =
            std::pow(relative, 0.55f) * 0.09f;

        return std::clamp(compressed, 0.055f, 0.55f);
    }

    return std::clamp(raw, 0.035f, 0.12f);
}



void SystemMapRenderer::render(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::PlanetMapSnapshot& planet,
    const world::celestial::HubMapSnapshot& hub,
    const world::celestial::PlayerNavigationState& nav
)
{
    
        
    if (!m_initialized)
        init();
    const Viewport& vp = viewport;

    glViewport(vp.x, vp.y, vp.width, vp.height);
    glScissor(vp.x, vp.y, vp.width, vp.height);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);

    drawBackground();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_mode == Mode::Planet)
    {
        renderPlanetMap(
            viewport,
            planet
        );

        return;
    }

    if (m_mode == Mode::Hub)
    {
        renderHubMap(
            viewport,
            hub
        );

        return;
    }


    if (m_mode == Mode::Galaxy)
        renderGalaxy(vp, galaxy, nav);
    else
        renderSystem(vp, system, nav);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}




glm::vec3 SystemMapRenderer::galaxyStarPosition(
    const world::celestial::GalaxyMapSystem& s
) const
{
    const float scale = 2.2f;

    return {
        static_cast<float>(s.positionLy.x * scale),
        static_cast<float>(s.positionLy.y * scale),
        static_cast<float>(s.positionLy.z * scale)
    };
}

glm::mat4 SystemMapRenderer::galaxyViewMatrix() const
{
    const float cp = std::cos(m_galaxyCamera.pitch);
    const float sp = std::sin(m_galaxyCamera.pitch);
    const float cy = std::cos(m_galaxyCamera.yaw);
    const float sy = std::sin(m_galaxyCamera.yaw);

    glm::vec3 dir {
        cp * sy,
        sp,
        cp * cy
    };

    glm::vec3 eye =
        m_galaxyCamera.target + dir * m_galaxyCamera.distance;

    return glm::lookAt(
        eye,
        m_galaxyCamera.target,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

glm::mat4 SystemMapRenderer::galaxyProjectionMatrix(const Viewport& vp) const
{
    const float aspect =
        vp.height > 0
            ? static_cast<float>(vp.width) / static_cast<float>(vp.height)
            : 1.0f;

    return glm::perspective(
        glm::radians(48.0f),
        aspect,
        0.1f,
        2000.0f
    );
}




glm::mat4 SystemMapRenderer::systemViewMatrix() const
{
    const float cp = std::cos(m_systemCamera.pitch);
    const float sp = std::sin(m_systemCamera.pitch);
    const float cy = std::cos(m_systemCamera.yaw);
    const float sy = std::sin(m_systemCamera.yaw);

    glm::vec3 dir {
        cp * sy,
        sp,
        cp * cy
    };

    glm::vec3 eye =
        m_systemCamera.target + dir * m_systemCamera.distance;

    return glm::lookAt(
        eye,
        m_systemCamera.target,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

glm::mat4 SystemMapRenderer::systemProjectionMatrix(const Viewport& vp) const
{
    const float aspect =
        vp.height > 0
            ? static_cast<float>(vp.width) / static_cast<float>(vp.height)
            : 1.0f;

    return glm::perspective(
        glm::radians(48.0f),
        aspect,
        0.1f,
        5000.0f
    );
}








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
    depth = 1.0f;

    if (std::abs(clip.w) < 0.00001f)
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

int SystemMapRenderer::pickGalaxySystem(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    double mouseX,
    double mouseY
) const
{
    const glm::mat4 mvp =
        galaxyProjectionMatrix(vp) * galaxyViewMatrix();

    int bestId = -1;
    float bestDist = 999999.0f;

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            galaxyStarPosition(s),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const float dx = screen.x - static_cast<float>(mouseX);
        const float dy = screen.y - static_cast<float>(mouseY);
        const float d = std::sqrt(dx * dx + dy * dy);

        const float pickRadius = 12.0f;

        if (d < pickRadius && d < bestDist)
        {
            bestDist = d;
            bestId = s.id;
        }
    }

    return bestId;
}





void SystemMapRenderer::handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    if (m_mode != Mode::Galaxy &&
        m_mode != Mode::System &&
        m_mode != Mode::Planet &&
        m_mode != Mode::Hub)
    {
        return;
    }

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return;

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);

    const double localMx =
        mx - static_cast<double>(vp.x);

    const double localMy =
        my - static_cast<double>(vp.y);

    const bool inside =
        mx >= vp.x &&
        my >= vp.y &&
        mx <= vp.x + vp.width &&
        my <= vp.y + vp.height;

    const bool leftDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    const bool rightDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    const double dx = mx - m_galaxyCamera.lastMouseX;
    const double dy = my - m_galaxyCamera.lastMouseY;

    // =========================================================
    // SYSTEM MODE INPUT
    // =========================================================
    if (m_mode == Mode::System)
    {
        if (inside && leftDown && !m_galaxyCamera.leftWasDown)
        {
            const int pickedBody =
                pickSystemBody(localMx, localMy);

            if (pickedBody >= 0)
            {
                m_selectedBodyId =
                    m_lastSystemBodyScreenPoints[pickedBody].bodyId;

                m_systemCamera.rotating = false;
            }
            else
            {
                m_systemCamera.rotating = true;
            }
        }

        if (!leftDown)
            m_systemCamera.rotating = false;

        if (inside && rightDown && !m_galaxyCamera.rightWasDown)
            m_systemCamera.panning = true;

        if (!rightDown)
            m_systemCamera.panning = false;

        if (m_systemCamera.rotating && leftDown)
        {
            m_systemCamera.yaw -= static_cast<float>(dx) * 0.008f;
            m_systemCamera.pitch += static_cast<float>(dy) * 0.008f;

            m_systemCamera.pitch =
                std::clamp(m_systemCamera.pitch, -1.35f, 1.35f);
        }

        if (m_systemCamera.panning && rightDown)
        {
            const glm::mat4 view = systemViewMatrix();

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

            const float panScale =
                m_systemCamera.distance * 0.0018f;

            m_systemCamera.target -= right * static_cast<float>(dx) * panScale;
            m_systemCamera.target += up    * static_cast<float>(dy) * panScale;
        }

        if (inside)
        {
            float zoom = 0.0f;

            if (g_systemMapScrollY != 0.0)
            {
                zoom += static_cast<float>(g_systemMapScrollY);
                g_systemMapScrollY = 0.0;
            }

            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                zoom += 1.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                zoom -= 1.0f;
            }

            if (zoom != 0.0f)
            {
                const float factor =
                    zoom > 0.0f ? 0.88f : 1.12f;

                m_systemCamera.distance *= factor;
                m_systemCamera.distance =
                    std::clamp(m_systemCamera.distance, 3.0f, 800.0f);
            }
        }

        m_galaxyCamera.leftWasDown = leftDown;
        m_galaxyCamera.rightWasDown = rightDown;
        m_galaxyCamera.lastMouseX = mx;
        m_galaxyCamera.lastMouseY = my;

        return;
    }

    // =========================================================
    // DETAILS MODE INPUT
    // =========================================================

        if (m_mode == Mode::Planet ||
            m_mode == Mode::Hub)
        {
            double wheel = 0.0;

            if (inside && g_systemMapScrollY != 0.0)
            {
                wheel = g_systemMapScrollY;
                g_systemMapScrollY = 0.0;
            }

        

            if (leftDown && !m_planetCamera.rotating)
            {
                m_planetCamera.rotating = true;
                m_planetCamera.lastMouseX = mx;
                m_planetCamera.lastMouseY = my;

                if (m_mode == Mode::Hub)
                {
                    const glm::dvec2 centerPx(
                        static_cast<double>(vp.width) * 0.5,
                        static_cast<double>(vp.height) * 0.5
                    );

                    const glm::dvec2 mousePx(
                        localMx,
                        localMy
                    );

                    const double safeScale =
                        std::max(
                            0.000001,
                            m_lastHubMapScale
                        );

                    glm::dvec3 pickedPivotLocalMeters {0.0, 0.0, 0.0};

                const bool pickedObject =
                    pickHubMapOrbitPivot(
                        mousePx,
                        pickedPivotLocalMeters
                    );

                if (pickedObject)
                {
                    m_hubMapOrbitPivotLocalMeters =
                        pickedPivotLocalMeters;
                }
                else
                {
                    // Fallback для пустоты.
                    // Да, тут глубина условная, но это лучше,
                    // чем вообще не иметь вращения.
                    m_hubMapOrbitPivotLocalMeters =
                        hubMapUnprojectCursorToLocal(
                            mousePx,
                            safeScale,
                            centerPx
                        );
                }

                m_hubMapOrbitPivotScreenPx =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );
                }
            }








        if (!leftDown)
            m_planetCamera.rotating = false;

        if (rightDown && !m_planetCamera.panning)
        {
            m_planetCamera.panning = true;
            m_planetCamera.lastMouseX = mx;
            m_planetCamera.lastMouseY = my;
        }

        if (!rightDown)
            m_planetCamera.panning = false;

        const double dx = mx - m_planetCamera.lastMouseX;
        const double dy = my - m_planetCamera.lastMouseY;



        if (m_planetCamera.rotating)
        {
            if (m_mode == Mode::Hub)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const double safeScale =
                    std::max(
                        0.000001,
                        m_lastHubMapScale
                    );

                const glm::dvec2 pivotScreenBefore =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );

                m_planetCamera.yaw += dx * 0.008;
                m_planetCamera.pitch += dy * 0.008;

                m_planetCamera.pitch =
                    std::clamp(
                        m_planetCamera.pitch,
                        -1.45,
                        1.45
                    );

                const glm::dvec2 pivotScreenAfter =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );

                // Компенсируем pan так, чтобы pivot остался
                // на том же экранном месте. Вот это и есть
                // настоящее вращение вокруг точки под курсором.
                m_planetCamera.pan +=
                    pivotScreenBefore - pivotScreenAfter;
            }
            else
            {
                m_planetCamera.yaw += dx * 0.008;
                m_planetCamera.pitch += dy * 0.008;

                m_planetCamera.pitch =
                    std::clamp(
                        m_planetCamera.pitch,
                        -1.45,
                        1.45
                    );
            }
        }

        if (m_planetCamera.panning)
        {
            m_planetCamera.pan.x += dx;
            m_planetCamera.pan.y += dy;
        }





        if (std::abs(wheel) > 0.001)
        {
            const double oldZoom =
                m_planetCamera.zoom;

            const double newZoom =
                std::clamp(
                    oldZoom * std::pow(1.12, wheel),
                    0.15,
                    64.0
                );

            if (std::abs(newZoom - oldZoom) > 0.000001)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const glm::dvec2 mousePx(
                    localMx,
                    localMy
                );

                const double zoomFactor =
                    newZoom / oldZoom;

                // Keep the world point under the cursor stable.
                m_planetCamera.pan =
                    mousePx -
                    centerPx -
                    (mousePx - centerPx - m_planetCamera.pan) * zoomFactor;

                m_planetCamera.zoom =
                    newZoom;
            }
        }





        m_planetCamera.lastMouseX = mx;
        m_planetCamera.lastMouseY = my;

        return;
    }


    // =========================================================
    // GALAXY MODE INPUT
    // =========================================================

    if (inside && leftDown && !m_galaxyCamera.leftWasDown)
    {
        bool pivotFound = false;

        const glm::vec3 pivot = nearestVisibleStarToViewportCenter(
            vp,
            galaxy,
            pivotFound
        );

        if (pivotFound)
            m_galaxyCamera.target = pivot;

        m_galaxyCamera.rotating = true;
    }

    if (!leftDown && m_galaxyCamera.leftWasDown)
    {
        if (inside)
        {
            const double move =
                std::abs(mx - m_galaxyCamera.lastMouseX) +
                std::abs(my - m_galaxyCamera.lastMouseY);

            if (move < 2.0)
            {
                const int picked =
                    pickGalaxySystem(vp, galaxy, localMx, localMy);

                if (picked >= 0)
                {
                    m_selectedSystemId = picked;
                    m_focusedSystemId = picked;
                }
            }
        }

        m_galaxyCamera.rotating = false;
    }

    if (inside && rightDown && !m_galaxyCamera.rightWasDown)
        m_galaxyCamera.panning = true;

    if (!rightDown)
        m_galaxyCamera.panning = false;

    if (m_galaxyCamera.rotating && leftDown)
    {
        m_galaxyCamera.yaw -= static_cast<float>(dx) * 0.008f;
        m_galaxyCamera.pitch += static_cast<float>(dy) * 0.008f;

        m_galaxyCamera.pitch =
            std::clamp(m_galaxyCamera.pitch, -1.35f, 1.35f);
    }

    if (m_galaxyCamera.panning && rightDown)
    {
        const glm::mat4 view = galaxyViewMatrix();

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

        const float panScale =
            m_galaxyCamera.distance * 0.0018f;

        m_galaxyCamera.target -= right * static_cast<float>(dx) * panScale;
        m_galaxyCamera.target += up    * static_cast<float>(dy) * panScale;
    }

    if (inside)
    {
        float zoom = 0.0f;

        if (g_systemMapScrollY != 0.0)
        {
            zoom += static_cast<float>(g_systemMapScrollY);
            g_systemMapScrollY = 0.0;
        }

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            zoom += 1.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            zoom -= 1.0f;
        }

        if (zoom != 0.0f)
        {
            const float factor =
                zoom > 0.0f ? 0.88f : 1.12f;

            m_galaxyCamera.distance *= factor;
            m_galaxyCamera.distance =
                std::clamp(m_galaxyCamera.distance, 8.0f, 700.0f);
        }
    }

    m_galaxyCamera.leftWasDown = leftDown;
    m_galaxyCamera.rightWasDown = rightDown;
    m_galaxyCamera.lastMouseX = mx;
    m_galaxyCamera.lastMouseY = my;
}




void SystemMapRenderer::renderGalaxy(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav
)
{
    const auto& systems = galaxy.systems;
    if (m_selectedSystemId < 0)
        m_selectedSystemId = nav.currentSystemId;

    const glm::mat4 proj = galaxyProjectionMatrix(vp);
    const glm::mat4 view = galaxyViewMatrix();
    const glm::mat4 mvp = proj * view;

    beginLines();

    const glm::vec4 gridColor { 0.10f, 0.28f, 0.43f, 0.22f };

    for (int i = -20; i <= 20; ++i)
    {
        const float v = static_cast<float>(i) * 5.0f;

        addLine({ -100.0f, 0.0f, v }, { 100.0f, 0.0f, v }, gridColor);
        addLine({ v, 0.0f, -100.0f }, { v, 0.0f, 100.0f }, gridColor);
    }

    // Пустой будущий слой навигации.
    // Здесь позже будут гравитационные складки, трассы, маяки, опасные зоны.
    drawNavigationLayerPlaceholder();

    m_lastGalaxyScreenPoints.clear();

    for (const auto& s : systems)
    {
        const glm::vec3 p = galaxyStarPosition(s);

        const bool isCurrent = s.id == nav.currentSystemId;
        const bool isSelected = s.id == m_selectedSystemId;

        glm::vec4 c =
            isSelected
                ? glm::vec4(0.55f, 0.95f, 1.00f, 1.0f)
                : isCurrent
                    ? glm::vec4(1.0f, 0.86f, 0.45f, 1.0f)
                    : colorForStarType(s.starType);

        const float size =
            isSelected ? 1.25f :
            isCurrent  ? 0.95f :
                         0.48f;

        addCross(p, size, c);

        if (isCurrent)
            addCircleXZ(p, 1.45f, { 1.0f, 0.82f, 0.35f, 0.9f }, 48);

        if (isSelected)
        {
            addCircleXZ(p, 2.10f, { 0.40f, 0.95f, 1.00f, 0.95f }, 64);
            addCircleXZ(p, 2.80f, { 0.40f, 0.95f, 1.00f, 0.35f }, 64);
        }

        ScreenPoint sp;
        sp.systemId = s.id;
        sp.name = s.name;
        sp.world = p;
        sp.screen = projectToScreen(p, mvp, vp, sp.visible, sp.depth);
        m_lastGalaxyScreenPoints.push_back(sp);
    }

    flushLines(mvp);
    drawGalaxyLabels(
        vp,
        galaxy,
        mvp
    );
}


void SystemMapRenderer::drawNavigationLayerPlaceholder()
{
    // Пока намеренно пусто.
    // Это точка расширения под:
    // - fold lanes;
    // - beacon coverage;
    // - gravity distortion fields;
    // - restricted jump zones.
}






void SystemMapRenderer::drawGalaxyLabels(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const glm::mat4& mvp
)
{
    if (m_galaxyCamera.distance > 130.0f)
        return;

   

    struct Label
    {
        std::string title;
        std::string subtitle;
        glm::vec2 screen;
        glm::vec2 lineEnd;
        glm::vec2 textPos;
        bool selected = false;
    };


    std::vector<Label> labels;
    std::vector<glm::vec4> occupied;

    const float screenFactor =
        std::clamp(static_cast<float>(vp.height) / 768.0f, 0.85f, 1.45f);

    const int titlePx =
        std::clamp(static_cast<int>(15.0f * screenFactor), 13, 20);

    const int selectedTitlePx =
        std::clamp(static_cast<int>(18.0f * screenFactor), 15, 25);

    const int subtitlePx =
        std::clamp(static_cast<int>(12.0f * screenFactor), 10, 16);

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            galaxyStarPosition(s),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const bool selected = s.id == m_selectedSystemId;

        // Пока игрок находится у текущей системы. Для galaxy-map этого достаточно.
        // Если позже игрок будет в межзвездном пространстве — добавим точную playerPositionLy.
        double distanceLy = 0.0;

        for (const auto& cur : galaxy.systems)
        {
            // У тебя текущая система сейчас визуально совпадает с Sol.
            // До передачи nav сюда считаем от системы id=0, если она есть.
            if (cur.id == 0)
            {
                const double dx = s.positionLy.x - cur.positionLy.x;
                const double dy = s.positionLy.y - cur.positionLy.y;
                const double dz = s.positionLy.z - cur.positionLy.z;

                distanceLy = std::sqrt(dx * dx + dy * dy + dz * dz);
                break;
            }
        }

        const std::string subtitle =
            (s.starType.empty() ? "Unknown" : s.starType) +
            std::string("  /  ") +
            fmtDistanceLy(distanceLy);

        const int px = selected ? selectedTitlePx : titlePx;

        const float titleW =
            static_cast<float>(s.name.size()) * static_cast<float>(px) * 0.58f;

        const float subtitleW =
            static_cast<float>(subtitle.size()) * static_cast<float>(subtitlePx) * 0.50f;

        const float w = std::max(titleW, subtitleW);
        const float h =
            static_cast<float>(px + subtitlePx) + 8.0f;

        // Смещение метки от звезды.
        // Чем выше разрешение — тем чуть дальше, но без дикости.


    // pos.y — это baseline текста, поэтому учитываем высоту блока.
    const float gap =
        std::clamp(static_cast<float>(vp.height) * 0.018f, 14.0f, 24.0f);

    glm::vec2 lineEnd {
        screen.x + gap,
        screen.y - gap
    };

    glm::vec2 textPos {
        lineEnd.x + 6.0f,
        lineEnd.y - 4.0f
    };

    // if (pos.x + w > vp.x + vp.width - 12.0f)
    //     pos.x = screen.x - w - gap;

    // if (pos.y - h < vp.y + 12.0f)
    //     pos.y = screen.y + h + gap;

    // debugLabelTraceToFile(s.name, screen, textPos);


    

        const glm::vec4 rect {
            textPos.x,
            textPos.y - static_cast<float>(px),
            textPos.x + w,
            textPos.y + static_cast<float>(subtitlePx) + 10.0f
        };

        bool overlap = false;

        for (const auto& r : occupied)
        {
            if (rect.x < r.z && rect.z > r.x &&
                rect.y < r.w && rect.w > r.y)
            {
                overlap = true;
                break;
            }
        }

        if (overlap && !selected)
            continue;

        occupied.push_back(rect);



        labels.push_back({
                s.name,
                subtitle,
                screen,
                lineEnd,
                textPos,
                selected
            });



        

    }

beginLines();

static int debugFrame = 0;
const bool logThisFrame = debugFrame < 20;

std::ofstream dbg;

if (logThisFrame)
{
    dbg.open("system_map_label_debug.txt", std::ios::app);

    dbg
        << "\nFRAME " << debugFrame
        << " vp=(" << vp.x << "," << vp.y
        << "," << vp.width << "," << vp.height << ")"
        << " cameraDistance=" << m_galaxyCamera.distance
        << "\n";
}

for (const auto& l : labels)
{
    const glm::vec4 lineColor =
        l.selected
            ? glm::vec4(0.98f, 0.72f, 0.34f, 0.55f)
            : glm::vec4(0.46f, 0.78f, 1.00f, 0.32f);

    

    addLine(
        glm::vec3(l.screen.x, l.screen.y, 0.0f),
        glm::vec3(l.lineEnd.x, l.lineEnd.y, 0.0f),
        lineColor
    );
}

if (logThisFrame)
    ++debugFrame;

// Label coordinates are local to the system-map viewport:
// x = 0..vp.width
// y = 0..vp.height
//
// Do NOT switch to full-window viewport here.
// TextRenderer also normalizes text using StateContext viewport size,
// so lines and text must stay in the same map-local coordinate space.
const glm::mat4 labelOrtho =
    glm::ortho(
        0.0f,
        static_cast<float>(vp.width),
        static_cast<float>(vp.height),
        0.0f,
        -1.0f,
        1.0f
    );






flushLines(labelOrtho);







auto& text = TextRenderer::instance();

text.beginFrameForViewport(
    vp.width,
    vp.height
);

for (const auto& l : labels)
{
    const int px = l.selected ? selectedTitlePx : titlePx;

    const glm::vec4 titleColor =
        l.selected
            ? glm::vec4(0.98f, 0.72f, 0.34f, 0.92f)
            : glm::vec4(0.46f, 0.78f, 1.00f, 0.68f);

    const glm::vec4 subtitleColor =
        l.selected
            ? glm::vec4(0.70f, 0.86f, 1.00f, 0.68f)
            : glm::vec4(0.38f, 0.64f, 0.90f, 0.46f);







    text.textDrawPx(
        l.title,
        l.textPos.x,
        l.textPos.y,
        px,
        titleColor
    );
























    text.textDrawPx(
        l.subtitle,
        l.textPos.x,
        l.textPos.y + static_cast<float>(px) + 2.0f,
        subtitlePx,
        subtitleColor
    );
}

text.endFrame();
    
}






void SystemMapRenderer::drawSystemLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById
)
{
    using world::celestial::BodyType;

    auto& text = TextRenderer::instance();

    text.beginFrameForViewport(vp.width, vp.height);

    const bool showMoons =
        m_systemCamera.distance < 7.0f;

    for (const auto& b : system.bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::AsteroidBelt &&
            !(showMoons && b.type == BodyType::Moon))
        {
            continue;
        }

        auto posIt = posById.find(b.id);
        if (posIt == posById.end())
            continue;

        auto radiusIt = drawRadiusById.find(b.id);

        const float r =
            radiusIt != drawRadiusById.end()
                ? radiusIt->second
                : 0.1f;

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen =
            projectToScreen(posIt->second, mvp, vp, visible, depth);

        if (!visible)
            continue;

        std::string subtitle;

        for (size_t i = 0; i < b.alternativeNames.size(); ++i)
        {
            const auto& alt = b.alternativeNames[i];

            if (alt.name.empty())
                continue;

            if (!subtitle.empty())
                subtitle += ", ";

            subtitle += alt.name;

            if (!alt.actors.empty())
            {
                subtitle += " (";

                for (size_t a = 0; a < alt.actors.size(); ++a)
                {
                    if (a > 0)
                        subtitle += ", ";

                    subtitle += alt.actors[a];
                }

                subtitle += ")";
            }
        }

        const float x = screen.x + r * 10.0f + 8.0f;
        const float y = screen.y - 6.0f;

        text.textDrawPx(
            b.name,
            x,
            y,
            14,
            glm::vec4(0.62f, 0.84f, 1.0f, 0.88f)
        );

        if (!subtitle.empty())
        {
            text.textDrawPx(
                "(" + subtitle + ")",
                x,
                y + 16.0f,
                10,
                glm::vec4(0.55f, 0.67f, 0.78f, 0.62f)
            );
        }
    }

    text.endFrame();
}




float SystemMapRenderer::smoothingAlpha() const
{
    using Clock = std::chrono::steady_clock;

    static double lastTime = 0.0;

    const auto now = Clock::now().time_since_epoch();
    const double nowSec =
        std::chrono::duration<double>(now).count();

    if (lastTime <= 0.0)
    {
        lastTime = nowSec;
        return 1.0f;
    }

    const double dt = std::clamp(nowSec - lastTime, 0.0, 0.1);
    lastTime = nowSec;

    // Чем больше число, тем быстрее карта догоняет новую позицию.
    constexpr double smoothSpeed = 6.0;

    return static_cast<float>(
        1.0 - std::exp(-smoothSpeed * dt)
    );
}

glm::vec3 SystemMapRenderer::smoothVec3(
    SmoothPoint& point,
    const glm::vec3& target,
    float alpha
)
{
    if (!point.initialized)
    {
        point.visual = target;
        point.initialized = true;
        return target;
    }

    point.visual += (target - point.visual) * alpha;
    return point.visual;
}






void SystemMapRenderer::renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
)
{
    const auto& bodies = system.bodies;

    m_lastSystemBodyScreenPoints.clear();

    if (bodies.empty())
        return;

    double maxAu = 1.0;

    for (const auto& b : bodies)
    {
        const double r =
            std::sqrt(
                b.positionAu.x * b.positionAu.x +
                b.positionAu.y * b.positionAu.y +
                b.positionAu.z * b.positionAu.z
            );

        maxAu = std::max(maxAu, r);

        for (const auto& ring : b.rings)
        {
            const double outerAu =
                ring.outerRadiusKm / AU_KM;

            maxAu = std::max(maxAu, r + outerAu);
        }
    }

    const float systemScale =
        70.0f / static_cast<float>(maxAu);

    const glm::mat4 proj = systemProjectionMatrix(vp);
    const glm::mat4 view = systemViewMatrix();
    const glm::mat4 mvp = proj * view;

    





    using world::celestial::BodyType;

    
    const float alpha = smoothingAlpha();

std::unordered_map<std::string, glm::vec3> rawPosById;
std::unordered_map<std::string, glm::vec3> targetPosById;
std::unordered_map<std::string, glm::vec3> posById;

for (const auto& b : bodies)
{
    rawPosById[b.id] = {
        static_cast<float>(b.positionAu.x) * systemScale,
        static_cast<float>(b.positionAu.y) * systemScale,
        static_cast<float>(b.positionAu.z) * systemScale
    };

    targetPosById[b.id] = rawPosById[b.id];
}


std::unordered_map<std::string, const world::celestial::SystemMapBody*> bodyById;
std::unordered_map<std::string, float> drawRadiusById;

for (const auto& b : bodies)
{
    bodyById[b.id] = &b;
    drawRadiusById[b.id] = bodyVisualRadius(b, systemScale);
}

// Визуально выносим луны наружу.
// Важно: направление считаем по RAW-позициям из snapshot,
// а сглаживаем только уже готовую визуальную позицию.
for (const auto& b : bodies)
{
    if (b.type != BodyType::Moon)
        continue;

    auto parentRawIt = rawPosById.find(b.parentId);
    auto moonRawIt = rawPosById.find(b.id);
    auto parentRadiusIt = drawRadiusById.find(b.parentId);
    auto moonRadiusIt = drawRadiusById.find(b.id);

    if (parentRawIt == rawPosById.end() ||
        moonRawIt == rawPosById.end() ||
        parentRadiusIt == drawRadiusById.end() ||
        moonRadiusIt == drawRadiusById.end())
    {
        continue;
    }

    glm::vec3 dir =
        moonRawIt->second - parentRawIt->second;

    if (glm::length(dir) < 0.000001f)
        dir = glm::vec3(1.0f, 0.0f, 0.0f);
    else
        dir = glm::normalize(dir);

    const float minVisualDistance =
        parentRadiusIt->second * 1.75f +
        moonRadiusIt->second * 1.40f;

    const float realVisualDistance =
        glm::length(moonRawIt->second - parentRawIt->second);

    const float finalDistance =
        std::max(realVisualDistance, minVisualDistance);

    targetPosById[b.id] =
        parentRawIt->second + dir * finalDistance;
}

for (const auto& b : bodies)
{
    posById[b.id] =
        smoothVec3(
            m_smoothBodyPositions[b.id],
            targetPosById[b.id],
            alpha
        );
}















    beginLines();
    beginSolids();

    const glm::vec4 gridColor { 0.10f, 0.28f, 0.43f, 0.18f };

    for (int i = -20; i <= 20; ++i)
    {
        const float v = static_cast<float>(i) * 5.0f;

        addLine({ -100.0f, 0.0f, v }, { 100.0f, 0.0f, v }, gridColor);
        addLine({ v, 0.0f, -100.0f }, { v, 0.0f, 100.0f }, gridColor);
    }

    

    // Орбиты планет, лун и астероидных поясов.
    for (const auto& b : bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::Moon)
        {
            continue;
        }

        
        if (!b.drawOrbit || b.orbitRadiusAu <= 0.0)
            continue;

       glm::vec3 center {0.0f};

        auto parentIt =
            bodyById.find(b.parentId);

        if (parentIt != bodyById.end())
        {
            const auto* parent =
                parentIt->second;

            auto posIt =
                posById.find(parent->id);

            if (posIt != posById.end())
                center = posIt->second;
        }

        float orbitR =
            static_cast<float>(b.orbitRadiusAu) * systemScale;

        if (b.type == BodyType::Moon)
        {
            auto parentRadiusIt = drawRadiusById.find(b.parentId);
            auto moonRadiusIt = drawRadiusById.find(b.id);

            if (parentRadiusIt != drawRadiusById.end() &&
                moonRadiusIt != drawRadiusById.end())
            {
                const float minVisualOrbit =
                    parentRadiusIt->second * 1.75f + moonRadiusIt->second * 1.40f;

                orbitR = std::max(orbitR, minVisualOrbit);
            }
        }




        const glm::vec4 orbitColor =
            b.type == BodyType::Moon
                ? glm::vec4(0.72f, 0.78f, 0.86f, 0.24f)
                : b.type == BodyType::AsteroidBelt
                    ? glm::vec4(0.62f, 0.66f, 0.72f, 0.30f)
                    : glm::vec4(0.48f, 0.76f, 1.00f, 0.34f);

        addCircleXZ(
            center,
            orbitR,
            orbitColor,
            b.type == BodyType::Moon ? 64 : 160
        );
    }

    // Тела, кольца, пояса.
    for (const auto& b : bodies)
    {
        const glm::vec3 p = posById[b.id];


        {
            BodyScreenPoint bp;
            bp.bodyId = b.id;
            bp.name = b.name;
            bp.screen = projectToScreen(
                p,
                mvp,
                vp,
                bp.visible,
                bp.depth
            );

            m_lastSystemBodyScreenPoints.push_back(bp);
        }


        const glm::vec4 c = colorForBodyType(b.type);
        const float r = drawRadiusById[b.id];


        if (b.type == BodyType::AsteroidBelt)
        {
            glm::vec3 center {0.0f};

            auto parentIt = posById.find(b.parentId);
            if (parentIt != posById.end())
                center = parentIt->second;

            const float beltR = static_cast<float>(b.orbitRadiusAu) * systemScale;

            addCircleXZ(center, beltR - 0.12f, {0.65f, 0.68f, 0.72f, 0.12f}, 160);
            addCircleXZ(center, beltR,         {0.65f, 0.68f, 0.72f, 0.24f}, 160);
            addCircleXZ(center, beltR + 0.12f, {0.65f, 0.68f, 0.72f, 0.12f}, 160);

            continue;
        }

        addBillboardBall(p, r, c, view, 32);

        if (b.id == m_selectedBodyId)
        {
            addCircleXZ(
                p,
                r * 1.8f,
                glm::vec4(1.0f, 0.82f, 0.25f, 0.95f),
                64
            );
        }


        for (const auto& ring : b.rings)
        {
            const float inner =
                std::max(
                    static_cast<float>((ring.innerRadiusKm / AU_KM) * systemScale * 8.0),
                    r * 1.35f
                );

            const float outer =
                std::max(
                    static_cast<float>((ring.outerRadiusKm / AU_KM) * systemScale * 8.0),
                    inner + r * 0.45f
                );

            addCircleXZ(p, inner, {0.68f, 0.74f, 0.82f, 0.26f}, 128);
            addCircleXZ(p, outer, {0.68f, 0.74f, 0.82f, 0.38f}, 128);

            const float mid = (inner + outer) * 0.5f;
            addCircleXZ(p, mid, {0.68f, 0.74f, 0.82f, 0.18f}, 128);
        }
    }

    if (system.systemId == nav.currentSystemId)
    {
        glm::vec3 player {
            static_cast<float>(nav.systemLocalAu.x) * systemScale,
            static_cast<float>(nav.systemLocalAu.y) * systemScale,
            static_cast<float>(nav.systemLocalAu.z) * systemScale
        };

        addCross(player, 0.85f, {1.0f, 0.82f, 0.35f, 1.0f});
        addCircleXZ(player, 1.25f, {1.0f, 0.82f, 0.35f, 0.55f}, 48);
    }


std::unordered_map<uint32_t, glm::vec3> objectVisualPosById;

for (const auto& obj : system.objects)
{
    const glm::vec3 target =
        systemObjectVisualPosition(
            system,
            obj,
            posById,
            drawRadiusById,
            systemScale
        );

    const glm::vec3 p =
        smoothVec3(
            m_smoothObjectPositions[obj.id.value],
            target,
            alpha
        );

    objectVisualPosById[obj.id.value] = p;

    addMapObjectCube(
        p,
        0.018f,
        glm::vec4(1.0f, 0.78f, 0.30f, 0.95f)
    );
}

    

    flushLines(mvp);

    drawSystemLabels(
        vp,
        system,
        mvp,
        posById,
        drawRadiusById
    );

    drawSystemObjectLabels(
        vp,
        system,
        mvp,
        objectVisualPosById
    );

    flushSolids(mvp);
}   


void SystemMapRenderer::drawPanelText(
    const Viewport& vp,
    const std::string& title,
    const std::vector<std::string>& lines
)
{
    auto& text = TextRenderer::instance();

    text.beginFrame();

    const float panelX = static_cast<float>(vp.width) - 386.0f;
    float y = 48.0f;

    // Title: same family as loading screen — orange, thin, technical.
    text.textDraw(
        title,
        panelX,
        y,
        0.34f,
        glm::vec3(1.0f, 0.68f, 0.38f)
    );

    y += 42.0f;

    for (const auto& line : lines)
    {
        if (line.empty())
        {
            y += 12.0f;
            continue;
        }

        const bool isHint =
            line.find("F11") != std::string::npos ||
            line.find("Next") != std::string::npos;

        const glm::vec3 color =
            isHint
                ? glm::vec3(0.38f, 0.58f, 0.78f)
                : glm::vec3(0.78f, 0.86f, 0.96f);

        text.textDraw(
            line,
            panelX,
            y,
            0.24f,
            color
        );

        y += 24.0f;
    }

    text.endFrame();
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

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bgShader);
    glBindVertexArray(m_bgVao);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}


void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}


glm::vec3 SystemMapRenderer::nearestVisibleStarToViewportCenter(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    bool& found
) const
{
    found = false;

    const glm::mat4 mvp =
        galaxyProjectionMatrix(vp) * galaxyViewMatrix();

    const glm::vec2 center {
        static_cast<float>(vp.x) + static_cast<float>(vp.width) * 0.5f,
        static_cast<float>(vp.y) + static_cast<float>(vp.height) * 0.5f
    };

    float bestDist2 = 999999999.0f;
    glm::vec3 bestWorld {0.0f};

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec3 world = galaxyStarPosition(s);

        const glm::vec2 screen = projectToScreen(
            world,
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const glm::vec2 d = screen - center;
        const float dist2 = glm::dot(d, d);

        if (dist2 < bestDist2)
        {
            bestDist2 = dist2;
            bestWorld = world;
            found = true;
        }
    }

    return bestWorld;
}

void SystemMapRenderer::debugLabelTraceToFile(
    const std::string& name,
    const glm::vec2& screen,
    const glm::vec2& pos
) const
{
    if (name.find("Tau") == std::string::npos &&
        name.find("Ceti") == std::string::npos)
    {
        return;
    }

    std::ofstream out(
        "galaxy_label_trace.txt",
        std::ios::app
    );

    out
        << name
        << " screen=(" << screen.x << "," << screen.y << ")"
        << " pos=(" << pos.x << "," << pos.y << ")"
        << " delta=(" << (pos.x - screen.x) << "," << (pos.y - screen.y) << ")"
        << " cameraDistance=" << m_galaxyCamera.distance
        << " target=("
        << m_galaxyCamera.target.x << ","
        << m_galaxyCamera.target.y << ","
        << m_galaxyCamera.target.z << ")"
        << "\n";
}





glm::vec3 SystemMapRenderer::systemObjectVisualPosition(
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::SystemMapObject& obj,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById,
    float systemScale
) const
{
    // Если объект не привязан к телу, рисуем по абсолютной AU-позиции.
    if (obj.parentBodyId.empty())
    {
        return glm::vec3 {
            static_cast<float>(obj.positionAu.x) * systemScale,
            static_cast<float>(obj.positionAu.y) * systemScale,
            static_cast<float>(obj.positionAu.z) * systemScale
        };
    }

    auto parentDrawIt =
        posById.find(obj.parentBodyId);

    if (parentDrawIt == posById.end())
    {
        return glm::vec3 {
            static_cast<float>(obj.positionAu.x) * systemScale,
            static_cast<float>(obj.positionAu.y) * systemScale,
            static_cast<float>(obj.positionAu.z) * systemScale
        };
    }

    glm::dvec3 parentPhysicalAu {0.0};

    bool foundParentPhysical = false;

    for (const auto& body : system.bodies)
    {
        if (body.id == obj.parentBodyId)
        {
            parentPhysicalAu = body.positionAu;
            foundParentPhysical = true;
            break;
        }
    }

    glm::vec3 dir {1.0f, 0.0f, 0.0f};

    if (foundParentPhysical)
    {
        const glm::dvec3 deltaAu =
            obj.positionAu - parentPhysicalAu;

        if (glm::length(deltaAu) > 0.0)
        {
            dir = glm::normalize(glm::vec3(deltaAu));
        }
    }

    auto radiusIt =
        drawRadiusById.find(obj.parentBodyId);

    const float parentDrawRadius =
        radiusIt != drawRadiusById.end()
            ? radiusIt->second
            : 0.18f;

    // Визуальная дистанция от центра планеты.
    // Это НЕ физика, только карта.
    const float objectOrbitVisualRadius =
        parentDrawRadius + 0.20f;

    return parentDrawIt->second + dir * objectOrbitVisualRadius;
}








void SystemMapRenderer::addMapObjectCube(
    const glm::vec3& center,
    float size,
    const glm::vec4& color
)
{
    const glm::vec3 p000 = center + glm::vec3(-size, -size, -size);
    const glm::vec3 p001 = center + glm::vec3(-size, -size,  size);
    const glm::vec3 p010 = center + glm::vec3(-size,  size, -size);
    const glm::vec3 p011 = center + glm::vec3(-size,  size,  size);

    const glm::vec3 p100 = center + glm::vec3( size, -size, -size);
    const glm::vec3 p101 = center + glm::vec3( size, -size,  size);
    const glm::vec3 p110 = center + glm::vec3( size,  size, -size);
    const glm::vec3 p111 = center + glm::vec3( size,  size,  size);

    addLine(p000, p001, color);
    addLine(p001, p011, color);
    addLine(p011, p010, color);
    addLine(p010, p000, color);

    addLine(p100, p101, color);
    addLine(p101, p111, color);
    addLine(p111, p110, color);
    addLine(p110, p100, color);

    addLine(p000, p100, color);
    addLine(p001, p101, color);
    addLine(p010, p110, color);
    addLine(p011, p111, color);
}


void SystemMapRenderer::drawSystemObjectLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById
)
{
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(vp.width, vp.height);

    for (const auto& obj : system.objects)
    {
        auto posIt = objectVisualPosById.find(obj.id.value);

        if (posIt == objectVisualPosById.end())
            continue;

        const glm::vec3 p = posIt->second;

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen =
            projectToScreen(p, mvp, vp, visible, depth);

        if (!visible)
            continue;

        text.textDrawPx(
            obj.name,
            screen.x + 8.0f,
            screen.y - 7.0f,
            13,
            glm::vec4(1.0f, 0.86f, 0.42f, 0.88f)
        );

        if (!obj.owner.empty())
        {
            text.textDrawPx(
                "(" + obj.owner + ")",
                screen.x + 8.0f,
                screen.y + 9.0f,
                10,
                glm::vec4(0.75f, 0.72f, 0.58f, 0.62f)
            );
        }
    }

    text.endFrame();
} 




glm::dvec2 SystemMapRenderer::planetMapProject(
    const glm::dvec3& worldMeters,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
) const
{
    glm::dvec3 p =
        worldMeters - planet.planetCenterMeters;

    const double cy = std::cos(m_planetCamera.yaw);
    const double sy = std::sin(m_planetCamera.yaw);
    const double cp = std::cos(m_planetCamera.pitch);
    const double sp = std::sin(m_planetCamera.pitch);

    // yaw вокруг Y
    glm::dvec3 a;
    a.x = p.x * cy - p.z * sy;
    a.y = p.y;
    a.z = p.x * sy + p.z * cy;

    // pitch вокруг X
    glm::dvec3 b;
    b.x = a.x;
    b.y = a.y * cp - a.z * sp;
    b.z = a.y * sp + a.z * cp;

    const double finalScale =
        scale * m_planetCamera.zoom;

    return glm::dvec2(
        centerPx.x + m_planetCamera.pan.x + b.x * finalScale,
        centerPx.y + m_planetCamera.pan.y - b.y * finalScale
    );
}





void SystemMapRenderer::drawPlanetMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCross(
    const glm::dvec2& p,
    float size
)
{
    glBegin(GL_LINES);
    glVertex2d(p.x - size, p.y);
    glVertex2d(p.x + size, p.y);
    glVertex2d(p.x, p.y - size);
    glVertex2d(p.x, p.y + size);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    if (segments < 8)
        segments = 8;

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        glVertex2d(
            center.x + std::cos(a) * radiusPx,
            center.y + std::sin(a) * radiusPx
        );
    }

    glEnd();
}


void SystemMapRenderer::drawPlanetMapAxes(
    const glm::dvec3& originMeters,
    const world::celestial::PlanetMapAxisSet& axes,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double axisLenMeters
)
{
    const glm::dvec2 o =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 x =
        planetMapProject(
            originMeters + axes.x * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 y =
        planetMapProject(
            originMeters + axes.y * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 z =
        planetMapProject(
            originMeters + axes.z * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.25f, 0.25f, 0.9f);
    drawPlanetMapLine(o, x);

    glColor4f(0.25f, 1.0f, 0.25f, 0.9f);
    drawPlanetMapLine(o, y);

    glColor4f(0.25f, 0.55f, 1.0f, 0.9f);
    drawPlanetMapLine(o, z);
}


void SystemMapRenderer::drawPlanetMapVelocityArrow(
    const glm::dvec3& originMeters,
    const glm::dvec3& velocityMps,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double lenMeters
)
{
    const double speed =
        glm::length(velocityMps);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocityMps / speed;

    const glm::dvec2 a =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 b =
        planetMapProject(
            originMeters + dir * lenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.92f, 0.25f, 0.95f);
    drawPlanetMapLine(a, b);

    drawPlanetMapCross(
        b,
        4.0f
    );
}


void SystemMapRenderer::renderPlanetMap(
    const Viewport& viewport,
    const world::celestial::PlanetMapSnapshot& planet
)
{
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

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(0.02f, 0.025f, 0.035f, 1.0f);

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!planet.valid)
        return;

    const glm::dvec2 centerPx(
        static_cast<double>(viewport.width) * 0.5,
        static_cast<double>(viewport.height) * 0.5
    );

    double maxRadiusMeters =
        planet.planetRadiusMeters;

    for (const auto& o : planet.hubOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    for (const auto& o : planet.playerOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    const double mapHalfPx =
        std::min(
            viewport.width,
            viewport.height
        ) * 0.42;

    const double scale =
        mapHalfPx / std::max(1.0, maxRadiusMeters);

    // Сначала тело планеты, потом сетка поверх.
    drawPlanetFilledDisk(
        planet,
        scale,
        centerPx
    );

    drawPlanetSphereGrid(
        planet,
        scale,
        centerPx
    );

    // Орбиты хабов.
    glColor4f(0.45f, 0.78f, 1.0f, 0.75f);

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    // Орбита игрока.
    glColor4f(1.0f, 0.75f, 0.25f, 0.9f);

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    const double axisLenMeters =
        std::max(
            50000.0,
            maxRadiusMeters * 0.035
        );

    const double velocityArrowLenMeters =
        std::max(
            70000.0,
            maxRadiusMeters * 0.05
        );

    // Хабы.
    for (const auto& hub : planet.hubs)
    {
        if (!hub.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                hub.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.3f, 0.9f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 7.0f);

        drawPlanetMapAxes(
            hub.positionMeters,
            hub.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            hub.positionMeters,
            hub.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    // Станции.
    for (const auto& station : planet.stations)
    {
        if (!station.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                station.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.8f, 0.95f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 6.0f);

        drawPlanetMapAxes(
            station.positionMeters,
            station.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            station.positionMeters,
            station.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters * 0.8
        );
    }

    // Корабли.
    for (const auto& ship : planet.ships)
    {
        if (!ship.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                ship.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(1.0f, 0.85f, 0.25f, 1.0f);
        drawPlanetMapCross(p, 8.0f);

        drawPlanetMapAxes(
            ship.positionMeters,
            ship.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            ship.positionMeters,
            ship.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    glEnable(GL_DEPTH_TEST);
}



const std::string& SystemMapRenderer::selectedBodyId() const
{
    return m_selectedBodyId;
}



int SystemMapRenderer::pickSystemBody(
    double mouseX,
    double mouseY
) const
{
    int bestIndex = -1;
    float bestDist = 999999.0f;

    for (int i = 0; i < static_cast<int>(m_lastSystemBodyScreenPoints.size()); ++i)
    {
        const auto& p = m_lastSystemBodyScreenPoints[i];

        if (!p.visible)
            continue;

        const float dx = p.screen.x - static_cast<float>(mouseX);
        const float dy = p.screen.y - static_cast<float>(mouseY);
        const float d = std::sqrt(dx * dx + dy * dy);

        if (d < 16.0f && d < bestDist)
        {
            bestDist = d;
            bestIndex = i;
        }
    }

    return bestIndex;
}



void SystemMapRenderer::drawPlanetSphereGrid(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters;

    if (r <= 1.0)
        return;

    glColor4f(0.18f, 0.42f, 0.85f, 0.75f);

    // Широты
    for (int latDeg = -60; latDeg <= 60; latDeg += 30)
    {
        const double lat =
            glm::radians(static_cast<double>(latDeg));

        const double y =
            std::sin(lat) * r;

        const double rr =
            std::cos(lat) * r;

        glBegin(GL_LINE_LOOP);

        for (int i = 0; i < 144; ++i)
        {
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) / 144.0;

            const glm::dvec3 p =
                planet.planetCenterMeters +
                glm::dvec3(
                    std::cos(a) * rr,
                    y,
                    std::sin(a) * rr
                );

            const glm::dvec2 s =
                planetMapProject(
                    p,
                    planet,
                    scale,
                    centerPx
                );

            glVertex2d(s.x, s.y);
        }

        glEnd();
    }

    // Долготы
    for (int lonDeg = 0; lonDeg < 180; lonDeg += 30)
    {
        const double lon =
            glm::radians(static_cast<double>(lonDeg));

        glBegin(GL_LINE_LOOP);

        for (int i = 0; i < 144; ++i)
        {
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) / 144.0;

            const glm::dvec3 p =
                planet.planetCenterMeters +
                glm::dvec3(
                    std::cos(lon) * std::cos(a) * r,
                    std::sin(a) * r,
                    std::sin(lon) * std::cos(a) * r
                );

            const glm::dvec2 s =
                planetMapProject(
                    p,
                    planet,
                    scale,
                    centerPx
                );

            glVertex2d(s.x, s.y);
        }

        glEnd();
    }

    // Ось вращения условно Y.
    const glm::dvec2 north =
        planetMapProject(
            planet.planetCenterMeters +
            glm::dvec3(0.0, r * 1.25, 0.0),
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 south =
        planetMapProject(
            planet.planetCenterMeters -
            glm::dvec3(0.0, r * 1.25, 0.0),
            planet,
            scale,
            centerPx
        );

    glColor4f(0.55f, 0.8f, 1.0f, 0.95f);
    drawPlanetMapLine(south, north);

    drawPlanetMapCross(north, 5.0f);
    drawPlanetMapCross(south, 5.0f);
}


void SystemMapRenderer::drawPlanetFilledDisk(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters *
        scale *
        m_planetCamera.zoom;

    const glm::dvec2 c {
        centerPx.x + m_planetCamera.pan.x,
        centerPx.y + m_planetCamera.pan.y
    };

    glColor4f(0.035f, 0.09f, 0.18f, 0.92f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2d(c.x, c.y);

    for (int i = 0; i <= 192; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            192.0;

        glVertex2d(
            c.x + std::cos(a) * r,
            c.y + std::sin(a) * r
        );
    }

    glEnd();
}


void SystemMapRenderer::drawPlanetMapOrbit3D(
    const world::celestial::PlanetMapOrbit& orbit,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (!orbit.valid || orbit.radiusMeters <= 1.0)
        return;

    segments =
        std::max(segments, 32);

    glm::dvec3 radial =
        orbit.radialAxis;

    glm::dvec3 prograde =
        orbit.progradeAxis;

    glm::dvec3 normal =
        orbit.normalAxis;

    if (glm::length(radial) < 0.001)
        radial = glm::dvec3(1.0, 0.0, 0.0);

    if (glm::length(prograde) < 0.001)
        prograde = glm::dvec3(0.0, 0.0, 1.0);

    radial =
        glm::normalize(radial);

    prograde =
        glm::normalize(
            prograde -
            radial * glm::dot(prograde, radial)
        );

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const glm::dvec3 p =
            orbit.centerMeters +
            radial * std::cos(a) * orbit.radiusMeters +
            prograde * std::sin(a) * orbit.radiusMeters;

        const glm::dvec2 s =
            planetMapProject(
                p,
                planet,
                scale,
                centerPx
            );

        glVertex2d(s.x, s.y);
    }

    glEnd();
}














glm::dvec2 SystemMapRenderer::hubMapProject(
    const glm::dvec3& localMeters,
    double scale,
    const glm::dvec2& centerPx
) const
{
    glm::dvec3 p =
        localMeters;

    const double cy = std::cos(m_planetCamera.yaw);
    const double sy = std::sin(m_planetCamera.yaw);
    const double cp = std::cos(m_planetCamera.pitch);
    const double sp = std::sin(m_planetCamera.pitch);

    glm::dvec3 a;
    a.x = p.x * cy - p.z * sy;
    a.y = p.y;
    a.z = p.x * sy + p.z * cy;

    glm::dvec3 b;
    b.x = a.x;
    b.y = a.y * cp - a.z * sp;
    b.z = a.y * sp + a.z * cp;

    const double finalScale =
        scale * m_planetCamera.zoom;

    return glm::dvec2(
        centerPx.x + m_planetCamera.pan.x + b.x * finalScale,
        centerPx.y + m_planetCamera.pan.y - b.y * finalScale
    );
}





    bool SystemMapRenderer::pickHubMapOrbitPivot(
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const
    {
        if (m_lastHubMapPickables.empty())
            return false;

        const HubMapPickable* best = nullptr;

        double bestScore =
            std::numeric_limits<double>::max();

        for (const auto& p : m_lastHubMapPickables)
        {
            const double dx =
                mousePx.x - p.screenCenterPx.x;

            const double dy =
                mousePx.y - p.screenCenterPx.y;

            const double dist =
                std::sqrt(dx * dx + dy * dy);

            // Не даём огромной станции захватывать весь экран.
            // Но и не требуем попадания в пиксель.
            const double pickRadius =
                std::clamp(
                    p.screenRadiusPx,
                    18.0,
                    140.0
                );

            // Разрешаем брать ближайший объект чуть за пределами радиуса,
            // иначе при wireframe-модели будет раздражающе трудно попасть.
            const double softLimit =
                pickRadius + 80.0;

            if (dist > softLimit)
                continue;

            // priority слегка улучшает счёт.
            // Игрок/корабли выигрывают у станции при близком расстоянии.
            const double priorityBonus =
                static_cast<double>(p.priority) * 18.0;

            const double score =
                dist - priorityBonus;

            if (score < bestScore)
            {
                bestScore = score;
                best = &p;
            }
        }

        if (!best)
            return false;

        outPivotLocalMeters =
            best->localCenterMeters;

        return true;
    }









glm::dvec3 SystemMapRenderer::hubMapUnprojectCursorToLocal(
    const glm::dvec2& mousePx,
    double scale,
    const glm::dvec2& centerPx
) const
{
    const double finalScale =
        scale * m_planetCamera.zoom;

    if (std::abs(finalScale) < 0.000001)
        return glm::dvec3(0.0);

    // hubMapProject:
    //
    // screen.x = center.x + pan.x + b.x * finalScale
    // screen.y = center.y + pan.y - b.y * finalScale
    //
    // Reverse that first.
    const double bx =
        (mousePx.x - centerPx.x - m_planetCamera.pan.x) /
        finalScale;

    const double by =
        -(mousePx.y - centerPx.y - m_planetCamera.pan.y) /
        finalScale;

    // No real depth information under cursor yet.
    // We choose the current view plane depth = 0.
    const glm::dvec3 b(
        bx,
        by,
        0.0
    );

    const double cy = std::cos(m_planetCamera.yaw);
    const double sy = std::sin(m_planetCamera.yaw);
    const double cp = std::cos(m_planetCamera.pitch);
    const double sp = std::sin(m_planetCamera.pitch);

    // Inverse pitch.
    glm::dvec3 a;
    a.x = b.x;
    a.y = b.y * cp + b.z * sp;
    a.z = -b.y * sp + b.z * cp;

    // Inverse yaw.
    glm::dvec3 p;
    p.x = a.x * cy + a.z * sy;
    p.y = a.y;
    p.z = -a.x * sy + a.z * cy;

    return p;
}















void SystemMapRenderer::drawHubMapBox(
    const glm::dvec3& center,
    const world::celestial::PlanetMapAxisSet& axes,
    const glm::dvec3& size,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec3 hx = axes.x * (size.x * 0.5);
    const glm::dvec3 hy = axes.y * (size.y * 0.5);
    const glm::dvec3 hz = axes.z * (size.z * 0.5);

    glm::dvec3 p[8];

    p[0] = center - hx - hy - hz;
    p[1] = center + hx - hy - hz;
    p[2] = center + hx + hy - hz;
    p[3] = center - hx + hy - hz;

    p[4] = center - hx - hy + hz;
    p[5] = center + hx - hy + hz;
    p[6] = center + hx + hy + hz;
    p[7] = center - hx + hy + hz;

    auto line =
        [&](int a, int b)
        {
            const glm::dvec2 sa =
                hubMapProject(p[a], scale, centerPx);

            const glm::dvec2 sb =
                hubMapProject(p[b], scale, centerPx);

            drawPlanetMapLine(sa, sb);
        };

    line(0, 1);
    line(1, 2);
    line(2, 3);
    line(3, 0);

    line(4, 5);
    line(5, 6);
    line(6, 7);
    line(7, 4);

    line(0, 4);
    line(1, 5);
    line(2, 6);
    line(3, 7);
}



void SystemMapRenderer::drawHubMapAxes(
    const glm::dvec3& center,
    const world::celestial::PlanetMapAxisSet& axes,
    double axisLenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec2 o =
        hubMapProject(center, scale, centerPx);

    glColor4f(1.0f, 0.2f, 0.2f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.x * axisLenMeters, scale, centerPx)
    );

    glColor4f(0.2f, 1.0f, 0.2f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.y * axisLenMeters, scale, centerPx)
    );

    glColor4f(0.25f, 0.55f, 1.0f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.z * axisLenMeters, scale, centerPx)
    );
}



void SystemMapRenderer::drawHubMapVelocityArrow(
    const glm::dvec3& center,
    const glm::dvec3& velocity,
    double lenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double speed =
        glm::length(velocity);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocity / speed;

    glColor4f(1.0f, 0.9f, 0.25f, 0.95f);

    drawPlanetMapLine(
        hubMapProject(center, scale, centerPx),
        hubMapProject(center + dir * lenMeters, scale, centerPx)
    );
}







glm::dvec3 SystemMapRenderer::hubMapObjectLocalToHubLocal(
    const glm::dvec3& objectCenter,
    const world::celestial::PlanetMapAxisSet& objectAxes,
    const glm::dvec3& localPoint
) const
{
    return
        objectCenter +
        objectAxes.x * localPoint.x +
        objectAxes.y * localPoint.y +
        objectAxes.z * localPoint.z;
}



bool SystemMapRenderer::drawHubMapAssemblyWire(
    ObjectType typeId,
    const glm::dvec3& objectCenter,
    const world::celestial::PlanetMapAxisSet& objectAxes,
    double scale,
    const glm::dvec2& centerPx
)
{
    using game::ship::geometry::AssemblyMeshLibrary;

    if (typeId == ObjectType::None)
        return false;

    if (!AssemblyMeshLibrary::has(typeId))
        return false;

    const auto& assembly =
        AssemblyMeshLibrary::get(typeId);

    bool drewAnything = false;

    // Для маленьких кораблей лучше цельный proxy, если он есть.
    // Это быстрее и чище на карте.
    if (assembly.hasWholeShipProxy)
    {
        const auto& mesh =
            assembly.wholeShipProxyMesh;

        for (const auto& edge : mesh.edges)
        {
            const glm::dvec3 a =
                hubMapObjectLocalToHubLocal(
                    objectCenter,
                    objectAxes,
                    glm::dvec3(edge.a)
                );

            const glm::dvec3 b =
                hubMapObjectLocalToHubLocal(
                    objectCenter,
                    objectAxes,
                    glm::dvec3(edge.b)
                );

            drawPlanetMapLine(
                hubMapProject(a, scale, centerPx),
                hubMapProject(b, scale, centerPx)
            );

            drewAnything = true;
        }

        if (drewAnything)
            return true;
    }

    // Для станции рисуем модульную LOD1-сборку.
    // Это как раз "цветок со стеблем", а не кирпич.
    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            const auto& mesh =
                part.lod1Mesh.edges.empty()
                    ? part.lod0Mesh
                    : part.lod1Mesh;

            for (const auto& edge : mesh.edges)
            {
                const glm::dvec3 localA =
                    glm::dvec3(module.localPosition) +
                    glm::dvec3(part.localOffset) +
                    glm::dvec3(edge.a);

                const glm::dvec3 localB =
                    glm::dvec3(module.localPosition) +
                    glm::dvec3(part.localOffset) +
                    glm::dvec3(edge.b);

                const glm::dvec3 a =
                    hubMapObjectLocalToHubLocal(
                        objectCenter,
                        objectAxes,
                        localA
                    );

                const glm::dvec3 b =
                    hubMapObjectLocalToHubLocal(
                        objectCenter,
                        objectAxes,
                        localB
                    );

                drawPlanetMapLine(
                    hubMapProject(a, scale, centerPx),
                    hubMapProject(b, scale, centerPx)
                );

                drewAnything = true;
            }
        }
    }

    return drewAnything;
}















void SystemMapRenderer::drawHubMapCircleLocalXY(
    const glm::dvec3& center,
    double radiusMeters,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (radiusMeters <= 0.0)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const glm::dvec3 p =
            center +
            glm::dvec3(
                std::cos(a) * radiusMeters,
                std::sin(a) * radiusMeters,
                0.0
            );

        const glm::dvec2 s =
            hubMapProject(
                p,
                scale,
                centerPx
            );

        glVertex2d(
            s.x,
            s.y
        );
    }

    glEnd();
}



void SystemMapRenderer::drawHubMapPlanetSurfaceHint(
    const world::celestial::HubMapSnapshot& hub,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (hub.parentPlanetRadiusMeters <= 0.0 ||
        hub.hubOrbitRadiusMeters <= 0.0)
    {
        return;
    }

    const glm::dvec3 planetCenter =
        hub.parentPlanetCenterLocalMeters;

    const glm::dvec2 planetCenterPx =
        hubMapProject(
            planetCenter,
            scale,
            centerPx
        );

    const double planetRadiusPx =
        hub.parentPlanetRadiusMeters *
        scale *
        m_planetCamera.zoom;

    // Если планета слишком огромная и центр далеко за экраном,
    // всё равно рисуем только геометрически корректную дугу/край.
    // Это дает глазу "низ", не превращая карту в синий блин.
    glColor4f(
        0.08f,
        0.22f,
        0.32f,
        0.22f
    );

    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(
        planetCenterPx.x,
        planetCenterPx.y
    );

    constexpr int fillSegments = 160;

    for (int i = 0; i <= fillSegments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(fillSegments);

        glVertex2d(
            planetCenterPx.x + std::cos(a) * planetRadiusPx,
            planetCenterPx.y + std::sin(a) * planetRadiusPx
        );
    }

    glEnd();

    // Контур поверхности.
    glColor4f(
        0.22f,
        0.58f,
        0.78f,
        0.75f
    );

    drawPlanetMapCircle(
        planetCenterPx,
        planetRadiusPx,
        192
    );

    // Орбита хаба вокруг планеты.
    glColor4f(
        0.95f,
        0.82f,
        0.32f,
        0.42f
    );

    drawHubMapCircleLocalXY(
        planetCenter,
        hub.hubOrbitRadiusMeters,
        scale,
        centerPx,
        256
    );

    // Радиальная линия: центр планеты -> поверхность -> хаб.
    const glm::dvec3 surfacePoint =
        planetCenter +
        glm::dvec3(
            0.0,
            hub.parentPlanetRadiusMeters,
            0.0
        );

    glColor4f(
        0.35f,
        0.85f,
        1.0f,
        0.45f
    );

    drawPlanetMapLine(
        hubMapProject(surfacePoint, scale, centerPx),
        hubMapProject(glm::dvec3(0.0), scale, centerPx)
    );

    // Маленькая отметка точки под хабом на поверхности.
    glColor4f(
        0.65f,
        0.95f,
        1.0f,
        0.85f
    );

    drawPlanetMapCross(
        hubMapProject(surfacePoint, scale, centerPx),
        5.0f
    );
}



void SystemMapRenderer::drawHubMapAdaptiveGrid(
    const Viewport& viewport,
    double scale,
    const glm::dvec2& centerPx,
    double worldRadiusMeters
)
{
    if (scale <= 0.0)
        return;

    const double visibleMeters =
        std::max(
            1000.0,
            worldRadiusMeters
        );

    double gridStep =
        100.0;

    while ((gridStep * scale * m_planetCamera.zoom) < 28.0)
        gridStep *= 2.0;

    while ((gridStep * scale * m_planetCamera.zoom) > 90.0 &&
           gridStep > 25.0)
    {
        gridStep *= 0.5;
    }

    const int gridN =
        static_cast<int>(
            std::ceil(
                visibleMeters / gridStep
            )
        ) + 2;

    glColor4f(
        0.12f,
        0.28f,
        0.38f,
        0.30f
    );

    for (int i = -gridN; i <= gridN; ++i)
    {
        const double v =
            static_cast<double>(i) *
            gridStep;

        drawPlanetMapLine(
            hubMapProject(glm::dvec3(-gridN * gridStep, 0.0, v), scale, centerPx),
            hubMapProject(glm::dvec3( gridN * gridStep, 0.0, v), scale, centerPx)
        );

        drawPlanetMapLine(
            hubMapProject(glm::dvec3(v, 0.0, -gridN * gridStep), scale, centerPx),
            hubMapProject(glm::dvec3(v, 0.0,  gridN * gridStep), scale, centerPx)
        );
    }

    // Главные оси плоскости хаба.
    glColor4f(
        0.20f,
        0.55f,
        0.75f,
        0.45f
    );

    drawPlanetMapLine(
        hubMapProject(glm::dvec3(-gridN * gridStep, 0.0, 0.0), scale, centerPx),
        hubMapProject(glm::dvec3( gridN * gridStep, 0.0, 0.0), scale, centerPx)
    );

    drawPlanetMapLine(
        hubMapProject(glm::dvec3(0.0, 0.0, -gridN * gridStep), scale, centerPx),
        hubMapProject(glm::dvec3(0.0, 0.0,  gridN * gridStep), scale, centerPx)
    );
}



glm::dvec3 SystemMapRenderer::visualSizeForHubShip(
    const world::celestial::HubMapShip& ship,
    double scale
) const
{
    // Реальный корабль можно держать маленьким, но на карте он не должен
    // превращаться в субатомную пыль. Поэтому размер физический,
    // но с минимальным экранным размером.
    glm::dvec3 physicalSizeMeters(
        90.0,
        35.0,
        160.0
    );

    if (ship.player)
    {
        physicalSizeMeters =
            glm::dvec3(
                130.0,
                50.0,
                210.0
            );
    }

    const double pixelsPerMeter =
        scale *
        m_planetCamera.zoom;

    if (pixelsPerMeter <= 0.0)
        return physicalSizeMeters;

    const double longestPx =
        std::max(
            physicalSizeMeters.x,
            std::max(
                physicalSizeMeters.y,
                physicalSizeMeters.z
            )
        ) * pixelsPerMeter;

    constexpr double minPlayerLongestPx = 18.0;
    constexpr double minOtherLongestPx = 11.0;

    const double minPx =
        ship.player
            ? minPlayerLongestPx
            : minOtherLongestPx;

    if (longestPx >= minPx)
        return physicalSizeMeters;

    const double factor =
        minPx /
        std::max(
            1.0,
            longestPx
        );

    return physicalSizeMeters * factor;
}







void SystemMapRenderer::renderHubMap(
    const Viewport& viewport,
    const world::celestial::HubMapSnapshot& hub
)
{
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

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(
        0.015f,
        0.020f,
        0.030f,
        1.0f
    );

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!hub.valid)
        return;

    const glm::dvec2 centerPx(
        static_cast<double>(viewport.width) * 0.5,
        static_cast<double>(viewport.height) * 0.5
    );

    double maxDist =
        1000.0;

    for (const auto& m : hub.modules)
    {
        if (!m.valid)
            continue;

        maxDist =
            std::max(
                maxDist,
                glm::length(m.localPositionMeters) +
                glm::length(m.sizeMeters)
            );
    }

    for (const auto& s : hub.ships)
    {
        if (!s.valid)
            continue;

        maxDist =
            std::max(
                maxDist,
                glm::length(s.localPositionMeters) +
                800.0
            );
    }

    // Планета и орбита не должны раздувать масштаб карты до состояния:
    // "станция стала точкой, поздравляем, вы снова ничего не видите".
    //
    // Поэтому масштаб выбираем по объектам хаба, а планету рисуем как
    // ориентир в том же масштабе. Если центр планеты далеко за экраном,
    // видна будет дуга поверхности/орбиты — именно это и нужно.
    maxDist =
        std::max(
            maxDist,
            2500.0
        );

    const double halfPx =
        std::min(
            viewport.width,
            viewport.height
        ) * 0.38;

    const double scale =
        halfPx /
        std::max(
            1.0,
            maxDist
        );

    m_lastHubMapScale = scale;
    m_lastHubMapCenterPx = centerPx;
    m_lastHubMapPickables.clear();

    drawHubMapPlanetSurfaceHint(
        hub,
        scale,
        centerPx
    );

    drawHubMapAdaptiveGrid(
        viewport,
        scale,
        centerPx,
        maxDist
    );

    // Оси хаба.
    drawHubMapAxes(
        glm::dvec3(0.0),
        hub.hubAxes,
        900.0,
        scale,
        centerPx
    );

    // Центр хаба / текущая орбитальная точка.
    glColor4f(
        1.0f,
        0.86f,
        0.35f,
        0.95f
    );

    drawPlanetMapCross(
        hubMapProject(glm::dvec3(0.0), scale, centerPx),
        6.0f
    );

    // Модули станции.
    for (const auto& mod : hub.modules)
    {
        if (!mod.valid)
            continue;

        {
            const glm::dvec2 screen =
                hubMapProject(
                    mod.localPositionMeters,
                    scale,
                    centerPx
                );

            const double objectRadiusMeters =
                glm::length(mod.sizeMeters) * 0.5;

            HubMapPickable pickable;
            pickable.localCenterMeters =
                mod.localPositionMeters;

            pickable.screenCenterPx =
                screen;

            pickable.screenRadiusPx =
                objectRadiusMeters *
                scale *
                m_planetCamera.zoom;

            pickable.priority =
                mod.prime ? 20 : 10;

            pickable.label =
                mod.name;

            m_lastHubMapPickables.push_back(
                pickable
            );
        }







        if (mod.prime || mod.kind == "station")
        {
            glColor4f(
                0.65f,
                0.92f,
                1.0f,
                0.95f
            );
        }
        else
        {
            glColor4f(
                0.45f,
                0.65f,
                0.85f,
                0.75f
            );
        }


        const bool modelDrawn =
            drawHubMapAssemblyWire(
                mod.typeId,
                mod.localPositionMeters,
                mod.localAxes,
                scale,
                centerPx
            );

        if (!modelDrawn)
        {
            drawHubMapBox(
                mod.localPositionMeters,
                mod.localAxes,
                mod.sizeMeters,
                scale,
                centerPx
            );
        }

        const double moduleAxisLen =
            std::max(
                350.0,
                glm::length(mod.sizeMeters) * 0.08
            );

        drawHubMapAxes(
            mod.localPositionMeters,
            mod.localAxes,
            moduleAxisLen,
            scale,
            centerPx
        );


        
    }

    // Игрок / корабли.
    for (const auto& ship : hub.ships)
    {
        if (!ship.valid)
            continue;

    {
        const glm::dvec2 screen =
            hubMapProject(
                ship.localPositionMeters,
                scale,
                centerPx
            );

        const double objectRadiusMeters =
            glm::length(ship.sizeMeters) * 0.5;

        HubMapPickable pickable;
        pickable.localCenterMeters =
            ship.localPositionMeters;

        pickable.screenCenterPx =
            screen;

        pickable.screenRadiusPx =
            std::max(
                18.0,
                objectRadiusMeters *
                scale *
                m_planetCamera.zoom
            );

        pickable.priority =
            ship.player ? 100 : 50;

        pickable.label =
            ship.player
                ? "PLAYER"
                : ship.name;

        m_lastHubMapPickables.push_back(
            pickable
        );
    }


        if (ship.player)
        {
            glColor4f(
                1.0f,
                0.78f,
                0.25f,
                1.0f
            );
        }
        else
        {
            glColor4f(
                0.95f,
                0.65f,
                0.35f,
                0.85f
            );
        }

        const glm::dvec3 shipSize =
            visualSizeForHubShip(
                ship,
                scale
            );


        const bool shipModelDrawn =
            drawHubMapAssemblyWire(
                ship.typeId,
                ship.localPositionMeters,
                ship.localAxes,
                scale,
                centerPx
            );

        if (!shipModelDrawn)
        {
            drawHubMapBox(
                ship.localPositionMeters,
                ship.localAxes,
                ship.sizeMeters,
                scale,
                centerPx
            );
        }

        const double shipAxisLen =
            std::max(
                12.0,
                glm::length(ship.sizeMeters) * 0.85
            );

        drawHubMapAxes(
            ship.localPositionMeters,
            ship.localAxes,
            shipAxisLen,
            scale,
            centerPx
        );

        drawHubMapVelocityArrow(
            ship.localPositionMeters,
            ship.localVelocityMps,
            std::max(80.0, shipAxisLen * 2.0),
            scale,
            centerPx
        );
    }







        {
            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                viewport.width,
                viewport.height
            );

            for (const auto& mod : hub.modules)
            {
                if (!mod.valid)
                    continue;

                const glm::dvec2 p =
                    hubMapProject(
                        mod.localPositionMeters,
                        scale,
                        centerPx
                    );

                text.textDrawPx(
                    mod.name,
                    static_cast<float>(p.x + 10.0),
                    static_cast<float>(p.y - 8.0),
                    13,
                    glm::vec4(0.65f, 0.92f, 1.0f, 0.88f)
                );

                if (!mod.kind.empty())
                {
                    text.textDrawPx(
                        mod.kind,
                        static_cast<float>(p.x + 10.0),
                        static_cast<float>(p.y + 8.0),
                        10,
                        glm::vec4(0.55f, 0.72f, 0.82f, 0.62f)
                    );
                }
            }

            for (const auto& ship : hub.ships)
            {
                if (!ship.valid)
                    continue;

                const glm::dvec2 p =
                    hubMapProject(
                        ship.localPositionMeters,
                        scale,
                        centerPx
                    );

                const std::string label =
                    ship.player
                        ? "PLAYER"
                        : ship.name;

                text.textDrawPx(
                    label,
                    static_cast<float>(p.x + 10.0),
                    static_cast<float>(p.y - 8.0),
                    13,
                    glm::vec4(1.0f, 0.78f, 0.25f, 0.92f)
                );
            }

            text.endFrame();
        }












    glEnable(GL_DEPTH_TEST);
}


