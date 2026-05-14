#include <glad/gl.h>
#include <iostream>
#include "RenderCockpitBitmapPass.h"
#include "src/render/ShaderUtils.h"



void RenderCockpitBitmapPass::init()
{
    // fullscreen quad
    float verts[] = {
        // pos      // uv
        -1, -1,     0, 0,
         1, -1,     1, 0,
         1,  1,     1, 1,
        -1,  1,     0, 1,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    shader = compileShaderFromFiles(
        "assets/shaders/cockpit/cockpit_bitmap.vert",
        "assets/shaders/cockpit/cockpit_bitmap.frag"
    );

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_FRAMEBUFFER_SRGB);


    
}



void RenderCockpitBitmapPass::render()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shader);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);

    if (glassIntensity == 0.0f)
    {
        glBindTexture(GL_TEXTURE_2D, baseTex);
        glUniform1f(glGetUniformLocation(shader, "uAlphaMul"), 1.0f);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, glassTex);
        glUniform1f(glGetUniformLocation(shader, "uAlphaMul"), glassIntensity);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
    glUseProgram(0);
}


void RenderCockpitBitmapPass::setBaseTexture(unsigned int tex)
{
    baseTex = tex;
}

void RenderCockpitBitmapPass::setGlassTexture(unsigned int tex)
{
    glassTex = tex;
}

void RenderCockpitBitmapPass::setGlassIntensity(float k)
{
    glassIntensity = k;
}

void RenderCockpitBitmapPass::renderBase()
{
    // std::cout << "renderBase\n";

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shader);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, baseTex);

    glUniform1f(glGetUniformLocation(shader, "uAlphaMul"), 1.0f);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
    glUseProgram(0);
}

void RenderCockpitBitmapPass::renderGlass()
{
    // std::cout << "renderGlass\n";

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shader);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glassTex);

    glUniform1f(glGetUniformLocation(shader, "uAlphaMul"), glassIntensity);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
    glUseProgram(0);
}