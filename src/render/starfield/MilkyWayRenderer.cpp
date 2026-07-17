#include "MilkyWayRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "src/render/ShaderLibrary.h"

using json = nlohmann::json;

namespace
{
struct SkyVertex
{
    glm::vec3 position;
    glm::vec2 uv;
};

float readFloat(const json& item, const char* key, float fallback)
{
    if (!item.contains(key) || !item[key].is_number())
        return fallback;

    return static_cast<float>(item[key].get<double>());
}

glm::vec3 readVec3(const json& item, const char* key, const glm::vec3& fallback)
{
    if (!item.contains(key) || !item[key].is_array())
        return fallback;

    const auto& a = item[key];

    if (a.size() < 3)
        return fallback;

    if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number())
        return fallback;

    return glm::vec3(
        static_cast<float>(a[0].get<double>()),
        static_cast<float>(a[1].get<double>()),
        static_cast<float>(a[2].get<double>())
    );
}
}

MilkyWayRenderer::~MilkyWayRenderer()
{
    shutdown();
}

bool MilkyWayRenderer::initialize(const std::string& path)
{
    if (m_initialized)
        return true;

    bool loaded = loadConfig(path);

    if (!loaded && path == "assets/data/galaxy/milky_way.json")
    {
        loaded = loadConfig("../assets/data/galaxy/milky_way.json");
    }

    if (!loaded)
    {
        std::cerr << "[MilkyWay] failed to load milky_way.json, using defaults" << std::endl;
    }

    const SkyVertex vertices[] =
    {
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}},

        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}},
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkyVertex),
        reinterpret_cast<void*>(offsetof(SkyVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkyVertex),
        reinterpret_cast<void*>(offsetof(SkyVertex, uv))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_initialized = true;

    std::cout
        << "[MilkyWay] initialized as full-screen galactic sky field"
        << " center=("
        << m_galacticCenterDir.x << ", "
        << m_galacticCenterDir.y << ", "
        << m_galacticCenterDir.z << ")"
        << " north=("
        << m_galacticNorthDir.x << ", "
        << m_galacticNorthDir.y << ", "
        << m_galacticNorthDir.z << ")"
        << std::endl;

    return true;
}

void MilkyWayRenderer::shutdown()
{
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    if (m_halfTexture != 0)
    {
        glDeleteTextures(
            1,
            &m_halfTexture
        );

        m_halfTexture = 0;
    }

    if (m_halfFramebuffer != 0)
    {
        glDeleteFramebuffers(
            1,
            &m_halfFramebuffer
        );

        m_halfFramebuffer = 0;
    }

    if (m_compositeVao != 0)
    {
        glDeleteVertexArrays(
            1,
            &m_compositeVao
        );

        m_compositeVao = 0;
    }

    m_halfWidth = 0;
    m_halfHeight = 0;

    m_initialized = false;
}

void MilkyWayRenderer::setObserverPositionLy(const glm::vec3& observerLy)
{
    // Пока направление на галактический центр считаем практически бесконечно дальним.
    // Для перемещений внутри локального пузыря это правильно: небо не должно прыгать.
    m_observerLy = observerLy;
}

bool MilkyWayRenderer::loadConfig(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        std::cerr << "[MilkyWay] config not found: " << path << std::endl;
        return false;
    }

    std::cout << "[MilkyWay] loading config: " << path << std::endl;

    json root;

    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[MilkyWay] failed to parse JSON: " << e.what() << std::endl;
        return false;
    }

    if (root.contains("orientation") && root["orientation"].is_object())
    {
        const auto& o = root["orientation"];

        m_galacticCenterDir = readVec3(
            o,
            "galactic_center_dir",
            m_galacticCenterDir
        );

        m_galacticNorthDir = readVec3(
            o,
            "galactic_north_dir",
            m_galacticNorthDir
        );
    }

    if (glm::length(m_galacticCenterDir) < 0.001f)
        m_galacticCenterDir = glm::vec3(-0.0549f, -0.8734f, -0.4838f);

    if (glm::length(m_galacticNorthDir) < 0.001f)
        m_galacticNorthDir = glm::vec3(-0.8676f, -0.1981f, 0.4559f);

    m_galacticCenterDir = glm::normalize(m_galacticCenterDir);
    m_galacticNorthDir = glm::normalize(m_galacticNorthDir);

    // Убираем возможную неортогональность из JSON.
    m_galacticNorthDir = glm::normalize(
        m_galacticNorthDir -
        m_galacticCenterDir * glm::dot(m_galacticNorthDir, m_galacticCenterDir)
    );

    if (root.contains("appearance") && root["appearance"].is_object())
    {
        const auto& a = root["appearance"];

        m_bandWidthDeg = readFloat(a, "band_width_deg", m_bandWidthDeg);
        m_coreWidthDeg = readFloat(a, "core_width_deg", m_coreWidthDeg);
        m_intensity = readFloat(a, "intensity", m_intensity);
        m_dustStrength = readFloat(a, "dust_strength", m_dustStrength);
    }

    m_bandWidthDeg = std::max(3.0f, m_bandWidthDeg);
    m_coreWidthDeg = std::max(4.0f, m_coreWidthDeg);
    m_intensity = std::max(0.0f, m_intensity);
    m_dustStrength = std::max(0.0f, std::min(1.0f, m_dustStrength));

    return true;
}


