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
        "mesh_fill_instanced",
        "assets/shaders/mesh/mesh_fill_instanced.vert",
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
        "edge_shader_instanced",
        "assets/shaders/mesh/edge_instanced.vert",
        "assets/shaders/mesh/edge_instanced.frag",
        ShaderProgram::MeshFill
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
        "planet_solid",
        "assets/shaders/planet/planet_solid.vert",
        "assets/shaders/planet/planet_solid.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "galaxy_starfield",
        "assets/shaders/starfield/galaxy_starfield.vert",
        "assets/shaders/starfield/galaxy_starfield.frag",
        ShaderProgram::Starfield
    );


    shaders.load(
        "system_map_lines",
        "assets/shaders/system_map/map_lines.vert",
        "assets/shaders/system_map/map_lines.frag",
        ShaderProgram::DebugLines
    );

    shaders.load(
        "system_map_background",
        "assets/shaders/system_map/map_background.vert",
        "assets/shaders/system_map/map_background.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "system_map_body_preview",
        "assets/shaders/system_map/map_body_preview.vert",
        "assets/shaders/system_map/map_body_preview.frag",
        ShaderProgram::Unknown
    );



    shaders.load(
        "celestial_cloud_texture_generate",
        "assets/shaders/celestial/cloud_texture_generate.vert",
        "assets/shaders/celestial/cloud_texture_generate.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "celestial_cloud_texture_blend",
        "assets/shaders/celestial/cloud_texture_blend.vert",
        "assets/shaders/celestial/cloud_texture_blend.frag",
        ShaderProgram::Unknown
    );


    shaders.load(
        "hub_planet_atmosphere",
        "assets/shaders/celestial/hub_planet_atmosphere.vert",
        "assets/shaders/celestial/hub_planet_atmosphere.frag",
        ShaderProgram::Unknown
    );


    shaders.load(
        "hub_planet_surface",
        "assets/shaders/celestial/hub_planet_surface.vert",
        "assets/shaders/celestial/hub_planet_surface.frag",
        ShaderProgram::Unknown
    );

    shaders.load(
        "planet_rings",
        "assets/shaders/celestial/planet_rings.vert",
        "assets/shaders/celestial/planet_rings.frag",
        ShaderProgram::Unknown
    );


    shaders.load(
        "galaxy_haze",
        "assets/shaders/starfield/galaxy_haze.vert",
        "assets/shaders/starfield/galaxy_haze.frag",
        ShaderProgram::Starfield
    );









    shaders.load(
        "ship_wire",
        "assets/shaders/wiremesh/ship_wire.vert",
        "assets/shaders/wiremesh/ship_wire.frag",
        ShaderProgram::Unknown
    );

    

    
}