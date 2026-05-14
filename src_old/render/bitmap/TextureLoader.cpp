#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "TextureLoader.h"
#include <glad/gl.h>
#include <iostream>

unsigned int TextureLoader::load2D(const std::string& path, bool useSRGB)
{
    int width, height, channels;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // GLenum internalFormat = useSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    GLenum internalFormat = GL_RGBA8;

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 internalFormat,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);

    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}
