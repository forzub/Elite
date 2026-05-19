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




