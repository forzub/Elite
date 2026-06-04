#include "UICameraView.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>



UICameraView::~UICameraView()
{
    if (fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &colorTex);
        glDeleteRenderbuffers(1, &depthRb);
    }
}



void UICameraView::render(
    const Viewport& vp,
    float parentX,
    float parentY,
    float parentW,
    float parentH
)
{
    if (!visible || !colorTex)
        return;

    const float px = parentX + position.x * parentW;
    const float py = parentY + position.y * parentH;
    const float pw = size.x * parentW;
    const float ph = size.y * parentH;

    const float localX = px;
    const float localY = py;

    // const float px = static_cast<float>(vp.x) + localX;
    // const float py = static_cast<float>(vp.y) + localY;

    const float minSide = std::min(pw, ph);

    float radius =
        cornerRadiusRel * minSide;

    float borderThickness =
        borderThicknessRel * minSide;

    radius = std::clamp(
        radius,
        0.0f,
        minSide * 0.5f
    );

    const float innerLeft   = px + radius;
    const float innerRight  = px + pw - radius;
    const float innerTop    = py + radius;
    const float innerBottom = py + ph - radius;

    auto texU = [&](float x) -> float
    {
        return (x - px) / pw;
    };

    // ВАЖНО:
    // UI-координата Y растёт сверху вниз,
    // а FBO texture coordinate Y в OpenGL растёт снизу вверх.
    // Поэтому здесь нужен flip.
    auto texV = [&](float y) -> float
    {
        return 1.0f - ((y - py) / ph);
    };

    auto texturedVertex = [&](float x, float y)
    {
        glTexCoord2f(texU(x), texV(y));
        glVertex2f(x, y);
    };

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, colorTex);

    // -------------------------------------------------
    // Центральный прямоугольник
    // -------------------------------------------------
    glBegin(GL_QUADS);

    texturedVertex(innerLeft,  innerTop);
    texturedVertex(innerRight, innerTop);
    texturedVertex(innerRight, innerBottom);
    texturedVertex(innerLeft,  innerBottom);

    glEnd();

    // -------------------------------------------------
    // Верхняя полоса
    // -------------------------------------------------
    glBegin(GL_QUADS);

    texturedVertex(innerLeft,  py);
    texturedVertex(innerRight, py);
    texturedVertex(innerRight, innerTop);
    texturedVertex(innerLeft,  innerTop);

    glEnd();

    // -------------------------------------------------
    // Нижняя полоса
    // -------------------------------------------------
    glBegin(GL_QUADS);

    texturedVertex(innerLeft,  innerBottom);
    texturedVertex(innerRight, innerBottom);
    texturedVertex(innerRight, py + ph);
    texturedVertex(innerLeft,  py + ph);

    glEnd();

    // -------------------------------------------------
    // Левая полоса
    // -------------------------------------------------
    glBegin(GL_QUADS);

    texturedVertex(px,        innerTop);
    texturedVertex(innerLeft, innerTop);
    texturedVertex(innerLeft, innerBottom);
    texturedVertex(px,        innerBottom);

    glEnd();

    // -------------------------------------------------
    // Правая полоса
    // -------------------------------------------------
    glBegin(GL_QUADS);

    texturedVertex(innerRight, innerTop);
    texturedVertex(px + pw,   innerTop);
    texturedVertex(px + pw,   innerBottom);
    texturedVertex(innerRight, innerBottom);

    glEnd();

    // -------------------------------------------------
    // Скруглённые углы
    // -------------------------------------------------
    const int segments = 16;

    auto drawCorner =
        [&](float cx, float cy, float startAngle)
    {
        glBegin(GL_TRIANGLE_FAN);

        texturedVertex(cx, cy);

        for (int i = 0; i <= segments; ++i)
        {
            const float a =
                startAngle +
                glm::half_pi<float>() *
                static_cast<float>(i) /
                static_cast<float>(segments);

            const float x =
                cx + std::cos(a) * radius;

            const float y =
                cy + std::sin(a) * radius;

            texturedVertex(x, y);
        }

        glEnd();
    };

    drawCorner(innerLeft,  innerTop,    glm::pi<float>());
    drawCorner(innerRight, innerTop,   -glm::half_pi<float>());
    drawCorner(innerRight, innerBottom, 0.0f);
    drawCorner(innerLeft,  innerBottom, glm::half_pi<float>());

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // -------------------------------------------------
    // Рамка
    // -------------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor3f(
        borderColor.r,
        borderColor.g,
        borderColor.b
    );

    glLineWidth(borderThickness);

    glBegin(GL_LINE_LOOP);

    auto drawBorderCorner =
        [&](float cx, float cy, float startAngle)
    {
        for (int i = 0; i <= segments; ++i)
        {
            const float a =
                startAngle +
                glm::half_pi<float>() *
                static_cast<float>(i) /
                static_cast<float>(segments);

            const float x =
                cx + std::cos(a) * radius;

            const float y =
                cy + std::sin(a) * radius;

            glVertex2f(x, y);
        }
    };

    drawBorderCorner(innerRight, innerTop,    -glm::half_pi<float>());
    drawBorderCorner(innerRight, innerBottom,  0.0f);
    drawBorderCorner(innerLeft,  innerBottom,  glm::half_pi<float>());
    drawBorderCorner(innerLeft,  innerTop,     glm::pi<float>());

    glEnd();

    renderChildren(vp, localX, localY, pw, ph);
}






