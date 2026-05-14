#include "RenderCockpitPass.h"
#include <glad/gl.h>
#include <iostream>

#include "render/ShaderUtils.h"
#include "src/game/ship/cockpit/CockpitMeshBuilder.h"
#include "src/game/ship/cockpit/CockpitStrokeBuilder.h"



//    ##                ##       ##
//                               ##
//   ###     #####     ###      #####
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ## ##
//   ####    ##  ##    ####       ###

void RenderCockpitPass::init()
{
    // glGenVertexArrays(1, &vao);
    // glGenBuffers(1, &vbo);
    // glGenBuffers(1, &ebo);

    shader = compileShaderFromFiles(
        "assets/shaders/cockpit/cockpit.vert",
        "assets/shaders/cockpit/cockpit.frag"
    );

    fillShader = compileShaderFromFiles(
        "assets/shaders/cockpit/fill.vert",
        "assets/shaders/cockpit/fill.frag"
    );

    strokeShader = compileShaderFromFiles(
        "assets/shaders/cockpit/stroke.vert",
        "assets/shaders/cockpit/stroke.frag"
    );

    // uniform locations
    glUseProgram(fillShader);
    uFillType = glGetUniformLocation(fillShader, "uFillType");

    uColorA   = glGetUniformLocation(fillShader, "uColorA");
    uColorB   = glGetUniformLocation(fillShader, "uColorB");
    uGradFrom = glGetUniformLocation(fillShader, "uGradFrom");
    uGradTo   = glGetUniformLocation(fillShader, "uGradTo");
    uStrokeColor = glGetUniformLocation(strokeShader, "uStrokeColor");
    uFillAmount = glGetUniformLocation(fillShader, "uFillAmount");      // --- для полосатых индикаторов -----
    uRotationDeg = glGetUniformLocation(fillShader, "uRotationDeg");    // ---- для стрелочного индикатора ---
    uPivot       = glGetUniformLocation(fillShader, "uPivot");
    glUseProgram(0);


//     std::cout << "uFillAmount location = " << uFillAmount << std::endl;
   
//    GLint linked;
//     glGetProgramiv(fillShader, GL_LINK_STATUS, &linked);

//     std::cout << "Fill shader linked = " << linked << std::endl;

//     GLint count;
//     glGetProgramiv(fillShader, GL_ACTIVE_UNIFORMS, &count);

//     std::cout << "Active uniforms: " << count << std::endl;

    // for (int i = 0; i < count; ++i)
    // {
    //     char name[256];
    //     GLsizei length;
    //     GLint size;
    //     GLenum type;

    //     glGetActiveUniform(fillShader, i, 256, &length, &size, &type, name);
    //     std::cout << "Uniform #" << i << " = " << name << std::endl;
    // }
  
}



//                                ###
//                                 ##
//  ######    ####    #####        ##    ####    ######
//   ##  ##  ##  ##   ##  ##    #####   ##  ##    ##  ##
//   ##      ######   ##  ##   ##  ##   ######    ##
//   ##      ##       ##  ##   ##  ##   ##        ##
//  ####      #####   ##  ##    ######   #####   ####

