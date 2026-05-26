#version 330 core

layout(location = 0) in vec3 position;

// mat4 instanceModel занимает locations 4,5,6,7
layout(location = 4) in mat4 instanceModel;

uniform mat4 VP;
uniform vec3 cameraPos;

uniform float edgeFadeStart;
uniform float edgeFadeEnd;

out float vEdgeAlpha;

void main()
{
    vec4 worldPos = instanceModel * vec4(position, 1.0);
    gl_Position = VP * worldPos;

    float distToCamera = length(worldPos.xyz - cameraPos);

    // 1.0 рядом, 0.0 после edgeFadeEnd
    vEdgeAlpha =
        1.0 - smoothstep(edgeFadeStart, edgeFadeEnd, distToCamera);
}