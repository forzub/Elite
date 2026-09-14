#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderUtils.h"

namespace game::system_map
{
class SystemMapGpuCircleBatch
{
public:
    enum class Plane : int
    {
        XZ = 0,
        XY = 1
    };

    void begin()
    {
        m_batches.clear();
    }

    void add(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments,
        Plane plane
    )
    {
        if (radius <= 0.0f)
            return;

        segments = std::max(12, segments);

        if (m_batches.empty() ||
            m_batches.back().segments != segments ||
            m_batches.back().plane != plane)
        {
            Batch batch;
            batch.segments = segments;
            batch.plane = plane;
            m_batches.push_back(std::move(batch));
        }

        Instance instance;
        instance.centerRadius[0] = center.x;
        instance.centerRadius[1] = center.y;
        instance.centerRadius[2] = center.z;
        instance.centerRadius[3] = radius;
        instance.color[0] = color.r;
        instance.color[1] = color.g;
        instance.color[2] = color.b;
        instance.color[3] = color.a;
        m_batches.back().instances.push_back(instance);
    }

    void flush(const glm::mat4& mvp)
    {
        if (m_batches.empty())
            return;

        ensureShader();
        if (!m_shader)
            return;

        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

        GLfloat oldLineWidth = 1.0f;
        glGetFloatv(GL_LINE_WIDTH, &oldLineWidth);

        glUseProgram(m_shader);
        glUniformMatrix4fv(
            m_mvpLoc,
            1,
            GL_FALSE,
            glm::value_ptr(mvp)
        );

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.5f);

        for (const Batch& batch : m_batches)
        {
            if (batch.instances.empty())
                continue;

            Mesh& mesh = ensureMesh(batch.segments);
            if (!mesh.vao)
                continue;

            glBindVertexArray(mesh.vao);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceBuffer);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    batch.instances.size() * sizeof(Instance)
                ),
                batch.instances.data(),
                GL_STREAM_DRAW
            );

            glUniform1i(
                m_planeLoc,
                static_cast<int>(batch.plane)
            );

            glDrawArraysInstanced(
                GL_LINE_LOOP,
                0,
                mesh.vertexCount,
                static_cast<GLsizei>(batch.instances.size())
            );
        }

        glLineWidth(oldLineWidth);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

private:
    struct Instance
    {
        float centerRadius[4] { 0.0f, 0.0f, 0.0f, 0.0f };
        float color[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct Batch
    {
        int segments = 0;
        Plane plane = Plane::XZ;
        std::vector<Instance> instances;
    };

    struct Mesh
    {
        GLuint vao = 0;
        GLuint unitBuffer = 0;
        GLuint instanceBuffer = 0;
        GLsizei vertexCount = 0;
    };

    void ensureShader()
    {
        if (m_shader)
            return;

        static constexpr const char* VertexShader = R"GLSL(
#version 430 core
layout(location = 0) in vec2 aUnit;
layout(location = 1) in vec4 iCenterRadius;
layout(location = 2) in vec4 iColor;

uniform mat4 uMVP;
uniform int uPlane;

out vec4 vColor;

void main()
{
    vec3 offset = uPlane == 0
        ? vec3(aUnit.x, 0.0, aUnit.y)
        : vec3(aUnit.x, aUnit.y, 0.0);

    vec3 worldPosition =
        iCenterRadius.xyz + offset * iCenterRadius.w;

    gl_Position = uMVP * vec4(worldPosition, 1.0);
    vColor = iColor;
}
)GLSL";

        static constexpr const char* FragmentShader = R"GLSL(
#version 430 core
in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vColor;
}
)GLSL";

        m_shader = compileShader(VertexShader, FragmentShader);
        if (!m_shader)
            return;

        m_mvpLoc = glGetUniformLocation(m_shader, "uMVP");
        m_planeLoc = glGetUniformLocation(m_shader, "uPlane");
    }

    Mesh& ensureMesh(int segments)
    {
        auto found = m_meshes.find(segments);
        if (found != m_meshes.end())
            return found->second;

        Mesh mesh;
        mesh.vertexCount = static_cast<GLsizei>(segments);

        std::vector<glm::vec2> unitCircle;
        unitCircle.reserve(static_cast<std::size_t>(segments));

        for (int i = 0; i < segments; ++i)
        {
            const float angle =
                static_cast<float>(i) /
                static_cast<float>(segments) *
                glm::two_pi<float>();

            unitCircle.emplace_back(
                std::cos(angle),
                std::sin(angle)
            );
        }

        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.unitBuffer);
        glGenBuffers(1, &mesh.instanceBuffer);

        glBindVertexArray(mesh.vao);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.unitBuffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                unitCircle.size() * sizeof(glm::vec2)
            ),
            unitCircle.data(),
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::vec2),
            nullptr
        );

        glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceBuffer);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Instance),
            reinterpret_cast<const void*>(offsetof(Instance, centerRadius))
        );
        glVertexAttribDivisor(1, 1);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Instance),
            reinterpret_cast<const void*>(offsetof(Instance, color))
        );
        glVertexAttribDivisor(2, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        auto inserted = m_meshes.emplace(segments, mesh);
        return inserted.first->second;
    }

    GLuint m_shader = 0;
    GLint m_mvpLoc = -1;
    GLint m_planeLoc = -1;
    std::unordered_map<int, Mesh> m_meshes;
    std::vector<Batch> m_batches;
};
} // namespace game::system_map
