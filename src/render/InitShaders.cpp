#include "ShaderLibrary.h"
#include "src/render/ShaderProgram.h"

void InitShaders()
{
    auto& shaders = ShaderLibrary::instance();

    shaders.load(
        "ship_wire",
        "assets/shaders/wiremesh/wireframe.vert",
        "assets/shaders/wiremesh/wireframe.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "mesh_fill",
        "assets/shaders/mesh/mesh_fill.vert",
        "assets/shaders/mesh/mesh_fill.frag",
        ShaderProgram::MeshFill
    );

    shaders.load(
        "edge_shader",
        "assets/shaders/mesh/edge.vert",
        "assets/shaders/mesh/edge.geom",
        "assets/shaders/mesh/edge.frag",
        ShaderProgram::Edge
    );

    shaders.load(
        "large_object_shader",
        "assets/shaders/mesh/large_object_fill.vert",
        "assets/shaders/mesh/large_object_fill.frag",
        ShaderProgram::LargeObjectFill
    );

    shaders.load(
        "ship_lines",
        "assets/shaders/wiremesh/ship_lines.vert",
        "assets/shaders/wiremesh/ship_lines.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "debug_lines",
        "assets/shaders/scene/debug_lines.vert",
        "assets/shaders/scene/debug_lines.frag",
        ShaderProgram::DebugLines
    );

    shaders.load(
        "ship_wire",
        "assets/shaders/wiremesh/ship_wire.vert",
        "assets/shaders/wiremesh/ship_wire.frag",
        ShaderProgram::Unknown
    );

    

    
}