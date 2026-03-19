#include "render/ShaderUtils.h"

#include <glad/gl.h>

#include <iostream>
#include <fstream>        // ← ОБЯЗАТЕЛЬНО
#include <sstream>        // ← ОБЯЗАТЕЛЬНО

static std::string loadTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "[ShaderUtils ERROR] Failed to open: " << path << std::endl;
        return {};
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}


unsigned int compileShaderFromFiles(
    const std::string& vertexPath,
    const std::string& fragmentPath
)
{
    std::string vertSrc = loadTextFile(vertexPath);
    std::string fragSrc = loadTextFile(fragmentPath);

    if (vertSrc.empty())
    {
        std::cerr
            << "[ShaderUtils ERROR] Vertex shader not found: "
            << vertexPath
            << std::endl;
        return 0;
    }

    if (fragSrc.empty())
    {
        std::cerr
            << "[ShaderUtils ERROR] Fragment shader not found: "
            << fragmentPath
            << std::endl;
        return 0;
    }

    unsigned int program =
        compileShader(vertSrc.c_str(), fragSrc.c_str());

    if (program == 0)
    {
        std::cerr
            << "[ShaderUtils ERROR] Shader compile FAILED\n"
            << "Vertex: " << vertexPath << "\n"
            << "Fragment: " << fragmentPath
            << std::endl;
    }

    return program;
}



static unsigned int compileSingle(GLenum type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "[ShaderUtils ERROR] Shader compile error:\n" << log << std::endl;
    }

    return shader;
}

unsigned int compileShader(const char* vertexSrc,
                           const char* fragmentSrc)
{
    unsigned int vs = compileSingle(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compileSingle(GL_FRAGMENT_SHADER, fragmentSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "[ShaderUtils ERROR] Shader link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}


unsigned int compileShaderFromFiles(
    const std::string& vertexPath,
    const std::string& geometryPath,
    const std::string& fragmentPath
)
{
    std::string vertSrc = loadTextFile(vertexPath);
    std::string geomSrc = loadTextFile(geometryPath);
    std::string fragSrc = loadTextFile(fragmentPath);

    if (vertSrc.empty() || geomSrc.empty() || fragSrc.empty())
    {
        std::cerr << "[ShaderUtils ERROR] Shader file missing\n";
        return 0;
    }

    unsigned int vs = compileSingle(GL_VERTEX_SHADER, vertSrc.c_str());
    unsigned int gs = compileSingle(GL_GEOMETRY_SHADER, geomSrc.c_str());
    unsigned int fs = compileSingle(GL_FRAGMENT_SHADER, fragSrc.c_str());

    unsigned int program = glCreateProgram();

    glAttachShader(program, vs);
    glAttachShader(program, gs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "[ShaderUtils ERROR] Shader link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);

    return program;
}

