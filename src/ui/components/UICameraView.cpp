#include "UICameraView.h"
#include <glad/gl.h>
#include <algorithm>



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

    float px = parentX + position.x * parentW;
    float py = parentY + position.y * parentH;
    float pw = size.x * parentW;
    float ph = size.y * parentH;

    float minSide = std::min(pw, ph);

    float radius = cornerRadiusRel * minSide;
    float borderThickness = borderThicknessRel * minSide;

    radius = std::clamp(radius, 0.0f, minSide * 0.5f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColor4f(1,1,1,1);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, colorTex);

    const int segments = 16;

    // ---- Рисуем скругленный quad ----
    glBegin(GL_TRIANGLE_FAN);

    glTexCoord2f(0.5f,0.5f);
    glVertex2f(px + pw*0.5f, py + ph*0.5f);

    auto drawCorner = [&](float cx, float cy, float startAngle)
    {
        for (int i=0; i<=segments; ++i)
        {
            float a = startAngle + (glm::half_pi<float>() * i / segments);
            float x = cx + cos(a) * radius;
            float y = cy + sin(a) * radius;

            float tx = (x - px) / pw;
            float ty = 1.0f - (y - py) / ph;

            glTexCoord2f(tx, ty);
            glVertex2f(x, y);
        }
    };

    drawCorner(px+pw-radius, py+radius,         -glm::half_pi<float>());
    drawCorner(px+pw-radius, py+ph-radius,       0.0f);
    drawCorner(px+radius,    py+ph-radius,       glm::half_pi<float>());
    drawCorner(px+radius,    py+radius,          glm::pi<float>());

    glEnd();

    glDisable(GL_TEXTURE_2D);

    // ---- Рамка ----
    glColor3f(borderColor.r, borderColor.g, borderColor.b);
    glLineWidth(borderThickness);

    glBegin(GL_LINE_LOOP);

    auto drawBorderCorner = [&](float cx, float cy, float startAngle)
    {
        for (int i=0; i<=segments; ++i)
        {
            float a = startAngle + (glm::half_pi<float>() * i / segments);
            float x = cx + cos(a) * radius;
            float y = cy + sin(a) * radius;
            glVertex2f(x, y);
        }
    };

    drawBorderCorner(px+pw-radius, py+radius,         -glm::half_pi<float>());
    drawBorderCorner(px+pw-radius, py+ph-radius,       0.0f);
    drawBorderCorner(px+radius,    py+ph-radius,       glm::half_pi<float>());
    drawBorderCorner(px+radius,    py+radius,          glm::pi<float>());

    glEnd();

    glEnable(GL_BLEND);

    renderChildren(vp, px, py, pw, ph);
}
// void UICameraView::render(
//     const Viewport& vp,
//     float parentX,
//     float parentY,
//     float parentW,
//     float parentH
// )
// {
//     if (!visible)
//         return;

//     float px = parentX + position.x * parentW;
//     float py = parentY + position.y * parentH;
//     float pw = size.x * parentW;
//     float ph = size.y * parentH;

//     glDisable(GL_DEPTH_TEST);
//     glDisable(GL_BLEND);
//     glColor4f(1,1,1,1);

//     glEnable(GL_TEXTURE_2D);
//     glBindTexture(GL_TEXTURE_2D, colorTex);

//     glBegin(GL_QUADS);
//         glTexCoord2f(0,1); glVertex2f(px,py);
//         glTexCoord2f(1,1); glVertex2f(px+pw,py);
//         glTexCoord2f(1,0); glVertex2f(px+pw,py+ph);
//         glTexCoord2f(0,0); glVertex2f(px,py+ph);
//     glEnd();

//     glDisable(GL_TEXTURE_2D);

//     // рамка
//     glColor3f(borderColor.r, borderColor.g, borderColor.b);
//     glLineWidth(borderThickness);

//     glBegin(GL_LINE_LOOP);
//         glVertex2f(px, py);
//         glVertex2f(px + pw, py);
//         glVertex2f(px + pw, py + ph);
//         glVertex2f(px, py + ph);
//     glEnd();

//     glEnable(GL_BLEND);

//     renderChildren(vp, px, py, pw, ph);
// }







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

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(0, 0, fboWidth, fboHeight);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    glViewport(0, 0, vp.width, vp.height);

    glEnable(GL_BLEND);   // возвращаем глобальный режим
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
