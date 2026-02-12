#pragma once

#include <string>

class TextureLoader
{
public:
    static unsigned int load2D(const std::string& path, bool useSRGB = false);
};