void UICameraView::initFBO(int width, int height)
{
    fboWidth = width;
    fboHeight = height;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           colorTex,
                           0);

    glGenRenderbuffers(1, &depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER,
                          GL_DEPTH24_STENCIL8,
                          width, height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              depthRb);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("UICameraView FBO not complete!\n");
        
    glColor4f(1,1,1,1);    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}








void UICameraView::renderToTexture(
    const Viewport& vp,
    std::function<void(const glm::mat4&, const glm::mat4&)> drawScene)
{
    if (!camera) return;

    ensureFBO(vp);

    if (fbo == 0) return;

    // Сохраняем состояние

GLint oldFBO;
glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);

GLboolean oldScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
GLint oldScissorBox[4] = {0, 0, 0, 0};
glGetIntegerv(GL_SCISSOR_BOX, oldScissorBox);

glPushAttrib(GL_VIEWPORT_BIT | GL_TRANSFORM_BIT);
   
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    // Сохраняем состояние


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(0, 0, fboWidth, fboHeight);

    // ВАЖНО:
    // Application держит GL_SCISSOR_TEST для letterbox.
    // При рендере в FBO этот scissor становится неверным и режет текстуру.
    glDisable(GL_SCISSOR_TEST);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (fboHeight <= 0) return;
    float aspect = (float)fboWidth / (float)fboHeight;
    camera->setAspect(aspect);

    glm::mat4 proj = camera->projectionMatrix();
    glm::mat4 view = camera->viewMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    drawScene(view, proj);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(vp.x, vp.y, vp.width, vp.height);
    glEnable(GL_BLEND);


    // Восстанавливаем состояние
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();

    if (oldScissorEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            oldScissorBox[0],
            oldScissorBox[1],
            oldScissorBox[2],
            oldScissorBox[3]
        );
    }
    else
    {
        glDisable(GL_SCISSOR_TEST);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
}



void UICameraView::ensureFBO(const Viewport& vp)
{
    int desiredW = static_cast<int>(size.x * vp.width);
    int desiredH = static_cast<int>(size.y * vp.height);

    if (desiredW <= 0 || desiredH <= 0)
        return;

    // если FBO уже создан и размер совпадает — ничего не делаем
    if (fbo != 0 && desiredW == fboWidth && desiredH == fboHeight)
        return;

    // если был старый — удаляем
    if (fbo != 0)
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &colorTex);
        glDeleteRenderbuffers(1, &depthRb);

        fbo = 0;
        colorTex = 0;
        depthRb = 0;
    }

    initFBO(desiredW, desiredH);
}
