#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/game/presentation/ClientHudPresentation.h"
#include "src/render/Font.h"
#include "src/render/types/Viewport.h"

namespace render::cockpit
{

class FlightVectorIndicatorRenderer
{
public:
    ~FlightVectorIndicatorRenderer();

    void init();

    void render(
        const game::presentation::FlightVectorIndicatorPresentation& presentation,
        const Viewport& viewport
    );

private:
    struct Vertex
    {
        glm::vec2 posNdc {0.0f};
        glm::vec4 color {1.0f};
    };

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    std::vector<Vertex> m_vertices;

    std::unique_ptr<Font> m_primaryFont;
    std::unique_ptr<Font> m_secondaryFont;
    std::string m_fontPath;
    int m_primaryPixelSize = 0;
    int m_secondaryPixelSize = 0;

    int m_screenW = 1;
    int m_screenH = 1;

    glm::vec2 pxToNdc(const glm::vec2& p) const;

    void beginBatch();
    void flushBatch();

    void emitTriangle(
        const glm::vec2& a,
        const glm::vec2& b,
        const glm::vec2& c,
        const glm::vec4& color
    );

    void emitLine(
        const glm::vec2& a,
        const glm::vec2& b,
        float thickness,
        const glm::vec4& color
    );

    void emitCircle(
        const glm::vec2& center,
        float radius,
        float thickness,
        const glm::vec4& color,
        int segments = 64
    );

    void ensureFonts(
        const std::string& fontPath,
        int primaryPixelSize,
        int secondaryPixelSize
    );

    void drawCenteredText(
        Font& font,
        const std::string& text,
        float centerX,
        float baselineY,
        const glm::vec3& color
    );

    void emitShipGlyph(
        const game::presentation::FlightVectorIndicatorPresentation& presentation,
        const glm::vec2& center,
        float scale,
        const glm::vec4& color
    );
};

} // namespace render::cockpit
