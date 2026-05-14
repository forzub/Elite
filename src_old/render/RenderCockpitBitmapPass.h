#pragma once

#include <glm/glm.hpp>

class RenderCockpitBitmapPass
{
public:
    void init();
    void render();

    void setBaseTexture(unsigned int tex);
    void setGlassTexture(unsigned int tex);

    void setGlassIntensity(float k); // 0..~2

    void renderGlass();
    void renderBase();

private:
    unsigned int shader = 0;
    unsigned int vao = 0;
    unsigned int vbo = 0;

    unsigned int baseTex = 0;
    unsigned int glassTex = 0;

    float glassIntensity = 1.0f;
};
