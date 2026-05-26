#include "MeshRenderer.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderProgram.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/MeshGPU.h"

void MeshRenderer::setupMeshPass(GLuint shader, const glm::mat4& mvp,
                                 const glm::mat4& model, const LightingParams& lighting,
                                 const glm::vec3& cameraPos)
{
    // Fill-pass должен писать настоящую глубину корпуса.
    // Иначе обшивка уезжает назад в depth-buffer,
    // а внутренние ребра двигателя начинают просвечивать.
    glDisable(GL_POLYGON_OFFSET_FILL);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void MeshRenderer::setupEdgePass(GLuint shader, const LightingParams& lighting,
                                const render::MeshGPU& mesh,
                                const glm::mat4& mvp,
                                const glm::mat4& model,
                                const glm::vec3& cameraPos,
                                int viewportWidth, int viewportHeight) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        GLint vp[4] = {0, 0, 800, 600};
        glGetIntegerv(GL_VIEWPORT, vp);
        viewportWidth = vp[2];
        viewportHeight = vp[3];
    }

    lighting.applyEdgeUniforms(shader, mvp, model, cameraPos, viewportWidth, viewportHeight);

    glEnable(GL_DEPTH_TEST);

// Ребра не должны перезаписывать depth,
// но должны честно проверяться по уже нарисованной обшивке.
glDepthMask(GL_FALSE);
glDepthFunc(GL_LESS);

// Линии слегка вытягиваем к камере только для собственных ребер поверхности,
// не для того, чтобы они пролезали сквозь корпус.
glEnable(GL_POLYGON_OFFSET_LINE);
glPolygonOffset(-0.5f, -0.5f);

mesh.drawEdges();

glDisable(GL_POLYGON_OFFSET_LINE);

glDepthFunc(GL_LESS);
glDepthMask(GL_TRUE);

    glDisable(GL_BLEND);
}

void MeshRenderer::draw(
    const render::MeshGPU& mesh,
    GLuint meshShader,
    GLuint edgeShader,
    const glm::mat4& mvp,
    const glm::mat4& model,
    const LightingParams& lighting,
    const glm::vec3& cameraPos,
    int viewportWidth,
    int viewportHeight
) {

    if (meshShader){


    
        ShaderProgram shaderType = ShaderLibrary::instance().getType(meshShader);

        if (shaderType == ShaderProgram::MeshFill){
            lighting.applyShipUniforms(meshShader, mvp, model, cameraPos);
        }

        if (shaderType == ShaderProgram::LargeObjectFill){
            lighting.applyLargeObjectUniforms(meshShader, mvp, model, cameraPos);
        }

        setupMeshPass(meshShader, mvp, model, lighting, cameraPos);
        mesh.draw();
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    

    if (edgeShader) {
        setupEdgePass(edgeShader, lighting, mesh, mvp, model, cameraPos, viewportWidth, viewportHeight);
    }
    
    glUseProgram(0);
}




void MeshRenderer::drawInstanced(
    const render::MeshGPU& mesh,
    GLuint meshShader,
    const glm::mat4& vp,
    const std::vector<glm::mat4>& models,
    const LightingParams& lighting,
    const glm::vec3& cameraPos
)
{
    if (!meshShader || models.empty())
        return;

    // Переиспользуем lighting uniforms от обычного mesh_fill.
    // MVP/M/normalMat он тоже выставит, но instanced shader их не использует.
    lighting.applyShipUniforms(
        meshShader,
        glm::mat4(1.0f),
        glm::mat4(1.0f),
        cameraPos
    );

    glUseProgram(meshShader);

    GLint vpLoc =
        glGetUniformLocation(meshShader, "VP");

    if (vpLoc != -1)
    {
        glUniformMatrix4fv(
            vpLoc,
            1,
            GL_FALSE,
            glm::value_ptr(vp)
        );
    }

    glDisable(GL_POLYGON_OFFSET_FILL);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    mesh.drawInstanced(models);

    glUseProgram(0);
}





void MeshRenderer::drawEdgesInstanced(
    const render::MeshGPU& mesh,
    GLuint edgeShader,
    const glm::mat4& vp,
    const std::vector<glm::mat4>& models,
    const glm::vec3& cameraPos,
    float edgeFadeStart,
    float edgeFadeEnd,
    const glm::vec3& edgeColor,
    float edgeIntensity
)
{
    if (!edgeShader || models.empty())
        return;

    glUseProgram(edgeShader);

    GLint vpLoc =
        glGetUniformLocation(edgeShader, "VP");

    if (vpLoc != -1)
    {
        glUniformMatrix4fv(
            vpLoc,
            1,
            GL_FALSE,
            glm::value_ptr(vp)
        );
    }

    GLint cameraLoc =
        glGetUniformLocation(edgeShader, "cameraPos");

    if (cameraLoc != -1)
    {
        glUniform3fv(
            cameraLoc,
            1,
            glm::value_ptr(cameraPos)
        );
    }

    GLint fadeStartLoc =
        glGetUniformLocation(edgeShader, "edgeFadeStart");

    if (fadeStartLoc != -1)
    {
        glUniform1f(fadeStartLoc, edgeFadeStart);
    }

    GLint fadeEndLoc =
        glGetUniformLocation(edgeShader, "edgeFadeEnd");

    if (fadeEndLoc != -1)
    {
        glUniform1f(fadeEndLoc, edgeFadeEnd);
    }

    GLint colorLoc =
        glGetUniformLocation(edgeShader, "edgeColor");

    if (colorLoc != -1)
    {
        glUniform3fv(
            colorLoc,
            1,
            glm::value_ptr(edgeColor)
        );
    }

    GLint intensityLoc =
        glGetUniformLocation(edgeShader, "edgeIntensity");

    if (intensityLoc != -1)
    {
        glUniform1f(intensityLoc, edgeIntensity);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDepthMask(GL_FALSE);

    mesh.drawEdgesInstanced(models);

    glDepthMask(GL_TRUE);

    glUseProgram(0);
}