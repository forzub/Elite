#version 330 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float thickness;
uniform vec2 viewport;

void main()
{
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    vec2 a = p0.xy / p0.w;
    vec2 b = p1.xy / p1.w;

    vec2 dir = normalize(b - a);
    vec2 normal = vec2(-dir.y, dir.x);

    vec2 offset = normal * thickness / viewport;

    vec4 off0 = vec4(offset * p0.w, 0.0, 0.0);
    vec4 off1 = vec4(offset * p1.w, 0.0, 0.0);

    gl_Position = p0 + off0;
    EmitVertex();

    gl_Position = p0 - off0;
    EmitVertex();

    gl_Position = p1 + off1;
    EmitVertex();

    gl_Position = p1 - off1;
    EmitVertex();

    EndPrimitive();
}