#pragma once

#include <unordered_map>
#include <string>
#include <glad/gl.h>

#include "src/render/ShaderProgram.h"

struct ShaderEntry {
    GLuint id;
    ShaderProgram type;
    std::string name;
};

class ShaderLibrary
{
public:
    static ShaderLibrary& instance();  // только объявление

    GLuint load(
        const std::string& name,
        const std::string& vert,
        const std::string& frag,
        ShaderProgram type = ShaderProgram::Unknown
    );

    GLuint load(
        const std::string& name,
        const std::string& vert,
        const std::string& geom,
        const std::string& frag,
        ShaderProgram type = ShaderProgram::Unknown
    );

    GLuint get(const std::string& name);
    ShaderProgram getType(GLuint program) const;     
    ShaderProgram getType(const std::string& name) const; 
    std::string getName(GLuint program) const;       

private:
    std::unordered_map<std::string, ShaderEntry> m_shaders;  
};