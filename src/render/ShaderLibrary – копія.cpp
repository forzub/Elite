#include "ShaderLibrary.h"
#include <iostream>
#include "src/render/ShaderProgram.h"

extern GLuint compileShaderFromFiles(
    const std::string&,
    const std::string&
);

extern GLuint compileShaderFromFiles(
    const std::string&,
    const std::string&,
    const std::string&
);

ShaderLibrary& ShaderLibrary::instance()
{
    static ShaderLibrary lib;
    return lib;
}

GLuint ShaderLibrary::load(
    const std::string& name,
    const std::string& vert,
    const std::string& frag,
    ShaderProgram type
)
{
    std::cout << "[LIBRARY Loading shader:] " << name 
              << " (" << vert << ", " << frag << ")" << std::endl;

    GLuint program = compileShaderFromFiles(vert, frag);
    m_shaders[name] = {program, type, name};
    return program;
}

GLuint ShaderLibrary::load(
    const std::string& name,
    const std::string& vert,
    const std::string& geom,
    const std::string& frag,
    ShaderProgram type
)
{
    std::cout << "[LIBRARY Loading shader:] " << name 
              << " (" << vert << ", " << geom << ", " << frag << ")" << std::endl;

    GLuint program = compileShaderFromFiles(vert, geom, frag);
    m_shaders[name] = {program, type, name};
    return program;
}

GLuint ShaderLibrary::get(const std::string& name)
{
    auto it = m_shaders.find(name);
    return it != m_shaders.end() ? it->second.id : 0;
}

ShaderProgram ShaderLibrary::getType(GLuint program) const
{
    for (const auto& pair : m_shaders) {
        if (pair.second.id == program) {
            return pair.second.type;
        }
    }
    return ShaderProgram::Unknown;
}

ShaderProgram ShaderLibrary::getType(const std::string& name) const
{
    auto it = m_shaders.find(name);
    return it != m_shaders.end() ? it->second.type : ShaderProgram::Unknown;
}

std::string ShaderLibrary::getName(GLuint program) const
{
    for (const auto& pair : m_shaders) {
        if (pair.second.id == program) {
            return pair.second.name;
        }
    }
    return "";
}