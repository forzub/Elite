#version 330 core
layout (location = 0) in vec2 inPos;

uniform vec2 uCenterPx;
uniform float uRadiusPx;
uniform vec2 uViewport;

out vec2 vLocal;

void main()
{
    // локальные координаты для круга
    vLocal = inPos;

    // позиция в пикселях
    vec2 posPx = uCenterPx + inPos * uRadiusPx;

    // пиксели → NDC
    vec2 ndc;
    ndc.x =  (posPx.x / uViewport.x) * 2.0 - 1.0;
    ndc.y = -(posPx.y / uViewport.y) * 2.0 + 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
}
