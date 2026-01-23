#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"

// ==========================================================================
void Renderer::beginFrame()
{
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}




// ==========================================================================
void Renderer::endFrame()
{
    // swapBuffers делается в Window
}



// ==========================================================================
void Renderer::drawGrid(int size, float step)
{
    glBegin(GL_LINES);

    glColor3f(0.3f, 0.3f, 0.3f);

    for (int i = -size; i <= size; ++i)
    {
        glVertex3f(i * step, 0.0f, -size * step);
        glVertex3f(i * step, 0.0f,  size * step);

        glVertex3f(-size * step, 0.0f, i * step);
        glVertex3f( size * step, 0.0f, i * step);
    }

    glEnd();
}





