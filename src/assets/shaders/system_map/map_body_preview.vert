#version 430 core

layout(location = 0) in vec3 aUnitPosition;
layout(location = 1) in vec2 aUv;

uniform mat4 uMVP;
uniform vec3 uBodyCenter;
uniform float uBodyRadius;
uniform vec3 uPrimeAxis;
uniform vec3 uNorthAxis;
uniform vec3 uEastAxis;
uniform vec4 uColor;

out vec2 vUv;
out vec4 vColor;

void main()
{
    vec3 bodyOffset =
        uPrimeAxis * aUnitPosition.x +
        uNorthAxis * aUnitPosition.y +
        uEastAxis * aUnitPosition.z;

    vec3 worldPosition =
        uBodyCenter + bodyOffset * uBodyRadius;

    gl_Position = uMVP * vec4(worldPosition, 1.0);
    vUv = aUv;
    vColor = uColor;
}
