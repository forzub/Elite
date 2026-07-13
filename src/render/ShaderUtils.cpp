#include "render/ShaderUtils.h"

#include <glad/gl.h>

#include <iostream>
#include <fstream>        // ← ОБЯЗАТЕЛЬНО
#include <sstream>        // ← ОБЯЗАТЕЛЬНО

#include <algorithm>
#include <string>

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



static unsigned int compileSingle(
    GLenum type,
    const char* src
)
{
    const unsigned int shader =
        glCreateShader(
            type
        );

    glShaderSource(
        shader,
        1,
        &src,
        nullptr
    );

    glCompileShader(
        shader
    );

    GLint success = GL_FALSE;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (success != GL_TRUE)
    {
        GLint logLength = 0;

        glGetShaderiv(
            shader,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        std::string log(
            static_cast<std::size_t>(
                std::max(
                    1,
                    logLength
                )
            ),
            '\0'
        );

        glGetShaderInfoLog(
            shader,
            static_cast<GLsizei>(
                log.size()
            ),
            nullptr,
            log.data()
        );

        std::cerr
            << "[ShaderUtils ERROR] Shader compile error:\n"
            << log
            << std::endl;

        glDeleteShader(
            shader
        );

        return 0;
    }

    return shader;
}







unsigned int compileShader(
    const char* vertexSrc,
    const char* fragmentSrc
)
{
    const unsigned int vertexShader =
        compileSingle(
            GL_VERTEX_SHADER,
            vertexSrc
        );

    if (vertexShader == 0)
    {
        return 0;
    }

    const unsigned int fragmentShader =
        compileSingle(
            GL_FRAGMENT_SHADER,
            fragmentSrc
        );

    if (fragmentShader == 0)
    {
        glDeleteShader(
            vertexShader
        );

        return 0;
    }

    const unsigned int program =
        glCreateProgram();

    glAttachShader(
        program,
        vertexShader
    );

    glAttachShader(
        program,
        fragmentShader
    );

    glLinkProgram(
        program
    );

    GLint success = GL_FALSE;

    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &success
    );

    glDeleteShader(
        vertexShader
    );

    glDeleteShader(
        fragmentShader
    );

    if (success != GL_TRUE)
    {
        GLint logLength = 0;

        glGetProgramiv(
            program,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        std::string log(
            static_cast<std::size_t>(
                std::max(
                    1,
                    logLength
                )
            ),
            '\0'
        );

        glGetProgramInfoLog(
            program,
            static_cast<GLsizei>(
                log.size()
            ),
            nullptr,
            log.data()
        );

        std::cerr
            << "[ShaderUtils ERROR] Shader link error:\n"
            << log
            << std::endl;

        glDeleteProgram(
            program
        );

        return 0;
    }

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

