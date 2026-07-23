#include "src/render/navigation/NavigationCellLabelLayer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

#include "render/Font.h"
#include "render/HUD/TextRenderer.h"

namespace render::navigation
{
namespace
{
constexpr float Pi = 3.14159265358979323846f;

bool projectPoint(
    const glm::vec3& point,
    const glm::mat4& mvp,
    const Viewport& viewport,
    glm::vec2& screen
)
{
    const glm::vec4 clip = mvp * glm::vec4(point, 1.0f);
    if (clip.w <= 0.00001f)
        return false;

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    if (ndc.z < -1.0f || ndc.z > 1.0f)
        return false;

    screen = {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(viewport.width),
        (1.0f - (ndc.y * 0.5f + 0.5f)) *
            static_cast<float>(viewport.height)
    };

    return true;
}

void popLastUtf8Codepoint(std::string& value)
{
    if (value.empty())
        return;

    std::size_t start = value.size() - 1;

    while (start > 0 &&
           (static_cast<unsigned char>(value[start]) & 0xC0u) == 0x80u)
    {
        --start;
    }

    value.erase(start);
}

std::string fitUtf8(
    Font& font,
    const std::string& source,
    float scale,
    float maxWidth
)
{
    if (source.empty() || maxWidth <= 0.0f)
        return {};

    if (font.measureText(source) * scale <= maxWidth)
        return source;

    const std::string ellipsis = "…";
    const float ellipsisWidth = font.measureText(ellipsis) * scale;

    if (ellipsisWidth > maxWidth)
        return {};

    std::string result = source;

    while (!result.empty())
    {
        popLastUtf8Codepoint(result);

        if (font.measureText(result) * scale + ellipsisWidth <= maxWidth)
            return result + ellipsis;
    }

    return {};
}

float exponentialStep(float current, float target, float dt, float seconds)
{
    const float safeSeconds = std::max(seconds, 0.001f);
    const float factor = 1.0f - std::exp(-dt / safeSeconds);
    return current + (target - current) * factor;
}
}

struct NavigationCellLabelLayer::Face
{
    glm::vec3 center {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec3 u {1.0f, 0.0f, 0.0f};
    glm::vec3 v {0.0f, 0.0f, 1.0f};
};

NavigationCellLabelLayer::NavigationCellLabelLayer() = default;
NavigationCellLabelLayer::~NavigationCellLabelLayer() = default;

void NavigationCellLabelLayer::clear()
{
    m_states.clear();
    m_lastTimeSeconds = 0.0;
}

void NavigationCellLabelLayer::ensureFont()
{
    if (m_font)
        return;

    const std::vector<std::string> candidates =
    {
        "assets/fonts/NotoSansCJK-Regular.otf",
        "src/assets/fonts/NotoSansCJK-Regular.otf",
        "assets/fonts/Roboto-Medium.ttf",
        "src/assets/fonts/Roboto-Medium.ttf"
    };

    for (const std::string& path : candidates)
    {
        if (std::filesystem::exists(path))
        {
            m_font = std::make_unique<Font>(path, 16);
            return;
        }
    }
}

std::string NavigationCellLabelLayer::stableKey(
    const game::navigation::NavigationCellId& id
)
{
    std::ostringstream stream;
    stream
        << id.frameId << '|'
        << id.level << '|'
        << id.x << '|'
        << id.y << '|'
        << id.z;
    return stream.str();
}

std::vector<NavigationCellLabelLayer::Face>
NavigationCellLabelLayer::facesFor(
    const NavigationCellLabelRequest& r
)
{
    const glm::vec3 nx = glm::normalize(r.halfAxisX);
    const glm::vec3 ny = glm::normalize(r.halfAxisY);
    const glm::vec3 nz = glm::normalize(r.halfAxisZ);

    return {
        {r.center + r.halfAxisX,  nx,  r.halfAxisZ, r.halfAxisY},
        {r.center - r.halfAxisX, -nx, -r.halfAxisZ, r.halfAxisY},
        {r.center + r.halfAxisY,  ny,  r.halfAxisX, r.halfAxisZ},
        {r.center - r.halfAxisY, -ny,  r.halfAxisX, -r.halfAxisZ},
        {r.center + r.halfAxisZ,  nz, -r.halfAxisX, r.halfAxisY},
        {r.center - r.halfAxisZ, -nz,  r.halfAxisX, r.halfAxisY}
    };
}

void NavigationCellLabelLayer::updateFace(
    LabelState& state,
    const std::vector<Face>& faces,
    const glm::mat4& mvp,
    const glm::vec3& cameraPosition,
    const Viewport& viewport,
    float dt
)
{




   
    
    auto scoreFace =
    [&](const Face& face) -> float
    {
        const glm::vec3 toCamera =
            glm::normalize(
                cameraPosition -
                face.center
            );

        const float facing =
            glm::dot(
                face.normal,
                toCamera
            );

        /*
            Обратную грань не используем.
        */
        if (facing <= 0.02f)
            return -1000.0f;

        glm::vec2 projectedCenter;

        if (!projectPoint(
                face.center,
                mvp,
                viewport,
                projectedCenter
            ))
        {
            return -1000.0f;
        }

        /*
            Грань выбирается только по направлению к камере.
            Размер и положение текста на экране больше не заставляют
            его постоянно перескакивать между соседними гранями.
        */
        return facing;
    };









    int bestFace = -1;
    float bestScore = -1000.0f;

    for (int i = 0; i < static_cast<int>(faces.size()); ++i)
    {
        const float score = scoreFace(faces[i]);
        if (score > bestScore)
        {
            bestScore = score;
            bestFace = i;
        }
    }

    if (state.currentFace < 0)
    {
        state.currentFace = bestFace;
        state.previousFace = -1;
        state.faceBlend = 1.0f;
    }
    else if (bestFace >= 0 && bestFace != state.currentFace)
    {
        const float currentScore = scoreFace(faces[state.currentFace]);

        if (currentScore < 0.08f ||
            bestScore > currentScore + 0.22f)
        {
            state.previousFace = state.currentFace;
            state.currentFace = bestFace;
            state.faceBlend = 0.0f;
        }
    }

    if (state.faceBlend < 1.0f)
    {
        state.faceBlend = std::min(
            1.0f,
            state.faceBlend + dt / 0.26f
        );

        if (state.faceBlend >= 1.0f)
            state.previousFace = -1;
    }
}

void NavigationCellLabelLayer::drawFaceLabel(
    const LabelState& state,
    const Face& face,
    const glm::mat4& mvp,
    const Viewport& viewport,
    const glm::vec3& cameraPosition,
    float alpha
)
{
    if (!m_font || alpha <= 0.005f)
        return;

    const glm::vec3 toCamera = glm::normalize(
        cameraPosition - face.center
    );

    const float facing = glm::dot(face.normal, toCamera);
    const float faceOpacity = glm::smoothstep(0.04f, 0.32f, facing);
    alpha *= faceOpacity;

    if (alpha <= 0.005f)
        return;

    
        


    glm::vec2 left;
    glm::vec2 right;
    glm::vec2 faceCenter;

    glm::vec2 positiveEdgeCenter;
    glm::vec2 negativeEdgeCenter;

    /*
        Сначала проецируем центр грани и центры двух
        противоположных рёбер.
    */
    if (!projectPoint(
            face.center,
            mvp,
            viewport,
            faceCenter
        ) ||
        !projectPoint(
            face.center + face.v,
            mvp,
            viewport,
            positiveEdgeCenter
        ) ||
        !projectPoint(
            face.center - face.v,
            mvp,
            viewport,
            negativeEdgeCenter
        ))
    {
        return;
    }

    /*
        В экранных координатах верхнее ребро имеет меньшее Y.
    */
    const glm::vec3 upperEdgeCenter =
        positiveEdgeCenter.y <= negativeEdgeCenter.y
            ? face.center + face.v
            : face.center - face.v;

    /*
        Берём точки вдоль верхнего ребра.
        Коэффициент 0.82 оставляет небольшой отступ от углов куба.
    */
    if (!projectPoint(
            upperEdgeCenter -
                face.u * 0.82f,
            mvp,
            viewport,
            left
        ) ||
        !projectPoint(
            upperEdgeCenter +
                face.u * 0.82f,
            mvp,
            viewport,
            right
        ))
    {
        return;
    }















    glm::vec2 direction = right - left;
    float availableWidth = glm::length(direction);

    if (availableWidth < 48.0f)
        return;

    direction /= availableWidth;

    if (direction.x < 0.0f)
    {
        std::swap(left, right);
        direction = -direction;
    }

    glm::vec2 inward(-direction.y, direction.x);

    const glm::vec2 midpoint = (left + right) * 0.5f;
    if (glm::dot(faceCenter - midpoint, inward) < 0.0f)
        inward = -inward;

    const float screenScale = std::clamp(
        static_cast<float>(viewport.height) / 1080.0f,
        0.72f,
        1.35f
    );

    const float titleScale = 0.72f * screenScale;
    const float subtitleScale = 0.62f * screenScale;
    const float inset = 8.0f * screenScale;

    availableWidth = std::max(0.0f, availableWidth - inset * 2.0f);
   
    

    /*
        Расстояние от верхнего ребра внутрь грани.
        Это главный параметр вертикального положения подписи.
    */
    const float edgeTextInsetPx =
        42.0f;

    const glm::vec2 origin =
        left +
        direction * inset +
        inward *
            (edgeTextInsetPx * screenScale);




    const float angle = std::atan2(direction.y, direction.x);

    const std::string coordinate = fitUtf8(
        *m_font,
        state.request.coordinateText,
        titleScale,
        availableWidth
    );

    std::string region = state.request.regionName;

    if (m_font->measureText(region) * subtitleScale > availableWidth &&
        !state.request.shortRegionName.empty())
    {
        region = state.request.shortRegionName;
    }

    region = fitUtf8(
        *m_font,
        region,
        subtitleScale,
        availableWidth
    );

    const glm::vec4 titleColor(
        state.request.color.r,
        state.request.color.g,
        state.request.color.b,
        state.request.color.a * alpha
    );

    const glm::vec4 subtitleColor(
        state.request.color.r,
        state.request.color.g,
        state.request.color.b,
        state.request.color.a * alpha * 0.72f
    );

    TextRenderer& text = TextRenderer::instance();

    if (!region.empty())
    {
        const glm::vec2 regionLine =
            coordinate.empty()
                ? origin
                : origin +
                    inward *
                    (15.0f * screenScale);

        text.textDrawRotated(
            *m_font,
            region,
            regionLine.x,
            regionLine.y,
            subtitleColor,
            subtitleScale,
            angle
        );
    }

    

    if (!region.empty())
    {
        const glm::vec2 secondLine =
            origin + inward * (15.0f * screenScale);

        text.textDrawRotated(
            *m_font,
            region,
            secondLine.x,
            secondLine.y,
            subtitleColor,
            subtitleScale,
            angle
        );
    }
}

void NavigationCellLabelLayer::draw(
    const Viewport& viewport,
    const glm::mat4& mvp,
    const glm::vec3& cameraPosition,
    const std::vector<NavigationCellLabelRequest>& requests,
    double nowSeconds
)
{
    ensureFont();
    if (!m_font)
        return;

    const float dt = m_lastTimeSeconds > 0.0
        ? static_cast<float>(
            std::clamp(nowSeconds - m_lastTimeSeconds, 0.0, 0.1)
        )
        : 0.0f;

    m_lastTimeSeconds = nowSeconds;

    for (auto& item : m_states)
    {
        item.second.seen = false;
        item.second.targetOpacity = 0.0f;
    }


    

    for (const NavigationCellLabelRequest& request : requests)
    {
        /*
            visualKey используется для постоянных визуальных ролей:
            selected и hovered.

            Если visualKey не задан, используется полный адрес куба.
        */
        const std::string key =
            request.visualKey.empty()
                ? stableKey(request.id)
                : request.visualKey;

        LabelState& state =
            m_states[key];

        state.request =
            request;

        state.seen =
            true;

        state.targetOpacity =
            request.targetOpacity;
    }








    TextRenderer& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    for (auto& item : m_states)
    {
        LabelState& state = item.second;

        const float fadeSeconds =
            state.targetOpacity > state.opacity ? 0.18f : 0.24f;

        state.opacity = exponentialStep(
            state.opacity,
            state.targetOpacity,
            dt,
            fadeSeconds
        );

        const std::vector<Face> faces = facesFor(state.request);

        updateFace(
            state,
            faces,
            mvp,
            cameraPosition,
            viewport,
            dt
        );

        if (state.previousFace >= 0)
        {
            drawFaceLabel(
                state,
                faces[state.previousFace],
                mvp,
                viewport,
                cameraPosition,
                state.opacity * (1.0f - state.faceBlend)
            );
        }

        if (state.currentFace >= 0)
        {
            drawFaceLabel(
                state,
                faces[state.currentFace],
                mvp,
                viewport,
                cameraPosition,
                state.opacity * state.faceBlend
            );
        }
    }

    text.endFrame();

    for (auto it = m_states.begin(); it != m_states.end();)
    {
        if (!it->second.seen && it->second.opacity < 0.01f)
            it = m_states.erase(it);
        else
            ++it;
    }
}

} // namespace render::navigation
