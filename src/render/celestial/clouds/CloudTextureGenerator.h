#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::celestial
{
    enum class CloudPattern
    {
        ScatteredCumulus,
        BrokenFields,
        DenseOvercast
    };

    struct CloudGenerationStyle
    {
        CloudPattern pattern =
            CloudPattern::BrokenFields;

        std::uint32_t seed = 1337u;

        int textureWidth = 1024;
        int textureHeight = 512;

        float coverageThreshold = 0.56f;
        float density = 0.82f;
        float edgeSharpness = 0.18f;
        float detailStrength = 0.34f;

        float macroScale = 2.2f;
        float cellScale = 12.0f;
        float detailScale = 38.0f;

        glm::vec3 cloudColor {
            0.96f,
            0.985f,
            1.0f
        };

        glm::vec3 shadowColor {
            0.56f,
            0.66f,
            0.76f
        };

        bool enabled = true;
    };

    class CloudTextureGenerator
    {
    public:
        ~CloudTextureGenerator();

        GLuint textureForStyle(
            const CloudGenerationStyle& style
        );

        void clearCache();

    private:
        static float smoothStep(
            float edge0,
            float edge1,
            float value
        );

        static float cloudDensityAt(
            float u,
            float v,
            const CloudGenerationStyle& style
        );

        static std::string keyForStyle(
            const CloudGenerationStyle& style
        );

        GLuint buildTexture(
            const CloudGenerationStyle& style
        );

    private:
        std::unordered_map<std::string, GLuint> m_textureByKey;
    };
}