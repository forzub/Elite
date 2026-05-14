#include "RadarRenderTarget.h"
#include <cstdio>

// RadarRenderTarget::RadarRenderTarget()
// {
// }



RadarRenderTarget::RadarRenderTarget()
    : m_fbo(0)
    , m_colorTex(0)
    , m_depthRb(0)
    , m_width(0)
    , m_height(0)
    , m_previousFBO(0)
{
    m_previousViewport[0] = 0;
    m_previousViewport[1] = 0;
    m_previousViewport[2] = 0;
    m_previousViewport[3] = 0;
}




RadarRenderTarget::~RadarRenderTarget()
{
    destroy();
}

void RadarRenderTarget::destroy()
{
    if (m_depthRb)
        glDeleteRenderbuffers(1, &m_depthRb);

    if (m_colorTex)
        glDeleteTextures(1, &m_colorTex);

    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);

    m_depthRb = 0;
    m_colorTex = 0;
    m_fbo = 0;
}

void RadarRenderTarget::ensureSize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    if (m_fbo && width == m_width && height == m_height)
        return;

    destroy();
    create(width, height);
}




void RadarRenderTarget::create(int width, int height)
{
    // Сохраняем размеры
    m_width = width;
    m_height = height;

    // Сохраняем текущий FBO, чтобы восстановить потом
    GLint previousFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

    // --- 1. Создаём FBO ---
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // --- 2. Создаём цветовую текстуру (RGBA8) ---
    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);

    // Выделяем память под текстуру (пустая, без данных)
    glTexImage2D(
        GL_TEXTURE_2D,          // target
        0,                      // level (mipmap level)
        GL_RGBA8,               // internal format (8 бит на канал)
        width,                  // width
        height,                 // height
        0,                      // border (должен быть 0)
        GL_RGBA,                // format (формат входных данных)
        GL_UNSIGNED_BYTE,        // type (тип входных данных)
        nullptr                  // data (NULL - пустая текстура)
    );

    // Настройка параметров текстуры для правильного отображения
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Важно: режим CLAMP_TO_EDGE предотвращает появление артефактов по краям
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Прикрепляем текстуру к FBO как цветовой буфер
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,                 // target
        GL_COLOR_ATTACHMENT0,           // attachment (цветовой буфер)
        GL_TEXTURE_2D,                  // textarget
        m_colorTex,                      // texture
        0                                // level
    );

    // --- 3. Создаём Renderbuffer для глубины и трафарета ---
    glGenRenderbuffers(1, &m_depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRb);

    // Выделяем память под depth+stencil buffer
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,    // internal format (24 бита глубина + 8 бит трафарет)
        width,
        height
    );

    // Прикрепляем renderbuffer к FBO
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,    // attachment (depth+stencil)
        GL_RENDERBUFFER,
        m_depthRb
    );

    // --- 4. Проверяем, что FBO полный и корректный ---
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        const char* statusStr = "unknown";
        switch (status) {
            case GL_FRAMEBUFFER_UNDEFINED: statusStr = "GL_FRAMEBUFFER_UNDEFINED"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"; break;
            case GL_FRAMEBUFFER_UNSUPPORTED: statusStr = "GL_FRAMEBUFFER_UNSUPPORTED"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: statusStr = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"; break;
        }
        printf("RadarRenderTarget: FBO not complete! Status: 0x%X (%s)\n", status, statusStr);
    }
    else
    {
        printf("RadarRenderTarget: FBO created successfully %dx%d\n", width, height);
    }

    // --- 5. Очищаем FBO прозрачным цветом ---
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // --- 6. Восстанавливаем предыдущий FBO ---
    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    
    // Отвязываем текстуру и renderbuffer для безопасности
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}





void RadarRenderTarget::bind()
{
    if (m_fbo == 0) {
        printf("RadarRenderTarget: Attempting to bind invalid FBO!\n");
        return;
    }

    // Сохраняем текущий viewport для восстановления
    GLint currentViewport[4];
    glGetIntegerv(GL_VIEWPORT, currentViewport);
    m_previousViewport[0] = currentViewport[0];
    m_previousViewport[1] = currentViewport[1];
    m_previousViewport[2] = currentViewport[2];
    m_previousViewport[3] = currentViewport[3];

    // Сохраняем текущий FBO для восстановления
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_previousFBO);

    // Биндим наш FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    // Устанавливаем viewport на весь размер FBO
    glViewport(0, 0, m_width, m_height);
    
    // Очищаем с прозрачным фоном
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}





void RadarRenderTarget::unbind()
{
    // Восстанавливаем предыдущий FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_previousFBO);
    
    // Восстанавливаем предыдущий viewport
    glViewport(m_previousViewport[0], m_previousViewport[1], 
               m_previousViewport[2], m_previousViewport[3]);
}



