#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Font.h"
#include "render/types/Viewport.h"

class TextRenderer
{
public:
    static TextRenderer& instance();

    // HantedPreview (the author 2026-11-27: "remove every text from the HUD except the HUD terminal"): while
    // suppressed every draw call is dropped - the HUD sets it around the glass texts it hides; the data and the
    // layout code still run, nothing else changes
    void setSuppressed(bool suppressed) noexcept { m_suppressed = suppressed; }
    // the last-chance translation of a name AS IT COMES FROM THE GAME DATA (audit #9 D7): a text with Cyrillic in it is
    // looked up by Application's filter (LocalizationService::dataText, whole string, then ", "-separated parts); set
    // once by Application - the render layer knows nothing of localization. Applied to drawing AND measuring alike.
    using TextFilter = std::string (*)(const std::string&);
    static void setTextFilter(TextFilter filter) noexcept { textFilterSlot() = filter; }
    static const std::string& filterText(const std::string& in, std::string& storage)
    {
        const TextFilter f = textFilterSlot();
        if (!f)
            return in;
        bool cyrillic = false;
        for (unsigned char c : in)
            if (c == 0xD0 || c == 0xD1)
            {
                cyrillic = true;
                break;
            }
        if (!cyrillic)
            return in;
        storage = f(in);
        return storage;
    }
    static TextFilter& textFilterSlot() noexcept
    {
        static TextFilter slot = nullptr;
        return slot;
    }
    bool suppressed() const noexcept { return m_suppressed; }

    void init();

    void beginFrame();
    void beginFrameForViewport(
        int width,
        int height
    );
    void endFrame();

    void textDrawBatched(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color
    );

    void textDraw(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color
    );

    void textDraw(
        Font& font,
        const std::string& text,
        float x,
        float y,
        const glm::vec4& color,
        float scale
    );



    /*
    Screen-space text rotated around its baseline origin.
    angleRad uses screen coordinates: positive angle rotates clockwise.
    */
    void textDrawRotated(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color,
        float scale,
        float angleRad
    );




    inline void textDraw(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec3& color
    )
    {
        textDraw(font, text, x, baselineY, glm::vec4(color, 1.0f));
    }




    void textDraw(
        const std::string& text,
        float x,
        float y,
        float scale,
        const glm::vec3& color
    );

    void textDraw(
        const std::string& text,
        float x,
        float y,
        float scale,
        const glm::vec4& color
    );







    void textDrawPx(
        const std::string& text,
        float x,
        float y,
        float pixelHeight,
        const glm::vec4& color
    );  

    void textDrawPx(
        const std::string& text,
        float x,
        float y,
        int pixelSize,
        const glm::vec4& color
    );

    float measureTextPx(
        const std::string& text,
        int pixelSize
    );

    void solidRectPx(
        float x,
        float y,
        float width,
        float height,
        const glm::vec4& color
    );

private:
    TextRenderer() = default;

private:
    struct TextVertex
    {
        float x;
        float y;
        float u;
        float v;

        float r;
        float g;
        float b;
        float a;
    };

    struct TextBatch
    {
        unsigned int textureID = 0;
        std::vector<TextVertex> vertices;
    };

private:
    TextBatch& batchForTexture(unsigned int textureID);

private:
    unsigned int m_vao    = 0;
    unsigned int m_vbo    = 0;
    unsigned int m_shader = 0;
    unsigned int m_solidTexture = 0;

    bool  m_batchActive = false;
public:
    // the pixel space the *Px calls draw in (a hit test must use the same one)
    float screenWidthPx() const { return m_screenW; }
    float screenHeightPx() const { return m_screenH; }

private:
    float m_screenW = 1.0f;
    float m_screenH = 1.0f;

    int m_locTextSampler = -1;

    std::vector<TextBatch> m_batches;
    bool m_suppressed = false;
};