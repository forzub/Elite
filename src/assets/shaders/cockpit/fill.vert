#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aLocal;
layout (location = 2) in vec4 aColor;

out vec2 vLocal;
out vec4 vColor;
uniform float uRotationDeg;
uniform vec2  uPivot;

void main()
{
    float a = radians(uRotationDeg);

    mat2 R = mat2(
        cos(a), -sin(a),
        sin(a),  cos(a)
    );

    vec2 pos = aPos - uPivot;
    pos = R * pos;
    pos += uPivot;

    gl_Position = vec4(pos, 0.0, 1.0);
}

