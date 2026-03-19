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
    glColor4f(1, 1, 1, 1);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, colorTex);

    const int segments = 16;

    // ---- Рисуем скругленный quad как 4 отдельных угла + центр ----
    // Внутренний прямоугольник (без скруглений)
    float innerLeft = px + radius;
    float innerRight = px + pw - radius;
    float innerTop = py + radius;
    float innerBottom = py + ph - radius;
    
    // Центральная часть (прямоугольник)
    glBegin(GL_QUADS);
    // Левый верхний внутренний угол
    glTexCoord2f(radius/pw, radius/ph);
    glVertex2f(innerLeft, innerTop);
    
    glTexCoord2f((pw-radius)/pw, radius/ph);
    glVertex2f(innerRight, innerTop);
    
    glTexCoord2f((pw-radius)/pw, (ph-radius)/ph);
    glVertex2f(innerRight, innerBottom);
    
    glTexCoord2f(radius/pw, (ph-radius)/ph);
    glVertex2f(innerLeft, innerBottom);
    glEnd();
    
    // Верхняя полоса
    glBegin(GL_QUADS);
    glTexCoord2f(radius/pw, 0);
    glVertex2f(innerLeft, py);
    
    glTexCoord2f((pw-radius)/pw, 0);
    glVertex2f(innerRight, py);
    
    glTexCoord2f((pw-radius)/pw, radius/ph);
    glVertex2f(innerRight, innerTop);
    
    glTexCoord2f(radius/pw, radius/ph);
    glVertex2f(innerLeft, innerTop);
    glEnd();
    
    // Нижняя полоса
    glBegin(GL_QUADS);
    glTexCoord2f(radius/pw, (ph-radius)/ph);
    glVertex2f(innerLeft, innerBottom);
    
    glTexCoord2f((pw-radius)/pw, (ph-radius)/ph);
    glVertex2f(innerRight, innerBottom);
    
    glTexCoord2f((pw-radius)/pw, 1);
    glVertex2f(innerRight, py + ph);
    
    glTexCoord2f(radius/pw, 1);
    glVertex2f(innerLeft, py + ph);
    glEnd();
    
    // Левая полоса
    glBegin(GL_QUADS);
    glTexCoord2f(0, radius/ph);
    glVertex2f(px, innerTop);
    
    glTexCoord2f(radius/pw, radius/ph);
    glVertex2f(innerLeft, innerTop);
    
    glTexCoord2f(radius/pw, (ph-radius)/ph);
    glVertex2f(innerLeft, innerBottom);
    
    glTexCoord2f(0, (ph-radius)/ph);
    glVertex2f(px, innerBottom);
    glEnd();
    
    // Правая полоса
    glBegin(GL_QUADS);
    glTexCoord2f((pw-radius)/pw, radius/ph);
    glVertex2f(innerRight, innerTop);
    
    glTexCoord2f(1, radius/ph);
    glVertex2f(px + pw, innerTop);
    
    glTexCoord2f(1, (ph-radius)/ph);
    glVertex2f(px + pw, innerBottom);
    
    glTexCoord2f((pw-radius)/pw, (ph-radius)/ph);
    glVertex2f(innerRight, innerBottom);
    glEnd();
    
    // 4 скругленных угла (как треугольные вееры)
    auto drawCorner = [&](float cx, float cy, float startAngle, float texCX, float texCY)
    {
        glBegin(GL_TRIANGLE_FAN);
        // Центр угла
        glTexCoord2f(texCX, texCY);
        glVertex2f(cx, cy);
        
        for (int i = 0; i <= segments; ++i)
        {
            float a = startAngle + (glm::half_pi<float>() * i / segments);
            float x = cx + cos(a) * radius;
            float y = cy + sin(a) * radius;
            
            // Текстурные координаты для точки на дуге
            float tx = (x - px) / pw;
            float ty = 1.0f - (y - py) / ph;  // Инвертируем Y для OpenGL
            glTexCoord2f(tx, ty);
            glVertex2f(x, y);
        }
        glEnd();
    };
    
    // Левый верхний угол
    drawCorner(innerLeft, innerTop, glm::pi<float>(), 
               radius/pw, radius/ph);
    
    // Правый верхний угол
    drawCorner(innerRight, innerTop, -glm::half_pi<float>(), 
               (pw-radius)/pw, radius/ph);
    
    // Правый нижний угол
    drawCorner(innerRight, innerBottom, 0.0f, 
               (pw-radius)/pw, (ph-radius)/ph);
    
    // Левый нижний угол
    drawCorner(innerLeft, innerBottom, glm::half_pi<float>(), 
               radius/pw, (ph-radius)/ph);

    glDisable(GL_TEXTURE_2D);

    // ---- Рамка (без изменений) ----
    glColor3f(borderColor.r, borderColor.g, borderColor.b);
    glLineWidth(borderThickness);

    glBegin(GL_LINE_LOOP);

    auto drawBorderCorner = [&](float cx, float cy, float startAngle)
    {
        for (int i = 0; i <= segments; ++i)
        {
            float a = startAngle + (glm::half_pi<float>() * i / segments);
            float x = cx + cos(a) * radius;
            float y = cy + sin(a) * radius;
            glVertex2f(x, y);
        }
    };

    drawBorderCorner(innerRight, innerTop, -glm::half_pi<float>());
    drawBorderCorner(innerRight, innerBottom, 0.0f);
    drawBorderCorner(innerLeft, innerBottom, glm::half_pi<float>());
    drawBorderCorner(innerLeft, innerTop, glm::pi<float>());

    glEnd();

    glEnable(GL_BLEND);
    renderChildren(vp, px, py, pw, ph);
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
    glPushAttrib(GL_VIEWPORT_BIT | GL_TRANSFORM_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    // Сохраняем состояние


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(0, 0, fboWidth, fboHeight);

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

    glViewport(0, 0, vp.width, vp.height);

    glEnable(GL_BLEND);   // возвращаем глобальный режим


    // Восстанавливаем состояние
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    
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
