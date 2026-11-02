#version 330 core

// Instanced twin of edge.vert for the production thick / faded edges: feeds
// the same edge.geom + edge.frag, so batched edges look identical.
layout(location = 0) in vec3 inPos;
layout(location = 1) in float inWeight; // structural edge weight
layout(location = 4) in mat4 instanceModel;

uniform mat4 VP;

out vec3 worldPos;
out vec3 viewPos;
out float edgeWeight;

void main()
{
    vec4 worldPos4 = instanceModel * vec4(inPos, 1.0);
    worldPos = worldPos4.xyz;
    gl_Position = VP * worldPos4;
    viewPos = gl_Position.xyz;
    edgeWeight = inWeight;
}