void MilkyWayRenderer::ensureHalfResolutionTarget(
    int width,
    int height
)
{
    width =
        std::max(
            1,
            width
        );

    height =
        std::max(
            1,
            height
        );

    if (m_compositeShader == 0)
    {
        m_compositeShader =
            ShaderLibrary::instance().get(
                "galaxy_haze_composite"
            );

        if (m_compositeShader != 0)
        {
            m_compositeTextureLocation =
                glGetUniformLocation(
                    m_compositeShader,
                    "uHazeTexture"
                );
        }
    }

    if (m_halfFramebuffer == 0)
    {
        glGenFramebuffers(
            1,
            &m_halfFramebuffer
        );
    }

    if (m_halfTexture == 0)
    {
        glGenTextures(
            1,
            &m_halfTexture
        );
    }

    if (m_compositeVao == 0)
    {
        glGenVertexArrays(
            1,
            &m_compositeVao
        );
    }

    if (m_halfWidth == width &&
        m_halfHeight == height)
    {
        return;
    }

    GLint previousTexture = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_halfTexture
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture
        )
    );

    m_halfWidth = width;
    m_halfHeight = height;
}


void MilkyWayRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    float skyRadius
)
{
    (void)skyRadius;

    if (!m_initialized ||
        m_vao == 0 ||
        m_vbo == 0)
    {
        return;
    }

    const GLuint hazeShader =
        ShaderLibrary::instance().get(
            "galaxy_haze"
        );

    if (hazeShader == 0)
        return;

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] =
    {
        0,
        0,
        1,
        1
    };

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = 0;
    GLint previousTexture0 = 0;

    GLint previousBlendSourceRgb = GL_SRC_ALPHA;
    GLint previousBlendDestinationRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint previousBlendSourceAlpha = GL_SRC_ALPHA;
    GLint previousBlendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;

    GLfloat previousClearColor[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };

    glGetFloatv(
        GL_COLOR_CLEAR_VALUE,
        previousClearColor
    );

    glGetIntegerv(
        GL_FRAMEBUFFER_BINDING,
        &previousFramebuffer
    );

    glGetIntegerv(
        GL_VIEWPORT,
        previousViewport
    );

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previousActiveTexture
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture0
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

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    const GLboolean scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );

    constexpr float hazeResolutionScale =
        0.50f;

    const int halfWidth =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    static_cast<double>(
                        previousViewport[2]
                    ) *
                    hazeResolutionScale
                )
            )
        );

    const int halfHeight =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    static_cast<double>(
                        previousViewport[3]
                    ) *
                    hazeResolutionScale
                )
            )
        );

    ensureHalfResolutionTarget(
        halfWidth,
        halfHeight
    );

    const bool halfResolutionReady =
        m_halfFramebuffer != 0 &&
        m_halfTexture != 0 &&
        m_compositeVao != 0 &&
        m_compositeShader != 0;

    if (!halfResolutionReady)
    {
        if (!m_halfResolutionWarningPrinted)
        {
            m_halfResolutionWarningPrinted = true;

            std::cerr
                << "[MilkyWay]"
                << " half-resolution haze unavailable;"
                << " falling back to direct rendering"
                << "\n";
        }
    }

    const glm::mat4 viewRotation =
        glm::mat4(
            glm::mat3(
                view
            )
        );

    const glm::mat4 invViewRotation =
        glm::inverse(
            viewRotation
        );

    const glm::mat4 invProjection =
        glm::inverse(
            projection
        );

    auto uploadHazeUniforms =
        [&]()
        {
            glUseProgram(
                hazeShader
            );

            const GLint locInvProjection =
                glGetUniformLocation(
                    hazeShader,
                    "uInvProjection"
                );

            if (locInvProjection >= 0)
            {
                glUniformMatrix4fv(
                    locInvProjection,
                    1,
                    GL_FALSE,
                    glm::value_ptr(
                        invProjection
                    )
                );
            }

            const GLint locInvViewRotation =
                glGetUniformLocation(
                    hazeShader,
                    "uInvViewRotation"
                );

            if (locInvViewRotation >= 0)
            {
                glUniformMatrix4fv(
                    locInvViewRotation,
                    1,
                    GL_FALSE,
                    glm::value_ptr(
                        invViewRotation
                    )
                );
            }

            const GLint locGalacticCenterDir =
                glGetUniformLocation(
                    hazeShader,
                    "uGalacticCenterDir"
                );

            if (locGalacticCenterDir >= 0)
            {
                glUniform3fv(
                    locGalacticCenterDir,
                    1,
                    glm::value_ptr(
                        m_galacticCenterDir
                    )
                );
            }

            const GLint locGalacticNorthDir =
                glGetUniformLocation(
                    hazeShader,
                    "uGalacticNorthDir"
                );

            if (locGalacticNorthDir >= 0)
            {
                glUniform3fv(
                    locGalacticNorthDir,
                    1,
                    glm::value_ptr(
                        m_galacticNorthDir
                    )
                );
            }

            const GLint locBandWidthDeg =
                glGetUniformLocation(
                    hazeShader,
                    "uBandWidthDeg"
                );

            if (locBandWidthDeg >= 0)
            {
                glUniform1f(
                    locBandWidthDeg,
                    m_bandWidthDeg
                );
            }

            const GLint locCoreWidthDeg =
                glGetUniformLocation(
                    hazeShader,
                    "uCoreWidthDeg"
                );

            if (locCoreWidthDeg >= 0)
            {
                glUniform1f(
                    locCoreWidthDeg,
                    m_coreWidthDeg
                );
            }

            const GLint locIntensity =
                glGetUniformLocation(
                    hazeShader,
                    "uIntensity"
                );

            if (locIntensity >= 0)
            {
                glUniform1f(
                    locIntensity,
                    m_intensity
                );
            }

            const GLint locDustStrength =
                glGetUniformLocation(
                    hazeShader,
                    "uDustStrength"
                );

            if (locDustStrength >= 0)
            {
                glUniform1f(
                    locDustStrength,
                    m_dustStrength
                );
            }
        };

    if (halfResolutionReady)
    {
        glBindFramebuffer(
            GL_FRAMEBUFFER,
            m_halfFramebuffer
        );

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            m_halfTexture,
            0
        );

        if (glCheckFramebufferStatus(
                GL_FRAMEBUFFER
            ) == GL_FRAMEBUFFER_COMPLETE)
        {
            glViewport(
                0,
                0,
                m_halfWidth,
                m_halfHeight
            );

            glDisable(
                GL_BLEND
            );

            glDisable(
                GL_DEPTH_TEST
            );

            glDisable(
                GL_CULL_FACE
            );

            glDisable(
                GL_SCISSOR_TEST
            );

            glClearColor(
                0.0f,
                0.0f,
                0.0f,
                0.0f
            );

            glClear(
                GL_COLOR_BUFFER_BIT
            );

            uploadHazeUniforms();

            glBindVertexArray(
                m_vao
            );

            glDrawArrays(
                GL_TRIANGLES,
                0,
                6
            );

            glBindFramebuffer(
                GL_FRAMEBUFFER,
                static_cast<GLuint>(
                    previousFramebuffer
                )
            );

            glViewport(
                previousViewport[0],
                previousViewport[1],
                previousViewport[2],
                previousViewport[3]
            );

            glDisable(
                GL_DEPTH_TEST
            );

            glDisable(
                GL_CULL_FACE
            );

            glDisable(
                GL_SCISSOR_TEST
            );

            glEnable(
                GL_BLEND
            );

            glBlendEquation(
                GL_FUNC_ADD
            );

            glBlendFunc(
                GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA
            );

            glUseProgram(
                m_compositeShader
            );

            glActiveTexture(
                GL_TEXTURE0
            );

            glBindTexture(
                GL_TEXTURE_2D,
                m_halfTexture
            );

            if (m_compositeTextureLocation >= 0)
            {
                glUniform1i(
                    m_compositeTextureLocation,
                    0
                );
            }

            glBindVertexArray(
                m_compositeVao
            );

            glDrawArrays(
                GL_TRIANGLES,
                0,
                3
            );
        }
        else
        {
            glBindFramebuffer(
                GL_FRAMEBUFFER,
                static_cast<GLuint>(
                    previousFramebuffer
                )
            );

            glViewport(
                previousViewport[0],
                previousViewport[1],
                previousViewport[2],
                previousViewport[3]
            );

            uploadHazeUniforms();

            glBindVertexArray(
                m_vao
            );

            glDrawArrays(
                GL_TRIANGLES,
                0,
                6
            );
        }
    }
    else
    {
        uploadHazeUniforms();

        glBindVertexArray(
            m_vao
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            6
        );
    }

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        static_cast<GLuint>(
            previousFramebuffer
        )
    );

    glViewport(
        previousViewport[0],
        previousViewport[1],
        previousViewport[2],
        previousViewport[3]
    );

    glBindVertexArray(
        static_cast<GLuint>(
            previousVao
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            previousProgram
        )
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture0
        )
    );

    glActiveTexture(
        static_cast<GLenum>(
            previousActiveTexture
        )
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

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glClearColor(
        previousClearColor[0],
        previousClearColor[1],
        previousClearColor[2],
        previousClearColor[3]
    );
}