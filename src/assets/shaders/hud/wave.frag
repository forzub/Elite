#version 330 core
in vec2 vLocal;
out vec4 FragColor;

uniform float uThickness;
uniform vec4  uColor;

void main()
{
    float dist = length(vLocal);   // 0 в центре, 1 на окружности
    float edge = abs(dist - 1.0);

    if (edge > uThickness)
        discard;

    FragColor = uColor;
}
