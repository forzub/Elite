
// // #pragma once

// // #include <glad/gl.h>
// // #include <glm/glm.hpp>
// // #include "LightingParams.h"

// // namespace render
// // {
// // class MeshGPU;
// // }

// // class MeshRenderer
// // {
// // public:
// //     void draw(
// //         const render::MeshGPU& mesh,
// //         GLuint fillShader,
// //         GLuint lineShader,
// //         const glm::mat4& mvp,
// //         const glm::mat4& model,
// //         const LightingParams& lighting = LightingParams()
// //     );
    
// //     // Глобальные параметры по умолчанию
// //     void setDefaultParams(const LightingParams& params) { m_defaultParams = params; }
// //     const LightingParams& getDefaultParams() const { return m_defaultParams; }
    
// // private:
// //     LightingParams m_defaultParams;
    
// //     void setupFillPass(GLuint shader, const glm::mat4& mvp, 
// //                        const glm::mat4& model, const LightingParams& lighting);
// //     void setupEdgePass(GLuint shader, const LightingParams& lighting, 
// //                        const render::MeshGPU& mesh, const glm::mat4& mvp);
// // };


// #pragma once

// #include <glad/gl.h>
// #include <glm/glm.hpp>
// #include "LightingParams.h"

// namespace render
// {
// class MeshGPU;
// }

// class MeshRenderer
// {
// public:
//     void draw(
//         const render::MeshGPU& mesh,
//         GLuint fillShader,
//         GLuint lineShader,
//         const glm::mat4& mvp,
//         const glm::mat4& model,
//         const LightingParams& lighting = LightingParams(),
//         const glm::vec3& cameraPos = glm::vec3(0.0f)  // позиция камеры
//     );
    
//     void setDefaultParams(const LightingParams& params) { m_defaultParams = params; }
//     const LightingParams& getDefaultParams() const { return m_defaultParams; }
    
// private:
//     LightingParams m_defaultParams;
    
//     void setupFillPass(GLuint shader, const glm::mat4& mvp, 
//                        const glm::mat4& model, const LightingParams& lighting,
//                        const glm::vec3& cameraPos);
    
//     void setupEdgePass(GLuint shader, const LightingParams& lighting, 
//                        const render::MeshGPU& mesh, const glm::mat4& mvp,
//                        const glm::vec3& cameraPos);
// };


#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "LightingParams.h"

namespace render
{
class MeshGPU;
}

class MeshRenderer
{
public:
    void draw(
        const render::MeshGPU& mesh,
        GLuint fillShader,
        GLuint lineShader,
        const glm::mat4& mvp,
        const glm::mat4& model,
        const LightingParams& lighting = LightingParams(),
        const glm::vec3& cameraPos = glm::vec3(0.0f),
        int viewportWidth = 800,    // добавили параметры viewport
        int viewportHeight = 600
    );
    
    void setDefaultParams(const LightingParams& params) { m_defaultParams = params; }
    const LightingParams& getDefaultParams() const { return m_defaultParams; }
    
private:
    LightingParams m_defaultParams;
    
    void setupMeshPass(GLuint shader, const glm::mat4& mvp, 
                       const glm::mat4& model, const LightingParams& lighting,
                       const glm::vec3& cameraPos);
    
    void setupEdgePass(GLuint shader, const LightingParams& lighting,
                                 const render::MeshGPU& mesh, const glm::mat4& mvp,
                                 const glm::vec3& cameraPos,
                                 int viewportWidth, int viewportHeight);  // добавили параметры
};