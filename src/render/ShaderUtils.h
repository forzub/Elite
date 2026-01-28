#pragma once

#include <string>          // ← ОБЯЗАТЕЛЬНО
#include <glad/gl.h>

unsigned int compileShader(const char* vertexSrc,
                           const char* fragmentSrc);

unsigned int compileShaderFromFiles(
    const std::string& vertexPath,
    const std::string& fragmentPath
);
