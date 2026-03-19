#include "MeshRenderer.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include "src/render/ShaderProgram.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/MeshGPU.h"

void MeshRenderer::setupMeshPass(GLuint shader, const glm::mat4& mvp, 
                                 const glm::mat4& model, const LightingParams& lighting,
                                 const glm::vec3& cameraPos) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    
}

void MeshRenderer::setupEdgePass(GLuint shader, const LightingParams& lighting,
                                 const render::MeshGPU& mesh, const glm::mat4& mvp,
                                 const glm::vec3& cameraPos,
                                 int viewportWidth, int viewportHeight) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    lighting.applyEdgeUniforms(shader, mvp, cameraPos, viewportWidth, viewportHeight);
    
    glDepthMask(GL_FALSE);
    mesh.drawEdges();
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
        setupEdgePass(edgeShader, lighting, mesh, mvp, cameraPos, 
                      viewportWidth, viewportHeight);
    }
    
    glUseProgram(0);
}




