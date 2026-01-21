#pragma once

class Renderer
{
public:
    void beginFrame();
    void endFrame();

    void drawGrid(int size, float step);
};