void RenderCockpitPass::render(const ShipCockpitState& state)
{
    if (commands.empty())
        return;

    glDisable(GL_DEPTH_TEST);

    for (const auto& cmd : commands)
    {
        const CockpitVisualOverride* ov = nullptr;

        auto it = state.overrides.find(cmd.id);
        if (it != state.overrides.end())
            ov = &it->second;


        if (ov && !ov->visible)
            continue;

        
        //======= стрелочное =========================
        glm::vec2 pivotNDC(0.0f);

        if (cmd.hasPivot)
        {
            pivotNDC.x = cmd.pivot01.x * 2.0f - 1.0f;
            pivotNDC.y = 1.0f - cmd.pivot01.y * 2.0f;
        }
        glUniform2fv(uPivot, 1, &pivotNDC.x);


        //  ============ заливочное =======================

        if (cmd.type == CockpitDrawType::Fill)
        {
            glUseProgram(fillShader);
            glm::vec4 colorA = cmd.colorA;
            float fillAmount = 1.0f;
            if (ov && ov->overrideFill)
                fillAmount = ov->fill01;

            glUniform1f(uFillAmount, fillAmount);
            if (ov && ov->overrideColor)
                colorA = ov->color;
            if (ov && ov->overrideAlpha)
                colorA.a *= ov->alpha;

            glUniform4fv(uColorA, 1, &colorA.x);
            glUniform4fv(uColorB, 1, &cmd.colorB.x);
            glUniform1i(uFillType, (int)cmd.fillType);
            glUniform2fv(uGradFrom, 1, &cmd.gradFrom.x);
            glUniform2fv(uGradTo,   1, &cmd.gradTo.x);
        }
        else
        {
            glUseProgram(strokeShader);

            glm::vec4 color = cmd.strokeColor;
            if (ov && ov->overrideColor)
                color = ov->color;

            glUniform4fv(uStrokeColor, 1, &color.x);

        }

        glBindVertexArray(cmd.mesh.vao);
        glDrawElements(GL_TRIANGLES, cmd.mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}


// void RenderCockpitPass::render(const ShipCockpitState* state = nullptr)
// {
//     if (commands.empty())
//         return;

//     glDisable(GL_DEPTH_TEST);

//     for (const auto& cmd : commands)
//     {
//         const CockpitVisualOverride* ov = nullptr;

//         if (state)
//         {
//             auto it = state->overrides.find(cmd.id);
//             if (it != state->overrides.end())
//                 ov = &it->second;
//         }


//          if (ov && !ov->visible)
//             continue;


//         if (cmd.type == CockpitDrawType::Fill)
//         {
//             glUseProgram(fillShader);
//             glUniform1i(uFillType, (int)cmd.fillType);
//             glUniform4fv(uColorA, 1, &cmd.colorA[0]);
//             glUniform4fv(uColorB, 1, &cmd.colorB[0]);
//             glUniform2fv(uGradFrom, 1, &cmd.gradFrom[0]);
//             glUniform2fv(uGradTo,   1, &cmd.gradTo[0]);
//         }
//         else // Stroke
//         {
           
//             glUseProgram(strokeShader);

//             GLint loc = glGetUniformLocation(strokeShader, "uStrokeColor");
//             glUniform4fv(loc, 1, &cmd.strokeColor[0]);

//         }

//         glBindVertexArray(cmd.mesh.vao);

//         glDrawElements(
//             GL_TRIANGLES,
//             cmd.mesh.indexCount,
//             GL_UNSIGNED_INT,
//             nullptr
//         );
//     }

//     glBindVertexArray(0);
//     glUseProgram(0);
//     glEnable(GL_DEPTH_TEST);
// }



//                      ##                                                             ##
//                      ##                                                             ##
//   ### ##   ####     #####             ### ##   ####     ####    ##  ##    ####     #####   ######   ##  ##
//  ##  ##   ##  ##     ##              ##  ##   ##  ##   ##  ##   #######  ##  ##     ##      ##  ##  ##  ##
//  ##  ##   ######     ##              ##  ##   ######   ##  ##   ## # ##  ######     ##      ##      ##  ##
//   #####   ##         ## ##            #####   ##       ##  ##   ##   ##  ##         ## ##   ##       #####
//      ##    #####      ###                ##    #####    ####    ##   ##   #####      ###   ####         ##
//  #####                               #####                                                          #####


void RenderCockpitPass::setGeometry(const CockpitGeometry& geometry)
{
    // очистка старых VAO
    for (auto& cmd : commands)
    {
        glDeleteVertexArrays(1, &cmd.mesh.vao);
        glDeleteBuffers(1, &cmd.mesh.vbo);
        glDeleteBuffers(1, &cmd.mesh.ebo);
    }
    commands.clear();

    for (const auto& draw : geometry.drawList)
    {
        CockpitGLCommand cmd;
        cmd.type = draw.type;
        cmd.id = draw.id;

        if (draw.type == CockpitDrawType::Fill)
        {
            Mesh2D mesh = buildMeshFromPolygon01(draw.polygon);
            cmd.mesh = uploadMesh(mesh);

            cmd.fillType = draw.polygon.fillType;
            cmd.colorA   = draw.polygon.colorA;
            cmd.colorB   = draw.polygon.colorB;

            // SVG → NDC
            // cmd.gradFrom = draw.polygon.gradFrom01 * 2.0f - 1.0f;
            // cmd.gradTo   = draw.polygon.gradTo01   * 2.0f - 1.0f;
                cmd.gradFrom = draw.polygon.gradFrom01;
                cmd.gradTo   = draw.polygon.gradTo01;
        }
        else
        {
            Mesh2D mesh = buildStrokeMesh(draw.stroke);
            cmd.mesh = uploadMesh(mesh);

            cmd.strokeColor = draw.stroke.color;
        }

        commands.push_back(cmd);
    }
}



//                     ###                          ###                                       ###
//                      ##                           ##                                        ##
//  ##  ##   ######     ##      ####     ####        ##            ##  ##    ####     #####    ##
//  ##  ##    ##  ##    ##     ##  ##       ##    #####            #######  ##  ##   ##        #####
//  ##  ##    ##  ##    ##     ##  ##    #####   ##  ##            ## # ##  ######    #####    ##  ##
//  ##  ##    #####     ##     ##  ##   ##  ##   ##  ##            ##   ##  ##            ##   ##  ##
//   ######   ##       ####     ####     #####    ######           ##   ##   #####   ######   ###  ##
//           ####


GLMesh RenderCockpitPass::uploadMesh(const Mesh2D& mesh)
{
    GLMesh gm{};
    gm.indexCount = (uint32_t)mesh.indices.size();

    glGenVertexArrays(1, &gm.vao);
    glGenBuffers(1, &gm.vbo);
    glGenBuffers(1, &gm.ebo);

    glBindVertexArray(gm.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        mesh.vertices.size() * sizeof(Vertex2D),
        mesh.vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        mesh.indices.size() * sizeof(uint32_t),
        mesh.indices.data(),
        GL_STATIC_DRAW);



    // glEnableVertexAttribArray(0);
    // glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
    //     sizeof(Vertex2D), (void*)offsetof(Vertex2D, position));

    // glEnableVertexAttribArray(1);
    // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
    //     sizeof(Vertex2D), (void*)offsetof(Vertex2D, color));

            // location = 0 → position
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(
                    0, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex2D),
                    (void*)offsetof(Vertex2D, position)
                );

                // location = 1 → local01
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(
                    1, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex2D),
                    (void*)offsetof(Vertex2D, local01)
                );

                // location = 2 → color
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(
                    2, 4, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex2D),
                    (void*)offsetof(Vertex2D, color)
                );

    glBindVertexArray(0);
    return gm;
}



